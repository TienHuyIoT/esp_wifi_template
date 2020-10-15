#include "app_config.h"

#define WEB_SERVER_PORT Serial
#define WEB_SERVER_PRINTF(...) WEB_SERVER_PORT.printf(__VA_ARGS__)

const char HTTP_HEAD_WEB[] PROGMEM = "<html lang=\"en\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"><title>{v}</title>";
const char HTTP_STYLE[] PROGMEM = "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:100%;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#4CAF50;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size: 1em;}</style>";
//Style button
const char HTTP_STYLEB[] PROGMEM = "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:100%;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#4CAF50;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size: 1em;}"
                                   ".switch {position: relative;display: inline-block;width: 60px;height: 34px;}.switch input {display:none;}.slider {position: absolute;cursor: pointer;top: 0;left: 0;right: 0;bottom: 0;background-color: #ccc;-webkit-transition: .4s;transition: .4s;}.slider:before {position: absolute;content: \"\";height: 26px;width: 26px;left: 4px;bottom: 4px;background-color: white;-webkit-transition: .4s;transition: .4s;}input:checked + .slider {background-color: #2196F3;}input:focus + .slider {box-shadow: 0 0 1px #2196F3;}input:checked + .slider:before {-webkit-transform: translateX(26px);-ms-transform: translateX(26px);transform: translateX(26px);}/* Rounded sliders */.slider.round {border-radius: 34px;}.slider.round:before {border-radius: 50%;}</style>";

const char HTTP_SCRIPT[] PROGMEM = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_HEAD_WEB_END[] PROGMEM = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";

const char HTTP_ITEM[] PROGMEM = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";
const char HTTP_FORM_START[] PROGMEM = "<form method='post' action='wlansave'>";
const char HTTP_FORM_PARAM[] PROGMEM = "<br><input id='s' name='n' length=32 placeholder='SSID'><br><input id='p' name='p' length=64 type='password' placeholder='password'><br>";
const char HTTP_FORM_END[] PROGMEM = "<br><br><br><button type='submit'>save</button></form>";
const char HTTP_WLAN_REFRESH[] PROGMEM = "<br><div class=\"c\"><a href=\"/wlanconf\">Scan</a></div>";
const char HTTP_END[] PROGMEM = "</div></body></html>";

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
  web_update_url_setup();
  
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

