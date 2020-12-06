#ifndef	_SERVER_DATA_PROCESS_
#define _SERVER_DATA_PROCESS_

#define DATA_GET_HANDLE_NUM     4
#define DATA_POST_HANDLE_NUM    3

typedef struct {
    char *path_arg;
    void (*cb)(void);
} server_post_handle_t;

typedef struct {
    char *path_arg;
    void (*cb)(void);
} server_get_handle_t;

void ata_ap_info_get(void);
void sta_network_get(void);
void sta_setting_get(void);
void ap_setting_get(void);

void sta_network_post(void);
void sta_setting_post(void);
void ap_setting_post(void);

server_get_handle_t client_get_handle[DATA_GET_HANDLE_NUM] = {
    {(char*)"ata_ap_info", ata_ap_info_get},
    {(char*)"sta_network", sta_network_get},
    {(char*)"sta_setting", sta_setting_get},
    {(char*)"ap_setting", ap_setting_get}
};

server_post_handle_t client_post_handle[DATA_POST_HANDLE_NUM] = {
    {(char*)"sta_network", sta_network_post},
    {(char*)"sta_setting", sta_setting_post},
    {(char*)"ap_setting", ap_setting_post}
};

#endif