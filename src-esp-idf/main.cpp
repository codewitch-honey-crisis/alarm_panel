#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern void run();
extern void loop();
extern "C" void app_main() {
    run();
    // loop task
    uint32_t ts = pdTICKS_TO_MS(xTaskGetTickCount());
    while (1) {
        loop();
        uint32_t ms = pdTICKS_TO_MS(xTaskGetTickCount());
        if (ms > ts + 200) {
            ms = pdTICKS_TO_MS(xTaskGetTickCount());
            vTaskDelay(5);
        }
    }
}