#pragma once
#ifdef M5STACK_CORE2
#define SD_PORT SPI3_HOST
#define SD_CS 4
#else
#define SDMMC_D0 40
#define SDMMC_CLK 39
#define SDMMC_CMD 38
#endif
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"

bool fs_sd_init();
sdmmc_card_t* fs_sd_card();
void fs_spiffs_init();
