
#include "rom/crc.h"
#include <esp_spi_flash.h>
#include "THIoT_ESPCoreDump.h"
#if (ESP_IDF_VERSION_MAJOR < 4)

#define BLANK_COREDUMP_SIZE 0xFFFFFFFF

esp_err_t esp_core_dump_image_get(size_t* out_addr, size_t *out_size)
{
    esp_err_t err;
    const void *core_data;
    spi_flash_mmap_handle_t core_data_handle;

    if (out_addr == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *core_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (!core_part) {
        ESP_LOGE(TAG, "No core dump partition found!");
        return ESP_FAIL;
    }
    if (core_part->size < sizeof(uint32_t)) {
        ESP_LOGE(TAG, "Too small core dump partition!");
        return ESP_FAIL;
    }

    err = esp_partition_mmap(core_part, 0,  sizeof(uint32_t),
                             SPI_FLASH_MMAP_DATA, &core_data, &core_data_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mmap core dump data (%d)!", err);
        return err;
    }
    uint32_t *dw = (uint32_t *)core_data;
    *out_size = *dw;
    spi_flash_munmap(core_data_handle);
    if (*out_size == 0xFFFFFFFF) {
        ESP_LOGD(TAG, "Blank core dump partition!");
        return ESP_ERR_INVALID_SIZE;
    } else if ((*out_size < sizeof(uint32_t)) || (*out_size > core_part->size)) {
        ESP_LOGE(TAG, "Incorrect size of core dump image: %d", *out_size);
        return ESP_ERR_INVALID_SIZE;
    }
    // remap full core dump with CRC
    err = esp_partition_mmap(core_part, 0, *out_size,
                             SPI_FLASH_MMAP_DATA, &core_data, &core_data_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mmap core dump data (%d)!", err);
        return err;
    }
    uint32_t *crc = (uint32_t *)(((uint8_t *)core_data) + *out_size);
    crc--; // Point to CRC field
    // Calc CRC over core dump data except for CRC field
    core_dump_crc_t cur_crc = crc32_le(0, (uint8_t const *)core_data, *out_size - sizeof(core_dump_crc_t));
    if (*crc != cur_crc) {
        ESP_LOGE(TAG, "Core dump data CRC check failed: 0x%x -> 0x%x!", *crc, cur_crc);
        spi_flash_munmap(core_data_handle);
        return ESP_FAIL;
    }

    spi_flash_munmap(core_data_handle);

    *out_addr = core_part->address;
    return ESP_OK;
}

esp_err_t esp_core_dump_image_erase(void)
{
    /* If flash is encrypted, we can only write blocks of 16 bytes, let's always
     * write a 16-byte buffer. */
    uint32_t helper[4] = { BLANK_COREDUMP_SIZE };
    _Static_assert(sizeof(helper) % 16 == 0, "esp_partition_write() needs multiple of 16 bytes long buffers");

    /* Find the partition that could potentially contain a (previous) core dump. */
    const esp_partition_t *core_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                                ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
                                                                NULL);
    if (!core_part) {
        ESP_LOGE(TAG, "No core dump partition found!");
        return ESP_ERR_NOT_FOUND;
    }
    if (core_part->size < sizeof(uint32_t)) {
        ESP_LOGE(TAG, "Too small core dump partition!");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = ESP_OK;
    err = esp_partition_erase_range(core_part, 0, core_part->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase core dump partition (%d)!", err);
        return err;
    }

    err = esp_partition_write(core_part, 0, helper, sizeof(helper));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write core dump partition size (%d)!", err);
    }

    return err;
}

#endif // (ESP_IDF_VERSION_MAJOR >= 4)