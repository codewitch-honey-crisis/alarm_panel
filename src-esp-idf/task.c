#include "task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
task_t task_init(task_fn_t task, size_t stack_size,int affinity, void* const arg) {
    TaskHandle_t result;
    if(affinity==TASK_AFFINITY_ANY) {
        xTaskCreate(task,NULL,stack_size,arg,2,&result);
    } else {
        xTaskCreatePinnedToCore(task,NULL,stack_size,arg,2,&result,affinity);
    }
    return result;
}
void task_end(task_t task) {
    vTaskDelete((TaskHandle_t)task);
}
task_mutex_t task_mutex_init() {
    return xSemaphoreCreateMutex();
}
void task_mutex_end(task_mutex_t mutex) {
    vSemaphoreDelete(mutex);
}
int task_mutex_lock(task_mutex_t mutex, int delay_ms) {
    if(delay_ms<0) {
        if(pdTRUE==xSemaphoreTake((SemaphoreHandle_t)mutex,portMAX_DELAY)) {
            return 0;
        }
    } else {
        if(pdTRUE==xSemaphoreTake((SemaphoreHandle_t)mutex,pdMS_TO_TICKS(delay_ms))) {
            return 0;
        }
    }
    return -1;
}
void task_mutex_unlock(task_mutex_t mutex) {
    xSemaphoreGive((SemaphoreHandle_t)mutex);
}

uint32_t task_ms() {
    return pdTICKS_TO_MS(xTaskGetTickCount());
}
void task_delay(uint32_t delay_ms) {
    uint64_t ticks = pdMS_TO_TICKS(delay_ms);
    if(ticks<5) {
        ticks = 5;
    }
    vTaskDelay(ticks);
}
