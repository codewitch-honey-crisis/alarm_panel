#pragma once
#ifdef M5STACK_CORE2
#define SPI_PORT SPI3_HOST
#define SPI_CLK 18
#define SPI_MISO 38
#define SPI_MOSI 23
#endif
#ifdef FREENOVE_DEVKIT
#define SPI_PORT SPI3_HOST
#define SPI_CLK 21
#define SPI_MOSI 20
#define SPI_MISO -1
#endif
void spi_init();