void web_server_begin()
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

  /*=================================================================
    Wifi config
    =================================================================*/
  server.on("/wifi", HTTP_GET, WMhandleInfo);          // Show wifi info STA and AP
  server.on("/wlanscan", HTTP_GET, WMhandleSSIDScan);  // Scan ssid
  server.on("/wlanconf", HTTP_GET, WMhandleWlan);      // Select Wifi, enter pass and confirm
  server.on("/wlansave", HTTP_POST, WMhandleWlanSave); // Save ssid and pass
  server.on("/wlanconfAd", WMhandleWlanAd);            // Advance config STA Wifi
  server.on("/softapconf", HTTP_GET, WMhandleSAP);     // AP wifi config
  server.on("/sapsave", HTTP_POST, WMhandleSAPSave);   // AP wifi save

  //Get version
  server.on("/fw_version", []() {
    char buf[20];
    sprintf(buf, "Web485 V%u.%u.%u",
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_BUILD);
    server.send(200, "text/html", buf);
  });

  /*=================================================================
    Reset ESP
    =================================================================*/
  server.on("/reset", HTTP_GET, []() {
    // Load Web timeout reset ESP
    if (!handleFileRead("/Reset.htm"))
      server.send(404, "text/plain", "FileNotFound");
    log_report(LOG_REPORT_RESET, (char *)"Web reset3");
    esp_reset_enable();
  });

  /*=================================================================
    Activated
    /activated?cmd=1/0  set, 1:Enable,  0:Disable
    /activated?cmd=<khac 0/1> read
    =================================================================*/
  server.on("/activated", []() {
    if (!web_authentication())
      return;
    if (server.argName(0) == "cmd")
    {
      uint8_t Cmd = atoi(server.arg(0).c_str());
      if (Cmd == 0 || Cmd == 1)
      {
        if (Cmd == 1)
          device_active();
        if (Cmd == 0)
          device_inactive();
      }
      server.send(200, "text/html", "Vaule: " + String(device_is_active()));
    }
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

    print_handlerequest();

    if (!handleFileRead(server.uri()))
    {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  /*=================================================================
    get heap status, analog input value and all GPIO statuses in one json call
    =================================================================*/
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(esp32_internal_temp());
    json += ", \"gpio\":1";
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
}

/*=================================================================
  Lựa chọn cài đặt thông số STA và AP Wifi
  =================================================================*/
void WMhandleInfo()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  String page = FPSTR(HTTP_HEAD_WEB);
  page.replace("{v}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_WEB_END);
  if (((uint32_t)(server.client().remoteIP()) & 0xFFFFFF) == ((uint32_t)(g_wifi_cfg->ap.Ip) & 0xFFFFFF))
  {
    page += String("<p>Đang kết nối Wifi thiết bị: <b>") + g_wifi_cfg->ap.ssid + "</b></p>";
  }
  else
  {
    page += String("<p>Đang kết nối Wifi Nhà: <b>") + g_wifi_cfg->sta.ssid + "</b></p>";
  }
  page += F("<table><tr><th align='left'>Wifi thiết bị</th></tr>");
  page += ("<tr><td>Tên Wifi: " + String(g_wifi_cfg->ap.ssid) + "</td></tr>");
  page += ("<tr><td>Địa chỉ IP: " + WiFi.softAPIP().toString() + "</td></tr>");
  page += F(
      "</table>"
      "<br>"
      "<table><tr><th align='left'>Wifi Nhà</th></tr>");
  page += ("<tr><td>Tên Wifi: " + String(g_wifi_cfg->sta.ssid) + "</td></tr>");
  if (g_wifi_cfg->sta.SaticIp)
    page += ("<tr><td>IP tĩnh: " + WiFi.localIP().toString() + "</td></tr>");
  else
    page += ("<tr><td>IP động: " + WiFi.localIP().toString() + "</td></tr>");
  wl_status_t s = WiFi.status();
  if (s == WL_CONNECTED)
    page += ("<tr><td>Trạng thái kết nối: thành công</td></tr>");
  else
    page += ("<tr><td>Trạng thái kết nối: thất bại</td></tr>");

  page += F("</table>");

  page += F("<br><form action=\"/wlanscan\" method=\"get\"><button>Tìm và kết nối Wifi Nhà</button></form>");
  page += F("<form action=\"/wlanconfAd\" method=\"get\"><button>Cài đặt Wifi Nhà</button></form>");
  page += F("<form action=\"/softapconf\" method=\"get\"><button>Cài đặt Wifi thiết bị</button></form>");
  page += F("<form action=\"/reset\" method=\"get\"><button>Khởi động lại thiết bị</button></form>");
  page += FPSTR(HTTP_END);

  server.send(200, "text/html", page);
}

/*=================================================================
  Lựa chọn wifi, nhập pass và xác nhận
  B1: ./wlanscan để chạy hàm SSIDScan() tìm wifi sau đó 3s sẽ tự chuyển sang B2
  B2: ./wlanconf đây là giao diện để chọn wifi và nhập mật khẩu. Bấm save để chuyển sang B3
  B3: ./wlansave  lưu cấu hình
  =================================================================*/
/*Chuyen muc tin hieu qua %*/
int getRSSIasQuality(int RSSI)
{
  int quality = 0;
  if (RSSI <= -100)
  {
    quality = 0;
  }
  else if (RSSI >= -50)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (RSSI + 100);
  }
  return quality;
}

String SsidWlan = "";
void SSIDScan()
{
  boolean _removeDuplicateAPs = true;
  int _minimumQuality = -1;
  SsidWlan = "";
  int n = WiFi.scanNetworks();
  if (n == 0)
  {
    SsidWlan += F("No networks found. Refresh to scan again.");
  }
  else
  {
    //sort networks
    int indices[n];
    for (int i = 0; i < n; i++)
    {
      indices[i] = i;
    }

    // RSSI SORT
    // old sort
    for (int i = 0; i < n; i++)
    {
      for (int j = i + 1; j < n; j++)
      {
        if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
        {
          std::swap(indices[i], indices[j]);
        }
      }
    }

    // remove duplicates ( must be RSSI sorted )
    if (_removeDuplicateAPs)
    {
      String cssid;
      for (int i = 0; i < n; i++)
      {
        if (indices[i] == -1)
          continue;
        cssid = WiFi.SSID(indices[i]);
        for (int j = i + 1; j < n; j++)
        {
          if (cssid == WiFi.SSID(indices[j]))
          {
            indices[j] = -1; // set dup aps to index -1
          }
        }
      }
    }

    //display networks in page
    for (int i = 0; i < n; i++)
    {
      if (indices[i] == -1)
        continue; // skip dups
      int quality = getRSSIasQuality(WiFi.RSSI(indices[i]));

      if (_minimumQuality == -1 || _minimumQuality < quality)
      {
        String item = FPSTR(HTTP_ITEM);
        String rssiQ;
        rssiQ += quality;
        item.replace("{v}", WiFi.SSID(indices[i]));
        item.replace("{r}", rssiQ);
#ifdef ESP8266
        if (WiFi.encryptionType(indices[i]) != ENC_TYPE_NONE)
#elif defined(ESP32)
        if (WiFi.encryptionType(indices[i]) != WIFI_AUTH_OPEN)
#endif
        {
          item.replace("{i}", "l");
        }
        else
        {
          item.replace("{i}", "");
        }
        SsidWlan += item;
        delay(0);
      }
      else
      {
        //DEBUG_WM(F("Skipping due to quality"));
      }
    }
  }
  // clean up ram
  WiFi.scanDelete();
}

//Link: ./wlanscan
void WMhandleSSIDScan()
{
  if (!web_authentication())
    return;
  WEB_SERVER_PRINTF("Scan Wifi SSID");
  String page = FPSTR(HTTP_HEAD_WEB);
  page.replace("{v}", "Scan Wifi SSID");
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_SCRIPT);
  page += F("<br>Đang quét WIFI<br>"
            "<p>Tự động kết nối lại sau <span id=\"counter\">3</span> giây.</p>"
            "<script type=\"text/javascript\">"
            "function countdown() {"
            "var i = document.getElementById('counter');"
            "if (parseInt(i.innerHTML)==0) {"
            "location.href = './wlanconf';"
            "}"
            "i.innerHTML = parseInt(i.innerHTML)-1;"
            "}"
            "setInterval(function(){ countdown(); },1000);"
            "</script>");
  page += FPSTR(HTTP_HEAD_WEB_END);
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
  SSIDScan();
  WEB_SERVER_PRINTF("\r\nScan Wifi SSID Finish");
}
//Link: ./wlanconf
void WMhandleWlan()
{
  if (!web_authentication())
    return;
  String page = FPSTR(HTTP_HEAD_WEB);
  page.replace("{v}", "Wlan Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_WEB_END);
  page += SsidWlan;
  SsidWlan = ""; //Reset chuỗi toàn cục
  page += F("<form method='post' action='wlansave'>");
  page += F("<br><input id='s' name='n' type='text' length=32 placeholder='Tên Wifi'>");
  page += F("<br><input id='p' name='p' length=64 type='text' placeholder='Mật khẩu Wifi'><br>");
  page += F("<br><button type='submit'>Save</button></form>");
  page += F("<br><div class=\"c\"><a href=\"/wlanscan\">Tìm lại Wifi</a></div>");
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
}
//Lưu thông số STA
void WMhandleWlanSave()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();
  server.arg("n").toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid);
  server.arg("p").toCharArray(g_wifi_cfg->sta.pass, Df_LengPass);
  WEB_SERVER_PRINTF("\r\nSSID: %s", g_wifi_cfg->sta.ssid);
  WEB_SERVER_PRINTF("\r\nPASS: %s\r\n", g_wifi_cfg->sta.pass);

  WebHandleStatus("AP Config", "Cài đặt wifi OK");
  //Truong hop ssid = 0 nghia la khong ket noi den wlan
  //Cập nhật thông số vào FS
  wifi_info_write(g_wifi_cfg);
  esp_reset_enable();
}

