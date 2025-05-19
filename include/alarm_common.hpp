#pragma once
#include "config.h"
extern void* alarm_sync;
extern bool alarm_values[];
void alarm_init();
void alarm_enable(size_t alarm, bool on);