#include "THIoT_ESPResetReason.h"
#include "THIoT_ESPOTAManager.h"
#include "THIoT_ESPLogTrace.h"

#define OTA_MANAGER_TAG_LOG(...)    FS_TAG_LOGI("[OTA_MANAGER]", __VA_ARGS__)

#define OTA_ROLLBACK_REQUESTED_VALUE 2
#define NO_INIT_OTA_VALIDATION_POWER_ON_VALUE         0x0EADBEEF
#define NO_INIT_OTA_VALIDATION_ROLLBACK_SUCCESS_VALUE 0xBEEFDEAD
#define NO_INIT_OTA_VALIDATION_ROLLBACK_FAILURE_VALUE 0xCAFEBABE
#define NO_INIT_OTA_VALIDATION_CONFIRM_VALUE          0xF00DBABE
RTC_NOINIT_ATTR uint32_t noInitOTAValidation;

bool esp_ota_app_is_valid() {
  return noInitOTAValidation == NO_INIT_OTA_VALIDATION_CONFIRM_VALUE;
}

void esp_ota_power_on_reset_handler() {

  // Power on reset or OTA forced reset detected
  if (esp_reset_reason_core(PRO_CPU0_ID) == POWERON_RESET) {
    noInitOTAValidation = NO_INIT_OTA_VALIDATION_POWER_ON_VALUE;
  }

  THIoT_ESPOTAManager otaManager;
  if (otaManager.initialize())
  {
#if (FIRMWARE_OTA_ROLLBACK_ENABLE == 0)
    otaManager.handlePendingVerify();
    if (otaManager.isValid())
    {
      OTA_MANAGER_CONSOLE_I("App is valid, no action needed\r\n");
      noInitOTAValidation = NO_INIT_OTA_VALIDATION_CONFIRM_VALUE;
    }
#else
    otaManager.printStatus();
    if (otaManager.isValid())
    {
      OTA_MANAGER_CONSOLE_I("App is valid, no action needed\r\n");
      if (esp_reset_reason_core(PRO_CPU0_ID) == POWERON_RESET) {
        noInitOTAValidation = NO_INIT_OTA_VALIDATION_CONFIRM_VALUE;
      }
      return;
    }

    if (otaManager.isInvalid() && noInitOTAValidation != NO_INIT_OTA_VALIDATION_ROLLBACK_SUCCESS_VALUE)
    {
      // Check if the app is in pending verify state on next boot 
      // due to system reset (not OTA forced reset or power on reset)
      bool isRollback = ((noInitOTAValidation - NO_INIT_OTA_VALIDATION_POWER_ON_VALUE) == OTA_ROLLBACK_REQUESTED_VALUE);
      if (rtc_get_reset_reason(PRO_CPU0_ID) == POWERON_RESET) {
        OTA_MANAGER_CONSOLE_I("Power on reset detected");
        isRollback = true;
      }
      OTA_MANAGER_CONSOLE_I("App is marked as pending verify state, rollback requested: %s, noInitOTAValidation %d/%d",
                            isRollback ? "true" : "false",
                            noInitOTAValidation - NO_INIT_OTA_VALIDATION_POWER_ON_VALUE,
                            OTA_ROLLBACK_REQUESTED_VALUE);

      // is Power on reset, not OTA forced reset
      if (isRollback)
      {
        OTA_MANAGER_CONSOLE_I("App marked as invalid and will reboot");
        if (otaManager.markAppInvalid())
        {
          return;
        }
      }

      ++noInitOTAValidation;
      esp_ota_start_timer_confirm_image_validation(OTA_MANAGER_CONFIRMATION_TIMEOUT_MS);
    }
#endif // #if (FIRMWARE_OTA_ROLLBACK_ENABLE == 1)
  }
}

void esp_ota_confirm_image_rollback() {
  if (noInitOTAValidation == NO_INIT_OTA_VALIDATION_ROLLBACK_SUCCESS_VALUE) {
    noInitOTAValidation = NO_INIT_OTA_VALIDATION_CONFIRM_VALUE;
    OTA_MANAGER_TAG_LOG("Image rollback success! Continuing with confirmation.");
  }
}

