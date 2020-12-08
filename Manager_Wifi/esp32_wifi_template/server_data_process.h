#ifndef	_SERVER_DATA_PROCESS_
#define _SERVER_DATA_PROCESS_

#include <ESPAsyncWebServer.h>

#define DATA_GET_HANDLE_NUM     11
#define DATA_POST_HANDLE_NUM    7

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
void device_address_get(AsyncWebServerRequest *request);
void time_setting_get(AsyncWebServerRequest *request);
void fw_version_get(AsyncWebServerRequest *request);
void restart_device_get(AsyncWebServerRequest *request);
void heap_temperature_get(AsyncWebServerRequest *request);
void activated_get(AsyncWebServerRequest *request);
void format_sd_card_get(AsyncWebServerRequest *request);

void sta_ap_info_post(AsyncWebServerRequest *request);
void sta_network_post(AsyncWebServerRequest *request);
void sta_setting_post(AsyncWebServerRequest *request);
void ap_setting_post(AsyncWebServerRequest *request);
void device_address_post(AsyncWebServerRequest *request);
void auth_access_post(AsyncWebServerRequest *request);
void time_setting_post(AsyncWebServerRequest *request);

/* /get?param_wifi=[param] */
server_get_handle_t client_get_handle[DATA_GET_HANDLE_NUM] = {
    {(char*)"sta_ap_info", sta_ap_info_get},
    {(char*)"sta_network", sta_network_get},
    {(char*)"sta_setting", sta_setting_get},
    {(char*)"ap_setting", ap_setting_get},
    {(char*)"device_address", device_address_get},
    {(char*)"time_setting", time_setting_get},
    {(char*)"fw_version", fw_version_get},
    {(char*)"restart", restart_device_get},
    {(char*)"heap_temperature", heap_temperature_get},
    {(char*)"activated", activated_get},
    {(char*)"format_sd_card", format_sd_card_get}
};

server_post_handle_t client_post_handle[DATA_POST_HANDLE_NUM] = {
    {(char*)"sta_ap_info", sta_ap_info_post},
    {(char*)"sta_network", sta_network_post},
    {(char*)"sta_setting", sta_setting_post},
    {(char*)"ap_setting", ap_setting_post},
    {(char*)"device_address", device_address_post},
    {(char*)"auth_access", auth_access_post},
    {(char*)"time_setting", time_setting_post}
};

#endif