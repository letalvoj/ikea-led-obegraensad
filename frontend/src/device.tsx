import {
  type Component,
  createEffect,
  createSignal,
  type JSX,
  on,
  onCleanup,
  onMount,
  Show,
} from "solid-js";

import Button from "./components/button";
import { Layout } from "./components/layout/layout";
import { useStore } from "./contexts/store";
import { useToast } from "./contexts/toast";
import { bleTransport, IS_BLE } from "./transport";
import type { DeviceConfig } from "./types";

const DEVICE_INFO_REFRESH_MS = 5000;

const inputClass =
  "w-full px-3 py-2 bg-paper border border-line rounded focus:ring-2 focus:ring-brand outline-none";

const Card: Component<{ title: string; children: JSX.Element }> = (props) => (
  <section class="bg-white p-4 lg:p-6 space-y-4">
    <h3 class="section-title">{props.title}</h3>
    {props.children}
  </section>
);

const Row: Component<{ label: string; children: JSX.Element }> = (props) => (
  <div class="flex justify-between gap-4 text-sm">
    <span class="text-muted">{props.label}</span>
    <span class="text-ink text-right break-all">{props.children}</span>
  </div>
);

const formatUptime = (seconds: number) => {
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return `${d ? `${d}d ` : ""}${h}h ${m}m`;
};

const useDeviceCommands = () => {
  const [, actions] = useStore();
  return (event: string, payload: object = {}) =>
    actions.send(JSON.stringify({ event, ...payload }));
};

const DeviceInfoCard: Component = () => {
  const [store] = useStore();
  const command = useDeviceCommands();

  return (
    <Card title="Device">
      <Show when={store.device} fallback={<p class="text-sm text-muted">Loading...</p>}>
        {(device) => (
          <div class="space-y-1">
            <Row label="Firmware">{device().firmware}</Row>
            <Row label="Uptime">{formatUptime(device().uptime)}</Row>
            <Row label="Free memory">{Math.round(device().freeHeap / 1024)} KB</Row>
            <Row label="Last restart">{device().resetReason}</Row>
            <Row label="Clock">
              {device().epoch > 1_600_000_000
                ? new Date(device().epoch * 1000).toLocaleString()
                : "not set"}
            </Row>
            <Row label="WiFi">
              {device().wifi.state}
              {device().wifi.ssid ? ` (${device().wifi.ssid})` : ""}
            </Row>
            <Show when={device().wifi.ipAddress}>
              <Row label="IP address">
                {device().wifi.ipAddress} · {device().wifi.rssi} dBm
              </Row>
            </Show>
            <Show when={device().ble}>
              {(ble) => (
                <Row label="Bluetooth">
                  {ble().name} · MTU {ble().mtu} · {ble().txPowerDbm} dBm
                </Row>
              )}
            </Show>
          </div>
        )}
      </Show>
      <Button onClick={() => command("time", { epoch: Math.floor(Date.now() / 1000) })} widthAuto>
        <i class="fa-regular fa-clock mr-2" />
        Sync clock with this computer
      </Button>
    </Card>
  );
};

