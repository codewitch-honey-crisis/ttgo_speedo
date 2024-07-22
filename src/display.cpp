#ifndef E_PAPER
#ifdef ESP_PLATFORM
#define LCD_IMPLEMENTATION
#include <lcd_init.h>
#endif
#endif
#include <display.hpp>
#ifndef ESP_PLATFORM
static lcd_t lcd;
#endif
#ifdef E_PAPER
#ifdef ARDUINO
arduino::lilygot54in7 epd;
#else
esp_idf::lilygot54in7 epd;
#endif
#endif

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
screen_t* active_screen = nullptr;
static bool display_sleeping = false;
#if defined(ESP_PLATFORM) && !defined(E_PAPER)
#ifdef LCD_DMA
// only needed if DMA enabled
static bool lcd_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                            esp_lcd_panel_io_event_data_t* edata, 
                            void* user_ctx) {
    if(active_screen!=nullptr) {
        active_screen->flush_complete();
    }
    return true;
}
#endif
#if !defined(ESP_PLATFORM)
static void uix_wait(void* state) {
}
#endif
static void uix_flush(const gfx::rect16& bounds, 
                    const void* bmp, 
                    void* state) {
                        //ESP_LOGI("display","FLUSH");
    lcd_panel_draw_bitmap(bounds.x1,bounds.y1,bounds.x2,bounds.y2,(void*)bmp);
    // no DMA, so we are done once the above completes
#ifndef LCD_DMA
    if(active_screen!=nullptr) {
        active_screen->flush_complete();
    }
#endif
}
void display_init() {
#ifdef LCD_PSRAM_BUFFER
    lcd_buffer1 = (uint8_t*)heap_caps_malloc(lcd_buffer_size,MALLOC_CAP_SPIRAM);
#else
    lcd_buffer1 = (uint8_t*)heap_caps_malloc(lcd_buffer_size,MALLOC_CAP_DMA);
#endif
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
#ifndef LCD_PIN_NUM_VSYNC
    lcd_panel_init(lcd_buffer_size,lcd_flush_ready);
#else
    lcd_panel_init();
#endif
}
#else
void display_init() {
#ifndef E_PAPER
    lcd.initialize();
#ifdef LCD_ROTATION
    lcd.rotation(LCD_ROTATION);
#endif
#else
    epd.initialize();
#ifdef LCD_ROTATION
    epd.rotation(LCD_ROTATION);
#endif
#endif
}
#ifdef LCD_DMA
void uix_wait(void* state) {
    gfx::draw::wait_all_async(lcd);
}
#endif
void uix_flush(const gfx::rect16& bounds, 
                    const void* bmp, 
                    void* state) {
    if(active_screen!=nullptr) {
        gfx::const_bitmap<screen_t::pixel_type,screen_t::palette_type> cbmp(bounds.dimensions(),bmp,active_screen->palette());
#ifndef E_PAPER
        gfx::draw::bitmap_async(lcd,bounds,cbmp,cbmp.bounds());
#else
        gfx::draw::bitmap_async(epd,bounds,cbmp,cbmp.bounds());
#endif
#ifndef LCD_DMA
        active_screen->flush_complete();
#endif
    }
}
#endif

void display_update() {
    if(active_screen!=nullptr) {
#ifdef E_PAPER
        bool dirty = active_screen->dirty();
        if(dirty) {
            gfx::draw::suspend(epd);
            epd.invalidate();
        }
        active_screen->update();
        if(dirty) {
            gfx::draw::resume(epd);
        }
#else
        active_screen->update();
#endif
    }
}

void display_screen(screen_t& new_screen) {
    if(active_screen!=nullptr) {
        if(active_screen->flushing()) {
            active_screen->update();
        }
    }
    active_screen = &new_screen;
    active_screen->on_flush_callback(uix_flush);
#ifndef ESP_PLATFORM
#ifdef LCD_DMA
    active_screen->wait_flush_callback(uix_wait);
#endif
#endif
    active_screen->invalidate();   
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