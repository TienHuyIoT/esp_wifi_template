#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <esp_ota_ops.h>
#include "THIoT_APPConfig.h"
#include "THIoT_PFSerialTrace.h"

#if (CONSOLE_TRACE_TASK_ENABLE == 1)
#define OTA_MANAGER_CONSOLE_I(...)      SERIAL_CONSOLE_I("[OTA_MANAGER]", __VA_ARGS__)
#define OTA_MANAGER_CONSOLE_E(...)      SERIAL_CONSOLE_E("[OTA_MANAGER]", __VA_ARGS__)
#else
#define OTA_MANAGER_CONSOLE_I(...)
#define OTA_MANAGER_CONSOLE_E(...)
#endif

#define NO_INIT_OTA_VALIDATION_POWER_ON_VALUE 0xDEADBEEF
#define NO_INIT_OTA_VALIDATION_REBOOT_VALUE 0xBEEFDEAD

extern RTC_NOINIT_ATTR uint32_t noInitOTAValidation;
#define OTA_MANAGER_CONFIRMATION_TIMEOUT_MS 120000U // 120 seconds
void esp_ota_power_on_reset_handler();
void esp_ota_confirm_image_rollback();
bool esp_ota_app_is_valid();
void esp_ota_rollback_request();
void esp_ota_confirm_image_valid(void *arg = (void *)"Calling");
void esp_ota_start_timer_confirm_image_validation(uint32_t timeout_ms = OTA_MANAGER_CONFIRMATION_TIMEOUT_MS);

class THIoT_ESPOTAManager {
private:
  const esp_partition_t* running_partition;
  esp_ota_img_states_t current_state;
  
public:
  THIoT_ESPOTAManager();
  
  bool initialize();
  bool updateCurrentState();
  esp_ota_img_states_t getCurrentState();
  const char* getStateString();
  
  bool isPendingVerify();
  bool isUndetermined();
  bool isValid();
  bool isInvalid();
  
  bool markAppValidAndCancelRollback();
  bool markAppInvalid();
  
  void printStatus();
  void printRecord();
  bool handlePendingVerify();
};

#endif // OTA_MANAGER_H
