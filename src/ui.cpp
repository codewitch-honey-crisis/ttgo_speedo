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

screen_t main_screen;
needle_t speed_needle(main_screen);
label_t speed_label(main_screen);

// initialize the main screen
void main_screen_init() {
    main_screen.dimensions({LCD_WIDTH,LCD_HEIGHT});
    main_screen.buffer_size(lcd_buffer_size);
    main_screen.buffer1(lcd_buffer1);
    main_screen.buffer2(lcd_buffer2);
    // declare a transparent pixel/color
    rgba_pixel<32> transparent(0, 0, 0, 0);
    // screen is black
    main_screen.background_color(color_t::black);
    speed_needle.bounds(srect16(0,0,127,127).center_vertical(main_screen.bounds()));
    speed_needle.needle_border_color(color32_t::red);
    rgba_pixel<32> nc = color32_t::red;
    nc.opacity(.5);
    speed_needle.needle_color(nc);
    speed_needle.angle(270);
    main_screen.register_control(speed_needle);
    speed_label.text_open_font(&text_font);
    const size_t speed_height = (int)floorf(main_screen.dimensions().height/1.5f);
    speed_label.text_line_height(speed_height);
    srect16 speed_rect = text_font.measure_text(ssize16::max(),spoint16::zero(),"888",text_font.scale(speed_height),0,speed_label.text_encoding()).bounds().center_vertical(main_screen.bounds());
    speed_rect.offset_inplace(main_screen.dimensions().width-speed_rect.width(),0);
    speed_label.text_justify(uix_justify::top_right);
    speed_label.border_color(transparent);
    speed_label.background_color(transparent);
    speed_label.text_color(color32_t::white);
    speed_label.bounds(speed_rect);
    speed_label.text("--");
    main_screen.register_control(speed_label);
    
}
