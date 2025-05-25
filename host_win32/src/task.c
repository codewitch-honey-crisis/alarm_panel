#include "task.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
typedef struct {
    task_fn_t fn;
    void* const arg;
} task_thunk_data_t;
static DWORD WINAPI task_thunk(void* arg) {
    task_thunk_data_t* data = (task_thunk_data_t*)arg;
    data->fn(data->arg);
    return TRUE;
}
task_t task_init(task_fn_t task, size_t stack_size,int affinity, void* const arg) {
    DWORD result;
    task_thunk_data_t thunk = {
        task,
        arg
    };
    CreateThread(NULL,  // default security attributes
                 stack_size ,     // default stack size
                 (LPTHREAD_START_ROUTINE)task_thunk,
                 &thunk,  // no thread function arguments
                 0,     // default creation flags
                 &result);
    task_delay(5); // hack, avoid a race
    if(result!=0 && affinity!=TASK_AFFINITY_ANY) {
        SetThreadAffinityMask((HANDLE)result,((DWORD_PTR)1)<<affinity);
    }
    return (task_t)result;
}
void task_end(task_t task) {
    TerminateThread((HANDLE)task,0);
}
task_mutex_t task_mutex_init() {
    return (task_mutex_t)CreateMutex(NULL,   // default security attributes
                             FALSE,  // initially not owned
                             NULL);  // unnamed mutex
}
void task_mutex_end(task_mutex_t mutex) {
    CloseHandle((HANDLE)mutex);
}
int task_mutex_lock(task_mutex_t mutex, int delay_ms) {
    if(delay_ms<0) {
        if(WAIT_OBJECT_0==WaitForSingleObject((HANDLE)mutex,  // handle to mutex
                        INFINITE)) {  // no time-out int
            return 0;                    
        }
    } else {
        if(WAIT_OBJECT_0==WaitForSingleObject((HANDLE)mutex,  // handle to mutex
                        delay_ms)) {   // time-out int
            return 0;                    
        }
    }
    return -1;
}
void task_mutex_unlock(task_mutex_t mutex) {
    ReleaseMutex((HANDLE)mutex);
}

uint32_t task_ms() {
    LARGE_INTEGER counter_freq;
    LARGE_INTEGER end_time;
    QueryPerformanceFrequency(&counter_freq);
    QueryPerformanceCounter(&end_time);
    return (uint32_t)((double)end_time.QuadPart  / counter_freq.QuadPart) * 1000;

}
void task_delay(uint32_t delay_ms) {
    uint32_t end = delay_ms + task_ms();
    while (task_ms() < end)
        ;
}
