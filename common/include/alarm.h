#ifndef ALARM_H
#define ALARM_H
#include <stddef.h>
#include <stdint.h>
#include "config.h"
#ifdef __cplusplus
extern "C" {
#endif
extern char alarm_values[];
int alarm_init();
void alarm_enable(size_t alarm, char on);
void alarm_lock();
void alarm_unlock();
void alarm_unpack_values(uint32_t data, size_t length, char* out_values);
uint32_t alarm_pack_values(char* values);
#ifdef __cplusplus
}
#endif
#endif // ALARM_H