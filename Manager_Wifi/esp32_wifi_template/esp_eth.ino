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
  
  ESP_ETH_PRINTF("\r\nETH Start\r\n");
  
  g_wifi_cfg = wifi_info_get();
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
  if (!g_wifi_cfg->sta.dhcp)
  {
    /* Config must be after begin function */
    ETH.config(g_wifi_cfg->sta.ip, g_wifi_cfg->sta.gw, g_wifi_cfg->sta.sn, g_wifi_cfg->sta.dns);
    ESP_ETH_PRINTF("\r\nstatic IP enable");
    ESP_ETH_PRINTF("\r\nIp: %s", g_wifi_cfg->sta.ip.toString().c_str());
    ESP_ETH_PRINTF("\r\nGw: %s", g_wifi_cfg->sta.gw.toString().c_str());
    ESP_ETH_PRINTF("\r\nSn: %s", g_wifi_cfg->sta.sn.toString().c_str());
    ESP_ETH_PRINTF("\r\nDns: %s\r\n", g_wifi_cfg->sta.dns.toString().c_str());
  }
  return true;
}

uint8_t eth_enable(void)
{
  ESP_ETH_PRINTF("\r\nETH Enable\r\n");
  eth_status = 1;
}

uint8_t eth_disable(void)
{
  ESP_ETH_PRINTF("\r\nETH Disable\r\n");
  eth_status = 0;
}

uint8_t eth_is_enable(void)
{
  ESP_ETH_PRINTF("\r\neth_status %u", eth_status);
  return eth_status;
}

#endif