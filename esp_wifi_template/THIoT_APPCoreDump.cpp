#include <esp_task_wdt.h>
#include <esp_partition.h>
#if (ESP_IDF_VERSION_MAJOR >= 4)
#include "esp_core_dump.h"
#else
#include "THIoT_ESPCoreDump.h"
#endif
#include "THIoT_ESPSDFSClass.h"
#include "THIoT_ESPFSHandle.h"
#include "THIoT_APPCoreDump.h"
#include "THIoT_ESPWatchDogTimer.h"
#include "THIoT_PFSerialTrace.h"
#include "THIoT_ESPLogTrace.h"

#if defined(CORE_DUMP_TRACE_RECORD) && (CORE_DUMP_TRACE_RECORD == 1)
#if (0)
#define CORE_DUMP_PORT(...)     CONSOLE_PRINTF(__VA_ARGS__)
#define CORE_DUMP_TAG_LOGI(...) CONSOLE_TAG_LOGI("[CORE_DUMP]", __VA_ARGS__)
#define CORE_DUMP_TAG_LOGE(...) CONSOLE_TAG_LOGE("[CORE_DUMP]", __VA_ARGS__)
#else
#define CORE_DUMP_PORT(...)     FS_PRINTF(__VA_ARGS__)
#define CORE_DUMP_TAG_LOGI(...) FS_TAG_LOGI("[CORE_DUMP]", __VA_ARGS__)
#define CORE_DUMP_TAG_LOGE(...) FS_TAG_LOGE("[CORE_DUMP]", __VA_ARGS__)
#endif

#define CORE_DUMP_READ_CHUNKS 4096U /* Core dump partition size must be a multiple of CORE_DUMP_READ_CHUNKS */

#define CORE_DUMP_MEMORY_SIZE_LIMITED 0x20000 /* 128kByte, size of coredump partition */

THIoTAppCoreDump::THIoTAppCoreDump(FS &fs, const char *fileName) :_fs(&fs), _fileName(fileName) {}
THIoTAppCoreDump::~THIoTAppCoreDump() {}

#if (ESP_IDF_VERSION_MAJOR >= 4)
#if defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && (CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH == 1)
#define CORE_DUMP_ENABLE_TO_FLASH 1
#else
#define CORE_DUMP_ENABLE_TO_FLASH 0
#endif
#else
#if defined(CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH) && (CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH == 1)
#define CORE_DUMP_ENABLE_TO_FLASH 1
#else
#define CORE_DUMP_ENABLE_TO_FLASH 0
#endif
#endif

