#ifndef	_SERVER_DATA_PROCESS_
#define _SERVER_DATA_PROCESS_

#include <ESPAsyncWebServer.h>
#include "hth_webserver.h"

class WebserverURLHandle : public WebserverURLHandleCallbacks {
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
    WebserverURLHandle(/* args */);
    ~WebserverURLHandle();
};

typedef struct {
    char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, WebserverURLHandle* client);
} server_post_handle_t;

typedef struct {
    char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, WebserverURLHandle* client);
} server_get_handle_t;

#endif