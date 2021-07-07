#include "app_config.h"
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
#include <ETH.h>
#include "lan8720a_cfg.h"
#endif
#include <ESPmDNS.h>

#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
#include <NetBIOS.h>
#endif

#include "board.h"
#include "wifi_data_file.h"

#define WIFI_EVENT_PORT Serial
#define WIFI_EVENT_TAG_PRINTF(fmt, ...) WIFI_EVENT_PORT.printf_P(PSTR("\r\n[WIFI EV] " fmt), ##__VA_ARGS__)
#define WIFI_EVENT_PRINTF(f_, ...) WIFI_EVENT_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

#define LED_WIFI_CONNECT_TIMER    500
#define LED_ETH_CONNECT_TIMER     100
#define LED_DISCONNECT_TIMER      1000

static led_callback_t _led_update = NULL;

#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
#include "esp_event_legacy.h"
#define m_ESP32_EVENT_WIFI_READY               ARDUINO_EVENT_WIFI_READY
#define m_ESP32_EVENT_SCAN_DONE                ARDUINO_EVENT_WIFI_SCAN_DONE
#define m_ESP32_EVENT_STA_START                ARDUINO_EVENT_WIFI_STA_START
#define m_ESP32_EVENT_STA_STOP                 ARDUINO_EVENT_WIFI_STA_STOP
#define m_ESP32_EVENT_STA_CONNECTED            ARDUINO_EVENT_WIFI_STA_CONNECTED
#define m_ESP32_EVENT_STA_DISCONNECTED         ARDUINO_EVENT_WIFI_STA_DISCONNECTED
#define m_ESP32_EVENT_STA_AUTHMODE_CHANGE      ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE
#define m_ESP32_EVENT_STA_GOT_IP               ARDUINO_EVENT_WIFI_STA_GOT_IP
#define m_ESP32_EVENT_STA_LOST_IP              ARDUINO_EVENT_WIFI_STA_LOST_IP
#define m_ESP32_EVENT_STA_BSS_RSSI_LOW         SYSTEM_EVENT_STA_BSS_RSSI_LOW
#define m_ESP32_EVENT_STA_WPS_ER_SUCCESS       ARDUINO_EVENT_WPS_ER_SUCCESS
#define m_ESP32_EVENT_STA_WPS_ER_FAILED        ARDUINO_EVENT_WPS_ER_FAILED
#define m_ESP32_EVENT_STA_WPS_ER_TIMEOUT       ARDUINO_EVENT_WPS_ER_TIMEOUT
#define m_ESP32_EVENT_STA_WPS_ER_PIN           ARDUINO_EVENT_WPS_ER_PIN
#define m_ESP32_EVENT_STA_WPS_ER_PBC_OVERLAP   ARDUINO_EVENT_WPS_ER_PBC_OVERLAP
#define m_ESP32_EVENT_AP_START                 ARDUINO_EVENT_WIFI_AP_START
#define m_ESP32_EVENT_AP_STOP                  ARDUINO_EVENT_WIFI_AP_STOP
#define m_ESP32_EVENT_AP_STACONNECTED          ARDUINO_EVENT_WIFI_AP_STACONNECTED
#define m_ESP32_EVENT_AP_STADISCONNECTED       ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
#define m_ESP32_EVENT_AP_STAIPASSIGNED         ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED
#define m_ESP32_EVENT_AP_PROBEREQRECVED        ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED
#define m_ESP32_EVENT_ACTION_TX_STATUS         SYSTEM_EVENT_ACTION_TX_STATUS
#define m_ESP32_EVENT_ROC_DONE                 SYSTEM_EVENT_ROC_DONE
#define m_ESP32_EVENT_STA_BEACON_TIMEOUT       SYSTEM_EVENT_STA_BEACON_TIMEOUT
#define m_ESP32_EVENT_FTM_REPORT               SYSTEM_EVENT_FTM_REPORT
#define m_ESP32_EVENT_STA_GOT_IP6              ARDUINO_EVENT_WIFI_STA_GOT_IP6
#define m_ESP32_EVENT_AP_GOT_IP6               ARDUINO_EVENT_WIFI_AP_GOT_IP6
#define m_ESP32_EVENT_ETH_START                ARDUINO_EVENT_ETH_START
#define m_ESP32_EVENT_ETH_STOP                 ARDUINO_EVENT_ETH_STOP
#define m_ESP32_EVENT_ETH_CONNECTED            ARDUINO_EVENT_ETH_CONNECTED
#define m_ESP32_EVENT_ETH_DISCONNECTED         ARDUINO_EVENT_ETH_DISCONNECTED
#define m_ESP32_EVENT_ETH_GOT_IP               ARDUINO_EVENT_ETH_GOT_IP
#define m_ESP32_EVENT_ETH_LOST_IP              SYSTEM_EVENT_ETH_LOST_IP
#define m_ESP32_EVENT_SC_SEND_ACK_DONE         ARDUINO_EVENT_SC_SEND_ACK_DONE
#define m_ESP32_EVENT_PROV_INIT                ARDUINO_EVENT_PROV_INIT
#define m_ESP32_EVENT_PROV_DEINIT              ARDUINO_EVENT_PROV_DEINIT
#define m_ESP32_EVENT_PROV_START               ARDUINO_EVENT_PROV_START
#define m_ESP32_EVENT_PROV_END                 ARDUINO_EVENT_PROV_END
#define m_ESP32_EVENT_PROV_CRED_RECV           ARDUINO_EVENT_PROV_CRED_RECV
#define m_ESP32_EVENT_PROV_CRED_FAIL           ARDUINO_EVENT_PROV_CRED_FAIL
#define m_ESP32_EVENT_PROV_CRED_SUCCESS        ARDUINO_EVENT_PROV_CRED_SUCCESS
#define m_ESP32_EVENT_MAX                      ARDUINO_EVENT_MAX
#else
#define m_ESP32_EVENT_WIFI_READY               SYSTEM_EVENT_WIFI_READY               /*!< ESP32 WiFi ready */
#define m_ESP32_EVENT_SCAN_DONE                SYSTEM_EVENT_SCAN_DONE                /*!< ESP32 finish scanning AP */
#define m_ESP32_EVENT_STA_START                SYSTEM_EVENT_STA_START                /*!< ESP32 station start */
#define m_ESP32_EVENT_STA_STOP                 SYSTEM_EVENT_STA_STOP                 /*!< ESP32 station stop */
#define m_ESP32_EVENT_STA_CONNECTED            SYSTEM_EVENT_STA_CONNECTED            /*!< ESP32 station connected to AP */
#define m_ESP32_EVENT_STA_DISCONNECTED         SYSTEM_EVENT_STA_DISCONNECTED         /*!< ESP32 station disconnected from AP */
#define m_ESP32_EVENT_STA_AUTHMODE_CHANGE      SYSTEM_EVENT_STA_AUTHMODE_CHANGE      /*!< the auth mode of AP connected by ESP32 station changed */
#define m_ESP32_EVENT_STA_GOT_IP               SYSTEM_EVENT_STA_GOT_IP               /*!< ESP32 station got IP from connected AP */
#define m_ESP32_EVENT_STA_LOST_IP              SYSTEM_EVENT_STA_LOST_IP              /*!< ESP32 station lost IP and the IP is reset to 0 */
#define m_ESP32_EVENT_STA_BSS_RSSI_LOW         SYSTEM_EVENT_STA_BSS_RSSI_LOW         /*!< ESP32 station connected BSS rssi goes below threshold */
#define m_ESP32_EVENT_STA_WPS_ER_SUCCESS       SYSTEM_EVENT_STA_WPS_ER_SUCCESS       /*!< ESP32 station wps succeeds in enrollee mode */
#define m_ESP32_EVENT_STA_WPS_ER_FAILED        SYSTEM_EVENT_STA_WPS_ER_FAILED        /*!< ESP32 station wps fails in enrollee mode */
#define m_ESP32_EVENT_STA_WPS_ER_TIMEOUT       SYSTEM_EVENT_STA_WPS_ER_TIMEOUT       /*!< ESP32 station wps timeout in enrollee mode */
#define m_ESP32_EVENT_STA_WPS_ER_PIN           SYSTEM_EVENT_STA_WPS_ER_PIN           /*!< ESP32 station wps pin code in enrollee mode */
#define m_ESP32_EVENT_STA_WPS_ER_PBC_OVERLAP   SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP   /*!< ESP32 station wps overlap in enrollee mode */
#define m_ESP32_EVENT_AP_START                 SYSTEM_EVENT_AP_START                 /*!< ESP32 soft-AP start */
#define m_ESP32_EVENT_AP_STOP                  SYSTEM_EVENT_AP_STOP                  /*!< ESP32 soft-AP stop */
#define m_ESP32_EVENT_AP_STACONNECTED          SYSTEM_EVENT_AP_STACONNECTED          /*!< a station connected to ESP32 soft-AP */
#define m_ESP32_EVENT_AP_STADISCONNECTED       SYSTEM_EVENT_AP_STADISCONNECTED       /*!< a station disconnected from ESP32 soft-AP */
#define m_ESP32_EVENT_AP_STAIPASSIGNED         SYSTEM_EVENT_AP_STAIPASSIGNED         /*!< ESP32 soft-AP assign an IP to a connected station */
#define m_ESP32_EVENT_AP_PROBEREQRECVED        SYSTEM_EVENT_AP_PROBEREQRECVED        /*!< Receive probe request packet in soft-AP interface */
#define m_ESP32_EVENT_ACTION_TX_STATUS         SYSTEM_EVENT_ACTION_TX_STATUS         /*!< Receive status of Action frame transmitted */
#define m_ESP32_EVENT_ROC_DONE                 SYSTEM_EVENT_ROC_DONE                 /*!< Indicates the completion of Remain-on-Channel operation status */
#define m_ESP32_EVENT_STA_BEACON_TIMEOUT       SYSTEM_EVENT_STA_BEACON_TIMEOUT       /*!< ESP32 station beacon timeout */
#define m_ESP32_EVENT_FTM_REPORT               SYSTEM_EVENT_FTM_REPORT               /*!< Receive report of FTM procedure */
#define m_ESP32_EVENT_STA_GOT_IP6              SYSTEM_EVENT_GOT_IP6                  /*!< ESP32 station or ap or ethernet interface v6IP addr is preferred */
#define m_ESP32_EVENT_AP_GOT_IP6               SYSTEM_EVENT_GOT_IP6
#define m_ESP32_EVENT_ETH_START                SYSTEM_EVENT_ETH_START                /*!< ESP32 ethernet start */
#define m_ESP32_EVENT_ETH_STOP                 SYSTEM_EVENT_ETH_STOP                 /*!< ESP32 ethernet stop */
#define m_ESP32_EVENT_ETH_CONNECTED            SYSTEM_EVENT_ETH_CONNECTED            /*!< ESP32 ethernet phy link up */
#define m_ESP32_EVENT_ETH_DISCONNECTED         SYSTEM_EVENT_ETH_DISCONNECTED         /*!< ESP32 ethernet phy link down */
#define m_ESP32_EVENT_ETH_GOT_IP               SYSTEM_EVENT_ETH_GOT_IP               /*!< ESP32 ethernet got IP from connected AP */
#define m_ESP32_EVENT_ETH_LOST_IP              SYSTEM_EVENT_ETH_LOST_IP              /*!< ESP32 ethernet lost IP and the IP is reset to 0 */
#define m_ESP32_EVENT_MAX                      SYSTEM_EVENT_MAX                       /*!< Number of members in this enum */
#endif

