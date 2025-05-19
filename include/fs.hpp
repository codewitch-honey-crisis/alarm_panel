#pragma once

#define SD_PORT SPI3_HOST
#define SD_CS 4

#include "esp_spiffs.h"
#include "esp_vfs_fat.h"

extern sdmmc_card_t* fd_sd_card;
bool fs_sd_init();
void fs_spiffs_init();
