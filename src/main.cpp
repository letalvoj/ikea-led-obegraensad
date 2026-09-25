#include <Arduino.h>
#include <BfButton.h>
#include <SPI.h>

#ifdef ESP32
#include <ESPmDNS.h>
#include <esp_ota_ops.h>
#endif

#include "PluginManager.h"
#include "config.h"
#include "scheduler.h"

#include "plugins/ArtNet.h"
#include "plugins/Blob.h"
#include "plugins/BoxDudePlugin.h"
#include "plugins/BreakoutPlugin.h"
#include "plugins/BubblesPlugin.h"
#include "plugins/CheckerboardPlugin.h"
#include "plugins/CirclePlugin.h"
#include "plugins/CometPlugin.h"
#include "plugins/DDPPlugin.h"
#include "plugins/DrawPlugin.h"
#include "plugins/FirefliesPlugin.h"
#include "plugins/FireworkPlugin.h"
#include "plugins/GameOfLifePlugin.h"
#include "plugins/LinesPlugin.h"
#include "plugins/MatrixRainPlugin.h"
#include "plugins/MeteorShowerPlugin.h"
#include "plugins/PongClockPlugin.h"
#include "plugins/RadarPlugin.h"
#include "plugins/RainPlugin.h"
#include "plugins/ScanlinesPlugin.h"
#include "plugins/SnakePlugin.h"
#include "plugins/SparkleFieldPlugin.h"
#include "plugins/SpiralPlugin.h"
#include "plugins/StarsPlugin.h"
#include "plugins/TickingClockPlugin.h"
#include "plugins/WaveBarsPlugin.h"
#include "plugins/WavePlugin.h"

#ifdef ENABLE_SERVER
#include "plugins/AnimationPlugin.h"
#include "plugins/BigClockPlugin.h"
#include "plugins/ClockPlugin.h"
#include "plugins/WeatherPlugin.h"
#endif

#include "asyncwebserver.h"
#include "ble.h"
#include "commands.h"
#include "messages.h"
#include "ota.h"
#include "screen.h"
#include "secrets.h"
#include "websocket.h"
#include "wifi_control.h"

BfButton btn(BfButton::STANDALONE_DIGITAL, PIN_BUTTON, true, LOW);

PluginManager pluginManager;
#ifdef ESP32
DRAM_ATTR volatile SYSTEM_STATUS currentStatus = NONE;
#else
volatile SYSTEM_STATUS currentStatus = NONE;
#endif

#ifdef ESP32
// Keep a freshly OTA-flashed image in "pending verify" instead of accepting it at
// boot; markFirmwareHealthy() accepts it once it has run for a while. If it crashes
// before that, the bootloader falls back to the previous image.
extern "C" bool verifyRollbackLater()
{
  return true;
}

void markFirmwareHealthy()
{
  static bool marked = false;
  if (marked || millis() < OTA_ROLLBACK_GRACE_MS)
  {
    return;
  }
  marked = true;

  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY)
  {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[OTA] new firmware marked as valid");
  }
}
#endif

// One status line on the serial console every few seconds, to see from the outside
// whether the loop, the screen refresh, the button and the radios are alive.
void logHeartbeat()
{
  static unsigned long lastLog = 0;
  static uint32_t lastRenders = 0;
  static uint32_t loops = 0;
  loops++;

  if (millis() - lastLog < DIAG_LOG_INTERVAL_MS)
  {
    return;
  }
  lastLog = millis();

  uint32_t renders = Screen.renderCount;
  Plugin *plugin = pluginManager.getActivePlugin();
  JsonDocument info;
  buildDeviceInfo(info);
  Serial.printf("[Diag] up %lus | mode %d %s | status %d | brightness %u | screen %lu frames "
                "(+%lu) | button pin %d | loops %lu | heap %u | wifi %s | ble %s\n",
                millis() / 1000,
                plugin ? plugin->getId() : -1,
                plugin ? plugin->getName() : "-",
                currentStatus,
                Screen.getCurrentBrightness(),
                (unsigned long)renders,
                (unsigned long)(renders - lastRenders),
                digitalRead(PIN_BUTTON),
                (unsigned long)loops,
                ESP.getFreeHeap(),
                info["wifi"]["state"] | "?",
                info["ble"]["paired"] ? "paired" : (info["ble"]["connected"] ? "connected" : "idle"));
  lastRenders = renders;
}

// Everything that needs a network. Runs once, the first time WiFi connects.
void startNetworkServices()
{
#ifdef ENABLE_SERVER
#ifdef ESP32
  if (MDNS.begin(WIFI_HOSTNAME))
  {
    MDNS.addService("http", "tcp", 80);
    MDNS.setInstanceName(WIFI_HOSTNAME);
  }
  else
  {
    Serial.println("Could not start mDNS!");
  }
#endif

  // lwIP SNTP stores the server-name pointer without copying it, so the strings
  // must outlive this call.
  static String tzInfo = config.getTzInfo();
  static String ntpServer = config.getNtpServer();
  configTzTime(tzInfo.c_str(), ntpServer.c_str());

  initOTA(server);
  initWebsocketServer(server);
  initWebServer();
#endif
}

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
  Serial.printf("[Button] pattern %d, status %d\n", pattern, currentStatus);
  switch (pattern)
  {
  case BfButton::SINGLE_PRESS:
    if (currentStatus != LOADING)
    {
      Scheduler.clearSchedule();
      pluginManager.activateNextPlugin();
    }
    break;

  case BfButton::LONG_PRESS:
    if (currentStatus != LOADING)
    {
      pluginManager.activatePersistedPlugin();
    }
    break;
  }
}

