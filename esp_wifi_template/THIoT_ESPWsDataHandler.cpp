#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "THIoT_ESPWsDataHandler.h"
#include "THIoT_ESPTimeSystem.h"
#include "THIoT_SerialTrace.h"

#define APP_WS_DBG_PRINT(...) SERIAL_TAG_LOGI("[APP_WS]", __VA_ARGS__)

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
  uint8_t result_cmd = 0;

  DynamicJsonBuffer djbpo;
  JsonObject &root = djbpo.parseObject(payload);
  if (!root.success())
  {
    APP_WS_DBG_PRINT("JSON parsing failed!");
    return;
  }

  if (!root["page"].success() || !root["cmd"].success())
  {
    return;
  }

  page = root["page"];
  cmd = root["cmd"];

  APP_WS_DBG_PRINT("Page: %s", ws_page_list[page]);
  APP_WS_DBG_PRINT("cmd : %s", page_card_user_list[cmd]);

  if (root["date"].success())
  {
    //"date":"Thu Jan 25 2018 19:39:48 GMT+0700 (SE Asia Standard Time)"
    const char *s = root["date"];
    if (s != nullptr)
    {
      ESPTime.GMTStringUpdate(s, ESPTimeSystem::RTC_WEB_UPDATE);
    }
  }
}