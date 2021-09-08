#ifndef __HTH_ESP_SNTP_H
#define __HTH_ESP_SNTP_H

#include <Arduino.h>

class hth_esp_sntp
{
private:
    static constexpr char *_ntpServer1 = "pool.ntp.org";
    static constexpr char *_ntpServer2 = "time.nist.gov";
    static constexpr long _gmtOffset_sec = 3600 * 7;
    static constexpr int _daylightOffset_sec = 0;
public:
    hth_esp_sntp(/* args */);
    ~hth_esp_sntp();
    void begin();
};

#endif // __HTH_ESP_SNTP_H