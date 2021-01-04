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

#define WEB_SERVER_DBG_PORT Serial
#define WEB_SERVER_DBG_PRINTF(...) WEB_SERVER_DBG_PORT.printf(__VA_ARGS__)

void update_printProgress(size_t prg, size_t sz) {
  static uint32_t update_percent = 0;
  uint32_t per = prg * 100 / update_content_len;
  if (update_percent != per)
  {
    char p[5];
    update_percent = per;
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\n", update_percent);    
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
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\n", sdfs_percent);    
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
    WEB_SERVER_DBG_PRINTF("Progress: %u%%\n", spiffs_percent);    
    sprintf(p, "%u", spiffs_percent);
    events.send(p, "spiffs");
  }  
}

void web_server_setup(void)
{
  wifi_file_json_t *g_wifi_cfg;  

  g_wifi_cfg = wifi_info_get();
  http_username = g_wifi_cfg->auth.user;
  http_password = g_wifi_cfg->auth.pass;
  
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);

  spiffs_editor.setAuthentication(http_username, http_password);
  spiffs_editor.onProgress(spiffs_printProgress);
  server.addHandler(&spiffs_editor);
#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  sd_editor.setAuthentication(http_username, http_password);
  sd_editor.onProgress(sdfs_printProgress);
  server.addHandler(&sd_editor);
#endif  

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
    {
      return request->requestAuthentication();
    }
    server_data_get_process(request);
  });

  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
    {
      return request->requestAuthentication();
    }
    server_data_get_process(request);
  });

  /* Serving files in directory. Serving static files with authentication */
  server.serveStatic("/", NAND_FS_SYSTEM, "/").setDefaultFile("index.htm").setAuthentication(http_username, http_password);

#ifdef ESP32
  Update.onProgress(update_printProgress);
#endif
  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
    {
      return request->requestAuthentication();
    }
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", !Update.hasError()?"Update OK":"Update FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
    if (!Update.hasError())
    {
      update_printProgress(update_content_len, UPDATE_SIZE_UNKNOWN);
      esp_reset_enable(500);
    }    
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      WEB_SERVER_DBG_PORT.printf("Update Start: %s\n", filename.c_str());
      update_content_len = request->contentLength();      
#ifdef ESP8266
      // if filename includes spiffs, update the spiffs partition
      int cmd = (filename.indexOf("spiffs") > -1) ? U_FS : U_FLASH;
      Update.runAsync(true);
      if (!Update.begin(update_content_len, cmd))
#else
      // if filename includes spiffs, update the spiffs partition
      int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
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
        WEB_SERVER_DBG_PORT.printf("Update Success: %uB\n", index+len);
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
      WEB_SERVER_DBG_PORT.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      WEB_SERVER_DBG_PORT.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      WEB_SERVER_DBG_PORT.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        WEB_SERVER_DBG_PORT.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        WEB_SERVER_DBG_PORT.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        WEB_SERVER_DBG_PORT.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
    {
      WEB_SERVER_DBG_PORT.printf("UploadStart: %s\n", filename.c_str());
    }
    WEB_SERVER_DBG_PORT.printf("%s", (const char*)data);
    if(final)
    {
      WEB_SERVER_DBG_PORT.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
    }
  });

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
    {
      WEB_SERVER_DBG_PORT.printf("BodyStart: %u\n", total);
    }
    WEB_SERVER_DBG_PORT.printf("%s", (const char*)data);
    if(index + len == total)
    {
      WEB_SERVER_DBG_PORT.printf("BodyEnd: %u\n", total);
    }
  });
}

void web_server_init(void)
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  
  if (g_wifi_cfg->TCPPort < 80)
  {
    g_wifi_cfg->TCPPort = 25123;
  }

  WEB_SERVER_DBG_PRINTF("\r\nInit Web Server Port: %u\r\n", g_wifi_cfg->TCPPort);

  server.begin(g_wifi_cfg->TCPPort);
}

void web_server_end(void)
{
  WEB_SERVER_DBG_PRINTF("\r\nEnd web server service");
  server.end();
}
