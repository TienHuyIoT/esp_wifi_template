#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "THIoT_ESPWsDataHandler.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_PFSerialTrace.h"

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

  DynamicJsonBuffer djbpo;
  JsonObject &root = djbpo.parseObject(payload);
  if (!root.success())
  {
    APP_WS_TAG_CONSOLE("JSON parsing failed!");
    return;
  }

  if (!root["page"].success() || !root["cmd"].success())
  {
    return;
  }

  page = root["page"];
  cmd = root["cmd"];

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