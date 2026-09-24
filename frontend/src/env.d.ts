interface ImportMetaEnv {
  /** "ble" builds the Web Bluetooth variant of the UI, anything else talks WebSocket */
  readonly VITE_TRANSPORT?: string;
  readonly VITE_WS_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
