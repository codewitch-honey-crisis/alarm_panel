#include "display.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <memory.h>
#include <driver/i2c_master.h>
#include "i2c.h"
#ifdef LCD_PIN_NUM_HSYNC
#include "esp_lcd_panel_rgb.h"
#else
#include "spi.h"
#include "driver/spi_master.h"
#endif
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "esp_lcd_touch.h"
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
static esp_lcd_panel_handle_t lcd_handle = NULL;
#endif
esp_lcd_touch_handle_t touch_handle = NULL;
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


#ifdef LCD_SPI_MASTER
static void lcd_on_flush_complete() {
    display_flush_complete();
}
#else
#ifndef LCD_PIN_NUM_HSYNC
static bool display_flush_cb(esp_lcd_panel_io_handle_t lcd_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx) {
    display_flush_complete();
    return true;
}
#endif
#endif
#ifdef WAVESHARE_ESP32S3_43
static void display_touch_pre_reset(void)
{
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_master_get_bus_handle((i2c_port_num_t)I2C_PORT,&bus));
    i2c_master_dev_handle_t i2c=NULL;
    i2c_device_config_t dev_cfg;
    memset(&dev_cfg,0,sizeof(dev_cfg));
    dev_cfg.scl_speed_hz = 200*1000;
    dev_cfg.device_address = 0x24;
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg,&i2c));
    uint8_t write_buf = 0x01;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c,&write_buf,1,1000));
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(i2c));
    dev_cfg.device_address = 0x38;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg,&i2c));
    // Reset the touch screen. It is recommended to reset the touch screen before using it.
    write_buf = 0x2C;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c,&write_buf,1,1000 ));
    esp_rom_delay_us(100 * 1000);
    gpio_set_level((gpio_num_t)4, 0);
    esp_rom_delay_us(100 * 1000);
    write_buf = 0x2E;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c,&write_buf,1,1000));
    esp_rom_delay_us(200 * 1000);
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(i2c));
}
#else
static void display_touch_pre_reset()
{
}
#endif
// initialize the screen 
int display_init() {
#ifndef LCD_SPI_MASTER
    esp_lcd_panel_io_handle_t io_handle = NULL;
#ifndef LCD_PIN_NUM_HSYNC
    esp_lcd_panel_io_spi_config_t io_config;
    esp_lcd_panel_dev_config_t lcd_config;
#endif
#endif
    esp_lcd_panel_io_i2c_config_t tio_cfg;
    esp_lcd_panel_io_handle_t tio_handle;
    i2c_master_bus_handle_t i2c_handle;
    esp_lcd_touch_config_t tp_cfg;
    
    // for the touch panel

#if defined(LCD_BL) && LCD_BL > 1
#ifdef LCD_BL_LOW
    static const int bl_on = !(LCD_BL_LOW);
#else
    static const int bl_on = 1;
#endif
    gpio_set_direction((gpio_num_t)LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LCD_BL, !bl_on);
#endif
#ifdef LCD_SPI_MASTER
    lcd_init_impl();
#else
#ifdef LCD_PIN_NUM_HSYNC
    esp_lcd_rgb_panel_config_t panel_config;
    memset(&panel_config,0,sizeof(esp_lcd_rgb_panel_config_t));
    
    panel_config.data_width = 16; // RGB565 in parallel mode, thus 16bit in width
    //panel_config.dma_burst_size = 64;
    panel_config.num_fbs = 1,
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT,
    panel_config.disp_gpio_num = -1,
    panel_config.pclk_gpio_num = LCD_PIN_NUM_CLK,
    panel_config.vsync_gpio_num = LCD_PIN_NUM_VSYNC,
    panel_config.hsync_gpio_num = LCD_PIN_NUM_HSYNC,
    panel_config.de_gpio_num = LCD_PIN_NUM_DE,
    panel_config.data_gpio_nums[0]=LCD_PIN_NUM_D00;
    panel_config.data_gpio_nums[1]=LCD_PIN_NUM_D01;
    panel_config.data_gpio_nums[2]=LCD_PIN_NUM_D02;
    panel_config.data_gpio_nums[3]=LCD_PIN_NUM_D03;
    panel_config.data_gpio_nums[4]=LCD_PIN_NUM_D04;
    panel_config.data_gpio_nums[5]=LCD_PIN_NUM_D05;
    panel_config.data_gpio_nums[6]=LCD_PIN_NUM_D06;
    panel_config.data_gpio_nums[7]=LCD_PIN_NUM_D07;
    panel_config.data_gpio_nums[8]=LCD_PIN_NUM_D08;
    panel_config.data_gpio_nums[9]=LCD_PIN_NUM_D09;
    panel_config.data_gpio_nums[10]=LCD_PIN_NUM_D10;
    panel_config.data_gpio_nums[11]=LCD_PIN_NUM_D11;
    panel_config.data_gpio_nums[12]=LCD_PIN_NUM_D12;
    panel_config.data_gpio_nums[13]=LCD_PIN_NUM_D13;
    panel_config.data_gpio_nums[14]=LCD_PIN_NUM_D14;
    panel_config.data_gpio_nums[15]=LCD_PIN_NUM_D15;

    memset(&panel_config.timings,0,sizeof(esp_lcd_rgb_timing_t));
    
    panel_config.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    panel_config.timings.h_res = LCD_HRES;
    panel_config.timings.v_res = LCD_VRES;
    panel_config.timings.hsync_back_porch = LCD_HSYNC_BACK_PORCH;
    panel_config.timings.hsync_front_porch = LCD_HSYNC_FRONT_PORCH;
    panel_config.timings.hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH;
    panel_config.timings.vsync_back_porch = LCD_VSYNC_BACK_PORCH;
    panel_config.timings.vsync_front_porch = LCD_VSYNC_FRONT_PORCH;
    panel_config.timings.vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH;
    panel_config.timings.flags.pclk_active_neg = true;
    panel_config.timings.flags.hsync_idle_low = false;
    panel_config.timings.flags.pclk_idle_high = LCD_CLK_IDLE_HIGH;
    panel_config.timings.flags.de_idle_high = LCD_DE_IDLE_HIGH;
    panel_config.timings.flags.vsync_idle_low = false;
    panel_config.flags.bb_invalidate_cache = true;
    panel_config.flags.disp_active_low = false;
    panel_config.flags.double_fb = false;
    panel_config.flags.no_fb = false;
    panel_config.flags.refresh_on_demand = false;
    panel_config.flags.fb_in_psram = true; // allocate frame buffer in PSRAM
    //panel_config.sram_trans_align = 4;
    //panel_config.psram_trans_align = 64;
    panel_config.num_fbs = 2;
#ifdef LCD_BOUNCE_HEIGHT
    panel_config.bounce_buffer_size_px = LCD_HRES*LCD_BOUNCE_HEIGHT;
#endif
    if(ESP_OK!=esp_lcd_new_rgb_panel(&panel_config, &lcd_handle)) {
        goto error;
    }
    if(ESP_OK!=esp_lcd_panel_reset(lcd_handle)) {
        goto error;
    }
    if(ESP_OK!=esp_lcd_panel_init(lcd_handle)) {
        goto error;
    }
#else
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
    io_config.on_color_trans_done = display_flush_cb;
    // Attach the LCD to the SPI bus
    if(ESP_OK!=esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_PORT, &io_config,
                             &io_handle)) {
        goto error;
    }
    memset(&lcd_config, 0, sizeof(lcd_config));
