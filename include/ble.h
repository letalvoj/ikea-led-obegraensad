#pragma once

#include "constants.h"

#ifdef ENABLE_BLE

#include <Arduino.h>
#include <ArduinoJson.h>

// Bluetooth LE transport for the device protocol (see commands.h), meant for the
// Web Bluetooth page. One GATT service with three characteristics:
//
//   RX  (write)          browser -> device, framed messages
//   TX  (notify)         device -> browser, framed messages
//   OTA (write w/o resp) firmware image chunks: [u32 LE offset][bytes...]
//
// RX and TX messages are split into chunks that fit the negotiated MTU. Every chunk
// starts with a header byte: the low bits carry the kind, BLE_FLAG_FINAL marks the
// last chunk of a message.
//
// All characteristics require an authenticated (passkey) pairing.

#define BLE_SERVICE_UUID "0b5e0001-1ea0-4d1a-9b7e-0bec0ffee000"
#define BLE_RX_UUID "0b5e0002-1ea0-4d1a-9b7e-0bec0ffee000"
#define BLE_TX_UUID "0b5e0003-1ea0-4d1a-9b7e-0bec0ffee000"
#define BLE_OTA_UUID "0b5e0004-1ea0-4d1a-9b7e-0bec0ffee000"

constexpr uint8_t BLE_KIND_JSON = 0x01;
constexpr uint8_t BLE_KIND_FRAME = 0x02;
constexpr uint8_t BLE_FLAG_FINAL = 0x80;

// the OTA sender waits for an acknowledgement after every window of this many bytes
constexpr size_t BLE_OTA_WINDOW = 4096;

void bleSetup();
void bleLoop();

bool bleConnected();
void bleDescribe(JsonObject out);

void bleSendText(const String &message);
void bleSendFrame(const uint8_t *frame, size_t len);

void bleOtaBegin(size_t size, const char *md5);
void bleOtaAbort();
void bleOtaReportStatus();

#endif
