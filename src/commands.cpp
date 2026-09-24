#include "commands.h"

#include <deque>
#include <string>
#include <sys/time.h>
#include <time.h>

#include "PluginManager.h"
#include "ble.h"
#include "config.h"
#include "messages.h"
#include "scheduler.h"
#include "screen.h"
#include "storage.h"
#include "websocket.h"
#include "wifi_control.h"

#ifdef ESP32
#include <esp_system.h>
#include <mutex>
#endif

namespace
{
// Commands arrive on the AsyncTCP and NimBLE tasks; they are executed on the main
// loop so plugins, storage and the scheduler are only ever touched from one place.
constexpr size_t MAX_QUEUED_COMMANDS = 16;
std::deque<std::string> commandQueue;

#ifdef ESP32
std::mutex queueMutex;
struct QueueLock
{
  std::lock_guard<std::mutex> guard{queueMutex};
};
#else
// the ESP8266 runs its network callbacks on the loop context, nothing to guard
struct QueueLock
{
};
#endif

bool restartPending = false;
unsigned long restartAt = 0;

const char *resetReason()
{
#ifdef ESP32
  switch (esp_reset_reason())
  {
  case ESP_RST_POWERON:
    return "power-on";
  case ESP_RST_SW:
    return "software";
  case ESP_RST_PANIC:
    return "panic";
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
    return "watchdog";
  case ESP_RST_BROWNOUT:
    return "brownout";
  case ESP_RST_DEEPSLEEP:
    return "deep-sleep";
  default:
    return "other";
  }
#else
  static String reason = ESP.getResetReason();
  return reason.c_str();
#endif
}

void sendJson(JsonDocument &doc)
{
  String output;
  serializeJson(doc, output);
  broadcastMessage(output);
}

void sendConfig()
{
  JsonDocument doc;
  deserializeJson(doc, config.toJson());
  doc["event"] = "config";
  sendJson(doc);
}

void sendDeviceInfo()
{
  JsonDocument doc;
  buildDeviceInfo(doc);
  doc["event"] = "device";
  sendJson(doc);
}

void sendResult(const char *command, bool ok, const char *message = nullptr)
{
  JsonDocument doc;
  doc["event"] = "result";
  doc["command"] = command;
  doc["ok"] = ok;
  if (message)
  {
    doc["message"] = message;
  }
  sendJson(doc);
}

void setClock(time_t epoch)
{
  timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  setenv("TZ", config.getTzInfo().c_str(), 1);
  tzset();
}

void addMessage(JsonDocument &request)
{
  std::vector<int> graph;
  for (JsonVariant value : request["graph"].as<JsonArray>())
  {
    graph.push_back(value.as<int>());
  }

  int delay = request["delay"] | 50;
  Messages.add(request["text"] | "",
               request["repeat"] | 0,
               request["id"] | 0,
               delay > 0 ? delay : 50,
               graph,
               request["miny"] | 0,
               request["maxy"] | 15);
}

void setConfig(JsonDocument &request)
{
  bool wifiWasEnabled = config.getWifiEnabled();

  String json;
  serializeJson(request["config"], json);
  if (!config.fromJson(json))
  {
    sendResult("config-set", false, "Invalid configuration");
    return;
  }
  config.save();
  sendConfig();

  // switching the radio mode is only reliable from a clean boot
  if (config.getWifiEnabled() != wifiWasEnabled)
  {
    sendResult("config-set", true, "Restarting to apply WiFi change");
    requestRestart();
  }
  else
  {
    sendResult("config-set", true);
  }
}

void factoryReset()
{
#ifdef ENABLE_STORAGE
  storage.begin("led-wall", false);
  storage.clear();
  storage.end();
#endif
  config.setDefaults();
  config.save();
  wifiForget();
  sendResult("factory-reset", true, "Restarting");
  requestRestart();
}

void handleCommand(JsonDocument &request)
{
  const char *event = request["event"] | "";

  pluginManager.getActivePlugin()->websocketHook(request);

  if (!strcmp(event, "plugin"))
  {
    if (currentStatus == WSBINARY)
    {
      currentStatus = NONE;
    }
    Scheduler.clearSchedule();
    pluginManager.setActivePluginById(request["plugin"].as<int>());
    sendInfo();
  }
  else if (!strcmp(event, "persist-plugin"))
  {
    pluginManager.persistActivePlugin();
    sendInfo();
  }
  else if (!strcmp(event, "rotate"))
  {
    bool isRight = !strcmp(request["direction"] | "", "right");
    Screen.setCurrentRotation((Screen.currentRotation + (isRight ? 1 : 3)) % 4, true);
    sendInfo();
  }
  else if (!strcmp(event, "info"))
  {
    sendInfo();
  }
  else if (!strcmp(event, "brightness"))
  {
    Screen.setBrightness(request["brightness"].as<uint8_t>(), true);
    sendInfo();
  }
  // --- drawing: any of these pauses the active plugin until "stream" is disabled
  else if (!strcmp(event, "frame"))
  {
    uint8_t frame[ROWS * COLS] = {0};
    JsonArray data = request["data"].as<JsonArray>();
    size_t i = 0;
    for (JsonVariant value : data)
    {
      if (i >= sizeof(frame))
        break;
      frame[i++] = value.as<uint8_t>();
    }
    showFrame(frame, sizeof(frame));
  }
  else if (!strcmp(event, "pixel"))
  {
    if (currentStatus != WSBINARY)
    {
      showFrame(Screen.getRenderBuffer(), ROWS * COLS);
    }
    Screen.setPixel(request["x"].as<uint8_t>(),
                    request["y"].as<uint8_t>(),
                    1,
                    request["value"] | MAX_BRIGHTNESS);
  }
  else if (!strcmp(event, "stream"))
  {
    if (request["enabled"] | false)
    {
      showFrame(Screen.getRenderBuffer(), ROWS * COLS);
    }
    else
    {
      stopStreaming();
    }
  }
  // --- scheduler
  else if (!strcmp(event, "schedule"))
  {
    String json;
    serializeJson(request["schedule"], json);
    bool ok = Scheduler.setScheduleByJSONString(json);
    if (ok)
    {
      Scheduler.start();
    }
    sendInfo();
    sendResult(event, ok, ok ? nullptr : "Schedule cannot be set");
  }
  else if (!strcmp(event, "schedule-start") || !strcmp(event, "schedule-stop"))
  {
    bool ok = !Scheduler.schedule.empty();
    if (ok)
    {
      !strcmp(event, "schedule-start") ? Scheduler.start() : Scheduler.stop();
    }
    sendInfo();
    sendResult(event, ok, ok ? nullptr : "No schedule found");
  }
  else if (!strcmp(event, "schedule-clear"))
  {
    Scheduler.clearSchedule(true);
    sendInfo();
    sendResult(event, true);
  }
  // --- scrolling messages
  else if (!strcmp(event, "message"))
  {
    addMessage(request);
    sendResult(event, true);
  }
  else if (!strcmp(event, "message-remove"))
  {
    Messages.remove(request["id"] | 0);
    sendResult(event, true);
  }
  // --- settings and device management
  else if (!strcmp(event, "config-get"))
  {
    sendConfig();
  }
  else if (!strcmp(event, "config-set"))
  {
    setConfig(request);
  }
  else if (!strcmp(event, "config-reset"))
  {
    config.setDefaults();
    config.save();
    sendConfig();
    sendResult(event, true);
  }
  else if (!strcmp(event, "device-info"))
  {
    sendDeviceInfo();
  }
  else if (!strcmp(event, "time"))
  {
    setClock(request["epoch"].as<time_t>());
    sendDeviceInfo();
  }
  else if (!strcmp(event, "wifi-credentials"))
  {
    bool ok = wifiSetCredentials(request["ssid"] | "", request["password"] | "");
    sendResult(event, ok, ok ? "Connecting" : "Enable WiFi and provide a network name first");
  }
  else if (!strcmp(event, "wifi-forget"))
  {
    wifiForget();
    sendResult(event, true);
    sendDeviceInfo();
  }
  else if (!strcmp(event, "reboot"))
  {
    sendResult(event, true, "Restarting");
    requestRestart();
  }
  else if (!strcmp(event, "factory-reset"))
  {
    factoryReset();
  }
#ifdef ENABLE_BLE
  else if (!strcmp(event, "ota-begin"))
  {
    bleOtaBegin(request["size"] | 0, request["md5"] | "");
  }
  else if (!strcmp(event, "ota-abort"))
  {
    bleOtaAbort();
  }
  else if (!strcmp(event, "ota-status"))
  {
    bleOtaReportStatus();
  }
#endif
}
} // namespace

