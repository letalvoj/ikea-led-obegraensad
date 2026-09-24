#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Transport-independent device protocol. The websocket and Bluetooth transports
// both feed JSON commands ({"event": "...", ...}) and raw 16x16 frames in here, and
// everything the device reports goes out to all connected clients of both.

// Queue a JSON command for execution on the main loop. Safe to call from any task.
void enqueueCommand(const char *json, size_t len);
// Execute queued commands and deferred actions; call from loop().
void commandsLoop();

// Restart the device from the main loop after a short delay, so replies still go out.
void requestRestart(unsigned long delayMs = 750);

// Show a raw 256-byte grayscale frame, pausing the active plugin (streaming mode).
void showFrame(const uint8_t *frame, size_t len);
// Leave streaming mode and hand the screen back to the active plugin.
void stopStreaming();

// Snapshots used by the broadcasts below and by the HTTP API.
void buildInfo(JsonDocument &doc, bool withFrame);
void buildDeviceInfo(JsonDocument &doc);

// Broadcast helpers, sent to every websocket and Bluetooth client.
void sendInfo();
void broadcastMessage(const String &message);
void sendLiveFrame();
