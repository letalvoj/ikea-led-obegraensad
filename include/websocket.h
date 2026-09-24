#pragma once

#include "commands.h"
#include "constants.h"

#ifdef ENABLE_SERVER
#include <ESPAsyncWebServer.h>

#include "storage.h"

void initWebsocketServer(AsyncWebServer &server);
void cleanUpClients();

void wsSendText(const String &message);
void wsSendFrame(const uint8_t *frame, size_t len);
size_t wsClientCount();

#ifdef WS_MAX_QUEUED_MESSAGES
#undef WS_MAX_QUEUED_MESSAGES
#define WS_MAX_QUEUED_MESSAGES 64
#endif

#endif