bool THIoTAppCoreDump::record(void)
{
#if (CORE_DUMP_ENABLE_TO_FLASH == 1)
  CORE_DUMP_PORT("\r\n\r\n\t\t\t=========== CoreDump: Start recording... =========== \r\n");
  const esp_partition_t *ptCoreDump = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
  if (ptCoreDump == NULL)
  {
    CORE_DUMP_TAG_LOGE("No core dump partition found!\n");
    return false;
  }
  CORE_DUMP_TAG_LOGI("CoreDump: partition found, label: %s, address: 0x%08x, size: %zu", ptCoreDump->label, ptCoreDump->address, ptCoreDump->size);

#if (ESP_IDF_VERSION_MAJOR >= 4)
  if (esp_core_dump_image_check() != ESP_OK)
  {
    CORE_DUMP_PORT("\t\t\t=========== CoreDump: No core dump data found =========== \r\n");
    CORE_DUMP_PORT("\r\n\r\n");
    return false;
  }
#endif

  size_t cd_addr, cd_size;
  if (esp_core_dump_image_get(&cd_addr, &cd_size) != ESP_OK)
  {
    CORE_DUMP_TAG_LOGE("CoreDump: cannot get image address and size\r\n\r\n");
    return false;
  }
  CORE_DUMP_TAG_LOGI("CoreDump: image address: 0x%08x, size: %zu", cd_addr, cd_size);

#if (ESP_IDF_VERSION_MAJOR >= 4)
  esp_core_dump_summary_t *summary = (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
  if (summary)
  {
    if (esp_core_dump_get_summary(summary) == ESP_OK)
    {
      CORE_DUMP_TAG_LOGI("CoreDump: Summary:");
      CORE_DUMP_PORT("\t\t\tBacktrace: ");
      if (summary->exc_bt_info.depth > 16) {
        summary->exc_bt_info.depth = 16; // Limit backtrace depth to 16
      }
      for (size_t i = 0; i < summary->exc_bt_info.depth; i++)
      {
        CORE_DUMP_PORT("0x%08x ", summary->exc_bt_info.bt[i]);
      }
      CORE_DUMP_PORT("\r\n");
      CORE_DUMP_TAG_LOGI("CoreDump: corrupted: %u Task: %.16s, TCB: 0x%08x, PC: 0x%08x, Version: %u",
        summary->exc_bt_info.corrupted, summary->exc_task, summary->exc_tcb, summary->exc_pc, summary->core_dump_version);
    }
  }
  free(summary);
#endif

  if (_fs->exists(_fileName)) {
    if (FSHandle.backup2Folder(*_fs, _fileName)) {
      CORE_DUMP_TAG_LOGI("CoreDump file %s: backup done", _fileName.c_str());
    }
  }

  uint8_t *buffer = (uint8_t *)malloc(CORE_DUMP_READ_CHUNKS);
  if (buffer == NULL)
  {
    CORE_DUMP_TAG_LOGE("CoreDump: ERROR allocating memory for read buffer\r\n\r\n");
    return false;
  }

  File fs_handle;
  fs_handle = _fs->open(_fileName, "w");
  if (!fs_handle) {
    CORE_DUMP_TAG_LOGE("CoreDump: ERROR open file %s\r\n\r\n", _fileName.c_str());
    return false;
  }
  size_t length = CORE_DUMP_READ_CHUNKS;
  size_t index = 0;
  size_t size = cd_size;
  CORE_DUMP_TAG_LOGI("CoreDump: reading from partition, total size: %zu", size);
  while (size > 0)
  {
    /* This loop is dumping 128KB, reset the WD... */
    esp_task_wdt_reset();
    /* Watch dog timer feed */
    wdt_reset();
    if (length > size)
    {
      length = size;
    }

    esp_err_t er = esp_partition_read(ptCoreDump, index, buffer, length);
    if (er != ESP_OK)
    {
      CORE_DUMP_TAG_LOGE("CoreDump: partition read error : %s", esp_err_to_name(er));
      break;
    }

    size_t w = fs_handle.write(buffer, length);
    if (w != length)
    {
      CORE_DUMP_TAG_LOGE("CoreDump: write error, expected %zu bytes, wrote %zu bytes", length, w);
      break;
    }

    index += length;
    size -= length;
    if (size <= 0)
    {
      CORE_DUMP_TAG_LOGI("CoreDump: read complete, total bytes read: %zu", index);
      break;
    }
  }

  free(buffer);
  fs_handle.flush();  // Ensure all data is written to the file system then size can be checked
  size_t f_size = fs_handle.size();
  fs_handle.close();

  CORE_DUMP_TAG_LOGI("CoreDump: read %zu bytes from partition", index);
  esp_core_dump_image_erase();

  if (size > 0) {
    CORE_DUMP_TAG_LOGE("CoreDump: read incomplete, %zu bytes left", size);
    _fs->remove(_fileName);
  }

  CORE_DUMP_PORT("\t\t\t=========== CoreDump: Record finished size %u =========== \r\n", f_size);
  CORE_DUMP_PORT("\r\n\r\n");
  return true;
#else
  CORE_DUMP_TAG_LOGE("CoreDump: not enabled, please enable it in menuconfig\r\n\r\n");
  return false;
#endif
}

THIoTAppCoreDump gAppCoreDump(CORE_DUMP_FS_LOG_SYSTEM, CORE_DUMP_RECORD_PATH);

#endif // defined(CORE_DUMP_TRACE_RECORD) && (CORE_DUMP_TRACE_RECORD == 1)