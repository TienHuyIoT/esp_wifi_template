#include "app_config.h"

#if (defined DDNS_CLIENT_ENABLE) && (DDNS_CLIENT_ENABLE == 1)
#include <Ticker.h>
#include "AsyncEasyDDNS.h"
#include "wifi_data_file.h"

#define DDNS_CLIENT_PORT Serial
#define DDNS_CLIENT_PRINTF(f_, ...) DDNS_CLIENT_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

Ticker ddns_ticker;

void ddns_client_init(void)
{
  wifi_file_json_t *g_wifi_cfg;

  g_wifi_cfg = wifi_info_get();

  if(g_wifi_cfg->ddns.disable)
  {
      DDNS_CLIENT_PRINTF("\r\nDDNS client disable");
  }
  
    /*
    List of supported DDNS providers:
    - "duckdns"
    - "noip"
    - "dyndns"
    - "dynu"
    - "enom"
    - "all-inkl"
    - "selfhost.de"
    - "dyndns.it"
    - "strato"
    - "freemyip"
    - "afraid.org"
  */
  AsyncEasyDDNS.service(g_wifi_cfg->ddns.service);

  /*
    For DDNS Providers where you get a token:
      Use this: AsyncEasyDDNS.client("domain", "token");
    
    For DDNS Providers where you get username and password: ( Leave the password field empty "" if not required )
      Use this: AsyncEasyDDNS.client("domain", "username", "password");
  */
  AsyncEasyDDNS.client(g_wifi_cfg->ddns.domain, g_wifi_cfg->ddns.user, g_wifi_cfg->ddns.pass);

  // Get Notified when your IP changes
  AsyncEasyDDNS.onUpdate([&](const char* oldIP, const char* newIP){
    DDNS_CLIENT_PORT.print("\r\nAsyncEasyDDNS - IP Change Detected: ");
    DDNS_CLIENT_PORT.println(newIP);
  });

  if (g_wifi_cfg->ddns.sync_time < 10)
  {
    g_wifi_cfg->ddns.sync_time = 10;
  }
  if (g_wifi_cfg->ddns.sync_time > 60)
  {
    g_wifi_cfg->ddns.sync_time = 60;
  }

  ddns_ticker.attach(g_wifi_cfg->ddns.sync_time, ddns_update); 
}

void ddns_update(void)
{
  wifi_file_json_t *g_wifi_cfg;
  uint32_t sync_time;

  g_wifi_cfg = wifi_info_get();
  if(!g_wifi_cfg->ddns.disable)
  {
    AsyncEasyDDNS.update();
  }  
}
#endif
