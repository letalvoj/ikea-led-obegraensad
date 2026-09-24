export enum SYSTEM_STATUS {
  NONE = "draw",
  WSBINARY = "wsbinary",
  // SYSTEM
  UPDATE = "update",
  LOADING = "loading",
}

export interface ScheduleItem {
  pluginId: number;
  duration: number;
}

/** Persisted device settings ("config" event, mirrors Config::toJson on the device). */
export interface DeviceConfig {
  weatherLocation: string;
  ntpServer: string;
  tzInfo: string;
  autoStartSchedule: boolean;
  wifiEnabled: boolean;
}

/** Diagnostics ("device" event, mirrors buildDeviceInfo on the device). */
export interface DeviceInfo {
  firmware: string;
  uptime: number;
  freeHeap: number;
  resetReason: string;
  epoch: number;
  tzInfo: string;
  wifi: {
    enabled: boolean;
    state: string;
    connected: boolean;
    ssid: string;
    portalSsid: string;
    hostname: string;
    macAddress: string;
    ipAddress?: string;
    rssi?: number;
  };
  ble?: {
    name: string;
    address: string;
    connected: boolean;
    paired: boolean;
    mtu: number;
    bonds: number;
    txPowerDbm: number;
  };
}

/** Outcome of a command ("result" event). */
export interface CommandResult {
  command: string;
  ok: boolean;
  message?: string;
}

export interface StoreActions {
  setIsActiveScheduler: (isActive: boolean) => void;
  setRotation: (rotation: number) => void;
  setPlugins: (plugins: []) => void;
  setPlugin: (plugin: number) => void;
  setBrightness: (brightness: number) => void;
  setIndexMatrix: (indexMatrix: number[]) => void;
  setLeds: (leds: number[]) => void;
  setSystemStatus: (systemStatus: SYSTEM_STATUS) => void;
  setSchedule: (items: ScheduleItem[]) => void;
  setArtnetUniverse: (artnetUniverse: number) => void;
  setGOLDelay: (GOLDelay: number) => void;
  send: (message: string | ArrayBuffer) => void;
}

export interface Store {
  isActiveScheduler: boolean;
  rotation: number;
  brightness: number;
  indexMatrix: number[];
  leds: number[];
  plugins: { id: number; name: string }[];
  plugin: number;
  artnetUniverse: number;
  GOLDelay: number;
  systemStatus: SYSTEM_STATUS;
  connectionState: () => number;
  connectionStatus?: string;
  schedule: ScheduleItem[];
  device?: DeviceInfo;
  config?: DeviceConfig;
  lastResult?: CommandResult;
}

export interface IToastContext {
  toast: (text: string, duration: number) => void;
}
