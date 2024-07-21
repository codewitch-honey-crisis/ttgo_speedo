#define MAX_SPEED 40.0f
#define MILES
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
#include <button.hpp>
#include <lcd_miser.hpp>
#include <uix.hpp>
#include "ui.hpp"
#include "lwgps.h"
#ifdef ARDUINO
using namespace arduino;
#else
using namespace esp_idf;
static uint32_t millis() {
    return pdTICKS_TO_MS(xTaskGetTickCount());
}
#endif

using namespace gfx;
using namespace uix;
using button_a_t = basic_button;
using button_b_t = basic_button;
button_a_t button_a_raw(0,10,true);
button_b_t button_b_raw(35,10,true);
multi_button button_a(button_a_raw);
multi_button button_b(button_b_raw);
static lcd_miser<4> dimmer;
static lwgps_t gps;
static char speed_units[32];
static char trip_units[16];
static lwgps_speed_t gps_units;
static uint64_t trip_counter = 0;
static char trip_buffer[64];
static char rx_buffer[1024];
static char speed_buffer[3];
static char loc_lat_buffer[64];
static char loc_lon_buffer[64];
static char loc_alt_buffer[64];
static char stat_sat_buffer[64];
static int current_screen = 0;
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
void toggle_units() {
    trip_counter = 0;
    if(gps_units==LWGPS_SPEED_KPH) {
        gps_units = LWGPS_SPEED_MPH;
        strcpy(speed_units,"mph");
        strcpy(trip_units,"miles");
    } else {
        gps_units = LWGPS_SPEED_KPH;
        strcpy(speed_units,"kph");
        strcpy(trip_units,"kilometers");
    }
}
void button_a_on_pressed_changed(bool pressed, void* state) {
    if(!pressed) {
        dimmer.wake();
        if(++current_screen==4) {
            current_screen=0;
        }
        switch(current_screen) {
            case 0:
                puts("Speed screen");
                display_screen(speed_screen);
                break;
            case 1:
                puts("Trip screen");
                display_screen(trip_screen);
                break;
            case 2:
                puts("Location screen");
                display_screen(loc_screen);
                break;
            case 3:
                puts("Stat screen");
                display_screen(stat_screen);
                break;
        }
    }
}
void button_b_on_click(int clicks, void* state) {
    dimmer.wake();
    clicks&=1;
    if(clicks) {
        toggle_units();
        speed_units_label.text(speed_units);
        trip_units_label.text(trip_units);
    }
}
void button_b_on_long_click(void* state) {
    dimmer.wake();
    if(current_screen==1) {
        trip_counter = 0;
        snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",0.0f);
        trip_label.text(trip_buffer);
    }
}
static void update_all() {
    size_t read = serial_read(rx_buffer,sizeof(rx_buffer));
    static uint8_t sats_old = 0;
    static int speed_old = 0;
    if(gps.sats_in_use!=sats_old) {
        printf("Satellites: %d/%d\n",(int)gps.sats_in_use,(int)gps.sats_in_view);
        sats_old = gps.sats_in_use;
    }
    static uint32_t counter_ts = millis();
    if(millis()>=counter_ts+1000) {
        counter_ts = millis();
        trip_counter+=speed_old;
        printf("Speed: %d %s\n",(int)speed_old,speed_units);
        double trip = (double)trip_counter/(60.0*60.0);
        printf("Trip: % .2f %s\n",trip,trip_units);
        snprintf(trip_buffer,sizeof(trip_buffer),"% .2f",trip);
        trip_label.text(trip_buffer);
        snprintf(stat_sat_buffer,sizeof(stat_sat_buffer),"%d/%d sats",(int)gps.sats_in_use,(int)gps.sats_in_view);
        stat_sat_label.text(stat_sat_buffer);
    }
    if(read>0) {
        lwgps_process(&gps,rx_buffer,read);
    }
    if(gps.is_valid) {
        float f = lwgps_to_speed(gps.speed,gps_units);
        if(f>MAX_SPEED) f=MAX_SPEED;
        int r = (int)roundf(f);
        if((int)floorf(f)>0) {
            dimmer.wake();
        }
        itoa(r,speed_buffer,10);
        speed_label.text(speed_buffer);
        int angle = (270 + ((int)roundf((f/MAX_SPEED)*180.0f)));
        while(angle>=360) angle-=360;
        speed_needle.angle(angle);
        speed_old = r;
        snprintf(loc_lat_buffer,sizeof(loc_lat_buffer),"lat: % .2f",gps.latitude);
        loc_lat_label.text(loc_lat_buffer);
        snprintf(loc_lon_buffer,sizeof(loc_lon_buffer),"lon: % .2f",gps.longitude);
        loc_lon_label.text(loc_lon_buffer);
        snprintf(loc_alt_buffer,sizeof(loc_lon_buffer),"alt: % .2f",gps.altitude);
        loc_alt_label.text(loc_alt_buffer);
    }
    display_update();
    if(current_screen!=0) {
        dimmer.wake();
    }
    button_a.update();
    button_b.update();
    dimmer.update();
}
static void initialize_common() {
    display_init();
    gps_units = LWGPS_SPEED_KPH;
    strcpy(speed_units,"kph");
    strcpy(trip_units,"kilometers");
    button_a.initialize();
    button_b.initialize();
    button_a.on_pressed_changed(button_a_on_pressed_changed);
    button_b.on_click(button_b_on_click);
    button_b.on_long_click(button_b_on_long_click);
    dimmer.initialize();
    lwgps_init(&gps);
    strcpy(speed_buffer,"--");
    speed_label.text(speed_buffer);
    // initialize the screens (ui.cpp)
    ui_init();
    speed_units_label.text(speed_units);
    trip_units_label.text(trip_units);
#ifdef MILES
    toggle_units();
#endif
    puts("Booted");

    display_screen(speed_screen);
}
#ifdef ARDUINO
void setup() {
    Serial.begin(115200);
    Serial1.begin(9600,SERIAL_8N1,22,21);
    initialize_common();
}

void loop() {
    update_all();    
}
#else
static void loop_task(void* state) {
    while(true) {
        update_all();
        vTaskDelay(5);
    }
}
void app_main() {
    uart_config_t ucfg;
    memset(&ucfg,0,sizeof(ucfg));
    ucfg.baud_rate = 9600;
    ucfg.data_bits = UART_DATA_8_BITS;
    ucfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    ucfg.parity = UART_PARITY_DISABLE;
    ucfg.stop_bits = UART_STOP_BITS_1;
    ucfg.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1,&ucfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 21, 22, -1, -1));
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
    gpio_reset_pin(GPIO_NUM_0);
    gpio_reset_pin(GPIO_NUM_35);
    initialize_common();
    TaskHandle_t htask = nullptr;
    xTaskCreate(loop_task,"loop_task",4096,nullptr,uxTaskPriorityGet(nullptr),&htask);
    if(htask==nullptr) {
        printf("Unable to create loop task\n");
    }
}
#endif