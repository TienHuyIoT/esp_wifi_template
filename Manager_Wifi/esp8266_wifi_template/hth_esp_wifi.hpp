#ifndef _HTH_ESP_WIFI_H
#define _HTH_ESP_WIFI_H

#include <Arduino.h>
#include "hth_esp_sntp.hpp"

class hth_esp_wifi
{
private:
    hth_esp_sntp* _sntp;
    int getRSSIasQuality(int RSSI);
public:
    hth_esp_wifi(/* args */);
    ~hth_esp_wifi();
    void begin();
    void end();
    void connect(const char* name, const char* pass);
    int ssidScan(String &json);
};

extern hth_esp_wifi HTH_EspWifi;

#endif // _HTH_ESP_WIFI_H
