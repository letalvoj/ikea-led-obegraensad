#include "ble.h"

#ifdef ENABLE_BLE

#include <NimBLEDevice.h>
#include <Update.h>
// Tells the Arduino core not to release the BLE controller memory at boot.
// NimBLE-Arduino does not include it itself.
#include <esp32-hal-alloc-ble-mem.h>

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "commands.h"
#include "ota.h"

namespace
{
constexpr size_t MAX_MESSAGE_SIZE = 8192;
constexpr size_t MAX_OUTBOX_CHUNKS = 96;
constexpr size_t MAX_OTA_QUEUE_BYTES = 2 * BLE_OTA_WINDOW;
constexpr size_t NOTIFY_BURST = 6;
constexpr unsigned long LIVE_FRAME_INTERVAL_MS = 200;
constexpr uint16_t ATT_HEADER_SIZE = 3;

NimBLEServer *server = nullptr;
NimBLECharacteristic *txCharacteristic = nullptr;

// Connection state is written by the NimBLE host task and read by the main loop.
volatile uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
volatile uint16_t mtu = 23;
volatile bool authenticated = false;
volatile bool subscribed = false;
volatile bool disconnectedDuringOta = false;

// Reassembly buffers for incoming chunked messages, only touched by the host task.
std::string rxJson;
std::vector<uint8_t> rxFrame;

std::mutex outboxMutex;
std::deque<std::string> outbox;

struct OtaChunk
{
  uint32_t offset;
  std::vector<uint8_t> data;
};

std::mutex otaMutex;
std::deque<OtaChunk> otaQueue;
size_t otaQueuedBytes = 0;

struct OtaState
{
  bool active = false;
  size_t size = 0;
  size_t written = 0;
  size_t nextAck = 0;
  size_t lastNackAt = SIZE_MAX;
} ota;

bool readyToSend()
{
  return connHandle != BLE_HS_CONN_HANDLE_NONE && authenticated && subscribed;
}

void clearOutbox()
{
  std::lock_guard<std::mutex> lock(outboxMutex);
  outbox.clear();
}

void clearOtaQueue()
{
  std::lock_guard<std::mutex> lock(otaMutex);
  otaQueue.clear();
  otaQueuedBytes = 0;
}

// Split a payload into MTU-sized chunks, each prefixed with the kind/final header.
void enqueueChunks(uint8_t kind, const uint8_t *data, size_t len)
{
  size_t chunkPayload = mtu - ATT_HEADER_SIZE - 1;

  std::lock_guard<std::mutex> lock(outboxMutex);
  if (outbox.size() + len / chunkPayload + 1 > MAX_OUTBOX_CHUNKS)
  {
    Serial.println("[BLE] outbox full, dropping message");
    return;
  }

  size_t offset = 0;
  do
  {
    size_t n = std::min(chunkPayload, len - offset);
    bool final = offset + n == len;

    std::string chunk;
    chunk.reserve(n + 1);
    chunk.push_back(static_cast<char>(kind | (final ? BLE_FLAG_FINAL : 0)));
    chunk.append(reinterpret_cast<const char *>(data) + offset, n);
    outbox.push_back(std::move(chunk));

    offset += n;
  } while (offset < len);
}

void sendOtaStatus(const char *state, const char *error = nullptr)
{
  JsonDocument doc;
  doc["event"] = "ota";
  doc["state"] = state;
  doc["size"] = ota.size;
  doc["written"] = ota.written;
  doc["mtu"] = mtu;
  doc["window"] = BLE_OTA_WINDOW;
  if (error)
  {
    doc["error"] = error;
  }
  String output;
  serializeJson(doc, output);
  bleSendText(output);
}

void failOta(const char *error)
{
  Serial.printf("[BLE] OTA failed: %s\n", error);
  Update.abort();
  clearOtaQueue();
  ota.active = false;
  sendOtaStatus("error", error);
  onOTAEnd(false);
}

void processOtaChunk(const OtaChunk &chunk)
{
  if (chunk.offset != ota.written)
  {
    // a chunk got lost; report where we are once, the sender resumes from there
    if (ota.lastNackAt != ota.written)
    {
      ota.lastNackAt = ota.written;
      ota.nextAck = ota.written + BLE_OTA_WINDOW;
      sendOtaStatus("resend");
    }
    return;
  }

  size_t len = std::min(chunk.data.size(), ota.size - ota.written);
  if (Update.write(const_cast<uint8_t *>(chunk.data.data()), len) != len)
  {
    failOta(Update.errorString());
    return;
  }
  ota.written += len;
  ota.lastNackAt = SIZE_MAX;

  if (ota.written == ota.size)
  {
    if (!Update.end())
    {
      failOta(Update.errorString());
      return;
    }
    ota.active = false;
    Serial.println("[BLE] OTA complete, restarting");
    sendOtaStatus("done");
    requestRestart(1500);
  }
  else if (ota.written >= ota.nextAck)
  {
    ota.nextAck = ota.written + BLE_OTA_WINDOW;
    sendOtaStatus("progress");
  }
}

void processOtaQueue()
{
  if (disconnectedDuringOta)
  {
    disconnectedDuringOta = false;
    if (ota.active)
    {
      failOta("Bluetooth disconnected");
    }
  }

  // bounded per loop pass, flash writes are slow and the loop has other work
  for (int i = 0; i < 16 && ota.active; i++)
  {
    OtaChunk chunk;
    {
      std::lock_guard<std::mutex> lock(otaMutex);
      if (otaQueue.empty())
        return;
      chunk = std::move(otaQueue.front());
      otaQueue.pop_front();
      otaQueuedBytes -= chunk.data.size();
    }
    processOtaChunk(chunk);
  }
}

void drainOutbox()
{
  if (!readyToSend())
  {
    return;
  }

  for (size_t i = 0; i < NOTIFY_BURST; i++)
  {
    std::string chunk;
    {
      std::lock_guard<std::mutex> lock(outboxMutex);
      if (outbox.empty())
        return;
      chunk = outbox.front();
    }

    // notify() fails when the controller is out of buffers; keep the chunk and retry
    if (!txCharacteristic->notify(reinterpret_cast<const uint8_t *>(chunk.data()),
                                  chunk.size(),
                                  connHandle))
    {
      return;
    }

    std::lock_guard<std::mutex> lock(outboxMutex);
    outbox.pop_front();
  }
}

class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
  {
    connHandle = connInfo.getConnHandle();
    mtu = connInfo.getMTU();
    authenticated = connInfo.isEncrypted() && connInfo.isAuthenticated();
    subscribed = false;
    rxJson.clear();
    rxFrame.clear();
    clearOutbox();
    Serial.println("[BLE] connected");
  }

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
  {
    connHandle = BLE_HS_CONN_HANDLE_NONE;
    authenticated = false;
    subscribed = false;
    if (ota.active)
    {
      disconnectedDuringOta = true;
    }
    Serial.printf("[BLE] disconnected (reason %d)\n", reason);
  }

