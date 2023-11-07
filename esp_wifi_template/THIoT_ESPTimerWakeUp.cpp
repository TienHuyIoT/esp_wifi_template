#include "THIoT_ESPTimerWakeUp.h"
#include "THIoT_ESPLogTrace.h"
#include "THIoT_PFSerialTrace.h"

#define TIMER_WAKEUP_PORT SERIAL_PORT
#define TIMER_WAKEUP_PRINTF(...) SERIAL_TAG_LOGI("[TIMER_WAKEUP] ", __VA_ARGS__)


#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
String esp_wakeup_reason_str()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  const __FlashStringHelper* buff;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : buff = F("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : buff = F("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : buff = F("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : buff = F("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : buff = F("Wakeup caused by ULP program"); break;
    default : buff = F("Wakeup was not caused by deep sleep"); break;
  }

  TIMER_WAKEUP_PRINTF("%s", buff);
  return String(buff);
}

/* Time ESP32 will go to sleep (in seconds) */
void timer_wakeup_second_enable(uint32_t sec)
{
/*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(sec * uS_TO_S_FACTOR);
  TIMER_WAKEUP_PRINTF("Setup ESP32 to sleep for every %u Seconds", sec);

  /*
  Next we decide what all peripherals to shut down/keep on
  By default, ESP32 will automatically power down the peripherals
  not needed by the wakeup source, but if you want to be a poweruser
  this is for you. Read in detail at the API docs
  http://esp-idf.readthedocs.io/en/latest/api-reference/system/deep_sleep.html
  Left the line commented as an example of how to configure peripherals.
  The line below turns off all RTC peripherals in deep sleep.
  */
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //Serial.println("Configured all RTC Peripherals to be powered down in sleep");

  /*
  Now that we have setup a wake cause and if needed setup the
  peripherals state in deep sleep, we can now start going to
  deep sleep.
  In the case that no wake up sources were provided but deep
  sleep was started, it will sleep forever unless hardware
  reset occurs.
  */
  TIMER_WAKEUP_PORT.flush(); 
  esp_deep_sleep_start();
}