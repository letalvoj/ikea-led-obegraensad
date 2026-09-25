import { type Component, createSignal, onMount, Show } from "solid-js";

import { useStore } from "../contexts/store";
import { bleTransport } from "../transport";
import { CONNECTING, isBluetoothAvailable, PAIRING_CODE } from "../transport/ble";
import Button from "./button";
import {
  AssemblyStep,
  CodeDrawing,
  LampDrawing,
  PairDrawing,
  PriceTag,
  RoomHero,
} from "./catalogue";

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

  const connecting = () => store.connectionState() === CONNECTING;

  return (
    <div class="max-w-3xl mx-auto space-y-8 text-left">
      <RoomHero>
        <h2 class="page-title text-inherit! max-w-[14ch]">
          {connecting() ? "Connecting..." : "No WiFi needed. Just Bluetooth."}
        </h2>
      </RoomHero>

      <p class="text-sm text-muted max-w-xl">
        Three steps, no tools. Works in Chrome or Edge, anywhere within a couple of metres of the
        lamp.
      </p>

      <ol class="grid grid-cols-1 sm:grid-cols-3 gap-6 sm:gap-8 bg-white p-5 lg:p-8">
        <AssemblyStep step={1} drawing={<LampDrawing class="w-full h-full" />}>
          Plug in the lamp. It starts drawing right away.
        </AssemblyStep>
        <AssemblyStep step={2} drawing={<PairDrawing class="w-full h-full" />}>
          Press <b>Connect</b> and pick <b>IKEA-LED</b>.
        </AssemblyStep>
        <AssemblyStep step={3} drawing={<CodeDrawing class="w-full h-full" />}>
          First time only: enter the pairing code
          <br />
          <PriceTag class="text-2xl mt-2">{PAIRING_CODE}</PriceTag>
        </AssemblyStep>
      </ol>

      <Show
        when={isBluetoothAvailable()}
        fallback={
          <p class="text-sm bg-accent p-3">
            This browser cannot talk Bluetooth. Open this page in Chrome or Edge on a desktop or
            Android device.
          </p>
        }
      >
        <Button type="button" class="py-3 text-base" onClick={connect} disabled={connecting()}>
          <i class="fa-brands fa-bluetooth-b" />
          {connecting() ? "Connecting..." : "Connect"}
        </Button>
      </Show>

      <Show when={error()}>
        <p class="text-sm bg-red-600 text-white p-3">{error()}</p>
      </Show>
    </div>
  );
};
