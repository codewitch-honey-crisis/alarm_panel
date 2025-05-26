#ifndef DISPLAY_H
#define DISPLAY_H
// screen dimensions
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
// indicates how much of the screen gets updated at once
// #define LCD_DIVISOR 2 // optional
// screen connections
#ifdef M5STACK_CORE2
#include "esp_lcd_panel_ili9342.h"
#define LCD_PORT SPI3_HOST
#define LCD_DC 15
#define LCD_CS 5
#define LCD_RST -1    // optional
#define LCD_BL -1     // optional
#define LCD_BL_LOW 0  // optional
#define LCD_PANEL esp_lcd_new_panel_ili9342
#define LCD_GAP_X 0                   // optional
#define LCD_GAP_Y 0                   // optional
#define LCD_SWAP_XY 0                 // optional
#define LCD_MIRROR_X 0                // optional
#define LCD_MIRROR_Y 0                // optional
#define LCD_INVERT_COLOR 1            // optional
#define LCD_BGR 1                     // optional
#define LCD_BIT_DEPTH 16              // optional
#define LCD_SPEED (40 * 1000 * 1000)  // optional
#endif
#ifdef FREENOVE_DEVKIT
#define LCD_SPI_MASTER
#define LCD_PORT SPI3_HOST
#define LCD_DC 0
#define LCD_CS 47
#define LCD_RST -1    // optional
#define LCD_BL -1     // optional
#define LCD_BIT_DEPTH 16              // optional
#define LCD_SPEED (80 * 1000 * 1000)  // optional
#endif
#ifdef LOCAL_PC
#define LCD_BIT_DEPTH 16
#endif
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initialize the display
void display_init(void);
/// @brief Update the display 
void display_update(void);
/// @brief switch the current screen
/// @param screen a pointer to a uix::screen_base
void display_screen(void* screen);
#ifdef __cplusplus
}
#endif
#endif // DISPLAY_H