#ifdef LCD_RST
    lcd_config.reset_gpio_num = LCD_RST;
#else
    lcd_config.reset_gpio_num = -1;
#endif
#if defined(LCD_BGR) && LCD_BGR != 0
    lcd_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
#else
    lcd_config.rgb_endian = LCD_RGB_ENDIAN_RGB;
#endif
#ifdef LCD_BIT_DEPTH
    lcd_config.bits_per_pixel = LCD_BIT_DEPTH;
#else
    lcd_config.bits_per_pixel = 16;
#endif

    // Initialize the LCD configuration
    if(ESP_OK!=LCD_PANEL(io_handle, &lcd_config, &lcd_handle)) {
        goto error;
    }

    // Reset the display
    if(ESP_OK!=esp_lcd_panel_reset(lcd_handle)) {
        goto error;
    }

    // Initialize LCD panel
    if(ESP_OK!=esp_lcd_panel_init(lcd_handle)) {
        goto error;
    }
#ifdef LCD_GAP_X
    static const unsigned int lcd_gap_x = LCD_GAP_X;
#else
    static const unsigned int lcd_gap_x = 0;
#endif
#ifdef LCD_GAP_Y
    static const unsigned int lcd_gap_y = LCD_GAP_Y;
#else
    static unsigned int lcd_gap_y = 0;
#endif
    esp_lcd_panel_set_gap(lcd_handle, lcd_gap_x, lcd_gap_y);
#if defined(LCD_SWAP_XY) && LCD_SWAP_XY == 1
    static const unsigned int lcd_swap_xy = 1;
#else
    static const unsigned int lcd_swap_xy = 0;
#endif
    esp_lcd_panel_swap_xy(lcd_handle, lcd_swap_xy);
#ifdef LCD_MIRROR_X
    static unsigned int lcd_mirror_x = LCD_MIRROR_X;
#else
    static const unsigned int lcd_mirror_x = 0;
#endif
#ifdef LCD_MIRROR_Y
    static const unsigned int lcd_mirror_y = LCD_MIRROR_Y;
#else
    static unsigned int lcd_mirror_y = 0;
#endif
    esp_lcd_panel_mirror(lcd_handle, lcd_mirror_x, lcd_mirror_y);
#ifdef LCD_INVERT_COLOR
    esp_lcd_panel_invert_color(lcd_handle, LCD_INVERT_COLOR);
#endif

    // Turn on the screen
    esp_lcd_panel_disp_on_off(lcd_handle, true);
#endif
#endif
#if defined(LCD_BL) && LCD_BL > 1
    gpio_set_level((gpio_num_t)LCD_BL, bl_on);
