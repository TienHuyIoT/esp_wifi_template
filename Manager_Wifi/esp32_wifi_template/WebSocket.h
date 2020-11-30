#ifndef	_WEB_SOCKET_H
#define _WEB_SOCKET_H

#include <WebSocketsServer.h>
#include <TimeOutEvent.h>

#define NUM_WS_CONNECTION_MAX   (WEBSOCKETS_SERVER_CLIENT_MAX - 3)
#define WS_INTERVAL_TIMEOUT_NUM  10000 /* MS */

typedef enum {
    WS_DISCONNECT,
    WS_CONNECTED
} ws_connection_status_t;

typedef struct {
    uint32_t timestamp;
    uint8_t ws_num;
    uint8_t status;
} ws_connection_info_t;

ws_connection_info_t ws_connection[NUM_WS_CONNECTION_MAX] = {0};

/* inteval timeout check temperature */
TimeOutEvent ws_interval_broadcast(WS_INTERVAL_TIMEOUT_NUM);

void web_socket_init(void (*cb)(uint8_t, uint8_t *));
void ws_disconnect(uint8_t ws_index);
void ws_connection_establish(uint8_t ws_num);
void ws_connection_remove(uint8_t ws_num);
uint8_t ws_connection_available(void);
uint8_t ws_connection_connected(void);
uint8_t ws_connection_index_has_tl_max(void);
void ws_send_txt(uint8_t ws_index, uint8_t *payload);
void ws_send_broadcast_txt(uint8_t *payload);
uint8_t ws_connection_connected(void);
void ws_interval_sync(void);

#endif