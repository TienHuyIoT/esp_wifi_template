#ifndef	_ESP_REBOOT_H_
#define _ESP_REBOOT_H_

#include <Arduino.h>
#include <Ticker.h>

class ESPSoftReset
{
private:
    Ticker _reset;
public:
    ESPSoftReset(/* args */) {}
    ~ESPSoftReset() {}
    void enable(uint32_t timeout);
};

extern ESPSoftReset HTH_softReset;

#endif // _ESP_REBOOT_H_
