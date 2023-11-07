#ifndef	__ESP_SOFT_RESET_H
#define __ESP_SOFT_RESET_H

#include <Arduino.h>
#include <functional>
#include "THIoT_PFTicker.h"

typedef std::function<void(uint32_t)> ResetHandler;

class ESPSoftReset
{
public:
    typedef enum {
        HOME_PAGE_TYPE = 0,
        I2C_DEVICE_ADDR_SETTING_TYPE,
        DEVICE_INFO_NAME_TYPE,
        DEVICE_AUTH_SETTING_TYPE,
        STA_INFO_SETTING_TYPE,
        AP_INFO_SETTING_TYPE,
        OTA_UPDATE_TYPE,
        HTTP_API_INFO_SETTING_TYPE,
        SCHEDULE_DAILY_TYPE,
        SCHEDULE_WEEKLY_TYPE,
        WIFI_RECONNECT_TYPE,
        ETH_DISCONNECT_TYPE
    } resetSource_t;
private:
    ticker_function_handle_t _resetTick;
    ResetHandler _resetFunc = nullptr;
    resetSource_t _reasonType;
public:
    ESPSoftReset();
    ~ESPSoftReset() {}
    boolean enable(uint32_t timeout, resetSource_t type = HOME_PAGE_TYPE);
    void onReset(ResetHandler handler) { _resetFunc = handler; }
};

extern ESPSoftReset SOFTReset;

#endif // __ESP_SOFT_RESET_H
