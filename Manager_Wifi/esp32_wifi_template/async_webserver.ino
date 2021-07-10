#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "app_config.h"
#include "sd_card.h"
#include "wifi_data_file.h"
#include "async_webserver.h"
#include "async_websocket.h"
#include "server_data_process.h"
#include "esp_reset.h"
#include "fs_editor.h"
#include "console_dbg.h"

#define WEB_SERVER_DBG_PRINTF(...) CONSOLE_LOGI(__VA_ARGS__)
#define WEB_SERVER_DBG_PORT CONSOLE_PORT

class RedirectUrlHandler : public AsyncWebHandler {
public:
  RedirectUrlHandler() {}
  virtual ~RedirectUrlHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    wifi_file_json_t *g_wifi_cfg;  
    g_wifi_cfg = wifi_info_get();
    String RedirectUrl = "http://";

    RedirectUrl += request->host();
    RedirectUrl += ":";
    RedirectUrl += g_wifi_cfg->port.tcp;
    RedirectUrl += request->url();

    request->redirect(RedirectUrl);
  }
};

void fs_editor_status(AsyncWebServerRequest *request)
{
  char buf_ttb[64];
  char buf_udb[64];
  String status = request->getParam("status")->value();
  
  if(status == "spiffs")
  {
#ifdef ESP8266
    FSInfo fs_info;
    NAND_FS_SYSTEM.info(fs_info);
    sprintf(buf_ttb,"%lu", fs_info.totalBytes);
    sprintf(buf_udb,"%lu", fs_info.usedBytes);
    WEB_SERVER_DBG_PORT.printf("\nNandflash Total space: %lu\r\n", fs_info.totalBytes);
    WEB_SERVER_DBG_PORT.printf("Nandflash Used space: %lu\r\n", fs_info.usedBytes);
#elif defined(ESP32)
    sprintf(buf_ttb,"%lu", NAND_FS_SYSTEM.totalBytes());
    sprintf(buf_udb,"%lu", NAND_FS_SYSTEM.usedBytes());
    WEB_SERVER_DBG_PORT.printf("\nNandflash Total space: %lu\r\n", NAND_FS_SYSTEM.totalBytes());
    WEB_SERVER_DBG_PORT.printf("Nandflash Used space: %lu\r\n", NAND_FS_SYSTEM.usedBytes());
#endif
  }
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)  
  else
  {
    sprintf(buf_ttb,"%llu", SD_FS_SYSTEM.totalBytes());
    sprintf(buf_udb,"%llu", SD_FS_SYSTEM.usedBytes());
    WEB_SERVER_DBG_PORT.printf("\nSD Total space: %lu\r\n", SD_FS_SYSTEM.totalBytes());
    WEB_SERVER_DBG_PORT.printf("SD Used space: %lu\r\n", SD_FS_SYSTEM.usedBytes());
  }
#endif

  String output = "{";

  output += "\"type\":\"";
  if(status == "spiffs")
  {
    output += "ESP";
  }
  else
  {
    output += "SD";
  }  
  output += "\", \"isOk\":";

  output += F("\"true\", \"totalBytes\":\"");
  output += buf_ttb;
  output += F("\", \"usedBytes\":\"");
  output += buf_udb;
  output += "\"";

  output += F(",\"unsupportedFiles\":\"");
  output += "";
  output += "\"}";
  request->send(200, "application/json", output);
  output = String();
}

void update_printProgress(size_t prg, size_t sz) {
  static uint32_t update_percent = 0;
  uint32_t per = prg * 100 / sz;
  if (update_percent != per)
  {
    char p[5];
    update_percent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", update_percent);    
    sprintf(p, "%u", update_percent);
    events.send(p, "dfu");
  }  
}

void sdfs_printProgress(size_t prg, size_t sz) {
  static uint32_t sdfs_percent = 0;
  uint32_t per = prg * 100 / sz;
  if (sdfs_percent != per)
  {
    char p[5];
    sdfs_percent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", sdfs_percent);    
    sprintf(p, "%u", sdfs_percent);
    events.send(p, "sdfs");
  }  
}

