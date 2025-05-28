#ifndef DISPLAY_H
#define DISPLAY_H
// screen dimensions
// indicates how much of the screen gets updated at once
// #define LCD_DIVISOR 2 // optional
// screen connections
#ifdef LOCAL_PC
#define LCD_HRES 320
#define LCD_VRES 240
#define LCD_BIT_DEPTH 16
#endif
#ifdef M5STACK_CORE2
#include "esp_lcd_ili9341.h"
#define LCD_HRES 320
#define LCD_VRES 240
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
#endif
#ifdef FREENOVE_DEVKIT
#define LCD_HRES 320
#define LCD_VRES 240
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