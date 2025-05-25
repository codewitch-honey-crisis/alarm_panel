#include "display.h"
#include "esp_i2c.hpp"  // i2c initialization
#include "ft6336.hpp"

#include "i2c.h"
#include "spi.h"
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef LCD_SPI_MASTER
#include "hal/gpio_ll.h"
#define DC_C GPIO.out_w1tc = (1 << LCD_DC);
#define DC_D GPIO.out_w1ts = (1 << LCD_DC);
static spi_device_handle_t lcd_spi_handle = NULL;
static spi_transaction_t lcd_trans[14];
static size_t lcd_trans_index = 0;
static volatile int lcd_flushing = 0;
static void lcd_on_flush_complete();
static void lcd_command(uint8_t cmd, const uint8_t* args, size_t len) {
    spi_transaction_t* tx = &lcd_trans[lcd_trans_index++];
    if (lcd_trans_index > 13) lcd_trans_index = 0;
    tx->length = 8;
    tx->tx_data[0] = cmd;
    tx->user = (void*)0;
    tx->flags = SPI_TRANS_USE_TXDATA;
    spi_device_queue_trans(lcd_spi_handle, tx, portMAX_DELAY);
    if (len && args) {
        tx = &lcd_trans[lcd_trans_index++];
        if (lcd_trans_index > 13) lcd_trans_index = 0;
        tx->length = 8 * len;
        if (len <= 4) {
            memcpy(tx->tx_data, args, len);
            tx->flags = SPI_TRANS_USE_TXDATA;
        } else {
            tx->tx_buffer = args;
            tx->flags = 0;
        }
        tx->user = (void*)1;
        spi_device_queue_trans(lcd_spi_handle, tx, portMAX_DELAY);
    }
}
IRAM_ATTR static void lcd_spi_pre_cb(spi_transaction_t* trans) {
    if (((int)trans->user) == 0) {
        DC_C;
    }
}
IRAM_ATTR static void lcd_spi_post_cb(spi_transaction_t* trans) {
    lcd_flushing = 0;
    if (((int)trans->user) == 0) {
        DC_D;
    } else {
        if (((int)trans->user) == 2) {
            lcd_on_flush_complete();
        }
    }
}
static void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t args[4];
    args[0] = (x1 >> 8);
    args[1] = (x1 & 0xFF);
    args[2] = (x2 >> 8);
    args[3] = (x2 & 0xFF);
    lcd_command(0x2A, args, 4);
    args[0] = (y1 >> 8);
    args[1] = (y1 & 0xFF);
    args[2] = (y2 >> 8);
    args[3] = (y2 & 0xFF);
    lcd_command(0x2B, args, 4);
}
static void lcd_write_bitmap(const void* data_in, uint32_t len) {
    if (len) {
        spi_transaction_t* tx = &lcd_trans[lcd_trans_index++];
        if (lcd_trans_index > 13) lcd_trans_index = 0;
        tx->user = (void*)0;
        tx->flags = SPI_TRANS_USE_TXDATA;
        tx->tx_data[0] = 0x2C;  // RAMWR
        tx->length = 8;
        ESP_ERROR_CHECK(
            spi_device_queue_trans(lcd_spi_handle, tx, portMAX_DELAY));

        tx = &lcd_trans[lcd_trans_index++];
        if (lcd_trans_index > 13) lcd_trans_index = 0;
        tx->flags = 0;
        tx->length = 8 * (len * 2);
        tx->tx_buffer = data_in;
        tx->user = (void*)2;
        ESP_ERROR_CHECK(
            spi_device_queue_trans(lcd_spi_handle, tx, portMAX_DELAY));
    } else {
        lcd_flushing = 0;
        lcd_on_flush_complete();
    }
}
#ifdef FREENOVE_DEVKIT
static void lcd_st7789_init();
#endif
void lcd_init_impl() {
    memset(lcd_trans, 0, sizeof(spi_transaction_t) * 14);
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = (((unsigned long long)1) << LCD_DC);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&gpio_conf);
    gpio_set_direction((gpio_num_t)LCD_CS, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LCD_CS, 0);

    spi_device_interface_config_t dev_cfg;
    memset(&dev_cfg, 0, sizeof(dev_cfg));
    dev_cfg.dummy_bits = 0;
    dev_cfg.queue_size = 14;
    dev_cfg.flags = SPI_DEVICE_NO_DUMMY | SPI_DEVICE_HALFDUPLEX;
    dev_cfg.spics_io_num = LCD_CS;
    dev_cfg.pre_cb = lcd_spi_pre_cb;
    dev_cfg.post_cb = lcd_spi_post_cb;
    dev_cfg.clock_speed_hz = LCD_SPEED;
    dev_cfg.cs_ena_posttrans = 1;
    dev_cfg.cs_ena_pretrans = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_PORT , &dev_cfg, &lcd_spi_handle));
    ESP_ERROR_CHECK(spi_device_acquire_bus(lcd_spi_handle, portMAX_DELAY));
    // if we don't configure GPIO 0 after we init the SPI it stays high for some
    // reason
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.pin_bit_mask = (1ULL << LCD_DC);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)LCD_DC, 0);
#ifdef FREENOVE_DEVKIT
    lcd_st7789_init();
