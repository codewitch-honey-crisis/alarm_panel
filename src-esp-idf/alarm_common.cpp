#include "alarm_common.hpp"

#include <memory.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "serial.hpp"
static_assert(alarm_count > 0 && alarm_count < 33, "Alarm count must be 1-32");
bool alarm_values[alarm_count];
void* alarm_sync = nullptr;

void alarm_init() {
    memset(alarm_values, 0, sizeof(bool) * alarm_count);
    alarm_sync = xSemaphoreCreateMutex();
    if (alarm_sync == nullptr) {
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
void alarm_lock() {
    xSemaphoreTake((SemaphoreHandle_t)alarm_sync,portMAX_DELAY);
}
void alarm_unlock() {
    xSemaphoreGive((SemaphoreHandle_t)alarm_sync);
}
