#define MAX_SPEED 40.0f
#define MILES
#define BAUD 9600
#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/uart.h>
extern "C" void app_main();
#endif
#define LCD_IMPLEMENTATION
#include "lcd_init.h"
#include <button.hpp>
#include <lcd_miser.hpp>
#include <uix.hpp>
#include "ui.hpp"
#include "lwgps.h"
#ifdef ARDUINO
using namespace arduino;
#else
using namespace esp_idf;
// shim for compatibility
static uint32_t millis() {
    return pdTICKS_TO_MS(xTaskGetTickCount());
}
#endif

using namespace gfx;
using namespace uix;

// two ttgo buttons
using button_a_t = basic_button;
using button_b_t = basic_button;
button_a_t button_a_raw(0,10,true);
button_b_t button_b_raw(35,10,true);
multi_button button_a(button_a_raw);
multi_button button_b(button_b_raw);

// screen dimmer
static lcd_miser<4> dimmer;

// gps decoder
static lwgps_t gps;

#define LCD_TRANSFER_KB 64

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
uint8_t* lcd_buffer1;
uint8_t* lcd_buffer2;
#else
#ifdef LCD_PSRAM_BUFFER
constexpr static const size_t lcd_buffer_size = LCD_PSRAM_BUFFER;
#else
constexpr static const size_t lcd_buffer_size = (LCD_TRANSFER_KB*1024) > 
    lcd_screen_total_size?lcd_screen_total_size:(LCD_TRANSFER_KB*1024);
#endif
uint8_t* lcd_buffer1;
static uint8_t* const lcd_buffer2 = nullptr;
#endif

using display_t = uix::display;
static display_t disp;
#ifdef LCD_DMA
// only needed if DMA enabled
static bool display_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                            esp_lcd_panel_io_event_data_t* edata, 
                            void* user_ctx) {
    disp.flush_complete();
    return true;
}
#endif
static void display_flush(const gfx::rect16& bounds, 
                    const void* bmp, 
                    void* state) {
    lcd_panel_draw_bitmap(bounds.x1,bounds.y1,bounds.x2,bounds.y2,(void*)bmp);
    // no DMA, so we are done once the above completes
#ifndef LCD_DMA
    disp.flush_complete();
#endif
}

static void display_init() {
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
    lcd_panel_init(lcd_buffer_size,display_flush_ready);
}

static bool display_sleeping = false;
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
// ui data
static char speed_units[16];
static char trip_units[16];
static lwgps_speed_t gps_units;
static double trip_counter_miles = 0;
static double trip_counter_kilos = 0;
static char trip_buffer[64];
static char speed_buffer[3];
static char loc_lat_buffer[64];
static char loc_lon_buffer[64];
static char loc_alt_buffer[64];
static char stat_sat_buffer[64];
static int current_screen = 0;

