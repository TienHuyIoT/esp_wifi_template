#ifndef	_SERVER_DATA_PROCESS_
#define _SERVER_DATA_PROCESS_

#define DATA_GET_HANDLE_NUM     11
#define DATA_POST_HANDLE_NUM    7

typedef struct {
    char *path_arg;
    void (*cb)(void);
} server_post_handle_t;

typedef struct {
    char *path_arg;
    void (*cb)(void);
} server_get_handle_t;

void sta_ap_info_get(void);
void sta_network_get(void);
void sta_setting_get(void);
void ap_setting_get(void);
void device_address_get(void);
void time_setting_get(void);
void fw_version_get(void);
void restart_device_get(void);
void heap_temperature_get(void);
void activated_get(void);
void format_sd_card_get(void);

void sta_ap_info_post(void);
void sta_network_post(void);
void sta_setting_post(void);
void ap_setting_post(void);
void device_address_post(void);
void auth_access_post(void);
void time_setting_post(void);

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