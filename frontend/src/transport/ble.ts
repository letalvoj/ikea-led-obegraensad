// Web Bluetooth transport for the lamp. It mimics the parts of the WebSocket API the
// store relies on (readyState, send, open/close/message events), so the rest of the
// UI does not care whether it talks over WiFi or Bluetooth.
//
// Wire format, mirrored in include/ble.h on the firmware side: messages are split
// into chunks, each prefixed with one header byte (kind | FINAL on the last chunk).

const SERVICE_UUID = "0b5e0001-1ea0-4d1a-9b7e-0bec0ffee000";
const RX_UUID = "0b5e0002-1ea0-4d1a-9b7e-0bec0ffee000";
const TX_UUID = "0b5e0003-1ea0-4d1a-9b7e-0bec0ffee000";
const OTA_UUID = "0b5e0004-1ea0-4d1a-9b7e-0bec0ffee000";

const KIND_JSON = 0x01;
const KIND_FRAME = 0x02;
const FLAG_FINAL = 0x80;

// Stays within the ATT payload of the smallest MTU browsers commonly negotiate
// (185 on macOS); larger platforms simply use more, smaller writes than necessary.
const WRITE_CHUNK = 180;

const ESP_IMAGE_MAGIC = 0xe9;
const OTA_ACK_TIMEOUT_MS = 5000;
const OTA_READY_TIMEOUT_MS = 20000;

export const CONNECTING = 0;
export const OPEN = 1;
export const CLOSING = 2;
export const CLOSED = 3;

export interface OtaStatus {
  event: "ota";
  state: "ready" | "progress" | "resend" | "done" | "error" | "idle";
  size: number;
  written: number;
  mtu: number;
  window: number;
  error?: string;
}

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export const isBluetoothAvailable = () => typeof navigator !== "undefined" && !!navigator.bluetooth;

export class BleTransport extends EventTarget {
  readyState = CLOSED;

  private device?: BluetoothDevice;
  private rx?: BluetoothRemoteGATTCharacteristic;
  private tx?: BluetoothRemoteGATTCharacteristic;
  private ota?: BluetoothRemoteGATTCharacteristic;

  // GATT operations must not overlap, so every write goes through this chain.
  private writeChain: Promise<unknown> = Promise.resolve();
  private rxText: number[] = [];
  private rxFrame: number[] = [];
  private closedByUser = false;

  private otaEvents: OtaStatus[] = [];
  private otaWaiter?: (status: OtaStatus) => void;

  get deviceName() {
    return this.device?.name ?? "";
  }

