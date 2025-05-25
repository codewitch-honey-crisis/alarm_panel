#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>
#include <stddef.h>
#include "config.h"
#ifdef M5STACK_CORE2
#define SERIAL_RX 32
#define SERIAL_TX 33
#endif
#ifdef FREENOVE_DEVKIT
#define SERIAL_RX 12
#define SERIAL_TX 13
#endif
typedef struct {
    uint8_t cmd;
    uint8_t arg;
} serial_event_t;
#ifdef __cplusplus
extern "C" {
#endif
void serial_init();
int serial_get_event(serial_event_t* out_event);
void serial_send_alarm(size_t i);
#ifdef __cplusplus
}
#endif

#endif // SERIAL_H