#ifndef FS_H
#define FS_H
#ifdef M5STACK_CORE2
#define SD_PORT SPI3_HOST
#define SD_CS 4
#else
#define SDMMC_D0 40
#define SDMMC_CLK 39
#define SDMMC_CMD 38
#endif
#ifdef __cplusplus
extern "C" {
#endif
int fs_external_init();
void fs_internal_init();
#ifdef __cplusplus
}
#endif
#endif // FS_H