void esp_ota_confirm_image_valid(void *arg) {
  if (noInitOTAValidation == NO_INIT_OTA_VALIDATION_CONFIRM_VALUE) {
    OTA_MANAGER_CONSOLE_I("Image validation already confirmed, no action needed\r\n");
    // If the validation is already confirmed, we do not need to proceed further
    return;
  }

  OTA_MANAGER_CONSOLE_I("%s: Confirming image validation...", (char *)arg);
  THIoT_ESPOTAManager otaManager;
  if (otaManager.initialize()) {
    if (otaManager.handlePendingVerify()) {
      noInitOTAValidation = NO_INIT_OTA_VALIDATION_CONFIRM_VALUE; // Set the validation value to indicate confirmation
      OTA_MANAGER_TAG_LOG("Image validation confirmed");
      otaManager.printRecord();
    }
  }
}

void esp_ota_rollback_request() {
  if (!esp_ota_check_rollback_is_possible ()) {
    OTA_MANAGER_TAG_LOG("Rollback is not possible, no valid firmware found");
    return;
  }

  OTA_MANAGER_TAG_LOG("Rollback is possible, marking app as invalid for rollback");
  THIoT_ESPOTAManager otaManager;
  if (otaManager.initialize()) {
    if (otaManager.markAppInvalid()) {
      // never returns if successful
      OTA_MANAGER_TAG_LOG("App marked as invalid and rollback requested");
    } else {
      OTA_MANAGER_TAG_LOG("Failed to mark app as invalid for rollback");
    }
  }
}

void esp_ota_start_timer_confirm_image_validation(uint32_t timeout_ms) {
    allocate_ticker_once_ms(timeout_ms, [](void *arg) {
        esp_ota_confirm_image_valid(arg);
    }, (void *)"Ticker timer fired");

    OTA_MANAGER_CONSOLE_I("Ticker timer will call esp_ota_confirm_image_valid after %u seconds\r\n", timeout_ms / 1000U);
}

THIoT_ESPOTAManager::THIoT_ESPOTAManager() {
  running_partition = nullptr;
  current_state = ESP_OTA_IMG_UNDEFINED;
}

bool THIoT_ESPOTAManager::initialize() {
  running_partition = esp_ota_get_running_partition();
  if (running_partition == nullptr) {
    OTA_MANAGER_CONSOLE_E("Failed to get running partition");
    return false;
  }
  return updateCurrentState();
}

bool THIoT_ESPOTAManager::updateCurrentState() {
  if (running_partition == nullptr) {
    OTA_MANAGER_CONSOLE_E("Running partition not initialized");
    return false;
  }
  
  esp_err_t result = esp_ota_get_state_partition(running_partition, &current_state);
  if (result != ESP_OK) {
    OTA_MANAGER_CONSOLE_E("Failed to get partition state: %d", result);
    return false;
  }
  return true;
}

esp_ota_img_states_t THIoT_ESPOTAManager::getCurrentState() {
  return current_state;
}

const char* THIoT_ESPOTAManager::getStateString() {
  switch (current_state) {
    case ESP_OTA_IMG_VALID:
      return "Valid";
    case ESP_OTA_IMG_INVALID:
      return "Invalid";
    case ESP_OTA_IMG_PENDING_VERIFY:
      return "Pending Verify";
    case ESP_OTA_IMG_ABORTED:
      return "Aborted";
    case ESP_OTA_IMG_NEW:
      return "New";
    case ESP_OTA_IMG_UNDEFINED:
    default:
      return "Undefined";
  }
}

bool THIoT_ESPOTAManager::isPendingVerify() {
  return current_state == ESP_OTA_IMG_PENDING_VERIFY || current_state == ESP_OTA_IMG_UNDEFINED;
}

bool THIoT_ESPOTAManager::isUndetermined() {
  return current_state == ESP_OTA_IMG_UNDEFINED;
}