// serial incoming
static char rx_buffer[1024];
// reads from Serial1/UART_NUM_1
static size_t serial_read(char* buffer, size_t size) {
#ifdef ARDUINO
    if(Serial1.available()) {
        return Serial1.read(buffer,size);
    } else {
        return 0;
    }
#else
    int ret = uart_read_bytes(UART_NUM_1,buffer,size,0);
    if(ret>0) {
        return (size_t)ret;
    } else if(ret<0) {
        puts("Serial error");
    }
    return 0;
#endif
}
// switch between imperial and metric units
void toggle_units() {
    if(gps_units==LWGPS_SPEED_KPH) {
        gps_units = LWGPS_SPEED_MPH;
        strcpy(speed_units,"mph");
        strcpy(trip_units,"miles");
    } else {
        gps_units = LWGPS_SPEED_KPH;
        strcpy(speed_units,"kph");
        strcpy(trip_units,"kilometers");
    }
    speed_label.invalidate();
    speed_big_label.invalidate();
    speed_units_label.text(speed_units);
    speed_big_units_label.text(speed_units);
    trip_label.invalidate();
    trip_units_label.text(trip_units);
}
// top button handler - switch screens
void button_a_on_pressed_changed(bool pressed, void* state) {
    if(!pressed) {
        display_wake();
        bool dim = dimmer.dimmed();
        dimmer.wake();
        if(dim) {
            return;
        }
        if(++current_screen==4) {
            current_screen=0;
        }
        switch(current_screen) {
            case 0:
                // puts("Speed screen");
                disp.active_screen(&speed_screen);
                break;
            case 1:
                // puts("Trip screen");
                disp.active_screen(&trip_screen);
                break;
            case 2:
                // puts("Location screen");
                disp.active_screen(&loc_screen);
                break;
            case 3:
                // puts("Stat screen");
                disp.active_screen(&stat_screen);
                break;
        }
    } 
}
// bottom button handler, toggle units
void button_b_on_click(int clicks, void* state) {
    display_wake();
    bool dim = dimmer.dimmed();
    dimmer.wake();
    if(dim) {
        return;
    }
    clicks&=1;
    if(clicks) {
        toggle_units();
        speed_units_label.text(speed_units);
        speed_big_units_label.text(speed_units);
        trip_units_label.text(trip_units);
    }
}
// long handler - reset trip counter
void button_b_on_long_click(void* state) {
    display_wake();
    dimmer.wake();
    switch(current_screen) {
        case 0:
            if(speed_label.visible()) {
                speed_label.visible(false);
                speed_units_label.visible(false);
                speed_needle.visible(false);
                speed_big_label.visible(true);
                speed_big_units_label.visible(true);
            } else {
                speed_label.visible(true);
                speed_units_label.visible(true);
                speed_needle.visible(true);
                speed_big_label.visible(false);
                speed_big_units_label.visible(false);
            }
        break;
        case 1:
            trip_counter_miles = 0;
            trip_counter_kilos = 0;
        
            snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",0.0f);
            trip_label.text(trip_buffer);
        break;
    }
}
// main application loop
static void update_all() {
    // try to read from the GPS
    size_t read = serial_read(rx_buffer,sizeof(rx_buffer));
    if(read>0) {
        lwgps_process(&gps,rx_buffer,read);
    }
    
    // if we have GPS data:
    if(gps.is_valid) {
        // old values so we know when changes occur
        // (avoid refreshing unless changed)
        static double old_trip = NAN;
        static int old_sats_in_use = -1;
        static int old_sats_in_view = -1;
        static float old_lat = NAN, old_lon = NAN, old_alt = NAN;
        static float old_mph = NAN, old_kph = NAN;
        static int old_angle = -1;

        // for timing the trip counter
        static uint32_t poll_ts = millis();
        static uint64_t total_ts = 0;
        // compute how long since the last
        uint32_t diff_ts = millis()-poll_ts;
        poll_ts = millis();
        // add it to the total
        total_ts += diff_ts;
        
        float kph = lwgps_to_speed(gps.speed,LWGPS_SPEED_KPH);
        float mph = lwgps_to_speed(gps.speed,LWGPS_SPEED_MPH);

        if(total_ts>=100) {
            while(total_ts>=100) {
                total_ts-=100;
                trip_counter_miles+=mph;
                trip_counter_kilos+=kph;
            }
            double trip = 
                (double)((gps_units==LWGPS_SPEED_KPH)? 
                trip_counter_kilos:
                trip_counter_miles)
                /(60.0*60.0*10.0);
            if(round(old_trip*100.0)!=round(trip*100.0)) {
                snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",trip);
                trip_label.text(trip_buffer);
                old_trip = trip;
            }
        }
        bool speed_changed = false;
        int sp;
        int old_sp;
        if(gps_units==LWGPS_SPEED_KPH) {
            sp=(int)roundf(kph);
            old_sp = (int)roundf(old_kph);
            if(old_sp!=sp) {
                speed_changed = true;
            }
        } else {
            sp=(int)roundf(mph);
            old_sp = (int)roundf(old_mph);
            if(old_sp!=sp) {
                speed_changed = true;
            }
        }
        old_mph = mph;
        old_kph = kph;
        if(!speed_changed && dimmer.faded()) {
            // if the speed isn't zero or it's not the speed screen wake the screen up
            if(current_screen!=0 || (gps_units==LWGPS_SPEED_KPH && ((int)roundf(kph))>0) || (gps_units==LWGPS_SPEED_MPH && ((int)roundf(mph))>0)) {
                display_wake();
                dimmer.wake();
            } else {
                // force a refresh next time the screen is woken
                if(old_angle!=-1) {
                    old_trip = NAN;
                    old_sats_in_use = -1;
                    old_sats_in_view = -1;
                    old_lat = NAN; old_lon = NAN; old_alt = NAN;
                    old_angle = -1;
                }
                
                // make sure we pump before returning
                button_a.update();
                button_b.update();
                dimmer.update();
                return;
            }
        }
        // update the speed
        if(speed_changed) {
            if((gps_units==LWGPS_SPEED_KPH && 
                ((int)roundf(kph))>0) || 
                    (gps_units==LWGPS_SPEED_MPH && 
                        ((int)roundf(mph))>0)) {
                display_wake();
                dimmer.wake();
            }
            itoa((int)roundf(sp>MAX_SPEED?MAX_SPEED:sp),speed_buffer,10);
            speed_label.text(speed_buffer);
            speed_big_label.text(speed_buffer);
            // figure the needle angle
            float f = gps_units == LWGPS_SPEED_KPH?kph:mph;
            int angle = (270 + 
                ((int)roundf(((f>MAX_SPEED?MAX_SPEED:f)/MAX_SPEED)*180.0f)));
            while(angle>=360) angle-=360;
            if(old_angle!=angle) {
                speed_needle.angle(angle);
                old_angle = angle;
            }
        }
        // update the position data
        if(roundf(old_lat*100.0f)!=roundf(gps.latitude*100.0f)) {
            snprintf(loc_lat_buffer,sizeof(loc_lat_buffer),"lat: % .2f",gps.latitude);
            loc_lat_label.text(loc_lat_buffer);
            old_lat = gps.latitude;
        }
        if(roundf(old_lon*100.0f)!=roundf(gps.longitude*100.0f)) {
            snprintf(loc_lon_buffer,sizeof(loc_lon_buffer),"lon: % .2f",gps.longitude);
            loc_lon_label.text(loc_lon_buffer);
            old_lon = gps.longitude;
        }
        if(roundf(old_alt*100.0f)!=roundf(gps.altitude*100.0f)) {
            snprintf(loc_alt_buffer,sizeof(loc_lon_buffer),"alt: % .2f",gps.altitude);
            loc_alt_label.text(loc_alt_buffer);
            old_alt = gps.altitude;
        }
        // update the stat data
        if(gps.sats_in_use!=old_sats_in_use||
                gps.sats_in_view!=old_sats_in_view) {
            snprintf(stat_sat_buffer,
                sizeof(stat_sat_buffer),"%d/%d sats",
                (int)gps.sats_in_use,
                (int)gps.sats_in_view);
            stat_sat_label.text(stat_sat_buffer);
            old_sats_in_use = gps.sats_in_use;
            old_sats_in_view = gps.sats_in_view;
        }
    }
    // only screen zero auto-dims
    if(current_screen!=0) {
        display_wake();
        dimmer.wake();
    }
    // update the various objects
    disp.update();
    button_a.update();
    button_b.update();
    dimmer.update();
    
    // if the backlight is off
    // sleep the display
    if(dimmer.faded()) {
        display_sleep();
    }
}
static void initialize_common() {
    display_init();
    button_a.initialize();
    button_b.initialize();
    button_a.on_pressed_changed(button_a_on_pressed_changed);
    button_b.on_click(button_b_on_click);
    button_b.on_long_click(button_b_on_long_click);
    dimmer.initialize();
    ui_init();
    lwgps_init(&gps);
    strcpy(speed_buffer,"--");
    speed_label.text(speed_buffer);
    speed_big_label.text(speed_buffer);
    gps_units = LWGPS_SPEED_KPH;
    strcpy(speed_units,"kph");
    strcpy(trip_units,"kilometers");
    speed_units_label.text(speed_units);
    speed_big_units_label.text(speed_units);
    trip_units_label.text(trip_units);
#ifdef MILES
    toggle_units();
#endif
    puts("Booted");
    disp.buffer_size(lcd_buffer_size);
    disp.buffer1(lcd_buffer1);
    disp.buffer2(lcd_buffer2);
    disp.on_flush_callback(display_flush);
    disp.active_screen(&speed_screen);
}
#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    Serial1.begin(BAUD,SERIAL_8N1,22,21);
    initialize_common();
}

void loop() {
    update_all();    
}
#else
static void loop_task(void* state) {
    while(true) {
        update_all();
        vTaskDelay(1);
    }
}
extern "C" void app_main() {
    uart_config_t ucfg;
    memset(&ucfg,0,sizeof(ucfg));
    ucfg.baud_rate = BAUD;
    ucfg.data_bits = UART_DATA_8_BITS;
    ucfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    ucfg.parity = UART_PARITY_DISABLE;
    ucfg.stop_bits = UART_STOP_BITS_1;
    ucfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1,&ucfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 21, 22, -1, -1));
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(
        UART_NUM_1, 
        uart_buffer_size, 
        uart_buffer_size, 
        10, 
        &uart_queue, 
        0));
    initialize_common();
    TaskHandle_t htask = nullptr;
    xTaskCreate(loop_task,"loop_task",4096,nullptr,uxTaskPriorityGet(nullptr),&htask);
    if(htask==nullptr) {
        printf("Unable to create loop task\n");
    }
}
#endif