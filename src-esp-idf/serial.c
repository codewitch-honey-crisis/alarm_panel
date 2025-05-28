#include "config.h"
#include "serial.h"

#include <memory.h>
#include <stdio.h>

#include "alarm.h"
#include "driver/gpio.h"
#include "driver/uart.h"

int serial_init(void) {
    uart_config_t uart_config;
    memset(&uart_config, 0, sizeof(uart_config));
    uart_config.baud_rate = ALARM_BAUD;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    if(ESP_OK!=uart_driver_install(SERIAL_PORT, 256, 0, 20, NULL, 0)) {
        return -1;
    }
    if(ESP_OK!=uart_param_config(SERIAL_PORT, &uart_config)) {
        uart_driver_delete(SERIAL_PORT);
        return -1;
    }
    if(ESP_OK!=uart_set_pin(SERIAL_PORT, SERIAL_TX, SERIAL_RX,UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) {
        uart_driver_delete(SERIAL_PORT);
        return -1;
    }
    return 0;
}
int serial_get_event(serial_event_t* out_event) {
    uint8_t payload[2];
    if (out_event && sizeof(payload) == uart_read_bytes(SERIAL_PORT, &payload,
                                                        sizeof(payload), 0)) {
        out_event->cmd = payload[0];
        out_event->arg = payload[1];
        return 0;
    }
    return -1;
}
void serial_send_event(const serial_event_t *event) {
    uart_write_bytes(SERIAL_PORT, (const uint8_t*)event, sizeof(serial_event_t));
}
