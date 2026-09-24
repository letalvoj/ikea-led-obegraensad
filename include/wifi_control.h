#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Non-blocking WiFi handling: the display keeps running while the device tries the
// saved network, falls back to the WiFiManager setup AP, and retries in the background.

// onFirstConnect is called once, from loop(), the first time the station gets an IP.
void wifiSetup(void (*onFirstConnect)());
void wifiLoop();

bool wifiIsConnected();
void wifiDescribe(JsonObject out);

// Store new station credentials and reconnect with them. Fails while WiFi is disabled.
bool wifiSetCredentials(const String &ssid, const String &password);
// Erase the stored station credentials.
void wifiForget();