const SettingsCard: Component = () => {
  const [store] = useStore();
  const command = useDeviceCommands();
  const [draft, setDraft] = createSignal<DeviceConfig>();

  // (re)load the form whenever the device reports its config
  createEffect(() => {
    if (store.config) {
      setDraft({ ...store.config });
    }
  });

  const update = <K extends keyof DeviceConfig>(key: K, value: DeviceConfig[K]) => {
    const current = draft();
    if (current) {
      setDraft({ ...current, [key]: value });
    }
  };

  const save = () => {
    const config = draft();
    if (!config) {
      return;
    }
    if (config.wifiEnabled !== store.config?.wifiEnabled) {
      const change = config.wifiEnabled ? "enable" : "disable";
      if (!confirm(`The lamp restarts to ${change} WiFi. Continue?`)) {
        return;
      }
    }
    command("config-set", { config });
  };

  return (
    <Card title="Settings">
      <Show when={draft()} fallback={<p class="text-sm text-muted">Loading...</p>}>
        {(config) => (
          <div class="space-y-3">
            <label class="flex items-center gap-2 text-sm">
              <input
                type="checkbox"
                checked={config().wifiEnabled}
                onChange={(e) => update("wifiEnabled", e.currentTarget.checked)}
              />
              WiFi enabled (turn off to run on Bluetooth only)
            </label>
            <label class="block text-sm">
              Weather location
              <input
                class={inputClass}
                value={config().weatherLocation}
                onInput={(e) => update("weatherLocation", e.currentTarget.value)}
              />
            </label>
            <label class="block text-sm">
              NTP server
              <input
                class={inputClass}
                value={config().ntpServer}
                onInput={(e) => update("ntpServer", e.currentTarget.value)}
              />
            </label>
            <label class="block text-sm">
              Timezone (POSIX, e.g. CET-1CEST,M3.5.0,M10.5.0/3)
              <input
                class={inputClass}
                value={config().tzInfo}
                onInput={(e) => update("tzInfo", e.currentTarget.value)}
              />
            </label>
            <label class="flex items-center gap-2 text-sm">
              <input
                type="checkbox"
                checked={config().autoStartSchedule}
                onChange={(e) => update("autoStartSchedule", e.currentTarget.checked)}
              />
              Start the schedule automatically after boot
            </label>
            <div class="flex gap-2">
              <Button onClick={save}>Save</Button>
              <Button
                onClick={() =>
                  confirm("Reset all settings to defaults?") && command("config-reset")
                }
              >
                Reset to defaults
              </Button>
            </div>
          </div>
        )}
      </Show>
    </Card>
  );
};

const WifiNetworkCard: Component = () => {
  const command = useDeviceCommands();
  const [ssid, setSsid] = createSignal("");
  const [password, setPassword] = createSignal("");

  return (
    <Card title="WiFi network">
      <form
        class="space-y-3"
        onSubmit={(e) => {
          e.preventDefault();
          command("wifi-credentials", { ssid: ssid(), password: password() });
          setPassword("");
        }}
      >
        <label class="block text-sm">
          Network name
          <input
            class={inputClass}
            value={ssid()}
            required
            onInput={(e) => setSsid(e.currentTarget.value)}
          />
        </label>
        <label class="block text-sm">
          Password
          <input
            class={inputClass}
            type="password"
            value={password()}
            onInput={(e) => setPassword(e.currentTarget.value)}
          />
        </label>
        <div class="flex gap-2">
          <Button type="submit">Connect</Button>
          <Button
            type="button"
            onClick={() => confirm("Forget the saved WiFi network?") && command("wifi-forget")}
          >
            Forget network
          </Button>
        </div>
      </form>
    </Card>
  );
};

const MessageCard: Component = () => {
  const command = useDeviceCommands();
  const [text, setText] = createSignal("");
  const [repeat, setRepeat] = createSignal(1);

  return (
    <Card title="Scrolling message">
      <form
        class="space-y-3"
        onSubmit={(e) => {
          e.preventDefault();
          command("message", { text: text(), repeat: repeat() });
          setText("");
        }}
      >
        <input
          class={inputClass}
          placeholder="Hello from the other side"
          value={text()}
          required
          onInput={(e) => setText(e.currentTarget.value)}
        />
        <label class="block text-sm">
          Repeat
          <input
            class={inputClass}
            type="number"
            min="0"
            max="100"
            value={repeat()}
            onInput={(e) => setRepeat(parseInt(e.currentTarget.value, 10) || 0)}
          />
        </label>
        <Button type="submit">Show message</Button>
      </form>
    </Card>
  );
};

