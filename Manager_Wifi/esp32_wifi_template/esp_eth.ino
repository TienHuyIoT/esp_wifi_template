#include "app_config.h"

#if (defined ETH_ENABLE) && (ETH_ENABLE == 1)

#include "wifi_data_file.h"
#include "lan8720a_cfg.h"

#define ESP_ETH_PORT Serial
#define ESP_ETH_PRINTF(f_, ...) ESP_ETH_PORT.printf_P(PSTR(f_), ##__VA_ARGS__)

static uint8_t eth_status = 0;

uint8_t eth_init(void)
{
  wifi_file_json_t *g_wifi_cfg;

#if (defined ETH_GPIO_ENABLE) && (ETH_GPIO_ENABLE != -1)
  ETH_GPIO_ENABLE_INIT();
  delayMicroseconds(20); /* Add delay to stable input logic before read input status */
  if (ETH_ENABLE_STATUS())
  {
    ESP_ETH_PRINTF("\r\nETH disable");
    return false;
  }  
#endif  
  
  ESP_ETH_PRINTF("\r\nETH Start");
  
  g_wifi_cfg = wifi_info_get();
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
  if (!g_wifi_cfg->sta.Dhcp)
  {
    /* Config must be after begin function */
    ETH.config(g_wifi_cfg->sta.Ip, g_wifi_cfg->sta.Gw, g_wifi_cfg->sta.Sn, g_wifi_cfg->sta.Dns);
    ESP_ETH_PRINTF("\r\nstatic IP enable");
    ESP_ETH_PRINTF("\r\nIp: %s", g_wifi_cfg->sta.Ip.toString().c_str());
    ESP_ETH_PRINTF("\r\nGw: %s", g_wifi_cfg->sta.Gw.toString().c_str());
    ESP_ETH_PRINTF("\r\nSn: %s", g_wifi_cfg->sta.Sn.toString().c_str());
    ESP_ETH_PRINTF("\r\nDns: %s\r\n", g_wifi_cfg->sta.Dns.toString().c_str());
  }
  eth_status = 1;
  return true;
}

uint8_t eth_is_enable(void)
{
  return eth_status;
}

#endif