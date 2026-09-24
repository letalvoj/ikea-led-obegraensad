import { type Component, createSignal, onMount, Show } from "solid-js";

import { useStore } from "../contexts/store";
import { bleTransport } from "../transport";
import { CONNECTING, isBluetoothAvailable } from "../transport/ble";
import Button from "./button";

/** Connect screen of the Bluetooth build, shown until a device is paired and open. */
export const BluetoothConnect: Component = () => {
  const [store] = useStore();
  const [error, setError] = createSignal("");

  // Chrome remembers granted devices; reconnect silently when it can.
  onMount(() => {
    void bleTransport?.reconnectKnownDevice();
  });

  const connect = async () => {
    setError("");
    try {
      await bleTransport?.connect();
    } catch (err) {
      // closing the picker is not an error worth showing
      if (err instanceof DOMException && err.name === "NotFoundError") {
        return;
      }
      setError(err instanceof Error ? err.message : String(err));
    }
  };

  return (
    <div class="max-w-md mx-auto space-y-5">
      <h2 class="text-4xl">
        {store.connectionState() === CONNECTING ? "Connecting..." : "OBEGRÄNSAD over Bluetooth"}
      </h2>

      <Show
        when={isBluetoothAvailable()}
        fallback={
          <p class="text-sm">
            This browser does not support Web Bluetooth. Open this page in Chrome or Edge on a
            desktop or Android device.
          </p>
        }
      >
        <Show when={store.connectionState() !== CONNECTING}>
          <Button type="button" class="py-3 text-base" onClick={connect}>
            <i class="fa-brands fa-bluetooth-b mr-2" />
            Connect to lamp
          </Button>
        </Show>
        <p class="text-sm">
          The first time, your computer asks for a pairing code: enter <b>003762</b>.
        </p>
      </Show>

      <Show when={error()}>
        <p class="text-sm bg-red-600/80 rounded p-2">{error()}</p>
      </Show>
    </div>
  );
};
