#ifndef __THIOT_ESP_CORE_DUMP_H__
#define __THIOT_ESP_CORE_DUMP_H__


#include <Arduino.h>
#include "esp_idf_version.h"
#include <sdkconfig.h>
#include "THIoT_APPConfig.h"
#include "THIoT_ESPConfig.h"

#if (ESP_IDF_VERSION_MAJOR < 4)
typedef uint32_t core_dump_crc_t;
esp_err_t esp_core_dump_image_get(size_t* out_addr, size_t *out_size);
esp_err_t esp_core_dump_image_erase(void);
#endif // (ESP_IDF_VERSION_MAJOR >= 4)

#endif // __THIOT_ESP_CORE_DUMP_H__