  /** Opens the browser's device picker. Must be called from a user gesture. */
  async connect() {
    if (!isBluetoothAvailable()) {
      throw new Error("This browser does not support Web Bluetooth. Use Chrome or Edge.");
    }

    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }, { namePrefix: "IKEA" }],
      optionalServices: [SERVICE_UUID],
    });
    await this.attach(device);
  }

  /** Reconnects to a device this origin was granted before, without the picker. */
  async reconnectKnownDevice() {
    // once a device is attached, reconnecting is handled by onDisconnected()
    if (this.device || !isBluetoothAvailable() || !navigator.bluetooth.getDevices) {
      return false;
    }
    const [device] = await navigator.bluetooth.getDevices();
    if (!device) {
      return false;
    }
    try {
      await this.attach(device);
      return true;
    } catch {
      this.setState(CLOSED);
      return false;
    }
  }

  close() {
    this.closedByUser = true;
    this.setState(CLOSING);
    this.device?.gatt?.disconnect();
  }

  send(data: string | ArrayBuffer) {
    if (this.readyState !== OPEN) {
      return;
    }
    const chunks =
      typeof data === "string"
        ? this.toChunks(KIND_JSON, data)
        : this.toChunks(KIND_FRAME, new Uint8Array(data));

    this.writeChain = this.writeChain
      .then(() => this.writeChunks(chunks))
      .catch((error) => {
        console.error("Bluetooth write failed:", error);
        this.dispatchEvent(new Event("error"));
      });
  }

  /**
   * Streams a firmware image to the device. The device acknowledges every window of
   * bytes; lost chunks are detected by their offset and resent from the last ack.
   */
  async uploadFirmware(image: Uint8Array, onProgress: (written: number, total: number) => void) {
    if (image[0] !== ESP_IMAGE_MAGIC) {
      throw new Error("This is not an ESP32 firmware image (.bin)");
    }
    const ota = this.ota;
    if (!ota || this.readyState !== OPEN) {
      throw new Error("Not connected");
    }

    this.otaEvents = [];
    this.send(JSON.stringify({ event: "ota-begin", size: image.length }));
    const ready = await this.nextOtaStatus(OTA_READY_TIMEOUT_MS);
    if (ready.state !== "ready") {
      throw new Error(ready.error ?? `Device refused the update (${ready.state})`);
    }

    const chunkSize = ready.mtu - 3 - 4; // ATT header, offset prefix
    let acked = 0;
    while (acked < image.length) {
      this.otaEvents = [];
      const windowEnd = Math.min(image.length, acked + ready.window);
      for (let offset = acked; offset < windowEnd; offset += chunkSize) {
        const data = image.subarray(offset, Math.min(offset + chunkSize, windowEnd));
        const packet = new Uint8Array(4 + data.length);
        new DataView(packet.buffer).setUint32(0, offset, true);
        packet.set(data, 4);
        await ota.writeValueWithoutResponse(packet);
      }

      const status = await this.awaitOtaAck();
      if (status.state === "error") {
        throw new Error(status.error ?? "Update failed");
      }
      acked = status.written;
      onProgress(acked, image.length);
      if (status.state === "done") {
        return;
      }
    }

    const final = await this.nextOtaStatus(OTA_ACK_TIMEOUT_MS);
    if (final.state !== "done") {
      throw new Error(final.error ?? "Update did not complete");
    }
  }

  abortFirmwareUpload() {
    this.send(JSON.stringify({ event: "ota-abort" }));
  }

  private async attach(device: BluetoothDevice) {
    this.closedByUser = false;
    this.setState(CONNECTING);

    if (this.device !== device) {
      this.device = device;
      device.addEventListener("gattserverdisconnected", () => this.onDisconnected());
    }

    const gatt = device.gatt;
    if (!gatt) {
      throw new Error("Device has no GATT server");
    }

    const server = await gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    this.rx = await service.getCharacteristic(RX_UUID);
    this.tx = await service.getCharacteristic(TX_UUID);
    this.ota = await service.getCharacteristic(OTA_UUID);

    const tx = this.tx;
    tx.addEventListener("characteristicvaluechanged", this.onNotification);
    await this.retry(() => tx.startNotifications());

    // The characteristics require an authenticated link, so this first write triggers
    // the OS pairing dialog (passkey 003762). Retry while the user is typing it.
    const clock = JSON.stringify({ event: "time", epoch: Math.floor(Date.now() / 1000) });
    await this.retry(() => this.writeChunks(this.toChunks(KIND_JSON, clock)));

    this.setState(OPEN);
    this.dispatchEvent(new Event("open"));
    this.send(JSON.stringify({ event: "info" }));
  }

  private async retry<T>(operation: () => Promise<T>, attempts = 10) {
    for (let attempt = 1; ; attempt++) {
      try {
        return await operation();
      } catch (error) {
        if (attempt >= attempts) {
          throw error;
        }
        await sleep(2000);
      }
    }
  }

  private onDisconnected() {
    this.rx = this.tx = this.ota = undefined;
    this.rxText = [];
    this.rxFrame = [];
    this.setState(CLOSED);
    this.dispatchEvent(new CloseEvent("close"));

    if (!this.closedByUser && this.device) {
      void this.reconnect(this.device);
    }
  }

  private async reconnect(device: BluetoothDevice) {
    for (let delay = 1000; delay <= 30000 && !this.closedByUser; delay *= 2) {
      await sleep(delay);
      try {
        await this.attach(device);
        return;
      } catch {
        this.setState(CLOSED);
      }
    }
  }

  private setState(state: number) {
    if (this.readyState !== state) {
      this.readyState = state;
      this.dispatchEvent(new Event("statechange"));
    }
  }

  private toChunks(kind: number, message: string | Uint8Array) {
    const payload = typeof message === "string" ? new TextEncoder().encode(message) : message;
    const chunks: Uint8Array<ArrayBuffer>[] = [];
    let offset = 0;
    do {
      const data = payload.subarray(offset, offset + WRITE_CHUNK);
      offset += data.length;
      const chunk = new Uint8Array(data.length + 1);
      chunk[0] = kind | (offset >= payload.length ? FLAG_FINAL : 0);
      chunk.set(data, 1);
      chunks.push(chunk);
    } while (offset < payload.length);
    return chunks;
  }

  private async writeChunks(chunks: Uint8Array<ArrayBuffer>[]) {
    for (const chunk of chunks) {
      await this.rx?.writeValueWithResponse(chunk);
    }
  }

  private onNotification = (event: Event) => {
    const value = (event.target as BluetoothRemoteGATTCharacteristic).value;
    if (!value || value.byteLength < 1) {
      return;
    }

    const header = value.getUint8(0);
    const kind = header & ~FLAG_FINAL;
    const bytes = new Uint8Array(value.buffer, value.byteOffset + 1, value.byteLength - 1);
    const buffer = kind === KIND_FRAME ? this.rxFrame : this.rxText;
    buffer.push(...bytes);

    if (!(header & FLAG_FINAL)) {
      return;
    }

    const message = Uint8Array.from(buffer);
    buffer.length = 0;

    if (kind === KIND_FRAME) {
      this.dispatchEvent(new MessageEvent("message", { data: message.buffer }));
      return;
    }

    const text = new TextDecoder().decode(message);
    this.captureOtaStatus(text);
    this.dispatchEvent(new MessageEvent("message", { data: text }));
  };

  private captureOtaStatus(text: string) {
    if (!text.includes('"event":"ota"')) {
      return;
    }
    const status = JSON.parse(text) as OtaStatus;
    if (this.otaWaiter) {
      this.otaWaiter(status);
      this.otaWaiter = undefined;
    } else {
      this.otaEvents.push(status);
    }
  }

  private nextOtaStatus(timeoutMs: number) {
    const queued = this.otaEvents.shift();
    if (queued) {
      return Promise.resolve(queued);
    }
    return new Promise<OtaStatus>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.otaWaiter = undefined;
        reject(new Error("The device stopped responding"));
      }, timeoutMs);
      this.otaWaiter = (status) => {
        clearTimeout(timer);
        resolve(status);
      };
    });
  }

  // Waits for the ack of the window just sent; asks for the status if it got lost.
  private async awaitOtaAck() {
    for (let attempt = 0; attempt < 3; attempt++) {
      try {
        return await this.nextOtaStatus(OTA_ACK_TIMEOUT_MS);
      } catch {
        this.send(JSON.stringify({ event: "ota-status" }));
      }
    }
    throw new Error("The device stopped acknowledging the upload");
  }
}
