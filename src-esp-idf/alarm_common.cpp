#include <memory.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "alarm_common.hpp"
#include "serial.hpp"

bool alarm_values[alarm_count];
void* alarm_sync = nullptr;

void alarm_init() {
    memset(alarm_values,0,sizeof(bool)*alarm_count);
    alarm_sync = xSemaphoreCreateMutex();
    if(alarm_sync==nullptr) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}
void alarm_enable(size_t alarm, bool on) {
    if (alarm < 0 || alarm >= alarm_count) return;
    if (alarm_values[alarm] != on) {
        alarm_values[alarm] = on;
        serial_send_alarm(alarm);
    }
}