bool THIoT_ESPOTAManager::isValid() {
  return current_state == ESP_OTA_IMG_VALID;
}

bool THIoT_ESPOTAManager::isInvalid() {
  return current_state != ESP_OTA_IMG_VALID;
}

bool THIoT_ESPOTAManager::markAppValidAndCancelRollback() {
  if (!isPendingVerify()) {
    OTA_MANAGER_CONSOLE_I("App is not in pending verify state, no rollback to cancel");
    return true;
  }
  
  esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
  if (result == ESP_OK) {
    OTA_MANAGER_CONSOLE_I("Rollback cancelled successfully");
    updateCurrentState(); // Update state after operation
    return true;
  } else {
    OTA_MANAGER_CONSOLE_E("Failed to cancel rollback: %d", result);
    return false;
  }
}

bool THIoT_ESPOTAManager::markAppInvalid() {
  if (!esp_ota_check_rollback_is_possible ()) {
    OTA_MANAGER_CONSOLE_I("Rollback is not possible, marking app as invalid anyway");
    return false;
  } else {
    OTA_MANAGER_CONSOLE_I("Rollback is possible, marking app as invalid\r\n");
  }
  
  // This function typically doesn't return as it reboots
  noInitOTAValidation = NO_INIT_OTA_VALIDATION_ROLLBACK_SUCCESS_VALUE; // Set the validation value to indicate rollback
  esp_err_t result = esp_ota_mark_app_invalid_rollback_and_reboot();
  noInitOTAValidation = NO_INIT_OTA_VALIDATION_ROLLBACK_FAILURE_VALUE;
  OTA_MANAGER_CONSOLE_E("Failed to mark app invalid and rollback: %d", result);
  return result == ESP_OK; // This will not return if successful, but we check for completeness
}

void THIoT_ESPOTAManager::printStatus() {
  if (running_partition == nullptr) {
    OTA_MANAGER_CONSOLE_E("OTA Manager not initialized");
    return;
  }
  
  OTA_MANAGER_CONSOLE_I("OTA Partition Status:");
  OTA_MANAGER_CONSOLE_I("  - Partition Label: %s", running_partition->label);
  OTA_MANAGER_CONSOLE_I("  - Partition Address: 0x%08x", running_partition->address);
  OTA_MANAGER_CONSOLE_I("  - Partition Size: %d bytes", running_partition->size);
  OTA_MANAGER_CONSOLE_I("  - Current State: %s", getStateString());
  
  if (isPendingVerify()) {
    OTA_MANAGER_CONSOLE_I("  - Action Required: Call markAppValidAndCancelRollback() to confirm update");
  }
}

void THIoT_ESPOTAManager::printRecord() {
  if (running_partition == nullptr) {
    OTA_MANAGER_TAG_LOG("OTA Manager not initialized");
    return;
  }
  
  OTA_MANAGER_TAG_LOG("OTA Partition Status:");
  OTA_MANAGER_TAG_LOG("  - Partition Label: %s", running_partition->label);
  OTA_MANAGER_TAG_LOG("  - Partition Address: 0x%08x", running_partition->address);
  OTA_MANAGER_TAG_LOG("  - Partition Size: %d bytes", running_partition->size);
  OTA_MANAGER_TAG_LOG("  - Current State: %s", getStateString());
}

bool THIoT_ESPOTAManager::handlePendingVerify() {
  if (isPendingVerify()) {
    OTA_MANAGER_CONSOLE_I("Running partition is pending verify");
    if (markAppValidAndCancelRollback()) {
      OTA_MANAGER_CONSOLE_I("Rollback cancelled - app marked as valid");
      printStatus();
      return true;
    } else {
      OTA_MANAGER_CONSOLE_E("Failed to cancel rollback");
    }
  } else if (isInvalid()) {
    OTA_MANAGER_CONSOLE_I("Running partition is invalid, marking as invalid");
    markAppInvalid(); // This will reboot the device
  } else {
    OTA_MANAGER_CONSOLE_I("No pending verify state, no action needed");
  }
  return false;
}
