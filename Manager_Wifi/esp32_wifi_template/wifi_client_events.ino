/*
* WiFi Events

0  SYSTEM_EVENT_WIFI_READY               < ESP32 WiFi ready
1  SYSTEM_EVENT_SCAN_DONE                < ESP32 finish scanning AP
2  SYSTEM_EVENT_STA_START                < ESP32 station start
3  SYSTEM_EVENT_STA_STOP                 < ESP32 station stop
4  SYSTEM_EVENT_STA_CONNECTED            < ESP32 station connected to AP
5  SYSTEM_EVENT_STA_DISCONNECTED         < ESP32 station disconnected from AP
6  SYSTEM_EVENT_STA_AUTHMODE_CHANGE      < the auth mode of AP connected by ESP32 station changed
7  SYSTEM_EVENT_STA_GOT_IP               < ESP32 station got IP from connected AP
8  SYSTEM_EVENT_STA_LOST_IP              < ESP32 station lost IP and the IP is reset to 0
9  SYSTEM_EVENT_STA_WPS_ER_SUCCESS       < ESP32 station wps succeeds in enrollee mode
10 SYSTEM_EVENT_STA_WPS_ER_FAILED        < ESP32 station wps fails in enrollee mode
11 SYSTEM_EVENT_STA_WPS_ER_TIMEOUT       < ESP32 station wps timeout in enrollee mode
12 SYSTEM_EVENT_STA_WPS_ER_PIN           < ESP32 station wps pin code in enrollee mode
13 SYSTEM_EVENT_AP_START                 < ESP32 soft-AP start
14 SYSTEM_EVENT_AP_STOP                  < ESP32 soft-AP stop
15 SYSTEM_EVENT_AP_STACONNECTED          < a station connected to ESP32 soft-AP
16 SYSTEM_EVENT_AP_STADISCONNECTED       < a station disconnected from ESP32 soft-AP
17 SYSTEM_EVENT_AP_STAIPASSIGNED         < ESP32 soft-AP assign an IP to a connected station
18 SYSTEM_EVENT_AP_PROBEREQRECVED        < Receive probe request packet in soft-AP interface
19 SYSTEM_EVENT_GOT_IP6                  < ESP32 station or ap or ethernet interface v6IP addr is preferred
20 SYSTEM_EVENT_ETH_START                < ESP32 ethernet start
21 SYSTEM_EVENT_ETH_STOP                 < ESP32 ethernet stop
22 SYSTEM_EVENT_ETH_CONNECTED            < ESP32 ethernet phy link up
23 SYSTEM_EVENT_ETH_DISCONNECTED         < ESP32 ethernet phy link down
24 SYSTEM_EVENT_ETH_GOT_IP               < ESP32 ethernet got IP from connected AP
25 SYSTEM_EVENT_MAX
*/

#include "wifi_data_file.h"

#define WIFI_EVENT_PORT Serial
#define WIFI_EVENT_PRINTF(f_, ...) WIFI_EVENT_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

void WiFiEvent(WiFiEvent_t event)
{
    WIFI_EVENT_PORT.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
        case SYSTEM_EVENT_WIFI_READY: 
            WIFI_EVENT_PORT.println("WiFi interface ready");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            WIFI_EVENT_PORT.println("Completed scan for access points");
            break;
        case SYSTEM_EVENT_STA_START:
            WIFI_EVENT_PORT.println("WiFi client started");
            break;
        case SYSTEM_EVENT_STA_STOP:
            WIFI_EVENT_PORT.println("WiFi clients stopped");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            WIFI_EVENT_PORT.println("Connected to access point");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            WIFI_EVENT_PORT.println("Disconnected from WiFi access point");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            WIFI_EVENT_PORT.println("Authentication mode of access point has changed");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            WIFI_EVENT_PORT.print("Obtained IP address: ");
            WIFI_EVENT_PORT.println(WiFi.localIP());
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            WIFI_EVENT_PORT.println("Lost IP address and IP address is reset to 0");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            WIFI_EVENT_PORT.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            WIFI_EVENT_PORT.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            WIFI_EVENT_PORT.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            WIFI_EVENT_PORT.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case SYSTEM_EVENT_AP_START:
            WIFI_EVENT_PORT.println("WiFi access point started");
            break;
        case SYSTEM_EVENT_AP_STOP:
            WIFI_EVENT_PORT.println("WiFi access point  stopped");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            WIFI_EVENT_PORT.println("Client connected");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            WIFI_EVENT_PORT.println("Client disconnected");
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            WIFI_EVENT_PORT.println("Assigned IP address to client");
            break;
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            WIFI_EVENT_PORT.println("Received probe request");
            break;
        case SYSTEM_EVENT_GOT_IP6:
            WIFI_EVENT_PORT.println("IPv6 is preferred");
            break;
        case SYSTEM_EVENT_ETH_START:
            WIFI_EVENT_PORT.println("Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            WIFI_EVENT_PORT.println("Ethernet stopped");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            WIFI_EVENT_PORT.println("Ethernet connected");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            WIFI_EVENT_PORT.println("Ethernet disconnected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            WIFI_EVENT_PORT.println("Obtained IP address");
            break;
        default: break;
    }}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    wifi_file_json_t *g_wifi_cfg;
    g_wifi_cfg = wifi_info_get();
    
    WIFI_EVENT_PORT.println("WiFi connected");
    WIFI_EVENT_PORT.println("IP address: ");
    WIFI_EVENT_PORT.println(IPAddress(info.got_ip.ip_info.ip.addr));
    sntp_setup();   // sntp sync time setup

    /* Smart config enable */
    if(g_wifi_cfg->sta.SmCfg)
    {
        if ( strcmp(WiFi.SSID().c_str(), g_wifi_cfg->sta.ssid) 
        || strcmp(WiFi.psk().c_str(), g_wifi_cfg->sta.pass))
        {
            WiFi.SSID().toCharArray(g_wifi_cfg->sta.ssid, Df_LengSsid + 1);
            WiFi.psk().toCharArray(g_wifi_cfg->sta.pass, Df_LengPass + 1);
            
            WIFI_EVENT_PRINTF("Update ssid and pass\r\n");
            wifi_info_write(g_wifi_cfg);
        }
    }    
}

void wifi_events_setup()
{
    // Examples of different ways to register wifi events
    WiFi.onEvent(WiFiEvent);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        WIFI_EVENT_PORT.print("WiFi lost connection. Reason: ");
        WIFI_EVENT_PORT.println(info.disconnected.reason);
        WiFi.reconnect();
    }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
}

void wifi_setup(const char* name, const char* pass)
{
    // We start by connecting to a WiFi network
    WIFI_EVENT_PORT.println();
    WIFI_EVENT_PORT.println();
    WIFI_EVENT_PORT.print("Connecting to ");
    WIFI_EVENT_PORT.println(name); 
    
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
    

    WIFI_EVENT_PORT.println();
    WIFI_EVENT_PORT.println();
    WIFI_EVENT_PORT.println("Wait for WiFi... ");
}