  void onMTUChange(uint16_t newMtu, NimBLEConnInfo &connInfo) override
  {
    mtu = newMtu;
  }

  uint32_t onPassKeyDisplay() override
  {
    return BLE_PASSKEY;
  }

  void onAuthenticationComplete(NimBLEConnInfo &connInfo) override
  {
    authenticated = connInfo.isEncrypted() && connInfo.isAuthenticated();
    if (!authenticated)
    {
      Serial.println("[BLE] pairing failed, disconnecting");
      server->disconnect(connInfo.getConnHandle());
      return;
    }
    Serial.println("[BLE] paired");
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override
  {
    NimBLEAttValue value = characteristic->getValue();
    if (value.size() < 1)
    {
      return;
    }

    uint8_t header = value[0];
    uint8_t kind = header & ~BLE_FLAG_FINAL;
    bool final = header & BLE_FLAG_FINAL;
    const uint8_t *payload = value.data() + 1;
    size_t len = value.size() - 1;

    if (kind == BLE_KIND_JSON)
    {
      if (rxJson.size() + len > MAX_MESSAGE_SIZE)
      {
        rxJson.clear();
        return;
      }
      rxJson.append(reinterpret_cast<const char *>(payload), len);
      if (final)
      {
        enqueueCommand(rxJson.data(), rxJson.size());
        rxJson.clear();
      }
    }
    else if (kind == BLE_KIND_FRAME)
    {
      if (rxFrame.size() + len > ROWS * COLS)
      {
        rxFrame.clear();
        return;
      }
      rxFrame.insert(rxFrame.end(), payload, payload + len);
      if (final)
      {
        showFrame(rxFrame.data(), rxFrame.size());
        rxFrame.clear();
      }
    }
  }
};

class TxCallbacks : public NimBLECharacteristicCallbacks
{
  void onSubscribe(NimBLECharacteristic *characteristic,
                   NimBLEConnInfo &connInfo,
                   uint16_t subValue) override
  {
    subscribed = subValue != 0;
    if (subscribed)
    {
      enqueueCommand("{\"event\":\"info\"}", 16);
    }
  }
};

class OtaCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override
  {
    NimBLEAttValue value = characteristic->getValue();
    if (!ota.active || value.size() <= 4)
    {
      return;
    }

