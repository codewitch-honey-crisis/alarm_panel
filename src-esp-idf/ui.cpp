#include "ui.hpp"
#include "i2c.hpp"
#include "spi.hpp"
#include <stdio.h>

#include <esp_i2c.hpp>  // i2c initialization
#include <ft6336.hpp>
#include <gfx.hpp>
#include <uix.hpp>

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
#define SHND ((SemaphoreHandle_t)alarm_sync)

// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "assets/OpenSans_Regular.h"  // our font
#define LEFT_ARROW_IMPLEMENTATION
#include "assets/left_arrow.h"
#define RIGHT_ARROW_IMPLEMENTATION
#include "assets/right_arrow.h"
#define ICONS_IMPLEMENTATION
#include "assets/icons.hpp"

#include "alarm_common.hpp"
#include "serial.hpp"
#include "power.hpp"
using namespace gfx;      // graphics
using namespace uix;      // user interface
using namespace esp_idf;  // devices

using color_t = color<rgb_pixel<16>>;     // screen color
using color32_t = color<rgba_pixel<32>>;  // UIX color

// fonts load from streams, so wrap our array in one
static const_buffer_stream font_stream(OpenSans_Regular,
                                       sizeof(OpenSans_Regular));
static tt_font text_font;

static const_buffer_stream left_stream(left_arrow, sizeof(left_arrow));
static const_buffer_stream right_stream(right_arrow, sizeof(right_arrow));

using screen_t = uix::screen<rgb_pixel<LCD_BIT_DEPTH>>;
using surface_t = screen_t::control_surface_type;

template <typename ControlSurfaceType>
class arrow_box : public control<ControlSurfaceType> {
    using base_type = control<ControlSurfaceType>;

   public:
    typedef void (*on_pressed_changed_callback_type)(bool pressed, void* state);

   private:
    bool m_pressed;
    bool m_dirty;
    sizef m_svg_size;
    matrix m_fit;
    on_pressed_changed_callback_type m_on_pressed_changed_callback;
    void* m_on_pressed_changed_callback_state;
    stream* m_svg;
    static rectf correct_aspect(srect16& sr, float aspect) {
        if (sr.width() > sr.height()) {
            sr.y2 /= aspect;
        } else {
            sr.x2 *= aspect;
        }
        return (rectf)sr;
    }

   public:
    arrow_box()
        : base_type(),
          m_pressed(false),
          m_dirty(true),
          m_on_pressed_changed_callback(nullptr),
          m_svg(nullptr) {}
    void svg(stream& svg_stream) {
        m_svg = &svg_stream;
        m_dirty = true;
        this->invalidate();
    }
    stream& svg() const { return *m_svg; }
    bool pressed() const { return m_pressed; }
    void on_pressed_changed_callback(on_pressed_changed_callback_type callback,
                                     void* state = nullptr) {
        m_on_pressed_changed_callback = callback;
        m_on_pressed_changed_callback_state = state;
    }

