#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <string.h> // strstr()
#include "THIoT_ESPWsDataHandler.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_PFConsoleTrace.h"

#define APP_WS_TAG_CONSOLE(...) SERIAL_TAG_LOGI("[APP_WS]", __VA_ARGS__)

const char *const ws_page_list[] PROGMEM = {
    "WS_PAGE_CARD_HISTORY",	
    "WS_PAGE_MANAGE",
    "WS_PAGE_CARD_INFO"
};

const char *const page_card_user_list[] PROGMEM = {
    "PAGE_CMD_CONNECT",	
    "PAGE_CMD_RECEIVE_DATA",
    "PAGE_CMD_REPLY_DATA",
    "PAGE_CMD_ASYNC_DATA"
};

ESPWsDataHandle::ESPWsDataHandle(/* args */)
{
}

ESPWsDataHandle::~ESPWsDataHandle()
{
}

void ESPWsDataHandle::onDataReceived(AsyncWebSocketClient *client, char *payload)
{
  uint8_t page;
  uint8_t cmd;
  uint8_t ws_index = client->id();

#if (CONSOLE_TRACE_WS_STREAM == 1)
  /* ---------------------- WebSocket console ------------------------------
   * Once ws_console.htm has registered itself (Page = P_WS_CONSOLE) every text
   * frame is a console command, not JSON: echo it and let exit/quit/close drop
   * the session (same behaviour as the reference firmware). */
  if (wsHandler.isPageType(ws_index, P_WS_CONSOLE))
  {
    CONSOLE_TRACE_STREAM.wsConsoleSet(true); // enable ws console
    client->text("Echo: ");
    client->text(payload);
    client->text("\r\n");
    if (strstr(payload, "exit") != nullptr ||
        strstr(payload, "quit") != nullptr ||
        strstr(payload, "close") != nullptr)
    {
      APP_WS_TAG_CONSOLE("Close ws console");
      client->close();
      CONSOLE_TRACE_STREAM.closeWs(); // close ws console
    }
    return;
  }
#endif

  DynamicJsonBuffer djbpo;
  JsonObject &root = djbpo.parseObject(payload);
  if (!root.success())
  {
    APP_WS_TAG_CONSOLE("JSON parsing failed!");
    return;
  }

  /* Accept both key styles: "page"/"cmd" (this project) and "Page"/"WsCmd"
   * (the pages ported from web485_v3, e.g. ws_console.htm). */
  if (root["page"].success()) { page = root["page"]; }
  else if (root["Page"].success()) { page = root["Page"]; }
  else
  {
    APP_WS_TAG_CONSOLE("Have not page field");
    return;
  }

  if (root["cmd"].success()) { cmd = root["cmd"]; }
  else if (root["WsCmd"].success()) { cmd = root["WsCmd"]; }
  else
  {
    APP_WS_TAG_CONSOLE("Have not cmd field");
    return;
  }

#if (CONSOLE_TRACE_WS_STREAM == 1)
  /* Register the console stream of this socket: every console record is pushed
   * to the page until the page disappears (pageTypeSend() fails). */
  if (P_WS_CONSOLE == page)
  {
    wsHandler.pageTypeUpdate(ws_index, P_WS_CONSOLE);
    APP_WS_TAG_CONSOLE("Ws console connected");
    client->text("Hello WebSerial !\r\n");
    CONSOLE_TRACE_STREAM.beginWs([](void *arg, const uint8_t *buffer, size_t length) -> bool {
      bool status = wsHandler.pageTypeSend(nullptr, (char *)buffer, P_WS_CONSOLE, length);
      if (!status) {
        ESPConsoleTrace *handle = (ESPConsoleTrace *) arg;
        handle->closeWs(); // close ws console
        APP_WS_TAG_CONSOLE("Ws console close");
      }
      return status;
    }, &CONSOLE_TRACE_STREAM);
    return;
  }
#endif

  APP_WS_TAG_CONSOLE("Page: %s", ws_page_list[page]);
  APP_WS_TAG_CONSOLE("cmd : %s", page_card_user_list[cmd]);

  if (root["date"].success())
  {
    //"date":"Thu Jan 25 2018 19:39:48 GMT+0700 (SE Asia Standard Time)"
    const char *s = root["date"];
    if (s != nullptr)
    {
      ESPTime.GMTStringUpdate(s, ESPTimeSystem::ESP_RTC_WEB_UPDATE);
    }
  }
}

ESPWsDataHandle WsDataHandler;