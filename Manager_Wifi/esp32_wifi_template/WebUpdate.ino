#define WEB_UPDATE_PORT Serial
#define WEB_UPDATE_PRINTF(...) WEB_UPDATE_PORT.printf(__VA_ARGS__)

void web_update_url_setup(void) {
  /*=================================================================
    Post data
    =================================================================*/
  server.on("/postdata", HTTP_POST, []() {
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    WEB_UPDATE_PRINTF("\r\nargName(0): %s", server.argName(0).c_str());
    size_t size = server.arg(0).length();
    WEB_UPDATE_PRINTF("\r\nserver.arg(0) %u: %s", (uint16_t)size, server.arg(0).c_str());
    
    if (server.argName(0) == "Auth" && server.argName(1) == "PassId") {
      DynamicJsonBuffer djbpo(size + 1);
      JsonObject& root = djbpo.parseObject(server.arg(0));
      if (!root.success()) {
        server.send(200, "text/html", "Json Format Error");
        WEB_UPDATE_PRINTF("\r\nJSON parsing failed!");
        return;
      }
      if (server.arg(1) == "All") {
        if (root["OldPass"] == g_wifi_cfg->auth.pass) {
          root["NewPass"].as<String>().toCharArray(g_wifi_cfg->auth.pass, Df_LengAuth + 1);
          wifi_info_write(g_wifi_cfg);
          server.send(200, "text/html", "Đổi mật khẩu đăng nhập thành công");
        } else server.send(200, "text/html", "Mật khẩu đăng nhập cũ sai !");
      }
      return;
    }
    
    server.send(200, "text/html", "Post OK");
  });

  /*=================================================================
    Upload file vao bo nho FS
    =================================================================*/
  server.on("/upload", HTTP_POST, []() {
    WebHandleStatus("Upload ESP", "Upload File OK");
  }, handleFileUpload);

  /*=================================================================
    Update Firmware
    =================================================================*/
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    String Buf = "Update ESP ";
    if (Update.hasError()) Buf += "FAIL";
    else Buf += "OK";
    WebHandleStatus("Update ESP", Buf);

    log_report(LOG_REPORT_RESET, (char*)"Upgrade Reset4");
    
    esp_reset_enable();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      WEB_UPDATE_PORT.setDebugOutput(true);
      WEB_UPDATE_PRINTF("\r\nUpdate: %s\r\n", upload.filename.c_str());

      if (!Update.begin()) { //start with max available size
        Update.printError(WEB_UPDATE_PORT);
      }     
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(WEB_UPDATE_PORT);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        WEB_UPDATE_PRINTF("\r\nUpdate Success: %u\nRebooting...\r\n", upload.totalSize);
      } else {
        Update.printError(WEB_UPDATE_PORT);
      }
      WEB_UPDATE_PORT.setDebugOutput(false);
    }      
  });
}




