#include "spi.h"
#include "display.h"
#include <memory.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "ui.h"
int spi_init(void) {
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = SPI_CLK;
    buscfg.mosi_io_num = SPI_MOSI;
    buscfg.miso_io_num = SPI_MISO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
#ifdef LCD_DIVISOR
    static const size_t lcd_divisor = LCD_DIVISOR;
#else
    static const size_t lcd_divisor = 10;
#endif
#ifdef LCD_BIT_DEPTH
    static const size_t lcd_pixel_size = (LCD_BIT_DEPTH + 7) / 8;
#else
    static const size_t lcd_pixel_size = 2;
#endif
    // the size of our transfer buffer(s)
    static const size_t lcd_transfer_buffer_size =
        LCD_WIDTH * LCD_HEIGHT * lcd_pixel_size / lcd_divisor;

    buscfg.max_transfer_sz =
        (lcd_transfer_buffer_size > 512 ? lcd_transfer_buffer_size : 512) + 8;
    // Initialize the SPI bus on VSPI (SPI3)
    if(ESP_OK!=spi_bus_initialize(SPI_PORT, &buscfg, SPI_DMA_CH_AUTO)) {
        return -1;
    }
    return 0;
}