#endif
    display_touch_pre_reset();
    memset(&tio_cfg,0,sizeof(tio_cfg));
    tio_cfg.dev_addr = LCD_TOUCH_ADDRESS;
    tio_cfg.control_phase_bytes = LCD_TOUCH_CONTROL_PHASE_BYTES;
    tio_cfg.dc_bit_offset = LCD_TOUCH_DC_OFFSET;
    tio_cfg.lcd_cmd_bits = LCD_TOUCH_CMD_BITS;
    tio_cfg.lcd_param_bits = LCD_TOUCH_PARAM_BITS;
#ifdef LCD_TOUCH_DISABLE_CONTROL_PHASE
    tio_cfg.flags.disable_control_phase = 1;
#endif
    tio_cfg.flags.dc_low_on_data = 0;
    tio_cfg.on_color_trans_done = NULL;
    tio_cfg.scl_speed_hz = LCD_TOUCH_SPEED;
    tio_cfg.user_ctx = NULL;
    if(ESP_OK!=i2c_master_get_bus_handle(I2C_PORT,&i2c_handle)) {
        goto error;
    }
    if(ESP_OK!=esp_lcd_new_panel_io_i2c(i2c_handle, &tio_cfg,&tio_handle)) {
        goto error;
    }
    memset(&tp_cfg,0,sizeof(tp_cfg));
    tp_cfg.x_max = LCD_TOUCH_HRES;
    tp_cfg.y_max = LCD_TOUCH_VRES;
    tp_cfg.rst_gpio_num = (gpio_num_t)LCD_TOUCH_PIN_NUM_RST;
    tp_cfg.int_gpio_num = (gpio_num_t)LCD_TOUCH_PIN_NUM_INT;
    tp_cfg.levels.reset = 0;
    tp_cfg.levels.interrupt = 0;
#ifdef LCD_TOUCH_SWAP_XY
    tp_cfg.flags.swap_xy = 1;
#endif
#ifdef LCD_TOUCH_MIRROR_X
    tp_cfg.flags.mirror_x = 1;
#endif
#ifdef LCD_TOUCH_MIRROR_Y
    tp_cfg.flags.mirror_y = 1;
#endif

    if(ESP_OK!=LCD_TOUCH_PANEL(tio_handle,&tp_cfg,&touch_handle)) {
        goto error;
    }

    return 0;
error:
#ifndef LCD_SPI_MASTER
    if(lcd_handle!=NULL) {
        esp_lcd_panel_del(lcd_handle);
        lcd_handle = NULL;
    }    
    if(io_handle!=NULL) {
        esp_lcd_panel_io_del(io_handle);
    }
#endif
    if(touch_handle!=NULL) {
        esp_lcd_touch_del(touch_handle);
        touch_handle = NULL;
    }
    if(tio_handle!=NULL) {
        esp_lcd_panel_io_del(tio_handle);
    }
    return -1;
}
int display_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const void* bmp) {
#if defined(LCD_PIN_NUM_HSYNC) && defined(LCD_SWAP_COLOR_BYTES)
    uint16_t* data = (uint16_t*)(void*)bmp;
    uint16_t w = x2-x1+1,h=y2-y1+1;
    size_t remaining = w*h;
    while(remaining--) {
        uint16_t tmp = *data;
        tmp = ((tmp & 0xFF00) >> 8) | ((tmp & 0x00FF) << 8);
        *data = tmp;
        ++data;
    }
#endif
#ifdef LCD_SPI_MASTER
    lcd_flush(x1,y1,x2,y2,bmp);
#else
    if(ESP_OK!=esp_lcd_panel_draw_bitmap(lcd_handle, x1, y1, x2+1,y2+1, (void*)bmp)) {
        return -1;
    }
#endif
#ifndef LCD_DMA
    display_flush_complete();
#endif
    return 0;
}
int display_touch_read(uint16_t* out_x_array,uint16_t* out_y_array, uint16_t* out_strength_array, size_t* in_out_touch_count) {
    size_t count = *in_out_touch_count;
    if(touch_handle==NULL || count==0) {return -1;}
    *in_out_touch_count = 0;
    uint8_t tmp;
    if(esp_lcd_touch_get_coordinates(touch_handle,out_x_array,out_y_array,out_strength_array,&tmp,count)) {
        *in_out_touch_count=count;
        static const uint16_t lcd_width = LCD_WIDTH;
        static const uint16_t lcd_height = LCD_HEIGHT;
        static const uint16_t lcd_touch_width = LCD_TOUCH_WIDTH - LCD_TOUCH_OVERHANG_X;
        static const uint16_t lcd_touch_height = LCD_TOUCH_HEIGHT - LCD_TOUCH_OVERHANG_Y;
        if(lcd_width!=lcd_touch_width||lcd_height!=lcd_touch_height) {
            const float xfactor = (float)lcd_width/(float)lcd_touch_width;
            const float yfactor = (float)lcd_height/(float)lcd_touch_height;
            for(int i = 0;i<count;++i) {
                out_x_array[i]*=xfactor;
                out_y_array[i]*=yfactor;
            }
        }
        return 0;
    }
    return 0;
}
void display_update(void) {
    if(touch_handle!=NULL) {
        esp_lcd_touch_read_data(touch_handle);
    }
}