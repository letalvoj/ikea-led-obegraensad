#include "wifi_control.h"

#ifdef ESP8266
/* Fix duplicate defs of HTTP_GET, HTTP_POST, ... in ESPAsyncWebServer.h */
#define WEBSERVER_H
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <WiFiManager.h>

#include "config.h"
#include "constants.h"
#include "secrets.h"

namespace
{
enum class WifiState
{
  Off,
  Connecting,
  Portal,
  Connected,
};

WiFiManager wifiManager;
WifiState state = WifiState::Off;
unsigned long stateSince = 0;
bool servicesStarted = false;
bool portalWasUsed = false;
void (*firstConnectCallback)() = nullptr;

const char *stateName()
{
  switch (state)
  {
  case WifiState::Off:
    return "disabled";
  case WifiState::Connecting:
    return "connecting";
  case WifiState::Portal:
    return "portal";
  case WifiState::Connected:
    return "connected";
  }
  return "unknown";
}

void enterState(WifiState next)
{
  state = next;
  stateSince = millis();
  Serial.printf("[WiFi] state: %s\n", stateName());
}

void beginStation()
{
  WiFi.mode(WIFI_STA);
#ifdef ESP32
  WiFi.setHostname(WIFI_HOSTNAME);
#else
  WiFi.hostname(WIFI_HOSTNAME);
#endif
  WiFi.setAutoReconnect(true);

#if defined(IP_ADDRESS) && defined(GWY) && defined(SUBNET) && defined(DNS1)
  IPAddress ip, gwy, subnet, dns;
  ip.fromString(IP_ADDRESS);
  gwy.fromString(GWY);
  subnet.fromString(SUBNET);
  dns.fromString(DNS1);
  WiFi.config(ip, gwy, subnet, dns);
#endif

  if (wifiManager.getWiFiIsSaved())
  {
    // connects with the credentials persisted by an earlier WiFi.begin(ssid, pass)
    WiFi.begin();
  }
  enterState(WifiState::Connecting);
}

void startPortal()
{
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
  wifiManager.startConfigPortal(WIFI_MANAGER_SSID);
  portalWasUsed = true;
  enterState(WifiState::Portal);
}

void stopPortal()
{
  if (wifiManager.getConfigPortalActive())
  {
    wifiManager.stopConfigPortal();
  }
}
} // namespace

void wifiSetup(void (*onFirstConnect)())
{
  firstConnectCallback = onFirstConnect;

  if (!config.getWifiEnabled())
  {
    WiFi.mode(WIFI_OFF);
    enterState(WifiState::Off);
    return;
  }

  beginStation();
}

void wifiLoop()
{
  switch (state)
  {
  case WifiState::Off:
    return;

  case WifiState::Connecting:
    if (WiFi.status() == WL_CONNECTED)
    {
      enterState(WifiState::Connected);
      Serial.print("[WiFi] connected, IP: ");
      Serial.println(WiFi.localIP());
    }
    else if (servicesStarted)
    {
      // The async web server already owns port 80, so the setup portal cannot run
      // any more; keep retrying the station. Bluetooth, or a reboot (which brings
      // the portal back), is the way to fix bad credentials from here.
      if (millis() - stateSince >= WIFI_CONNECT_TIMEOUT_MS)
      {
        beginStation();
      }
    }
    else if (!wifiManager.getWiFiIsSaved() || millis() - stateSince >= WIFI_CONNECT_TIMEOUT_MS)
    {
      startPortal();
    }
    break;

  case WifiState::Portal:
    wifiManager.process();
    if (WiFi.status() == WL_CONNECTED)
    {
      // The setup portal ran a synchronous WebServer on port 80, which does not
      // coexist with the async one. The new credentials are stored, so a clean boot
      // is the reliable way to bring the real server up.
      Serial.println("[WiFi] connected via setup portal - rebooting");
      delay(500);
      ESP.restart();
    }
    else if (!wifiManager.getConfigPortalActive())
    {
      // portal timed out without anyone configuring it, try the saved network again
      beginStation();
    }
    break;

  case WifiState::Connected:
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("[WiFi] connection lost");
      enterState(WifiState::Connecting);
    }
    break;
  }

  if (state == WifiState::Connected && !servicesStarted)
  {
    servicesStarted = true;
    if (firstConnectCallback)
    {
      firstConnectCallback();
    }
  }
}

bool wifiIsConnected()
{
  return state != WifiState::Off && WiFi.status() == WL_CONNECTED;
}

void wifiDescribe(JsonObject out)
{
  out["enabled"] = config.getWifiEnabled();
  out["state"] = stateName();
  out["connected"] = wifiIsConnected();
  out["ssid"] = wifiManager.getWiFiSSID(true);
  out["portalSsid"] = WIFI_MANAGER_SSID;
  out["portalUsed"] = portalWasUsed;
  out["hostname"] = WIFI_HOSTNAME;
  out["macAddress"] = WiFi.macAddress();
  if (wifiIsConnected())
  {
    out["ipAddress"] = WiFi.localIP().toString();
    out["rssi"] = WiFi.RSSI();
  }
}

bool wifiSetCredentials(const String &ssid, const String &password)
{
  if (state == WifiState::Off || ssid.length() == 0)
  {
    return false;
  }

  stopPortal();
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  enterState(WifiState::Connecting);
  return true;
}

void wifiForget()
{
  stopPortal();
  WiFi.persistent(true);
#ifdef ESP32
  WiFi.disconnect(false, true);
#else
  WiFi.disconnect(false); // with persistent on, this clears the stored SDK config
#endif
  if (state != WifiState::Off)
  {
    enterState(WifiState::Connecting);
  }
}
