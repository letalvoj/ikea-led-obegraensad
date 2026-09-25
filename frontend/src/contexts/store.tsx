import { createEventSignal } from "@solid-primitives/event-listener";
import { batch, createContext, createEffect, type JSX, useContext } from "solid-js";
import { createStore } from "solid-js/store";

import { createConnectionState, transport } from "../transport";
import {
  type CommandResult,
  type DeviceConfig,
  type DeviceInfo,
  type ScheduleItem,
  type Store,
  type StoreActions,
  SYSTEM_STATUS,
} from "../types";
import { ToastProvider } from "./toast";

const connectionState = createConnectionState();

const connectionStatus = ["Connecting", "Connected", "Disconnecting", "Disconnected"];

const [mainStore, setStore] = createStore<Store>({
  isActiveScheduler: false,
  rotation: 0,
  plugins: [],
  plugin: 1,
  brightness: 0,
  artnetUniverse: 1,
  GOLDelay: 150,
  indexMatrix: [...new Array(256)].map((_, i) => i),
  leds: [...new Array(256)].fill(0),
  systemStatus: SYSTEM_STATUS.NONE,
  connectionState,
  connectionStatus: connectionStatus[0],
  schedule: [],
});

const actions: StoreActions = {
  setIsActiveScheduler: (isActive) => setStore("isActiveScheduler", isActive),
  setRotation: (rotation) => setStore("rotation", rotation),
  setPlugins: (plugins) => setStore("plugins", plugins),
  setPlugin: (plugin) => setStore("plugin", plugin),
  setBrightness: (brightness) => setStore("brightness", brightness),
  setArtnetUniverse: (artnetUniverse) => setStore("artnetUniverse", artnetUniverse),
  setGOLDelay: (GOLDelay) => setStore("GOLDelay", GOLDelay),
  setIndexMatrix: (indexMatrix) => setStore("indexMatrix", indexMatrix),
  setLeds: (leds) => setStore("leds", leds),
  setSystemStatus: (systemStatus: SYSTEM_STATUS) => setStore("systemStatus", systemStatus),
  setSchedule: (items: ScheduleItem[]) => setStore("schedule", items),
  send: (message) => transport.send(message),
};

const store: [Store, StoreActions] = [mainStore, actions] as const;

const StoreContext = createContext<[Store, StoreActions]>(store);

const isValidNumber = (value: unknown): value is number =>
  typeof value === "number" && !Number.isNaN(value);

const isValidBoolean = (value: unknown): value is boolean => typeof value === "boolean";

const isValidArray = (value: unknown): value is unknown[] => Array.isArray(value);

export const StoreProvider = (props?: { value?: Store; children?: JSX.Element }) => {
  const messageEvent = createEventSignal<{ message: MessageEvent }>(transport, "message");
  const errorEvent = createEventSignal<{ error: Event }>(transport, "error");

  createEffect(() => {
    const state = connectionState();

    if (state >= 0 && state < connectionStatus.length) {
      setStore("connectionStatus", connectionStatus[state]);
    }

    if (state === WebSocket.CLOSED || state === WebSocket.CLOSING) {
      console.warn("Device disconnected.");
    }
  });

  createEffect(() => {
    const error = errorEvent();
    if (error) {
      console.error("Device connection error:", error);
    }
  });

  createEffect(() => {
    try {
      const data = messageEvent()?.data;
      if (data instanceof ArrayBuffer) {
        if (data.byteLength === 256) {
          actions.setLeds(Array.from(new Uint8Array(data)));
        }
        return;
      }

      const json = JSON.parse(data || "{}");

      if (!json.event || typeof json.event !== "string") {
        return;
      }

      switch (json.event) {
        case "info":
          batch(() => {
            if (
              isValidNumber(json.status) &&
              json.status >= 0 &&
              json.status < Object.values(SYSTEM_STATUS).length
            ) {
              actions.setSystemStatus(Object.values(SYSTEM_STATUS)[json.status]);
            }

            if (isValidNumber(json.rotation)) {
              actions.setRotation(json.rotation);
            }

            if (isValidNumber(json.brightness)) {
              actions.setBrightness(json.brightness);
            }

            if (isValidBoolean(json.scheduleActive)) {
              actions.setIsActiveScheduler(json.scheduleActive);
            }

            if (isValidArray(json.schedule)) {
              actions.setSchedule(json.schedule as ScheduleItem[]);
            }

            if (!mainStore.plugins.length && isValidArray(json.plugins)) {
              actions.setPlugins(json.plugins);
            }

            if (isValidNumber(json.plugin)) {
              actions.setPlugin(json.plugin);
            }

            if (mainStore.plugins.find((p) => p.id === mainStore.plugin)?.name === "Draw") {
              actions.setIndexMatrix([...new Array(256)].map((_, i) => i));
            }

            if (isValidArray(json.data)) {
              actions.setLeds(json.data as number[]);
            }
          });
          break;

        case "device":
          setStore("device", json as DeviceInfo);
          break;

        case "config":
          setStore("config", json as DeviceConfig);
          break;

        case "result":
          setStore("lastResult", json as CommandResult);
          break;
      }
    } catch (error) {
      console.error("Failed to parse device message:", error);
    }
  });

  return (
    <ToastProvider>
      <StoreContext.Provider value={store}>{props?.children}</StoreContext.Provider>
    </ToastProvider>
  );
};

export const useStore = () => {
  if (StoreContext) return useContext(StoreContext);

  return [{}, {}] as [Store, StoreActions];
};