   protected:
    virtual void on_before_paint() override {
        if (m_dirty) {
            m_svg_size = {0.f, 0.f};
            if (m_svg != nullptr) {
                m_svg->seek(0);
                canvas::svg_dimensions(*m_svg, &m_svg_size);
                srect16 sr = this->dimensions().bounds();
                ssize16 dim = this->dimensions();
                const float xo = dim.width / 8;
                const float yo = dim.height / 8;
                rectf corrected = correct_aspect(sr, m_svg_size.aspect_ratio())
                                      .inflate(-xo, -yo);
                m_fit = matrix::create_fit_to(
                    m_svg_size,
                    corrected.offset((dim.width - corrected.width()) * .5f +
                                         (xo * m_pressed),
                                     (dim.height - corrected.height()) * .5f +
                                         (yo * m_pressed)));
            }
            m_dirty = false;
        }
    }
    virtual void on_paint(ControlSurfaceType& dst,
                          const srect16& clip) override {
        if (m_dirty || m_svg == nullptr) {
            puts("Paint not ready");
            return;
        }
        canvas cvs((size16)this->dimensions());
        cvs.initialize();
        draw::canvas(dst, cvs);
        m_svg->seek(0);
        if (gfx_result::success != cvs.render_svg(*m_svg, m_fit)) {
            puts("SVG render error");
        }
    }
    virtual bool on_touch(size_t locations_size,
                          const spoint16* locations) override {
        if (!m_pressed) {
            m_pressed = true;
            if (m_on_pressed_changed_callback != nullptr) {
                m_on_pressed_changed_callback(
                    true, m_on_pressed_changed_callback_state);
            }
            m_dirty = true;
            this->invalidate();
        }
        return true;
    }
    virtual void on_release() override {
        if (m_pressed) {
            m_pressed = false;
            if (m_on_pressed_changed_callback != nullptr) {
                m_on_pressed_changed_callback(
                    false, m_on_pressed_changed_callback_state);
            }
            m_dirty = true;
            this->invalidate();
        }
    }
};

static uix::display lcd;

