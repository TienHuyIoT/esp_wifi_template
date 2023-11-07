#ifndef __ESP_WEB_SERVER_URL_HANDLE_H
#define __ESP_WEB_SERVER_URL_HANDLE_H

#include <ESPAsyncWebServer.h>
#include "THIoT_ESPWebserver.h"

#define DATA_GET_HANDLE_NUM     18
#define DATA_POST_HANDLE_NUM    11

class WebServerURLHandle : public WebServerURLHandleCallbacks {
private:
    String _argName;
    /**
     * Handler called after once request with method GET and authenticated.
     */
    int onHttpGetAuth(AsyncWebServerRequest* request);
    /**
     * Handler called after once request with method GET.
     */
    int onHttpGet(AsyncWebServerRequest* request);
    /**
     * Handler called after once request with method POST and authenticated.
     */
    int onHttpPostAuth(AsyncWebServerRequest* request);
public:
    WebServerURLHandle(const char *argName);
    ~WebServerURLHandle();
};

typedef struct {
    const char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, WebServerURLHandle* client);
} server_post_handle_t;

typedef struct {
    const char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, WebServerURLHandle* client);
} server_get_handle_t;

/* get */
void sta_ap_info_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void sta_network_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void sta_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void ap_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void sntp_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void device_info_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void time_setting_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void fw_version_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void restart_device_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void heap_temperature_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void activated_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void format_sd_card_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void ddns_client_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void pass_common_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void format_spiffs_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void reset_all_password_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void reset_system_params_get(AsyncWebServerRequest *request, WebServerURLHandle* client);
void clean_log_trace_get(AsyncWebServerRequest *request, WebServerURLHandle* client);

/* Post */
void sta_ap_info_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void sta_network_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void sta_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void ap_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void sntp_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void device_info_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void auth_access_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void time_setting_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void ddns_client_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void auth_user_access_post(AsyncWebServerRequest *request, WebServerURLHandle* client);
void pass_common_post(AsyncWebServerRequest *request, WebServerURLHandle* client);

#endif // __ESP_WEB_SERVER_URL_HANDLE_H