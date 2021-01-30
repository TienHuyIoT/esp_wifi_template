#ifndef	_SERVER_DATA_PROCESS_
#define _SERVER_DATA_PROCESS_

#include <ESPAsyncWebServer.h>

#define DATA_GET_HANDLE_NUM     12
#define DATA_POST_HANDLE_NUM    8

typedef struct {
    char *path_arg;
    void (*cb)(AsyncWebServerRequest *request);
} server_post_handle_t;

typedef struct {
    char *path_arg;
    void (*cb)(AsyncWebServerRequest *request);
} server_get_handle_t;

void server_data_get_process(AsyncWebServerRequest *request);
void server_data_post_process(AsyncWebServerRequest *request);

void print_handlerequest(AsyncWebServerRequest *request, String &message);

void sta_ap_info_get(AsyncWebServerRequest *request);
void sta_network_get(AsyncWebServerRequest *request);
void sta_setting_get(AsyncWebServerRequest *request);
void ap_setting_get(AsyncWebServerRequest *request);
void device_info_get(AsyncWebServerRequest *request);
void time_setting_get(AsyncWebServerRequest *request);
void fw_version_get(AsyncWebServerRequest *request);
void restart_device_get(AsyncWebServerRequest *request);
void heap_temperature_get(AsyncWebServerRequest *request);
void activated_get(AsyncWebServerRequest *request);
void format_sd_card_get(AsyncWebServerRequest *request);
void ddns_client_get(AsyncWebServerRequest *request);

void sta_ap_info_post(AsyncWebServerRequest *request);
void sta_network_post(AsyncWebServerRequest *request);
void sta_setting_post(AsyncWebServerRequest *request);
void ap_setting_post(AsyncWebServerRequest *request);
void device_info_post(AsyncWebServerRequest *request);
void auth_access_post(AsyncWebServerRequest *request);
void time_setting_post(AsyncWebServerRequest *request);
void ddns_client_post(AsyncWebServerRequest *request);

/* /get?param_wifi=[param] */
server_get_handle_t client_get_handle[DATA_GET_HANDLE_NUM] = {
/*00*/{(char*)"sta_ap_info", sta_ap_info_get},
/*01*/{(char*)"sta_network", sta_network_get},
/*02*/{(char*)"sta_setting", sta_setting_get},
/*03*/{(char*)"ap_setting", ap_setting_get},
/*04*/{(char*)"device_info", device_info_get},
/*05*/{(char*)"time_setting", time_setting_get},
/*06*/{(char*)"fw_version", fw_version_get},
/*07*/{(char*)"restart", restart_device_get},
/*08*/{(char*)"heap_temperature", heap_temperature_get},
/*09*/{(char*)"activated", activated_get},
/*10*/{(char*)"format_sd_card", format_sd_card_get},
/*11*/{(char*)"ddns_client", ddns_client_get}
};

server_post_handle_t client_post_handle[DATA_POST_HANDLE_NUM] = {
/*00*/{(char*)"sta_ap_info", sta_ap_info_post},
/*01*/{(char*)"sta_network", sta_network_post},
/*02*/{(char*)"sta_setting", sta_setting_post},
/*03*/{(char*)"ap_setting", ap_setting_post},
/*04*/{(char*)"device_info", device_info_post},
/*05*/{(char*)"auth_access", auth_access_post},
/*06*/{(char*)"time_setting", time_setting_post},
/*07*/{(char*)"ddns_client", ddns_client_post}
};

#endif