void enqueueCommand(const char *json, size_t len)
{
  QueueLock lock;
  if (commandQueue.size() >= MAX_QUEUED_COMMANDS)
  {
    Serial.println("[Commands] queue full, dropping command");
    return;
  }
  commandQueue.emplace_back(json, len);
}

void commandsLoop()
{
  while (true)
  {
    std::string json;
    {
      QueueLock lock;
      if (commandQueue.empty())
        break;
      json = std::move(commandQueue.front());
      commandQueue.pop_front();
    }

    JsonDocument request;
    DeserializationError error = deserializeJson(request, json);
    if (error)
    {
      Serial.print(F("[Commands] deserializeJson() failed: "));
      Serial.println(error.f_str());
      continue;
    }
    handleCommand(request);
  }

  if (restartPending && (long)(millis() - restartAt) >= 0)
  {
    ESP.restart();
  }
}

void requestRestart(unsigned long delayMs)
{
  restartAt = millis() + delayMs;
  restartPending = true;
}

void showFrame(const uint8_t *frame, size_t len)
{
  if (len != ROWS * COLS || currentStatus == UPDATE)
  {
    return;
  }

  bool entering = currentStatus != WSBINARY;
  currentStatus = WSBINARY;
  if (frame != Screen.getRenderBuffer())
  {
    Screen.setRenderBuffer(frame, true);
  }
  if (entering)
  {
    sendInfo();
  }
}

