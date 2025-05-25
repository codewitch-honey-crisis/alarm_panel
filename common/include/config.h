#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include <stddef.h>
#define ALARM_COUNT 4
#define ALARM_BAUD 115200
// the number of alarms
static const size_t alarm_count = ALARM_COUNT;

enum COMMAND_ID {
    SET_ALARM = 1, // followed by 1 byte, alarm id
    CLEAR_ALARM = 2, // followed by 1 byte, alarm id
    ALARM_THROWN = 3 // followed by 1 byte, alarm id
};

// The fire alarm switches - must have <alarm_count> entries
static const uint8_t alarm_switch_pins[ALARM_COUNT] = {
    27,14,12,13
};
// The fire alarm enable pins - must have <alarm_count> entries
static const uint8_t alarm_enable_pins[ALARM_COUNT] = {
    28,9,10,11
};

#endif