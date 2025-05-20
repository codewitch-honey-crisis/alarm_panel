#include "ui.hpp"

#include <stdio.h>

#include <esp_i2c.hpp>  // i2c initialization
#include <ft6336.hpp>
#include <gfx.hpp>
#include <uix.hpp>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ili9342.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#define SHND ((SemaphoreHandle_t)alarm_sync)

// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "assets/OpenSans_Regular.h"  // our font
#define LEFT_ARROW_IMPLEMENTATION
#include "assets/left_arrow.h"
#define RIGHT_ARROW_IMPLEMENTATION
#include "alarm_common.hpp"
#include "assets/right_arrow.h"
#include "serial.hpp"
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

// initialize the screen using the esp panel API
static void lcd_init() {
    // for the touch panel
    using touch_t = ft6336<320, 280, 16>;
    static touch_t touch(esp_i2c<1, 21, 22>::instance);

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
#if defined(LCD_BL) && LCD_BL > 1
    gpio_set_level((gpio_num_t)LCD_BL, bl_on);
#endif
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
    lcd.buffer2(lcd_transfer_buffer2);
    lcd.on_flush_callback(
        [](const rect16& bounds, const void* bmp, void* state) {
            int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 + 1,
                y2 = bounds.y2 + 1;
            esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)state, x1, y1, x2,
                                      y2, (void*)bmp);
        },
        lcd_handle);
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
    touch.rotation(0);
}

using button_t = vbutton<surface_t>;
using switch_t = vswitch<surface_t>;
using label_t = label<surface_t>;
using qr_t = qrcode<surface_t>;
using arrow_t = arrow_box<surface_t>;

static screen_t main_screen;
static arrow_t left_button;
static arrow_t right_button;
static button_t reset_all;
static button_t web_link;
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
    // update the display and touch device
    if (SHND != nullptr) {
        xSemaphoreTake(SHND, portMAX_DELAY);
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