void stopStreaming()
{
  if (currentStatus != WSBINARY)
  {
    return;
  }
  currentStatus = NONE;
  Screen.clear();
  pluginManager.setupActivePlugin();
  sendInfo();
}

void buildInfo(JsonDocument &doc, bool withFrame)
{
  if (withFrame)
  {
    JsonArray data = doc["data"].to<JsonArray>();
    for (int i = 0; i < ROWS * COLS; i++)
    {
      data.add(Screen.getRenderBuffer()[i]);
    }
  }

  doc["event"] = "info";
  doc["rows"] = ROWS;
  doc["cols"] = COLS;
  doc["status"] = currentStatus;
  doc["plugin"] = pluginManager.getActivePlugin()->getId();
  doc["persist-plugin"] = pluginManager.getPersistedPluginId();
  doc["rotation"] = Screen.currentRotation;
  doc["brightness"] = Screen.getCurrentBrightness();
  doc["scheduleActive"] = Scheduler.isActive;

  JsonArray scheduleArray = doc["schedule"].to<JsonArray>();
  for (const auto &item : Scheduler.schedule)
  {
    JsonObject scheduleItem = scheduleArray.add<JsonObject>();
    scheduleItem["pluginId"] = item.pluginId;
    scheduleItem["duration"] = item.duration / 1000; // milliseconds to seconds
  }

  JsonArray plugins = doc["plugins"].to<JsonArray>();
  for (Plugin *plugin : pluginManager.getAllPlugins())
  {
    JsonObject object = plugins.add<JsonObject>();
    object["id"] = plugin->getId();
    object["name"] = plugin->getName();
  }
}

void buildDeviceInfo(JsonDocument &doc)
{
  doc["firmware"] = __DATE__ " " __TIME__;
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["resetReason"] = resetReason();
  doc["epoch"] = (long)time(nullptr);
  doc["tzInfo"] = config.getTzInfo();

  wifiDescribe(doc["wifi"].to<JsonObject>());
#ifdef ENABLE_BLE
  bleDescribe(doc["ble"].to<JsonObject>());
#endif
}

void sendInfo()
{
#ifdef ENABLE_SERVER
  if (wsClientCount() > 0)
  {
    JsonDocument doc;
    buildInfo(doc, true);
    String output;
    serializeJson(doc, output);
    wsSendText(output);
  }
#endif
#ifdef ENABLE_BLE
  if (bleConnected())
  {
    // the live frame follows separately, keep the Bluetooth payload small
    JsonDocument doc;
    buildInfo(doc, false);
    String output;
    serializeJson(doc, output);
    bleSendText(output);
  }
#endif
}

void broadcastMessage(const String &message)
{
#ifdef ENABLE_SERVER
  wsSendText(message);
#endif
#ifdef ENABLE_BLE
  bleSendText(message);
#endif
}

void sendLiveFrame()
{
  uint8_t frame[ROWS * COLS];
  memcpy(frame, Screen.getRenderBuffer(), sizeof(frame));
#ifdef ENABLE_SERVER
  wsSendFrame(frame, sizeof(frame));
#endif
#ifdef ENABLE_BLE
  bleSendFrame(frame, sizeof(frame));
#endif
}