void spiffs_printProgress(size_t prg, size_t sz) {
  static uint32_t spiffs_percent = 0;
  uint32_t per = prg * 100 / sz;
  if (spiffs_percent != per)
  {
    char p[5];
    spiffs_percent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\r\n", spiffs_percent);    
    sprintf(p, "%u", spiffs_percent);
    events.send(p, "spiffs");
  }  
}

uint8_t authentication_level(AsyncWebServerRequest *request)
{
  uint8_t level;
  if(request->authenticate("admin", "25123"))
  {
    level = HTTP_AUTH_LV0;
  }
  else if(request->authenticate(http_username, http_password))
  {
    level = HTTP_AUTH_LV0;
  }
  else if(request->authenticate(http_username1, http_password1))
  {
    level = HTTP_AUTH_LV1;
  }
  else
  {
    level = HTTP_AUTH_FAIL;
    request->requestAuthentication();
  }

  return level;
}

void web_server_setup(void)
{
  wifi_file_json_t *g_wifi_cfg;  

  g_wifi_cfg = wifi_info_get();
  http_username = g_wifi_cfg->auth.user;
  http_password = g_wifi_cfg->auth.pass;
  http_username1 = g_wifi_cfg->auth_user.user;
  http_password1 = g_wifi_cfg->auth_user.pass;

  /* redirect port 80 to tcp port */
  if(g_wifi_cfg->port.tcp != 80)
  {
    server80.addHandler(new RedirectUrlHandler());
  }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
    WEB_SERVER_DBG_PRINTF("\r\nevents connect: %u", client->lastId());
  });
  server.addHandler(&events);

  spiffs_editor.onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });
  spiffs_editor.onProgress(spiffs_printProgress);
  spiffs_editor.onStatus(fs_editor_status);
  server.addHandler(&spiffs_editor);
  
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  sd_editor.onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });
  sd_editor.onProgress(sdfs_printProgress);
  sd_editor.onStatus(fs_editor_status);
  server.addHandler(&sd_editor);
#endif  

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    if(authentication_level(request) != HTTP_AUTH_FAIL)
    {
      server_data_get_process(request);
    }   
  });

  server.on("/get_open", HTTP_GET, [](AsyncWebServerRequest *request){
    server_data_get_process(request);
  });

  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
    if(authentication_level(request) != HTTP_AUTH_FAIL)
    {
      server_data_post_process(request);
    }
  });

  /* Serving files in directory. Serving static files with authentication */
  // server.serveStatic("/", NAND_FS_SYSTEM, "/").setAuthentication(http_username, http_password);
  /*
  WebHandlerlmpl.h edit
  line 32 add: typedef std::function<bool(AsyncWebServerRequest *request)> ArRequestAuthenticateFunction;
  line 49 add: ArRequestAuthenticateFunction _onAuthenticate;
  line 62 add: AsyncStaticWebHandler& onAuthenticate(ArRequestAuthenticateFunction fn) {_onAuthenticate = fn; return *this;}

  WebHandlers.cpp edit
  Line 193
  if(_username != "" && _password != "")
    {
      if(!request->authenticate(_username.c_str(), _password.c_str()))
      {
        return request->requestAuthentication();
      } 
    }     
    else
    {
      if(_onAuthenticate)
      {
        if(!_onAuthenticate(request))
        {
          return;
        }
      }
    }
  */
 server.serveStatic("/", NAND_FS_SYSTEM, "/").setDefaultFile("index.htm").onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  server.serveStatic("/onsd", SD_FS_SYSTEM, "/").onAuthenticate([](AsyncWebServerRequest *request){
    return (authentication_level(request) != HTTP_AUTH_FAIL);
  });
#endif  

#ifdef ESP32
  Update.onProgress(update_printProgress);
#endif
  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if(authentication_level(request) == HTTP_AUTH_LV0)
    {
      request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
    }
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", !Update.hasError()?"Update OK":"Update FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
    if (!Update.hasError())
    {
      update_printProgress(100, 100); // 100%
      if (U_FLASH == update_cmd)
      {
        esp_reset_enable(500);
      }
      else
      {
        wifi_file_json_t *g_wifi_cfg;
        g_wifi_cfg = wifi_info_get();
        wifi_info_write(g_wifi_cfg);
      }
    }
    else
    {
      update_printProgress(101, 100); // 101%
    }
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      size_t length = request->contentLength();
      WEB_SERVER_DBG_PORT.printf("Update Start: %s, size=%u\r\n", filename.c_str(), length);
