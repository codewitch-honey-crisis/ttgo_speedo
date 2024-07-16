// required to implort the icon implementation
// must appear only once in the project
//#define DISCONNECTED_ICON_IMPLEMENTATION
//#include <disconnected_icon.hpp>
// our font for the UI. 
#define OPENSANS_REGULAR_IMPLEMENTATION
#include "fonts/OpenSans_Regular.hpp"
#include <ui.hpp>
// for easier modification
const gfx::open_font& text_font = OpenSans_Regular;

using namespace uix;
using namespace gfx;

screen_t speed_screen;
needle_t speed_needle(speed_screen);
label_t speed_label(speed_screen);

screen_t trip_screen;
label_t trip_label(trip_screen);

screen_t stat_screen;
label_t stat_sat_label(stat_screen);

// initialize the main screen
void ui_init() {
    speed_screen.dimensions({LCD_WIDTH,LCD_HEIGHT});
    speed_screen.buffer_size(lcd_buffer_size);
    speed_screen.buffer1(lcd_buffer1);
    speed_screen.buffer2(lcd_buffer2);
    // declare a transparent pixel/color
    rgba_pixel<32> transparent(0, 0, 0, 0);
    // screen is black
    speed_screen.background_color(color_t::black);
    speed_needle.bounds(srect16(0,0,127,127).center_vertical(speed_screen.bounds()));
    speed_needle.needle_border_color(color32_t::red);
    rgba_pixel<32> nc = color32_t::red;
    nc.opacity(.5);
    speed_needle.needle_color(nc);
    speed_needle.angle(270);
    speed_screen.register_control(speed_needle);
    speed_label.text_open_font(&text_font);
    const size_t text_height = (int)floorf(speed_screen.dimensions().height/1.5f);
    speed_label.text_line_height(text_height);
    srect16 speed_rect = text_font.measure_text(ssize16::max(),spoint16::zero(),"888",text_font.scale(text_height),0,speed_label.text_encoding()).bounds().center_vertical(speed_screen.bounds());
    speed_rect.offset_inplace(speed_screen.dimensions().width-speed_rect.width(),0);
    speed_label.text_justify(uix_justify::top_right);
    speed_label.border_color(transparent);
    speed_label.background_color(transparent);
    speed_label.text_color(color32_t::white);
    speed_label.bounds(speed_rect);
    speed_label.text("--");
    speed_screen.register_control(speed_label);

    trip_screen.dimensions({LCD_WIDTH,LCD_HEIGHT});
    trip_screen.buffer_size(lcd_buffer_size);
    trip_screen.buffer1(lcd_buffer1);
    trip_screen.buffer2(lcd_buffer2);
    trip_label.text_open_font(&text_font);
    trip_label.text_justify(uix_justify::top_right);
    trip_label.text_line_height(text_height);
    trip_label.padding({10,0});
    trip_label.background_color(transparent);
    trip_label.border_color(transparent);
    trip_label.text_color(color32_t::orange);
    trip_label.bounds(srect16(0,0,trip_screen.bounds().x2,text_height+1));
    trip_label.text("0.00");
    trip_screen.register_control(trip_label);

    stat_screen.dimensions({LCD_WIDTH,LCD_HEIGHT});
    stat_screen.buffer_size(lcd_buffer_size);
    stat_screen.buffer1(lcd_buffer1);
    stat_screen.buffer2(lcd_buffer2);
    stat_sat_label.text_open_font(&text_font);
    stat_sat_label.text_justify(uix_justify::top_middle);
    stat_sat_label.text_line_height(text_height/2);
    stat_sat_label.padding({10,0});
    stat_sat_label.background_color(transparent);
    stat_sat_label.border_color(transparent);
    stat_sat_label.text_color(color32_t::light_blue);
    stat_sat_label.bounds(srect16(0,0,stat_screen.bounds().x2,text_height+1));
    stat_sat_label.text("-/- sats");
    stat_screen.register_control(stat_sat_label);
}
