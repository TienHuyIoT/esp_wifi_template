#ifndef	_SERVER_DATA_PROCESS_
#define _SERVER_DATA_PROCESS_

#include <ESPAsyncWebServer.h>
#include "hth_webserver.h"

class hth_httpserver_url : public serverUrlCallbacks {
private:
    /**
     * Handler called after once request with method GET and authenticated.
     */
    void onHttpGetAuth(AsyncWebServerRequest* request);
    /**
     * Handler called after once request with method GET.
     */
    void onHttpGet(AsyncWebServerRequest* request);
    /**
     * Handler called after once request with method POST and authenticated.
     */
    void onHttpPostAuth(AsyncWebServerRequest* request);
public:
    hth_httpserver_url(/* args */);
    ~hth_httpserver_url();
};

typedef struct {
    char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, hth_httpserver_url* client);
} server_post_handle_t;

typedef struct {
    char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, hth_httpserver_url* client);
} server_get_handle_t;

#endif