#ifdef LCD_SPI_MASTER
static void lcd_on_flush_complete() {
    lcd.flush_complete();
}
#endif
// initialize the screen using the esp panel API
static void lcd_init() {
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
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
    lcd.buffer2(lcd_transfer_buffer2);
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

using button_t = vbutton<surface_t>;
using switch_t = vswitch<surface_t>;
using label_t = label<surface_t>;
using qr_t = qrcode<surface_t>;
using arrow_t = arrow_box<surface_t>;
using painter_t = painter<surface_t>;

static screen_t main_screen;
static arrow_t left_button;
static arrow_t right_button;
static button_t reset_all;
static button_t web_link;
static painter_t battery_icon;

static constexpr const size_t switches_count =
    math::min(alarm_count, (size_t)(LCD_WIDTH / 40) / 2);
static switch_t switches[switches_count];
static label_t switch_labels[switches_count];
static char switch_text[switches_count][6];
static size_t switch_index = 0;
static bool switches_updating = false;
static screen_t qr_screen;
static qr_t qr_link;
static button_t qr_return;

void ui_update_switches(bool lock) {
    if (lock && alarm_sync != nullptr) {
        xSemaphoreTake(SHND, portMAX_DELAY);
    }
    switches_updating = true;
    for (size_t i = 0; i < switches_count; ++i) {
        itoa(1 + i + switch_index, switch_text[i], 10);
        switch_labels[i].text(switch_text[i]);
        switches[i].value(alarm_values[i + switch_index]);
    }
    left_button.visible(switch_index != 0);
    right_button.visible(switch_index < alarm_count - switches_count);
    switches_updating = false;
    if (lock && alarm_sync != nullptr) {
        xSemaphoreGive(SHND);
    }
}

void ui_init() {
    lcd_init();
    main_screen.dimensions({LCD_WIDTH, LCD_HEIGHT});
    main_screen.background_color(color_t::black);

    left_button.bounds(srect16(0, 0, LCD_WIDTH / 8.77f, LCD_WIDTH / 8)
                           .center_vertical(main_screen.bounds())
                           .offset(LCD_WIDTH / 53, 0));
    left_button.svg(left_stream);
    left_button.on_pressed_changed_callback([](bool pressed, void* state) {
        if (pressed) {
            if (switch_index > 0) {
                --switch_index;
                // we're already locking from just outside lcd.update()
                ui_update_switches(false);
            }
        }
    });
    left_button.visible(false);
    main_screen.register_control(left_button);
    right_button.bounds(srect16(0, 0, LCD_WIDTH / 8.77f, LCD_WIDTH / 8)
                            .center_vertical(main_screen.bounds())
                            .offset(main_screen.bounds().x2 -
                                        left_button.dimensions().width -
                                        LCD_WIDTH / 53,
                                    0));
    right_button.svg(right_stream);
    right_button.visible(alarm_count > switches_count);
    right_button.on_pressed_changed_callback([](bool pressed, void* state) {
        if (pressed) {
            if (switch_index < alarm_count - switches_count) {
                ++switch_index;
                // we're already locking from just outside lcd.update()
                ui_update_switches(false);
            }
        }
    });
    main_screen.register_control(right_button);

    srect16 sr(0, 0, main_screen.dimensions().width / 2,
               main_screen.dimensions().width / 8);
    reset_all.bounds(sr.offset(0, main_screen.dimensions().height - sr.y2 - 1)
                         .center_horizontal(main_screen.bounds()));
    reset_all.back_color(color32_t::dark_red);
    reset_all.color(color32_t::black);
    reset_all.border_color(color32_t::dark_gray);
    reset_all.font(font_stream);
    reset_all.font_size(sr.height() - 4);
    reset_all.text("Reset all");
    reset_all.radiuses({5, 5});
    reset_all.on_pressed_changed_callback([](bool pressed, void* state) {
        if (pressed) {
            for (size_t i = 0; i < alarm_count; ++i) {
                alarm_enable(i, false);
            }
            switches_updating = true;
            for (size_t i = 0; i < switches_count; ++i) {
                switches[i].value(false);
            }
            switches_updating = false;
        }
    });
    main_screen.register_control(reset_all);
    web_link.bounds(sr.offset(0, main_screen.dimensions().height - sr.y2 - 1)
                        .offset(reset_all.dimensions().width, 0));
    web_link.back_color(color32_t::light_blue);
    web_link.color(color32_t::dark_blue);
    web_link.border_color(color32_t::dark_gray);
    web_link.font(font_stream);
    web_link.font_size(sr.height() - 4);
    web_link.text("QR Link");
    web_link.radiuses({5, 5});
    web_link.on_pressed_changed_callback([](bool pressed, void* state) {
        if (!pressed) {
            lcd.active_screen(qr_screen);
        }
    });
    web_link.visible(false);
    main_screen.register_control(web_link);

    text_font = tt_font(font_stream, main_screen.dimensions().height / 6,
                        font_size_units::px);
    text_font.initialize();
    char sz[16];
    itoa(alarm_count, sz, 10);
    text_info ti(sz, text_font);
    size16 area;
    // measure the size of the largest number and set all the text labels to
    // that width:
    text_font.measure((uint16_t)-1, ti, &area);
    sr = srect16(0, 0, main_screen.dimensions().width / 8,
                 main_screen.dimensions().height / 3);
    const uint16_t swidth = math::max(area.width, (uint16_t)sr.width());
    const uint16_t total_width = swidth * switches_count;
    const uint16_t xofs = (main_screen.dimensions().width - total_width) / 2;
    const uint16_t yofs = main_screen.dimensions().height / 12;
    uint16_t x = 0;
    // init the fire switch controls + labels
    for (size_t i = 0; i < switches_count; ++i) {
        const uint16_t sofs = (swidth - sr.width()) / 2;
        switch_t& s = switches[i];
        s.bounds(srect16(x + xofs + sofs, yofs,
                         x + xofs + sr.width() - 1 + sofs, yofs + sr.height()));
        s.back_color(color32_t::dark_blue);
        s.border_color(color32_t::dark_gray);
        s.knob_color(color32_t::white);
        s.knob_border_color(color32_t::dark_gray);
        s.knob_border_width(1);
        s.border_width(1);
        s.radiuses({10, 10});
        s.orientation(uix_orientation::vertical);
        s.on_value_changed_callback(
            [](bool value, void* state) {
                if (!switches_updating) {
                    switch_t* psw = (switch_t*)state;
                    const size_t i = (size_t)(psw - switches) + switch_index;
                    alarm_enable(i, value);
                }
            },
            &s);
        main_screen.register_control(s);
        itoa(i + 1, switch_text[i], 10);
        label_t& l = switch_labels[i];
        l.text(switch_text[i]);
        l.bounds(srect16(x + xofs, yofs + sr.height() + 1,
                         x + xofs + swidth - 1,
                         yofs + sr.height() + area.height));
        l.font(text_font);
        l.color(color32_t::white);
        l.text_justify(uix_justify::top_middle);
        l.padding({0, 0});
        main_screen.register_control(l);
        x += swidth + 2;
    }
    // set up a custom canvas for displaying our battery icon
    battery_icon.bounds(
        (srect16)faBatteryEmpty.dimensions().bounds());
    battery_icon.on_paint_callback([](surface_t& destination, 
                                const srect16& clip, 
                                void* state) {
        // show in green if it's on ac power.
        const int pct = power_battery_level();
        auto px = color_t::white;
        if(!power_ac() && pct<25) {
            px=color_t::red;
        }
        if(!power_ac()) {
            puts("BATTERY");
            // draw an empty battery
            draw::icon(destination,point16::zero(),faBatteryEmpty,px);
            // now fill it up
            if(pct==100) {
                // if we're at 100% fill the entire thing
                draw::filled_rectangle(destination,rect16(3,7,22,16),px);
            } else {
                // otherwise leave a small border
                draw::filled_rectangle(destination,rect16(4,9,4+(0.18f*pct),14),px);
            }
        }
    });
    main_screen.register_control(battery_icon);
    
    // initialize the QR screen
    qr_screen.dimensions(main_screen.dimensions());
    // initialize the controls
    sr = srect16(0, 0, qr_screen.dimensions().width / 2,
                 qr_screen.dimensions().width / 8);
    qr_link.bounds(srect16(0, 0, qr_screen.dimensions().width / 2,
                           qr_screen.dimensions().width / 2)
                       .center_horizontal(qr_screen.bounds()));
    qr_link.text("about:blank");
    qr_screen.register_control(qr_link);
    qr_return.bounds(
        sr.center_horizontal(qr_screen.bounds())
            .offset(0, qr_screen.dimensions().height - sr.height()));
    qr_return.back_color(color32_t::gray);
    qr_return.color(color32_t::white);
    qr_return.border_color(color32_t::dark_gray);
    qr_return.font(font_stream);
    qr_return.font_size(sr.height() - 4);
    qr_return.text("Main screen");
    qr_return.radiuses({5, 5});
    qr_return.on_pressed_changed_callback([](bool pressed, void* state) {
        if (!pressed) {
            lcd.active_screen(main_screen);
        }
    });
    qr_screen.register_control(qr_return);
    // set the display to our main screen
    lcd.active_screen(main_screen);
}

void ui_update() {
    // update the display and battery info
    if (SHND != nullptr) {
        xSemaphoreTake(SHND, portMAX_DELAY);
    }
    static bool ac_in = power_ac();
    if(power_ac()!=ac_in) {
        ac_in = power_ac();
        battery_icon.invalidate();
    }
    lcd.update();
    if (SHND != nullptr) {
        xSemaphoreGive(SHND);
    }
}
bool ui_web_link() { return web_link.visible(); }
void ui_web_link(const char* addr) {
    if (addr != nullptr) {
        // move the "Reset all" button to the left
        const int16_t diff = -reset_all.bounds().x1;
        reset_all.bounds(reset_all.bounds().offset(diff, 0));
        qr_link.text(addr);
        // now show the link
        web_link.visible(true);
    } else {
        // we disconnected for some reason
        // if it's not the main screen, set it to the main screen
        if (&lcd.active_screen() != &main_screen) {
            lcd.active_screen(main_screen);
        }
        // hide the QR Link button
        web_link.visible(false);
        // center the "Reset all" button
        reset_all.bounds(
            reset_all.bounds().center_horizontal(main_screen.bounds()));
    }
}