#endif
}
#else
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#endif

#ifdef FREENOVE_DEVKIT
static void lcd_st7789_init() {
    lcd_command(0x01, NULL, 0);      // reset
    vTaskDelay(pdMS_TO_TICKS(120));  // Wait for reset to complete
    lcd_command(0x11, NULL, 0);      // Sleep out
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_command(0x13, NULL, 0);  // Normal display mode on
    static const uint8_t params1 = 0x08;
    lcd_command(0x36, &params1, 1);
    static const uint8_t params2[] = {0x0A, 0xB2};
    lcd_command(0xB6, params2, 2);
    static const uint8_t params3[] = {0x00, 0xE0};
    lcd_command(0xB0, params3, 2);
    static const uint8_t params4 = 0x55;
    lcd_command(0x3A, &params4, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    static const uint8_t params5[] = {0x0C, 0xC, 0x00, 0x33, 0x33};
    lcd_command(0xB2, params5, 5);
    static const uint8_t params6 = 0x35;
    lcd_command(0xB7, &params6, 1);  // Voltages: VGH / VGL
    static const uint8_t params7 = 0x28;
    lcd_command(0xBB, &params7, 1);
    static const uint8_t params8 = 0x0C;
    lcd_command(0xC0, &params8, 1);
    static const uint8_t params9[] = {0x01, 0xFF};
    lcd_command(0xC2, params9, 2);
    static const uint8_t params10 = 0x10;
    lcd_command(0xC3, &params10, 1);  // voltage VRHS
    static const uint8_t params11 = 0x20;
    lcd_command(0xC4, &params11, 1);
    static const uint8_t params12 = 0x0F;
    lcd_command(0xC6, &params12, 1);
    static const uint8_t params13[] = {0xA4, 0xA1};
    lcd_command(0xD0, params13, 2);
    static const uint8_t params14[] = {0xD0, 0x00, 0x02, 0x07, 0x0A,
                                       0x28, 0x32, 0x44, 0x42, 0x06,
                                       0x0E, 0x12, 0x14, 0x17};
    lcd_command(0xE0, params14, 14);
    static const uint8_t params15[] = {0xD0, 0x00, 0x02, 0x07, 0x0A,
                                       0x28, 0x31, 0x54, 0x47, 0x0E,
                                       0x1C, 0x17, 0x1B, 0x1E};
    lcd_command(0xE1, params15, 14);
    lcd_command(0x21, NULL, 0);
    static const uint8_t params16[] = {0x00, 0x00, 0x00, 0xEF};
    lcd_command(0x2A, params16, 4);  // Column address set
    static const uint8_t params17[] = {0x00, 0x00, 0x01, 0x3F};
    lcd_command(0x2B, params17, 4);  // Row address set
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_command(0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_command(0x20, NULL, 0);
    static const uint8_t params18 = (0x20 | 0x80 | 0x08);
    lcd_command(0x36, &params18, 1);
}
void lcd_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
               const void* bitmap) {
    lcd_flushing=1;
    lcd_set_window(x1, y1, x2, y2);
    int w = x2 - x1 + 1, h = y2 - y1 + 1;
    lcd_write_bitmap(bitmap, w * h);
}
#endif

#include "gfx.hpp"
#include "uix.hpp"


using namespace uix;
using namespace uix;
using namespace esp_idf;

static uix::display lcd;

#ifdef LCD_SPI_MASTER
static void lcd_on_flush_complete() {
    lcd.flush_complete();
}
#endif
// initialize the screen 
void display_init() {
    // for the touch panel
#ifdef M5STACK_CORE2
    constexpr static const uint16_t touch_hres = LCD_WIDTH;
    constexpr static const uint16_t touch_vres = LCD_HEIGHT+40;
#endif
#ifdef FREENOVE_DEVKIT
    constexpr static const uint16_t touch_hres = LCD_HEIGHT;
    constexpr static const uint16_t touch_vres = LCD_WIDTH;
#endif

    using touch_t = ft6336<touch_hres, touch_vres, 16>;
    static touch_t touch(esp_i2c<I2C_PORT, I2C_SDA, I2C_SCL>::instance);

#ifdef LCD_DIVISOR
    static constexpr const size_t lcd_divisor = LCD_DIVISOR;
#else
    static constexpr const size_t lcd_divisor = 10;
#endif
#ifdef LCD_BIT_DEPTH
    static constexpr const size_t lcd_pixel_size = (LCD_BIT_DEPTH + 7) / 8;
#else
    static constexpr const size_t lcd_pixel_size = 2;
#endif
    // the size of our transfer buffer(s)
    static const constexpr size_t lcd_transfer_buffer_size =
        LCD_WIDTH * LCD_HEIGHT * lcd_pixel_size / lcd_divisor;

    uint8_t* lcd_transfer_buffer1 =
        (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size, MALLOC_CAP_DMA);
    uint8_t* lcd_transfer_buffer2 =
        (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size, MALLOC_CAP_DMA);
    if (lcd_transfer_buffer1 == nullptr || lcd_transfer_buffer2 == nullptr) {
        puts("Out of memory allocating transfer buffers");
        while (1) vTaskDelay(5);
    }
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
    lcd.buffer2(lcd_transfer_buffer2);
#if defined(LCD_BL) && LCD_BL > 1
#ifdef LCD_BL_LOW
    static constexpr const int bl_on = !(LCD_BL_LOW);
#else
    static constexpr const int bl_on = 1;
#endif
    gpio_set_direction((gpio_num_t)LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LCD_BL, !bl_on);
#endif
#ifdef LCD_SPI_MASTER
    lcd_init_impl();
#else
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config;
    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = LCD_DC;
    io_config.cs_gpio_num = LCD_CS;
#ifdef LCD_SPEED
    io_config.pclk_hz = LCD_SPEED;
#else
    io_config.pclk_hz = 20 * 1000 * 1000;
#endif
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = [](esp_lcd_panel_io_handle_t lcd_io,
                                       esp_lcd_panel_io_event_data_t* edata,
                                       void* user_ctx) {
        lcd.flush_complete();
        return true;
    };
    // Attach the LCD to the SPI bus
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_PORT, &io_config,
                             &io_handle);

    esp_lcd_panel_handle_t lcd_handle = NULL;
    esp_lcd_panel_dev_config_t lcd_config;
    memset(&lcd_config, 0, sizeof(lcd_config));
#ifdef LCD_RST
    lcd_config.reset_gpio_num = LCD_RST;
#else
    lcd_config.reset_gpio_num = -1;
#endif
#if defined(LCD_BGR) && LCD_BGR != 0
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    lcd_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#else
    lcd_config.color_space = ESP_LCD_COLOR_SPACE_BGR;
#endif
#else
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    lcd_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
#else
    lcd_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
#endif
#endif
#ifdef LCD_BIT_DEPTH
    lcd_config.bits_per_pixel = LCD_BIT_DEPTH;
#else
    lcd_config.bits_per_pixel = 16;
#endif

    // Initialize the LCD configuration
    LCD_PANEL(io_handle, &lcd_config, &lcd_handle);

    // Reset the display
    esp_lcd_panel_reset(lcd_handle);

    // Initialize LCD panel
    esp_lcd_panel_init(lcd_handle);
#ifdef LCD_GAP_X
    static constexpr int lcd_gap_x = LCD_GAP_X;
#else
    static constexpr int lcd_gap_x = 0;
#endif
#ifdef LCD_GAP_Y
    static constexpr int lcd_gap_y = LCD_GAP_Y;
#else
    static constexpr int lcd_gap_y = 0;
#endif
    esp_lcd_panel_set_gap(lcd_handle, lcd_gap_x, lcd_gap_y);
#ifdef LCD_SWAP_XY
    esp_lcd_panel_swap_xy(lcd_handle, LCD_SWAP_XY);
#endif
#ifdef LCD_MIRROR_X
    static constexpr int lcd_mirror_x = LCD_MIRROR_X;
#else
    static constexpr int lcd_mirror_x = 0;
#endif
#ifdef LCD_MIRROR_Y
    static constexpr int lcd_mirror_y = LCD_MIRROR_Y;
#else
    static constexpr int lcd_mirror_y = 0;
#endif
    esp_lcd_panel_mirror(lcd_handle, lcd_mirror_x, lcd_mirror_y);
#ifdef LCD_INVERT_COLOR
    esp_lcd_panel_invert_color(lcd_handle, LCD_INVERT_COLOR);
#endif

    // Turn on the screen
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#else
    esp_lcd_panel_disp_off(lcd_handle, false);
#endif
#endif
#if defined(LCD_BL) && LCD_BL > 1
    gpio_set_level((gpio_num_t)LCD_BL, bl_on);
#endif
    
#ifdef LCD_SPI_MASTER
    lcd.on_flush_callback(
        [](const rect16& bounds, const void* bmp, void* state) {
            int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2,
                y2 = bounds.y2;
            lcd_flush(x1,y1,x2,y2,bmp);
        },
    nullptr);
#else
    lcd.on_flush_callback(
        [](const rect16& bounds, const void* bmp, void* state) {
            int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1,
                y2 = bounds.y2 + 1;
            esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)state, x1, y1, x2,
                                      y2, (void*)bmp);
                                    },
        lcd_handle);
#endif
    lcd.on_touch_callback(
        [](point16* out_locations, size_t* in_out_locations_size, void* state) {
            touch.update();
            // UIX supports multiple touch points.
            // so does the FT6336 so we potentially have
            // two values
            *in_out_locations_size = 0;
            uint16_t x, y;
            if (touch.xy(&x, &y)) {
                out_locations[0] = point16(x, y);
                ++*in_out_locations_size;
                if (touch.xy2(&x, &y)) {
                    out_locations[1] = point16(x, y);
                    ++*in_out_locations_size;
                }
            }
        });
    touch.initialize();
#ifdef FREENOVE_DEVKIT
    touch.rotation(3);
#else
    touch.rotation(0);
#endif
    
}
void display_update() {
    lcd.update();
}
void display_screen(void* screen) {
    if(screen!=nullptr) {
        lcd.active_screen(*(uix::screen_base*)screen);
    }
}