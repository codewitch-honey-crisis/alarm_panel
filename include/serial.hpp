#pragma once
#include "config.h"
struct serial_event {
    COMMAND_ID cmd;
    uint8_t arg;
};
void serial_init();
bool serial_get_event(serial_event* out_event);
void serial_send_alarm(size_t i);