void baseSetup()
{
  Serial.begin(115200);

  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);

#if !defined(ESP32) && !defined(ESP8266)
  Screen.setup();
#endif

  config.begin();

  // The clock can also be set over Bluetooth without any NTP, so apply the
  // timezone right away instead of waiting for configTzTime().
  setenv("TZ", config.getTzInfo().c_str(), 1);
  tzset();

  pluginManager.addPlugin(new DrawPlugin());
  pluginManager.addPlugin(new BreakoutPlugin());
  pluginManager.addPlugin(new SnakePlugin());
  pluginManager.addPlugin(new GameOfLifePlugin());
  pluginManager.addPlugin(new StarsPlugin());
  pluginManager.addPlugin(new LinesPlugin());
  pluginManager.addPlugin(new CirclePlugin());
  pluginManager.addPlugin(new RainPlugin());
  pluginManager.addPlugin(new MatrixRainPlugin());
  pluginManager.addPlugin(new FireworkPlugin());
  pluginManager.addPlugin(new BlobPlugin());
  pluginManager.addPlugin(new SpiralPlugin());
  pluginManager.addPlugin(new WavePlugin());
  pluginManager.addPlugin(new CheckerboardPlugin());
  pluginManager.addPlugin(new RadarPlugin());
  pluginManager.addPlugin(new BubblesPlugin());
  pluginManager.addPlugin(new CometPlugin());
  pluginManager.addPlugin(new FirefliesPlugin());
  pluginManager.addPlugin(new MeteorShowerPlugin());
  pluginManager.addPlugin(new ScanlinesPlugin());
  pluginManager.addPlugin(new SparkleFieldPlugin());
  pluginManager.addPlugin(new WaveBarsPlugin());

#ifdef ENABLE_SERVER
  pluginManager.addPlugin(new BigClockPlugin());
  pluginManager.addPlugin(new ClockPlugin());
  pluginManager.addPlugin(new PongClockPlugin());
  pluginManager.addPlugin(new TickingClockPlugin());
  pluginManager.addPlugin(new WeatherPlugin());
  pluginManager.addPlugin(new AnimationPlugin());
  pluginManager.addPlugin(new DDPPlugin());
  pluginManager.addPlugin(new ArtNetPlugin());
#endif

  // appended last so the ids of the existing plugins (and saved schedules) stay put
  pluginManager.addPlugin(new BoxDudePlugin());

  Screen.clear();
  pluginManager.init();
  Scheduler.init();

  btn.onPress(pressHandler).onDoublePress(pressHandler).onPressFor(pressHandler, 1000);
}

// Connectivity comes up after the display is already running and never blocks it.
void connectivitySetup()
{
#ifdef ENABLE_SERVER
  wifiSetup(startNetworkServices);
#endif
#ifdef ENABLE_BLE
  bleSetup();
#endif
}

#ifdef ESP32
TaskHandle_t screenDrawingTaskHandle = NULL;

void screenDrawingTask(void *parameter)
{
  Screen.setup();
  Serial.printf("[Screen] refresh running on core %d\n", xPortGetCoreID());
  for (;;)
  {
    pluginManager.runActivePlugin();
    vTaskDelay(1);
  }
}

void setup()
{
  baseSetup();
  xTaskCreatePinnedToCore(screenDrawingTask,
                          "screenDrawingTask",
                          10000,
                          NULL,
                          1,
                          &screenDrawingTaskHandle,
                          0);
  connectivitySetup();
}
#endif
#ifdef ESP8266
void screenDrawingTask()
{
  Screen.setup();
  pluginManager.runActivePlugin();
  yield();
}

void setup()
{
  baseSetup();
  Scheduler.start();
  connectivitySetup();
}
#endif

void loop()
{
  static uint8_t taskCounter = 0;

  btn.read();

#ifdef ENABLE_SERVER
  wifiLoop();
  ElegantOTA.loop();
#endif
#ifdef ENABLE_BLE
  bleLoop();
#endif
  commandsLoop();
  logHeartbeat();

#if !defined(ESP32) && !defined(ESP8266)
  pluginManager.runActivePlugin();
#endif

  if (currentStatus == NONE)
  {
    Scheduler.update();

    if ((taskCounter & 0x03) == 0)
    {
      Messages.scrollMessageEveryMinute();
    }
  }

  taskCounter++;
  if (taskCounter > 16)
  {
    taskCounter = 0;
  }

  static unsigned long lastLiveFrameMillis = 0;
  const unsigned long currentMillis = millis();
  if (currentMillis - lastLiveFrameMillis >= 100)
  {
    lastLiveFrameMillis = currentMillis;
    sendLiveFrame();
  }

#ifdef ENABLE_SERVER
  cleanUpClients();
#endif
#ifdef ESP32
  markFirmwareHealthy();
  vTaskDelay(1);
#else
  delay(1);
#endif
}
