#include "fs.h"
#include "driver/sdmmc_host.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include <memory.h>
#include <stdio.h>
static sdmmc_card_t* fs_card = NULL;
int fs_external_init() {
    static const char mount_point[] = "/sdcard";
    esp_vfs_fat_sdmmc_mount_config_t mount_config;
    memset(&mount_config, 0, sizeof(mount_config));
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 0;
#ifdef SD_CS
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_PORT;
    sdspi_device_config_t slot_config;
    memset(&slot_config, 0, sizeof(slot_config));
    slot_config.host_id = (spi_host_device_t)SD_PORT;
    slot_config.gpio_cs = (gpio_num_t)SD_CS;
    slot_config.gpio_cd = SDSPI_SLOT_NO_CD;
    slot_config.gpio_wp = SDSPI_SLOT_NO_WP;
    slot_config.gpio_int = GPIO_NUM_NC;
    if (ESP_OK != esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config,
                                          &mount_config, &fs_card)) {
        return -1;
    }
    return 0;
    
#elif defined(SDMMC_CLK)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz =20*1000;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = (gpio_num_t)SDMMC_CLK;
    slot_config.cmd = (gpio_num_t)SDMMC_CMD;
    slot_config.d0 = (gpio_num_t)SDMMC_D0;
    slot_config.width = 1;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &fs_card);
    if(ret!=ESP_OK) {
        return -1;
    }
    return 0;
#else
    return -1;
#endif
}
void fs_internal_init() {
    esp_vfs_spiffs_conf_t conf;
    memset(&conf, 0, sizeof(conf));
    conf.base_path = "/spiffs";
    conf.partition_label = NULL;
    conf.max_files = 5;
    conf.format_if_mount_failed = 1;
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}