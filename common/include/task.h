#ifndef TASK_H
#define TASK_H
#include <stddef.h>
#include <stdint.h>
#define TASK_AFFINITY_ANY (-1)
#define TASK_STACK_DEFAULT (2048)
/// @brief The handle for a task
typedef void* task_t;
/// @brief The handle for a mutex
typedef void* task_mutex_t;
/// @brief The method for a task
typedef void(*task_fn_t)(void* arg);
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Creates a new task
/// @param task The function to handle the task
/// @param stack_size The stack size, in bytes or TASK_STACK_DEFAULT
/// @param affinity The core to run on or TASK_AFFINITY_ANY
/// @param arg The argument to pass to the task
/// @return On success a task_t handle, otherwise NULL
task_t task_init(task_fn_t task, size_t stack_size,int affinity, void*const arg);
/// @brief Ends a task
/// @param task The task to end
void task_end(task_t task);
/// @brief Creates a mutex
/// @return The handle to the new mutex, or NULL if failed
task_mutex_t task_mutex_init(void);
/// @brief Deletes a mutex
/// @param mutex The mutex to delete
void task_mutex_end(task_mutex_t mutex);
/// @brief Acquires the mutex
/// @param mutex The mutex to acquire
/// @param delay_ms The time to wait before timeout, or -1 for no timeout
/// @return 0 if lock acquired, otherwise non-zero
int task_mutex_lock(task_mutex_t mutex, int delay_ms);
/// @brief Releases a mutex
/// @param mutex The mutex to release
void task_mutex_unlock(task_mutex_t mutex);
/// @brief Reports the number of milliseconds since system start
/// @return The number of milliseconds
uint32_t task_ms();
/// @brief Sleeps the current task for a number of milliseconds
/// @param delay_ms The number of milliseconds to delay for
void task_delay(uint32_t delay_ms);
#ifdef __cplusplus
}
#endif
#endif // TASK_H