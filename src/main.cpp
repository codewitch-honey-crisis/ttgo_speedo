#define MAX_SPEED 40.0f
#define MPH
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

static lcd_miser<4> dimmer;
static lwgps_t gps;
#ifdef MPH
static constexpr const char* speed_units = "mph";
static constexpr const char* trip_units = "miles";
static constexpr const lwgps_speed_t gps_units = LWGPS_SPEED_MPH;
#else
static constexpr const char* speed_units = "kph";
static constexpr const char* trip_units = "km";
static constexpr const lwgps_speed_t gps_units = LWGPS_SPEED_KPH;
#endif
static uint64_t trip_counter = 0;
static char rx_buffer[1024];
static char speed_buffer[3];
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
static void update_all() {
    size_t read = serial_read(rx_buffer,sizeof(rx_buffer));
    static uint8_t sats_old = 0;
    static int speed_old = 0;
    if(gps.sats_in_use!=sats_old) {
        printf("Satellites: %d\n",(int)gps.sats_in_use);
        sats_old = gps.sats_in_use;
    }
    static uint32_t counter_ts = millis();
    if(millis()>=counter_ts+1000) {
        counter_ts = millis();
        trip_counter+=speed_old;
        printf("Speed: %d %s\n",(int)speed_old,speed_units);
        printf("Trip: % .2f %s\n",(double)trip_counter/(60.0*60.0),trip_units);
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
        if(angle>=360) angle-=360;
        speed_needle.angle(angle);
        speed_old = r;
    }
    display_update();
    dimmer.update();
}
static void initialize_common() {
    display_init();
    dimmer.initialize();
    lwgps_init(&gps);
    strcpy(speed_buffer,"--");
    speed_label.text(speed_buffer);
    puts("Booted");
    
    // initialize the main screen (ui.cpp)
    main_screen_init();

    display_screen(main_screen);
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
    initialize_common();
    TaskHandle_t htask = nullptr;
    xTaskCreate(loop_task,"loop_task",4096,nullptr,uxTaskPriorityGet(nullptr),&htask);
    if(htask==nullptr) {
        printf("Unable to create loop task\n");
    }
}
#endif