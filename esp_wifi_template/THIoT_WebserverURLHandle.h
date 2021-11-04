#ifndef	__WEBSERVER_URL_HANDLE_H
#define __WEBSERVER_URL_HANDLE_H

#include <ESPAsyncWebServer.h>
#include "THIoT_ESPWebserver.h"

class WebserverURLHandle : public WebserverURLHandleCallbacks {
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
    WebserverURLHandle(const char *argName);
    ~WebserverURLHandle();
};

typedef struct {
    const char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, WebserverURLHandle* client);
} server_post_handle_t;

typedef struct {
    const char *path_arg;
    void (*cb)(AsyncWebServerRequest *request, WebserverURLHandle* client);
} server_get_handle_t;

#endif // __WEBSERVER_URL_HANDLE_H