#ifdef ESP8266
      // if filename includes spiffs, update the spiffs partition
      if ((filename.indexOf("spiffs") > -1) || (filename.indexOf("littlefs") > -1))
      {
        update_cmd = U_FS;
        length = UPDATE_SIZE_UNKNOWN;
      }
      else
      {
        update_cmd = U_FLASH;
      }
      Update.runAsync(true);
      if (!Update.begin(length, update_cmd))
#else
      // if filename includes spiffs, update the spiffs partition
      if ((filename.indexOf("spiffs") > -1) || (filename.indexOf("littlefs") > -1))
      {
        update_cmd = U_SPIFFS;
        length = UPDATE_SIZE_UNKNOWN;
      }
      else
      {
        update_cmd = U_FLASH;
      }

      if (!Update.begin(length, update_cmd))
#endif
      {
        Update.printError(WEB_SERVER_DBG_PORT);
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(WEB_SERVER_DBG_PORT);
      }
    }
    if(final){
      if(Update.end(true)){
        WEB_SERVER_DBG_PORT.printf("Update Success: %uB\r\n", index+len);
      } else {
        Update.printError(WEB_SERVER_DBG_PORT);
      }
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    WEB_SERVER_DBG_PORT.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      WEB_SERVER_DBG_PORT.printf("GET");
    else if(request->method() == HTTP_POST)
      WEB_SERVER_DBG_PORT.printf("POST");
    else if(request->method() == HTTP_DELETE)
      WEB_SERVER_DBG_PORT.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      WEB_SERVER_DBG_PORT.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      WEB_SERVER_DBG_PORT.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      WEB_SERVER_DBG_PORT.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      WEB_SERVER_DBG_PORT.printf("OPTIONS");
    else
      WEB_SERVER_DBG_PORT.printf("UNKNOWN");
    WEB_SERVER_DBG_PORT.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      WEB_SERVER_DBG_PORT.printf("_CONTENT_TYPE: %s\r\n", request->contentType().c_str());
      WEB_SERVER_DBG_PORT.printf("_CONTENT_LENGTH: %u\r\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      WEB_SERVER_DBG_PORT.printf("_HEADER[%s]: %s\r\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        WEB_SERVER_DBG_PORT.printf("_FILE[%s]: %s, size: %u\r\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        WEB_SERVER_DBG_PORT.printf("_POST[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
      } else {
        WEB_SERVER_DBG_PORT.printf("_GET[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
    {
      WEB_SERVER_DBG_PORT.printf("UploadStart: %s\r\n", filename.c_str());
    }
    WEB_SERVER_DBG_PORT.printf("%s", (const char*)data);
    if(final)
    {
      WEB_SERVER_DBG_PORT.printf("UploadEnd: %s (%u)\r\n", filename.c_str(), index+len);
    }
  });

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
    {
      WEB_SERVER_DBG_PORT.printf("BodyStart: %u\r\n", total);
    }
    WEB_SERVER_DBG_PORT.printf("%s", (const char*)data);
    if(index + len == total)
    {
      WEB_SERVER_DBG_PORT.printf("BodyEnd: %u\r\n", total);
    }
  });
}

void web_server_init(void)
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  
  if (g_wifi_cfg->port.tcp < 80)
  {
    g_wifi_cfg->port.tcp = 25123;
  }

  WEB_SERVER_DBG_PRINTF("\r\nInit Web Server Port: %u\r\n", g_wifi_cfg->port.tcp);

  server.begin(g_wifi_cfg->port.tcp);  
  if(g_wifi_cfg->port.tcp != 80)
  {
    server80.begin();
  }
}

void web_server_end(void)
{
  WEB_SERVER_DBG_PRINTF("\r\nEnd web server service");
  server.end();
}
