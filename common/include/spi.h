#ifndef SPI_H
#define SPI_H
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
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initializes the SPI bus
/// @return 0 on success, nonzero on error
int spi_init(void);
#ifdef __cplusplus
}
#endif
#endif // SPI_H