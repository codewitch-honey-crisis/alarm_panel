#include <memory.h>
#include "alarm.h"
#include "task.h"
#include "serial.h"
//static_assert(alarm_count > 0 && alarm_count < 33, "Alarm count must be 1-32");
char alarm_values[ALARM_COUNT];
static task_mutex_t alarm_sync =NULL;

int alarm_init() {
    if(alarm_sync!=NULL) {
        return 0;
    }
    memset(alarm_values, 0, sizeof(char) * alarm_count);
    alarm_sync = task_mutex_init();
    if (alarm_sync == NULL) {
        return -1;     
    }
    return 0;
}
void alarm_enable(size_t alarm, char on) {
    if (alarm < 0 || alarm >= alarm_count) return;
    if (alarm_values[alarm] != on) {
        alarm_values[alarm] = on;
        serial_event_t evt;
        evt.cmd = alarm_values[alarm] ? SET_ALARM : CLEAR_ALARM;
        evt.arg = alarm;
        serial_send_event(&evt);
    }
}
void alarm_lock() {
    if(alarm_sync==NULL) return;
    task_mutex_lock(alarm_sync,-1);
}
void alarm_unlock() {
    if(alarm_sync==NULL) return;
    task_mutex_unlock(alarm_sync);
}
void alarm_unpack_values(uint32_t data, size_t length, char* out_values) {
    if (length < 1 || length > 32 || out_values == NULL) {
        return;
    }
    // unpack the alarm values into the buffer
    for (int i = 0; i < length; ++i) {
        out_values[i] = (data & 1);
        data >>= 1;
    }
}
uint32_t alarm_pack_values(char* values) {
    // pack the alarm values into the buffer
    uint32_t accum = 0;
    size_t i = alarm_count;
    while (i) {
        accum <<= 1;
        accum |= (int)values[i - 1];
        --i;
    }
    return accum;
}
