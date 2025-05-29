#ifndef DISPLAY_H
#define DISPLAY_H
// screen dimensions
// indicates how much of the screen gets updated at once
// #define LCD_DIVISOR 2 // optional
// screen connections
#ifdef LOCAL_PC
#define LCD_HRES 800
#define LCD_VRES 480
#define LCD_BIT_DEPTH 16
#endif
#ifdef M5STACK_CORE2
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_ft5x06.h"
#define LCD_HRES 320
#define LCD_VRES 240
#define LCD_DMA
#define LCD_PORT SPI3_HOST
#define LCD_DC 15
#define LCD_CS 5
#define LCD_RST -1    // optional
#define LCD_BL -1     // optional
#define LCD_BL_LOW 0  // optional
#define LCD_PANEL esp_lcd_new_panel_ili9341
#define LCD_GAP_X 0                   // optional
#define LCD_GAP_Y 0                   // optional
#define LCD_SWAP_XY 0                 // optional
#define LCD_MIRROR_X 0                // optional
#define LCD_MIRROR_Y 0                // optional
#define LCD_INVERT_COLOR 1            // optional
#define LCD_BGR 1                     // optional
#define LCD_BIT_DEPTH 16              // optional
#define LCD_SPEED (40 * 1000 * 1000)  // optional
#define LCD_TOUCH_PIN_NUM_RST -1
#define LCD_TOUCH_PIN_NUM_INT -1
#define LCD_TOUCH_ADDRESS ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS
#define LCD_TOUCH_PANEL esp_lcd_touch_new_i2c_ft5x06
#define LCD_TOUCH_VRES 280
#define LCD_TOUCH_DISABLE_CONTROL_PHASE
#endif
#ifdef FREENOVE_DEVKIT
#include "esp_lcd_touch_ft5x06.h"
#define LCD_HRES 240
#define LCD_VRES 320
#define LCD_DMA
#define LCD_SWAP_XY 1                 // optional
#define LCD_SPI_MASTER
#define LCD_PORT SPI3_HOST
#define LCD_DC 0
#define LCD_CS 47
#define LCD_RST -1    // optional
#define LCD_BL -1     // optional
#define LCD_BIT_DEPTH 16              // optional
#define LCD_SPEED (80 * 1000 * 1000)  // optional
#define LCD_TOUCH_PIN_NUM_RST -1
#define LCD_TOUCH_PIN_NUM_INT -1
#define LCD_TOUCH_ADDRESS ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS
#define LCD_TOUCH_PANEL esp_lcd_touch_new_i2c_ft5x06
#define LCD_TOUCH_PIN_NUM_RST -1
#define LCD_TOUCH_PIN_NUM_INT -1
#define LCD_TOUCH_DISABLE_CONTROL_PHASE
#endif
#ifdef WAVESHARE_ESP32S3_43
#include "esp_lcd_touch_gt911.h"
#define LCD_PIN_NUM_DE 5
#define LCD_PIN_NUM_VSYNC 3
#define LCD_PIN_NUM_HSYNC 46
#define LCD_PIN_NUM_CLK 7
#define LCD_PIN_NUM_D00 14
#define LCD_PIN_NUM_D01 38
#define LCD_PIN_NUM_D02 18
#define LCD_PIN_NUM_D03 17
#define LCD_PIN_NUM_D04 10
#define LCD_PIN_NUM_D05 39
#define LCD_PIN_NUM_D06 0
#define LCD_PIN_NUM_D07 45
#define LCD_PIN_NUM_D08 48
#define LCD_PIN_NUM_D09 47
#define LCD_PIN_NUM_D10 21
#define LCD_PIN_NUM_D11 1
#define LCD_PIN_NUM_D12 2
#define LCD_PIN_NUM_D13 42
#define LCD_PIN_NUM_D14 41
#define LCD_PIN_NUM_D15 40
#define LCD_HSYNC_POLARITY 0
#define LCD_HSYNC_FRONT_PORCH 8
#define LCD_HSYNC_PULSE_WIDTH 4
#define LCD_HSYNC_BACK_PORCH 8
#define LCD_VSYNC_POLARITY 0
#define LCD_VSYNC_FRONT_PORCH 8
#define LCD_VSYNC_PULSE_WIDTH 4
#define LCD_VSYNC_BACK_PORCH 8
#define LCD_CLK_IDLE_HIGH 0
#define LCD_DE_IDLE_HIGH 0
#define LCD_BIT_DEPTH 16
#define LCD_BOUNCE_HEIGHT 10
#define LCD_DIVISOR 1
#define LCD_PSRAM_BUFFER
//#define LCD_PANEL esp_lcd_new_panel_st7701
#define LCD_HRES 800
#define LCD_VRES 480
#define LCD_COLOR_SPACE ESP_LCD_COLOR_SPACE_BGR
#define LCD_SWAP_COLOR_BYTES 1
#ifdef CONFIG_SPIRAM_MODE_QUAD
    #define LCD_PIXEL_CLOCK_HZ (6 * 1000 * 1000)