    OtaChunk chunk;
    chunk.offset = value[0] | (value[1] << 8) | (value[2] << 16) | (uint32_t(value[3]) << 24);
    chunk.data.assign(value.data() + 4, value.data() + value.size());

    std::lock_guard<std::mutex> lock(otaMutex);
    // dropped chunks are detected by their offset and resent by the browser
    if (otaQueuedBytes + chunk.data.size() > MAX_OTA_QUEUE_BYTES)
    {
      return;
    }
    otaQueuedBytes += chunk.data.size();
    otaQueue.push_back(std::move(chunk));
  }
};

ServerCallbacks serverCallbacks;
RxCallbacks rxCallbacks;
TxCallbacks txCallbacks;
OtaCallbacks otaCallbacks;
} // namespace

void bleSetup()
{
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setPower(BLE_TX_POWER_DBM);
  NimBLEDevice::setMTU(517);

  // bonding + MITM protection: the OS asks for the passkey the first time
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(BLE_PASSKEY);

  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);
  server->advertiseOnDisconnect(true);

  NimBLEService *service = server->createService(BLE_SERVICE_UUID);

  NimBLECharacteristic *rx = service->createCharacteristic(
      BLE_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
  rx->setCallbacks(&rxCallbacks);

  txCharacteristic = service->createCharacteristic(
      BLE_TX_UUID,
      NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC |
          NIMBLE_PROPERTY::READ_AUTHEN);
  txCharacteristic->setCallbacks(&txCallbacks);

  NimBLECharacteristic *otaCharacteristic = service->createCharacteristic(
      BLE_OTA_UUID,
      NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
  otaCharacteristic->setCallbacks(&otaCallbacks);

  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->setName(BLE_DEVICE_NAME);
  advertising->enableScanResponse(true);
  advertising->start();

  Serial.println("[BLE] advertising as " BLE_DEVICE_NAME);
}

void bleLoop()
{
  processOtaQueue();
  drainOutbox();
}

bool bleConnected()
{
  return readyToSend();
}

void bleDescribe(JsonObject out)
{
  out["name"] = BLE_DEVICE_NAME;
  out["address"] = NimBLEDevice::getAddress().toString();
  out["connected"] = connHandle != BLE_HS_CONN_HANDLE_NONE;
  out["paired"] = (bool)authenticated;
  out["mtu"] = mtu;
  out["bonds"] = NimBLEDevice::getNumBonds();
  out["txPowerDbm"] = BLE_TX_POWER_DBM;
}

void bleSendText(const String &message)
{
  if (!readyToSend())
  {
    return;
  }
  enqueueChunks(BLE_KIND_JSON, reinterpret_cast<const uint8_t *>(message.c_str()), message.length());
}

void bleSendFrame(const uint8_t *frame, size_t len)
{
  static unsigned long lastFrame = 0;
  if (!readyToSend() || ota.active || millis() - lastFrame < LIVE_FRAME_INTERVAL_MS)
  {
    return;
  }

  {
    // live preview is best effort; never let it queue up behind real messages
    std::lock_guard<std::mutex> lock(outboxMutex);
    if (!outbox.empty())
      return;
  }

  lastFrame = millis();
  enqueueChunks(BLE_KIND_FRAME, frame, len);
}

void bleOtaBegin(size_t size, const char *md5)
{
  if (ota.active)
  {
    Update.abort();
  }
  clearOtaQueue();
  ota = OtaState{};

  if (size == 0 || !Update.begin(size, U_FLASH))
  {
    sendOtaStatus("error", size == 0 ? "Missing size" : Update.errorString());
    return;
  }
  if (strlen(md5) == 32)
  {
    Update.setMD5(md5);
  }

  ota.active = true;
  ota.size = size;
  ota.nextAck = BLE_OTA_WINDOW;
  onOTAStart();
  Serial.printf("[BLE] OTA started, %u bytes\n", size);
  sendOtaStatus("ready");
}

void bleOtaAbort()
{
  if (ota.active)
  {
    failOta("Aborted");
  }
}

void bleOtaReportStatus()
{
  sendOtaStatus(ota.active ? "progress" : "idle");
}

#endif
