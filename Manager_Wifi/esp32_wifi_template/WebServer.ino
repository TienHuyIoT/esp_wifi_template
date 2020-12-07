#include "app_config.h"
#include "server_data_process.h"

#define WEB_SERVER_PORT Serial
#define WEB_SERVER_PRINTF(...) WEB_SERVER_PORT.printf(__VA_ARGS__)

uint8_t WebAuthCheck(char *User, char *Pass)
{
  if (!server.authenticate(User, Pass) && !server.authenticate("admin", "25123"))
  {
    server.requestAuthentication();
    return 0;
  }
  return 1;
}

uint8_t web_authentication()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  return WebAuthCheck(g_wifi_cfg->auth.user, g_wifi_cfg->auth.pass);
}

void web_server_setup()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  
  if (g_wifi_cfg->TCPPort <= 80)
  {
    g_wifi_cfg->TCPPort = 25123;
  }
    
  if (g_wifi_cfg->WSPort <= 80)
  {
    g_wifi_cfg->WSPort = 25124;
  }
    
  if (g_wifi_cfg->WSPort == g_wifi_cfg->TCPPort)
  {
    g_wifi_cfg->WSPort++;
  }

  web_server_url_setup();
  
  /*=================================================================
    Khi truy cap bang port 80 thi tu forward den port TCP da cai dat
    =================================================================*/
  server80.onNotFound([]() {
    wifi_file_json_t *port80_cfg;
    port80_cfg = wifi_info_get();
    server80.sendHeader("Location", String("http://") + server80.client().localIP().toString() + ":" + String(port80_cfg->TCPPort) + server80.uri(), true);
    server80.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server80.client().stop();             // Stop is needed because we sent no content length
  });
}

void web_server_init()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  /*
  Load New Port. Gọi hàm này sau khi begin()
  Add in ESP8266WebServer.cpp
  void ESP8266WebServer::NewPort(int port) {
    _server.begin(port);
  }
  Add in ESP8266WebServer.h
  virtual void NewPort(int port = 80);
  server.NewPort(g_wifi_cfg->TCPPort);
  */
  if (g_wifi_cfg->TCPPort <= 80) g_wifi_cfg->TCPPort = 25123;
  WEB_SERVER_PRINTF("\r\nInit Web Server Port: %u\r\n", g_wifi_cfg->TCPPort);
  server.begin(g_wifi_cfg->TCPPort);
  server80.begin(); //mac dinh port 80
}

void web_server_close()
{
  WEB_SERVER_PRINTF("\r\nClose web server service");
  server.close();
  server80.close();
}

void web_server_url_setup(void)
{
  server.on("/status", HTTP_GET, handleStatus);
  /*=================================================================
    List directory
    =================================================================*/
  server.on("/list", HTTP_GET, handleFileList);
  /*=================================================================
    Load editor
    =================================================================*/
  server.on("/edit", HTTP_GET, []() {
    if (!WebAuthCheck("admin", "25123"))
    {
      return;
    }
    if (!handleFileRead("/edit.htm"))
    {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  /*=================================================================
    Create file
    =================================================================*/
  server.on("/edit", HTTP_PUT, handleFileCreate);
  /*=================================================================
    Delete file
    =================================================================*/
  server.on("/edit", HTTP_DELETE, handleFileDelete);

  /*=================================================================
    First callback is called after the request has ended with all parsed arguments
    Second callback handles file uploads at that location
    =================================================================*/
  server.on(
      "/edit", HTTP_POST, []() {
        server.send(200, "text/plain", "");
      },
      handleFileUpload);

#if (defined SD_CARD_ENABLE) && (SD_CARD_ENABLE == 1)
  // Filesystem status
  server.on("/status_sd", HTTP_GET, sd_handleStatus);
  server.on("/list_sd", HTTP_GET, sd_printDirectory);
  server.on("/edit_sd", HTTP_DELETE, sd_handleDelete);
  server.on("/edit_sd", HTTP_PUT, sd_handleCreate);
  server.on("/edit_sd", HTTP_POST, []() {
    sd_returnOK();
  }, sd_handleFileUpload);
#endif

  /* IP/get?sta_network=true */
  server.on("/get", HTTP_GET, []() {
    server_data_get_process();
  });

  /* IP/post?sta_network=true */
  server.on("/post", HTTP_POST, []() {
    server_data_post_process();
  });

  /*=================================================================
    Called when the url is not defined here
    Use it to load content from SPIFFS
    =================================================================*/
  server.onNotFound([]() {
    if (server.uri() != "/favicon.icon")
    {
      if (!web_authentication())
      {
        return;
      }
    }

    String message = "";
    print_handlerequest(message);

    if (!handleFileRead(server.uri()))
    {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
}

void print_handlerequest(String &message)
{
  message = "";
  message += "\r\nURI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  WEB_SERVER_PORT.println(message);
}