#ifndef	__APP_CONFIG_H
#define __APP_CONFIG_H

/** @brief configure kind of time zone
 * 0: TzTime type
 * 1: gtmOffset (recommend for accuracy)
*/
#define TIME_ZONE_TYPE_CFG              1

/** @brief Mutex memory PFTicker
*/
#define MUTEX_PLATFORM_TICKER           1

/**
 * @brief reset esp after timeout if network disconnect
*/
#define NETWORK_CONNECTION_TIMEOUT_RESET 1

#endif // __APP_CONFIG_H
