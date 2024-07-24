#pragma once
#include "display.hpp"
#include <uix.hpp>
#include <gfx.hpp>
#include "svg_needle.hpp"

// for our controls
using surface_t = screen_t::control_surface_type;
using label_t = uix::label<surface_t>;
using needle_t = svg_needle<surface_t>;
// X11 colors (used for screen)
using color_t = gfx::color<typename screen_t::pixel_type>;
// RGBA8888 X11 colors (used for controls)
using color32_t = gfx::color<gfx::rgba_pixel<32>>;

// the screen that holds the controls
extern screen_t speed_screen;
extern needle_t speed_needle;
extern label_t speed_label;
extern label_t speed_units_label;
extern label_t speed_big_label;
extern label_t speed_big_units_label;

extern screen_t trip_screen;
extern label_t trip_label;
extern label_t trip_units_label;

extern screen_t loc_screen;
extern label_t loc_lat_label;
extern label_t loc_lon_label;
extern label_t loc_alt_label;

extern screen_t stat_screen;
extern label_t stat_sat_label;

extern void ui_init();