#else
    #define LCD_PIXEL_CLOCK_HZ (16 * 1000 * 1000)
#endif
#define LCD_TOUCH_PANEL esp_lcd_touch_new_i2c_gt911
#define LCD_TOUCH_ADDRESS ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS
#define LCD_TOUCH_CMD_BITS 16
#define LCD_TOUCH_PARAM_BITS 0
#define LCD_TOUCH_DISABLE_CONTROL_PHASE
#define LCD_TOUCH_SPEED (400*1000)
#endif
#ifdef LOCAL_PC
#define LCD_BIT_DEPTH 16
#endif

#ifndef LCD_WIDTH
#ifdef LCD_SWAP_XY
#if LCD_SWAP_XY
#define LCD_WIDTH LCD_VRES
#define LCD_HEIGHT LCD_HRES
#else
#define LCD_WIDTH LCD_HRES
#define LCD_HEIGHT LCD_VRES
#endif
#else
#define LCD_WIDTH LCD_HRES
#define LCD_HEIGHT LCD_VRES
#endif
#endif
#ifndef LCD_BIT_DEPTH
#define LCD_BIT_DEPTH 16
#endif
#ifndef LCD_X_ALIGN
#define LCD_X_ALIGN 1
#endif
#ifndef LCD_Y_ALIGN
#define LCD_Y_ALIGN 1
#endif
#ifndef LCD_FRAME_ADAPTER
#define LCD_FRAME_ADAPTER gfx::bitmap<gfx::rgb_pixel<LCD_BIT_DEPTH>>
#endif
#ifndef LCD_DC_BIT_OFFSET
#define LCD_DC_BIT_OFFSET 0
#endif
#ifdef LCD_TOUCH_PANEL
#ifndef LCD_TOUCH_HRES
#define LCD_TOUCH_HRES LCD_HRES
#endif
#ifndef LCD_TOUCH_VRES
#define LCD_TOUCH_VRES LCD_VRES
#endif
#ifndef LCD_TOUCH_PIN_NUM_RST
#define LCD_TOUCH_PIN_NUM_RST -1
#endif
#ifndef LCD_TOUCH_PIN_NUM_INT
#define LCD_TOUCH_PIN_NUM_INT -1
#endif
#ifndef LCD_TOUCH_CMD_BITS
#define LCD_TOUCH_CMD_BITS 8
#endif
#ifndef LCD_TOUCH_PARAM_BITS
#define LCD_TOUCH_PARAM_BITS 8
#endif
#ifndef LCD_TOUCH_DC_OFFSET
#define LCD_TOUCH_DC_OFFSET 0
#endif
#ifndef LCD_TOUCH_CONTROL_PHASE_BYTES
#define LCD_TOUCH_CONTROL_PHASE_BYTES 1
#endif
#ifndef LCD_TOUCH_SPEED
#define LCD_TOUCH_SPEED I2C_SPEED
#endif
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initialize the display
/// @returns 0 on success, otherwise non-zero
int display_init(void);
void display_update(void);
void display_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const void* bmp);
void display_flush_complete(); // to be implemented by the user
int display_touch_read(uint16_t* out_x_array,uint16_t* out_y_array, uint16_t* out_strength_array, size_t* in_out_touch_count);
#ifdef __cplusplus
}
#endif
#endif // DISPLAY_H