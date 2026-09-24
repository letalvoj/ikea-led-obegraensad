import { createReconnectingWS, createWSState } from "@solid-primitives/websocket";
import { type Accessor, createSignal } from "solid-js";

import { BleTransport } from "./ble";

export const IS_BLE = import.meta.env.VITE_TRANSPORT === "ble";

/** What the store needs from a connection, satisfied by WebSocket and BleTransport. */
export type DeviceTransport = EventTarget & {
  readonly readyState: number;
  send(data: string | ArrayBuffer): void;
};

const createWebSocket = () => {
  const url = import.meta.env.PROD
    ? `${window.location.protocol === "https:" ? "wss:" : "ws:"}//${window.location.host}/`
    : import.meta.env.VITE_WS_URL;
  const ws = createReconnectingWS(`${url}ws`);
  ws.addEventListener("open", (event) => {
    (event.currentTarget as WebSocket).binaryType = "arraybuffer";
  });
  return ws;
};

/** Only set in the Bluetooth build; used for connecting and firmware uploads. */
export const bleTransport = IS_BLE ? new BleTransport() : undefined;

export const transport: DeviceTransport = bleTransport ?? createWebSocket();

/** WebSocket-style readyState (0 connecting, 1 open, 2 closing, 3 closed) as a signal. */
export const createConnectionState = (): Accessor<number> => {
  if (!bleTransport) {
    return createWSState(transport as WebSocket);
  }
  const [state, setState] = createSignal(bleTransport.readyState);
  bleTransport.addEventListener("statechange", () => setState(bleTransport.readyState));
  return state;
};
