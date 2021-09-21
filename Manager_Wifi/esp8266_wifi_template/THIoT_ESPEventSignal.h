#ifndef __ESP_EVENT_SIGNAL_H
#define __ESP_EVENT_SIGNAL_H

#include "THIoT_ESPConfig.h"
#ifdef ESP32
#include "esp_event_legacy.h"
#if ESP_IDF_VERSION_MAJOR >= 4
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
#elif defined(ESP8266)
#endif

#endif // __ESP_EVENT_SIGNAL_H