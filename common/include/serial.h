#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>
#include <stddef.h>
#ifdef M5STACK_CORE2
#define SERIAL_RX 32
#define SERIAL_TX 33
#define SERIAL_PORT UART_NUM_1
#endif
#ifdef FREENOVE_DEVKIT
#define SERIAL_RX 12
#define SERIAL_TX 13
#define SERIAL_PORT UART_NUM_1
#endif
#ifdef WAVESHARE_ESP32S3_43
#define SERIAL_RX 4
#define SERIAL_TX 5
#define SERIAL_PORT UART_NUM_1
#endif
#ifdef MATOUCH_PARALLEL_43
#define SERIAL_RX 19
#define SERIAL_TX 20
#define SERIAL_PORT UART_NUM_1
#endif

/// @brief A serial event
typedef struct {
    /// @brief The command
    uint8_t cmd;
    /// @brief The command argument
    uint8_t arg;
} serial_event_t;
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initialize the serial port
int serial_init(void);
/// @brief Get an event
/// @param out_event The event
/// @return zero if data was retrieved, otherwise non-zero
int serial_get_event(serial_event_t* out_event);
/// @brief Send an alarm to the serial port
/// @param i The index of the alarm to throw
/// @return 0 on success, nonzero on error
int serial_send_event(const serial_event_t* event);
#ifdef __cplusplus
}
#endif

#endif // SERIAL_H