#pragma once
#define LCD_TRANSFER_KB 64
#if __has_include(<Arduino.h>)
#include <Arduino.h>
#endif
#include <gfx.hpp>
#include "lcd_config.h"
#include <uix.hpp>

// here we compute how many bytes are needed in theory to store the total screen.
constexpr static const size_t lcd_screen_total_size = 
    gfx::bitmap<typename LCD_FRAME_ADAPTER::pixel_type>
        ::sizeof_buffer(LCD_WIDTH,LCD_HEIGHT);
// define our transfer buffer(s) 
// For devices with no DMA we only use one buffer.
// Our total size is either LCD_TRANSFER_KB 
// Or the lcd_screen_total_size - whatever
// is smaller
// Note that in the case of DMA the memory
// is divided between two buffers.

#ifdef LCD_DMA
constexpr static const size_t lcd_buffer_size = (LCD_TRANSFER_KB*512) >
    lcd_screen_total_size?lcd_screen_total_size:(LCD_TRANSFER_KB*512);
extern uint8_t* lcd_buffer1;
extern uint8_t* lcd_buffer2;
#else
#ifdef LCD_PSRAM_BUFFER
constexpr static const size_t lcd_buffer_size = LCD_PSRAM_BUFFER;
#else
constexpr static const size_t lcd_buffer_size = (LCD_TRANSFER_KB*1024) > 
    lcd_screen_total_size?lcd_screen_total_size:(LCD_TRANSFER_KB*1024);
#endif
extern uint8_t* lcd_buffer1;
static uint8_t* const lcd_buffer2 = nullptr;
#endif

// declare the screen type
using screen_t = uix::screen_ex<LCD_FRAME_ADAPTER,LCD_X_ALIGN,LCD_Y_ALIGN>;

// the active screen pointer
extern screen_t* display_active_screen;

// initializes the display
extern void display_init();
// updates the display, redrawing as necessary
extern void display_update();
// switches the active screen
extern void display_screen(screen_t& new_screen);
// puts the LCD to sleep
extern void display_sleep();
// wakes the LCD
extern void display_wake();