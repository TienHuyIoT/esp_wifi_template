#ifndef	__ESP_SOFT_RESET_H
#define __ESP_SOFT_RESET_H

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

extern ESPSoftReset SOFTReset;

#endif // __ESP_SOFT_RESET_H
