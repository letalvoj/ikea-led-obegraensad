#include "websocket.h"

#include "commands.h"

#ifdef ENABLE_SERVER

AsyncWebSocket ws("/ws");

void wsSendText(const String &message)
{
  if (ws.count() > 0)
  {
    ws.textAll(message);
  }
}

void wsSendFrame(const uint8_t *frame, size_t len)
{
  if (ws.count() == 0 || !ws.availableForWriteAll())
  {
    return;
  }
  ws.binaryAll(frame, len);
}

size_t wsClientCount()
{
  return ws.count();
}

void onWsEvent(AsyncWebSocket *server,
               AsyncWebSocketClient *client,
               AwsEventType type,
               void *arg,
               uint8_t *data,
               size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    enqueueCommand("{\"event\":\"info\"}", 16);
    return;
  }

  if (type != WS_EVT_DATA)
  {
    return;
  }

  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (!info->final || info->index != 0 || info->len != len)
  {
    return;
  }

  if (info->opcode == WS_BINARY)
  {
    showFrame(data, len);
  }
  else if (info->opcode == WS_TEXT)
  {
    enqueueCommand(reinterpret_cast<const char *>(data), len);
  }
}

void initWebsocketServer(AsyncWebServer &server)
{
  server.addHandler(&ws);
  ws.onEvent(onWsEvent);
}

void cleanUpClients()
{
  ws.cleanupClients();
}

#endif