void WiFiEvent(WiFiEvent_t event)
{
    WIFI_EVENT_TAG_PRINTF("[WiFi-event] event: %d", event);

    switch (event) {
        case m_ESP32_EVENT_WIFI_READY: 
            WIFI_EVENT_TAG_PRINTF("WiFi interface ready");
            break;
        case m_ESP32_EVENT_SCAN_DONE:
            WIFI_EVENT_TAG_PRINTF("Completed scan for access points");
            sta_wifi_network_scan_send_event();
            break;
        case m_ESP32_EVENT_STA_START:
            WIFI_EVENT_TAG_PRINTF("WiFi client started");
            break;
        case m_ESP32_EVENT_STA_STOP:
            WIFI_EVENT_TAG_PRINTF("WiFi clients stopped");
            break;
        case m_ESP32_EVENT_STA_CONNECTED:
            WIFI_EVENT_TAG_PRINTF("Connected to access point");
            break;
        case m_ESP32_EVENT_STA_DISCONNECTED:
            WIFI_EVENT_TAG_PRINTF("Disconnected from WiFi access point");
            break;
        case m_ESP32_EVENT_STA_AUTHMODE_CHANGE:
            WIFI_EVENT_TAG_PRINTF("Authentication mode of access point has changed");
            break;
        case m_ESP32_EVENT_STA_GOT_IP:
            WIFI_EVENT_TAG_PRINTF("Obtained IP address: %s", WiFi.localIP().toString().c_str());
            break;
        case m_ESP32_EVENT_STA_LOST_IP:
            WIFI_EVENT_TAG_PRINTF("Lost IP address and IP address is reset to 0");
            break;
        case m_ESP32_EVENT_STA_WPS_ER_SUCCESS:
            WIFI_EVENT_TAG_PRINTF("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case m_ESP32_EVENT_STA_WPS_ER_FAILED:
            WIFI_EVENT_TAG_PRINTF("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case m_ESP32_EVENT_STA_WPS_ER_TIMEOUT:
            WIFI_EVENT_TAG_PRINTF("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case m_ESP32_EVENT_STA_WPS_ER_PIN:
            WIFI_EVENT_TAG_PRINTF("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case m_ESP32_EVENT_AP_START:
            WIFI_EVENT_TAG_PRINTF("WiFi access point started");
            break;
        case m_ESP32_EVENT_AP_STOP:
            WIFI_EVENT_TAG_PRINTF("WiFi access point  stopped");
            break;
        case m_ESP32_EVENT_AP_STACONNECTED:
            WIFI_EVENT_TAG_PRINTF("Client connected");
            break;
        case m_ESP32_EVENT_AP_STADISCONNECTED:
            WIFI_EVENT_TAG_PRINTF("Client disconnected");
            break;
        case m_ESP32_EVENT_AP_STAIPASSIGNED:
            WIFI_EVENT_TAG_PRINTF("Assigned IP address to client");
            break;
        case m_ESP32_EVENT_AP_PROBEREQRECVED:
            WIFI_EVENT_TAG_PRINTF("Received probe request");
            break;
        case m_ESP32_EVENT_STA_GOT_IP6:
        case m_ESP32_EVENT_AP_GOT_IP6:
            WIFI_EVENT_TAG_PRINTF("IPv6 is preferred");
            break;
        case m_ESP32_EVENT_ETH_START:
        {
            //set eth hostname here
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
            wifi_file_json_t *g_wifi_cfg;
            g_wifi_cfg = wifi_info_get();     
            ETH.setHostname(g_wifi_cfg->sta.hostname);
#endif
            WIFI_EVENT_TAG_PRINTF("Ethernet started");
            break;
        }
        case m_ESP32_EVENT_ETH_STOP:
            WIFI_EVENT_TAG_PRINTF("Ethernet stopped");
            break;
        case m_ESP32_EVENT_ETH_CONNECTED:
            WIFI_EVENT_TAG_PRINTF("Ethernet connected");
            break;
        case m_ESP32_EVENT_ETH_DISCONNECTED:
            WIFI_EVENT_TAG_PRINTF("Ethernet disconnected");
            if(_led_update)
            {
                _led_update(LED_DISCONNECT_TIMER);
            }
            break;
        case m_ESP32_EVENT_ETH_GOT_IP:
            WIFI_EVENT_TAG_PRINTF("Obtained IP address");
            break;
        default: break;
    }}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();

    if(_led_update)
    {
        _led_update(LED_WIFI_CONNECT_TIMER);
    }
        
    sntp_setup();   // sntp sync time setup

    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp32.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 0)
    if (!MDNS.begin(g_wifi_cfg->sta.hostname)) 
    {
        WIFI_EVENT_TAG_PRINTF("Error setting up MDNS responder!");
    }
    else
    {
        WIFI_EVENT_TAG_PRINTF("mDNS responder started");
    }
#endif

    MDNS.addService("http","tcp",80); /* alway 80, because we have redirect port 80 to server port */

#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
    NBNS.begin(g_wifi_cfg->sta.hostname);
#endif

    WIFI_EVENT_TAG_PRINTF("WiFi connected");
    WIFI_EVENT_TAG_PRINTF("IP address: %s", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
    /* Smart config enable */
    if(g_wifi_cfg->sta.smart_cfg)
    {
        if ( strcmp(WiFi.SSID().c_str(), g_wifi_cfg->sta.ssid) 
        || strcmp(WiFi.psk().c_str(), g_wifi_cfg->sta.pass))
        {
            WiFi.SSID().toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid + 1);
            WiFi.psk().toCharArray(g_wifi_cfg->sta.pass, Df_LengPass + 1);
            
            WIFI_EVENT_TAG_PRINTF("Update ssid and pass");
            wifi_info_write(g_wifi_cfg);
        }
    }
}

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
void ETHGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();

    if(_led_update)
    {
        _led_update(LED_ETH_CONNECT_TIMER);
    }
        
    sntp_setup();   // sntp sync time setup

    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp32.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
#if (defined OTA_ARDUINO_ENABLE) && (OTA_ARDUINO_ENABLE == 0)
    if (!MDNS.begin(g_wifi_cfg->sta.hostname)) 
    {
        WIFI_EVENT_TAG_PRINTF("Error setting up MDNS responder!");
    }
    else
    {
        WIFI_EVENT_TAG_PRINTF("mDNS responder started");
    }    
#endif
    MDNS.addService("http","tcp",80); /* alway 80, because we have redirect port 80 to server port */

#if (defined NBNS_SERVICE_ENABLE) && (NBNS_SERVICE_ENABLE == 1)
    NBNS.begin(g_wifi_cfg->sta.hostname);
#endif

    WIFI_EVENT_TAG_PRINTF("ETH MAC: %s, IPv4: %s", ETH.macAddress().c_str(), ETH.localIP().toString().c_str());
    if (ETH.fullDuplex()) {
        WIFI_EVENT_PRINTF(", FULL_DUPLEX");
    }
    WIFI_EVENT_PRINTF(", %uMbps\r\n", ETH.linkSpeed()); 
}
#endif

void wifi_events_setup(led_callback_t cb)
{
    _led_update = cb;
    // Examples of different ways to register wifi events
    WiFi.onEvent(WiFiEvent);
#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)
    if(eth_is_enable())
    {
        ESP_ETH_PRINTF("\r\nonEvent m_ESP32_EVENT_ETH_GOT_IP\r\n");
        WiFi.onEvent(ETHGotIP, WiFiEvent_t::m_ESP32_EVENT_ETH_GOT_IP);
    }    
#endif
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::m_ESP32_EVENT_STA_GOT_IP);
    WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 4
        WIFI_EVENT_TAG_PRINTF("WiFi lost connection. Reason: %u", info.wifi_sta_disconnected.reason);
#else
        WIFI_EVENT_TAG_PRINTF("WiFi lost connection. Reason: %u", info.disconnected.reason);
#endif
        WiFi.reconnect();
        if(_led_update)
        {
            _led_update(LED_DISCONNECT_TIMER);
        }
    }, WiFiEvent_t::m_ESP32_EVENT_STA_DISCONNECTED);
}

void wifi_setup(const char* name, const char* pass)
{
    // We start by connecting to a WiFi network
    WIFI_EVENT_TAG_PRINTF("Connecting to %s", name);
    
    /* Set whether module will attempt to reconnect
     to an access point in case it is disconnected. 
    */
    WiFi.setAutoReconnect(true);
    if ( strlen(pass) >= 8)
    {
        WiFi.begin(name, pass);
    }
    else
    {
        WiFi.begin(name);
    }

    WIFI_EVENT_TAG_PRINTF("Wait for WiFi...");
}
