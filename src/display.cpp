#define LCD_IMPLEMENTATION
#include <lcd_init.h>
#include <display.hpp>


// our transfer buffers
// For screens with no DMA we only 
// have one buffer
#ifdef LCD_DMA
uint8_t* lcd_buffer1=nullptr;
uint8_t* lcd_buffer2=nullptr;
#else
uint8_t* lcd_buffer1=nullptr;
#endif

// the active screen
screen_t* display_active_screen = nullptr;

// whether the display is sleeping
static bool display_sleeping = false;

#ifdef LCD_DMA
// only needed if DMA enabled
static bool lcd_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                            esp_lcd_panel_io_event_data_t* edata, 
                            void* user_ctx) {
    if(display_active_screen!=nullptr) {
        display_active_screen->flush_complete();
    }
    return true;
}
#endif
static void uix_flush(const gfx::rect16& bounds, 
                    const void* bmp, 
                    void* state) {
    puts("flushing");
    lcd_panel_draw_bitmap(bounds.x1,bounds.y1,bounds.x2,bounds.y2,(void*)bmp);
    // no DMA, so we are done once the above completes
#ifndef LCD_DMA
    if(active_screen!=nullptr) {
        active_screen->flush_complete();
    }
#endif
}
void display_init() {
    lcd_buffer1 = (uint8_t*)heap_caps_malloc(lcd_buffer_size,MALLOC_CAP_DMA);
    if(lcd_buffer1==nullptr) {
        puts("Error allocating LCD buffer 1");
        while(1);
    }
#ifdef LCD_DMA
    lcd_buffer2 = (uint8_t*)heap_caps_malloc(lcd_buffer_size,MALLOC_CAP_DMA);
    if(lcd_buffer2==nullptr) {
        puts("Error allocating LCD buffer 2");
        while(1);
    }
#endif
    lcd_panel_init(lcd_buffer_size,lcd_flush_ready);
}

void display_update() {
    if(display_active_screen!=nullptr) {
        display_active_screen->update();
    }
}

void display_screen(screen_t& new_screen) {
    display_active_screen = &new_screen;
    display_active_screen->on_flush_callback(uix_flush);
    display_active_screen->invalidate();   
}
void display_sleep() {
    if(!display_sleeping) {
        //esp_lcd_panel_io_tx_param(lcd_io_handle,0x10,NULL,0);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        esp_lcd_panel_disp_on_off(lcd_handle, false);
#else
        esp_lcd_panel_disp_off(lcd_handle, true);
#endif
        display_sleeping = true;
    }
}
void display_wake() {
    if(display_sleeping) {
        //esp_lcd_panel_io_tx_param(lcd_io_handle,0x11,NULL,0);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        esp_lcd_panel_disp_on_off(lcd_handle, true);
#else
        esp_lcd_panel_disp_off(lcd_handle, false);
#endif
        display_sleeping = false;
    }
}