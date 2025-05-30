#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
#include "display.h"
#include "ui.h"
#include <stdio.h>
#include <gfx.hpp>
#include <uix.hpp>

// font is a TTF/OTF from downloaded from fontsquirrel.com
// converted to a header with https://honeythecodewitch.com/gfx/converter
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "assets/OpenSans_Regular.h"  // our font
#define LEFT_ARROW_IMPLEMENTATION
#include "assets/left_arrow.h"
#define RIGHT_ARROW_IMPLEMENTATION
#include "assets/right_arrow.h"
#define ICONS_IMPLEMENTATION
#include "assets/icons.h"

#include "alarm.h"
#include "power.h"
using namespace gfx;      // graphics
using namespace uix;      // user interface

using color_t = color<rgb_pixel<LCD_BIT_DEPTH>>;     // screen color
using color32_t = color<rgba_pixel<32>>;  // UIX color

const gfx::const_bitmap<gfx::alpha_pixel<8>> faBatteryEmpty({FABATTERYEMPTY_WIDTH,FABATTERYEMPTY_HEIGHT}, faBatteryEmpty_data);

static uix::display lcd;

// fonts load from streams, so wrap our array in one
static const_buffer_stream font_stream(OpenSans_Regular,
                                       sizeof(OpenSans_Regular));
static tt_font text_font;

static const_buffer_stream left_stream(left_arrow, sizeof(left_arrow));
static const_buffer_stream right_stream(right_arrow, sizeof(right_arrow));

using screen_t = uix::screen_ex<LCD_FRAME_ADAPTER,LCD_X_ALIGN,LCD_Y_ALIGN>;
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
    math::min_(alarm_count, (size_t)(LCD_WIDTH / 40) / 2);
static switch_t switches[switches_count];
static label_t switch_labels[switches_count];
static char switch_text[switches_count][6];
static size_t switch_index = 0;
static bool switches_updating = false;
static screen_t qr_screen;
static qr_t qr_link;
static button_t qr_return;

void ui_update_switches(char lock) {
    if (lock ) {
        alarm_lock();
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
    if (lock) {
        alarm_unlock();
    }
}

int ui_init() {

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
#ifdef LCD_PSRAM_BUFFER
        (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size,MALLOC_CAP_SPIRAM);
#else
        (uint8_t*)malloc(lcd_transfer_buffer_size);
#endif
    if (lcd_transfer_buffer1 == nullptr) {
        puts("Out of memory allocating transfer buffers");
        return -1;
    }
#ifdef LCD_DMA
    uint8_t* lcd_transfer_buffer2 =
#ifdef LCD_PSRAM_BUFFER
        (uint8_t*)heap_caps_malloc(lcd_transfer_buffer_size,MALLOC_CAP_SPIRAM);
#else
        (uint8_t*)malloc(lcd_transfer_buffer_size);
#endif
    if (lcd_transfer_buffer2 == nullptr) {
        free(lcd_transfer_buffer1);
        puts("Out of memory allocating transfer buffers");
        return -1;
    }
#endif
    lcd.buffer_size(lcd_transfer_buffer_size);
    lcd.buffer1(lcd_transfer_buffer1);
#ifdef LCD_DMA
    lcd.buffer2(lcd_transfer_buffer2);
#endif
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
    web_link.color(color32_t::black);
    web_link.border_color(color32_t::dark_gray);
    web_link.font(font_stream);
    web_link.font_size(sr.height() - 4);
    web_link.text("QR Link");
    web_link.radiuses({5, 5});
    web_link.on_pressed_changed_callback([](bool pressed, void* state) {
        if (!pressed) {
            ui_screen(&qr_screen);
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
    const uint16_t swidth = math::max_(area.width, (uint16_t)sr.width());
    const uint16_t total_width = swidth * switches_count;
    const uint16_t xofs = (main_screen.dimensions().width - total_width) / 2;
    const uint16_t yofs = main_screen.dimensions().height / 12;
    uint16_t x = 0;
    // init the switch controls + labels
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
    // set up a custom painter for displaying our battery icon
    battery_icon.bounds(
        (srect16)faBatteryEmpty.dimensions().bounds());
    battery_icon.on_paint_callback([](surface_t& destination, 
                                const srect16& clip, 
                                void* state) {
        const int pct = power_battery_level();
        auto px = color_t::white;
        if(!power_ac() && pct<25) {
            px=color_t::red;
        }
        // don't show if it's on ac power.
        if(!power_ac()) {
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
    qr_link.bounds(srect16(0, 0, qr_screen.dimensions().width / 3,
                           qr_screen.dimensions().width / 3)
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
            ui_screen(&main_screen);
        }
    });
    qr_screen.register_control(qr_return);
    lcd.on_flush_callback(
        [](const rect16& bounds, const void* bmp, void* state) {
            int x1 = bounds.x1, y1 = bounds.y1, x2 = bounds.x2 ,
                y2 = bounds.y2;
            if(display_flush(x1, y1, x2,y2, bmp)) {
                lcd.flush_complete(); // make sure this fires on error
            }
        });                                    

    lcd.on_touch_callback(
        [](point16* out_locations, size_t* in_out_locations_size, void* state) {
            // UIX supports multiple touch points.
            // so does the FT6336 so we potentially have
            // two values
            *in_out_locations_size = 0;
            uint16_t x[2];
            uint16_t y[2];
            uint16_t strength[2];
            size_t count = 2;
            if(0==display_touch_read(x, y, strength, &count) && count>0) {
                *in_out_locations_size = count;
            }
            if(count>0) {
                out_locations[0] = point16(x[0], y[0]);
                ++*in_out_locations_size;
            }
            if(count>1) {
                out_locations[1] = point16(x[1], y[1]);
                ++*in_out_locations_size;
            }
        });

    // set the display to our main screen
    ui_screen(&main_screen);
    return 0;
}
void display_flush_complete() {
    lcd.flush_complete();
}
void ui_update() {
    alarm_lock();
    display_update();
    alarm_unlock();
    // update the battery info
    static bool ac_in = power_ac();
    int bat_cmp = power_battery_level();
    if(power_ac()!=(int)ac_in) {
        ac_in = power_ac();
        battery_icon.invalidate();
    }
    static int bat_pct =0;
    if(!ac_in && bat_pct!=bat_cmp) {
        bat_pct =  power_battery_level();
        battery_icon.invalidate();
    }
    alarm_lock();
    lcd.update();
    alarm_unlock();
}
int ui_web_link_visible() { return web_link.visible(); }
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
        ui_screen(&main_screen);
        
        // hide the QR Link button
        web_link.visible(false);
        // center the "Reset all" button
        reset_all.bounds(
            reset_all.bounds().center_horizontal(main_screen.bounds()));
    }
}
void ui_screen(void* screen) {
    if(screen!=nullptr) {
        lcd.active_screen(*(uix::screen_base*)screen);
    }
}