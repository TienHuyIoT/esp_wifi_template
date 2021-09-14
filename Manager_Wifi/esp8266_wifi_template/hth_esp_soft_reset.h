#ifndef	_ESP_REBOOT_H_
#define _ESP_REBOOT_H_

#include <Arduino.h>
#include <Ticker.h>

class hth_esp_soft_reset
{
private:
    Ticker _reset;
public:
    hth_esp_soft_reset(/* args */) {}
    ~hth_esp_soft_reset() {}
    void enable(uint32_t timeout);
};

extern hth_esp_soft_reset HTH_softReset;

#endif // _ESP_REBOOT_H_