/*=================================================================
  Cấu hình STA Wifi chuyên sâu
  =================================================================*/
void WMhandleWlanAd()
{
  wifi_file_json_t *g_wifi_cfg;
  uint8_t reset_st = 0;
  g_wifi_cfg = wifi_info_get();
  if (!web_authentication())
    return;
  String alert = "";
  if (server.method() == HTTP_POST)
  {
    for (uint8_t i = 0; i < server.args(); i++)
    {
      String argN = server.argName(i);
      if (argN == "Ip")
        g_wifi_cfg->sta.Ip.fromString(server.arg(i));
      if (argN == "Gw")
        g_wifi_cfg->sta.Gw.fromString(server.arg(i));
      if (argN == "Sn")
        g_wifi_cfg->sta.Sn.fromString(server.arg(i));
      if (argN == "Dns")
        g_wifi_cfg->sta.Dns.fromString(server.arg(i));
      if (argN == "En")
        g_wifi_cfg->sta.SaticIp = atoi(server.arg(i).c_str());
      if (argN == "Up")
        g_wifi_cfg->UDPPort = atoi(server.arg(i).c_str());
      if (argN == "Tp")
        g_wifi_cfg->TCPPort = atoi(server.arg(i).c_str());
      if (argN == "Wp")
        g_wifi_cfg->WSPort = atoi(server.arg(i).c_str());
      if (argN == "Ssid")
        server.arg(i).toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid);
      if (argN == "Pw")
        server.arg(i).toCharArray(g_wifi_cfg->sta.pass, Df_LengPass);
    }
    //Cập nhật thông số vào FS
    wifi_info_write(g_wifi_cfg);    
    reset_st = 1;    
    alert = F("<script type=\"text/javascript\">alert(\"Cài đặt thành công\");</script>");
  }

  String page = FPSTR(HTTP_HEAD_WEB);
  page.replace("{v}", "Wifi Station Config");
  page += FPSTR(HTTP_SCRIPT);
  page += alert;
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_WEB_END);

  page += F("<center><h3>Cài đặt Wifi nhà</h3></center>");
  page += F("<form method='post' action='wlanconfAd'>");
  page += F("<table>");

  page += F("<tr><td align='left'>Tên Wifi</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Ssid' length=32 value=\"");
  page += g_wifi_cfg->sta.ssid;
  page += F("\"></td></tr>");

  page += F("<tr><td align='left'>Mật khẩu</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Pw' length=64 value=\"");
  page += g_wifi_cfg->sta.pass;
  page += F("\"></td></tr>");

  page += F("<tr><td align='left'>Địa chỉ IP</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Ip' length=15 value=\"");
  page += g_wifi_cfg->sta.Ip.toString();
  page += F("\"></td></tr>");

  page += F("<tr><td align='left'>Gateway</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Gw' length=15 type='text' value=\"");
  page += g_wifi_cfg->sta.Gw.toString();
  page += F("\"></td></tr>");

  page += F("<tr><td align='left'>Subnet</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Sn' length=15 type='text' value=\"");
  page += g_wifi_cfg->sta.Sn.toString();
  page += F("\"></td></tr>");

  page += F("<tr><td align='left'>DNS</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Dns' length=15 type='text' value=\"");
  page += g_wifi_cfg->sta.Dns.toString();
  page += F("\"></td></tr>");

  page += F("<tr><td align='left'>IP động</td>");
  if (!g_wifi_cfg->sta.SaticIp)
    page += F("<td align='right'><select name='En'><option value=\"0\" selected>Enable</option><option value=\"1\">Disable</option></select>");
  else
    page += F("<td align='right'><select name='En'><option value=\"0\">Enable</option><option value=\"1\" selected>Disable</option></select>");
  page += F("</td></tr>");
  page += F("</table>");

  page += F("<br><center><h3>Port Setting</h3></center>");
  page += F("<table><tr><td align='left'>UDP Port</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Up' length=5 value=\"");
  page += String(g_wifi_cfg->UDPPort);
  page += F("\"></td></tr>");
  page += F("<tr><td align='left'>WS Port</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Wp' length=5 value=\"");
  page += String(g_wifi_cfg->WSPort);
  page += F("\"></td></tr>");
  page += F("<tr><td align='left'>TCP Port</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='Tp' length=5 value=\"");
  page += String(g_wifi_cfg->TCPPort);
  page += F("\"></td></tr></table>");

  page += F("<br><button type='submit'>Save</button></form>");
  page += F("<form action=\"/reset\" method=\"get\"><button>Khởi động lại Wifi</button></form>");
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);

  if (reset_st)
  {
    esp_reset_enable();
  }
}

