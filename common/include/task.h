#ifndef TASK_H
#define TASK_H
#include <stddef.h>
#include <stdint.h>
#define TASK_AFFINITY_ANY (-1)
#define TASK_STACK_DEFAULT (2048)
typedef void* task_t;
typedef void* task_mutex_t;
typedef void(*task_fn_t)(void* arg);
#ifdef __cplusplus
extern "C" {
#endif
task_t task_init(task_fn_t task, size_t stack_size,int affinity, void*const arg);
void task_end(task_t task);
task_mutex_t task_mutex_init();
void task_mutex_end(task_mutex_t mutex);
int task_mutex_lock(task_mutex_t mutex, int delay_ms);
void task_mutex_unlock(task_mutex_t mutex);
uint32_t task_ms();
void task_delay(uint32_t delay_ms);
#ifdef __cplusplus
}
#endif
#endif // TASK_H