const FirmwareCard: Component = () => {
  const { toast } = useToast();
  const [file, setFile] = createSignal<File>();
  const [progress, setProgress] = createSignal<number>();
  const [uploading, setUploading] = createSignal(false);

  const upload = async () => {
    const image = file();
    if (!image || !bleTransport) {
      return;
    }
    setUploading(true);
    setProgress(0);
    try {
      await bleTransport.uploadFirmware(
        new Uint8Array(await image.arrayBuffer()),
        (written, total) => setProgress(written / total),
      );
      toast("Firmware uploaded, the lamp is restarting", 5000);
    } catch (err) {
      bleTransport.abortFirmwareUpload();
      toast(`Update failed: ${err instanceof Error ? err.message : err}`, 6000);
    } finally {
      setUploading(false);
    }
  };

  return (
    <Card title="Firmware update">
      <Show
        when={IS_BLE}
        fallback={
          <a href="/update" class="text-sm text-ink underline">
            Open the WiFi firmware update page
          </a>
        }
      >
        <p class="text-sm text-muted">
          Upload a <code>firmware.bin</code> built for this board. It takes a minute or two over
          Bluetooth. If the new firmware fails to start, the lamp returns to the current one. The
          latest build for esp32dev is published next to this page:{" "}
          <a href="firmware-esp32dev.bin" download="firmware-esp32dev.bin" class="underline">
            firmware-esp32dev.bin
          </a>
          .
        </p>
        <input
          type="file"
          accept=".bin"
          disabled={uploading()}
          onChange={(e) => setFile(e.currentTarget.files?.[0])}
          class="text-sm"
        />
        <Show when={progress() !== undefined}>
          <div class="h-2 bg-line rounded">
            <div
              class="h-2 bg-brand rounded transition-all"
              style={{ width: `${Math.round((progress() ?? 0) * 100)}%` }}
            />
          </div>
          <p class="text-sm text-muted">{Math.round((progress() ?? 0) * 100)}%</p>
        </Show>
        <Button onClick={upload} disabled={!file() || uploading()}>
          {uploading() ? "Uploading..." : "Upload firmware"}
        </Button>
      </Show>
    </Card>
  );
};

const DangerZoneCard: Component = () => {
  const command = useDeviceCommands();

  return (
    <Card title="Maintenance">
      <div class="flex flex-col sm:flex-row gap-2">
        <Button onClick={() => confirm("Restart the lamp?") && command("reboot")}>Restart</Button>
        <Show when={IS_BLE}>
          <Button onClick={() => bleTransport?.close()}>Disconnect Bluetooth</Button>
        </Show>
        <Button
          class="hover:bg-red-600"
          onClick={() =>
            confirm("Erase all settings, drawings, schedules and the saved WiFi network?") &&
            command("factory-reset")
          }
        >
          Factory reset
        </Button>
      </div>
    </Card>
  );
};

const Device: Component = () => {
  const [store] = useStore();
  const { toast } = useToast();
  const command = useDeviceCommands();

  // (re)load everything whenever the connection opens
  createEffect(() => {
    if (store.connectionState() === 1) {
      command("config-get");
      command("device-info");
    }
  });

  onMount(() => {
    const timer = setInterval(() => command("device-info"), DEVICE_INFO_REFRESH_MS);
    onCleanup(() => clearInterval(timer));
  });

  createEffect(
    on(
      () => store.lastResult,
      (result) => {
        if (result?.message || (result && !result.ok)) {
          toast(result.message ?? `${result.command} failed`, 3000);
        }
      },
      { defer: true },
    ),
  );

  return (
    <Layout
      content={
        <div class="space-y-4 p-5 pb-24 lg:pb-5">
          <h3 class="page-title">Device</h3>
          <div class="grid gap-4 xl:grid-cols-2">
            <DeviceInfoCard />
            <SettingsCard />
            <WifiNetworkCard />
            <MessageCard />
            <FirmwareCard />
            <DangerZoneCard />
          </div>
        </div>
      }
      sidebar={
        <div class="mt-auto pt-6 border-t border-line">
          <a href="#/" class="inline-flex items-center text-ink hover:text-brand font-medium">
            <i class="fa-solid fa-arrow-left mr-2" />
            Back to Main
          </a>
        </div>
      }
    />
  );
};

export default Device;