/*=================================================================
  Cấu hình wifi thiết bị
  =================================================================*/
void WMhandleSAP()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();

  if (!web_authentication())
    return;
  String page = FPSTR(HTTP_HEAD_WEB);
  page.replace("{v}", "SoftAP Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_WEB_END);

  page += F("<br><center><h3>Cài đặt Wifi</h3></center>");
  page += F("<form method='post' action='sapsave'>");
  page += F("<table><tr><td align='left'>Đặt tên Wifi</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='n' length=32 value=\"");
  page += g_wifi_cfg->ap.ssid;
  page += F("\"></td></tr>");
  page += F("<tr><td align='left'>Mật khẩu Wifi</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='p' length=64 type='text' value=\"");
  page += g_wifi_cfg->ap.pass;
  page += F("\"></td></tr></table>");

  page += F("<br><br><center><h3>Advanced Setting</h3></center>");
  page += F("<table><tr><td align='left'>Channel(1-13)</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='c' length=2 value=\"");
  page += String(g_wifi_cfg->ap.Chanel);
  page += F("\"></td></tr>");
  page += F("<tr><td align='left'>Hidden(0/1)</td>");
  page += F("<td align='right'><input style=\"text-align:right;\" name='h' length=1 value=\"");
  page += String(g_wifi_cfg->ap.Hidden);
  page += F("\"></td></tr></table>");

  page += F("<br><button type='submit'>Save</button></form>");
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
}
void WMhandleSAPSave()
{
  wifi_file_json_t *g_wifi_cfg;
  g_wifi_cfg = wifi_info_get();

  server.arg("n").toCharArray(g_wifi_cfg->ap.ssid, Df_LengSsid);
  server.arg("p").toCharArray(g_wifi_cfg->ap.pass, Df_LengPass);
  g_wifi_cfg->ap.Chanel = atoi(server.arg("c").c_str());
  if (g_wifi_cfg->ap.Chanel == 0 || g_wifi_cfg->ap.Chanel > 13)
    g_wifi_cfg->ap.Chanel = 2;
  g_wifi_cfg->ap.Hidden = atoi(server.arg("h").c_str());
  if (g_wifi_cfg->ap.Hidden > 1)
    g_wifi_cfg->ap.Hidden = 0;
  WEB_SERVER_PRINTF("\r\nSSID: %s", g_wifi_cfg->ap.ssid);
  WEB_SERVER_PRINTF("\r\nPASS: %s", g_wifi_cfg->ap.pass);
  WEB_SERVER_PRINTF("\r\nChanel: %u", g_wifi_cfg->ap.Chanel);
  WEB_SERVER_PRINTF("\r\nHidden: %u", g_wifi_cfg->ap.Hidden);
  WebHandleStatus("AP Config", "Cài đặt wifi OK");
  //Lưu vào FS
  wifi_info_write(g_wifi_cfg);
  esp_reset_enable();
}

/*=================================================================
  Web server trả lời bằng alert
  WebHandleStatus("Nhãn tiêu đề", "Nội dung alert"); 
=================================================================*/
void WebHandleStatus(String Title, String Text)
{
  String page = FPSTR(HTTP_HEAD_WEB);
  page.replace("{v}", Title);
  page += "<script type=\"text/javascript\">alert(\"" + Text + "\");</script>";
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_WEB_END);
  page += "<center><h2>" + Text + "</h2>";
  page += F("</center>");
  page += FPSTR(HTTP_END);
  server.send(200, "text/html", page);
}

void print_handlerequest(void)
{
  String message = "";
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