#include "config.h"
#include "serial.h"

#include <memory.h>
#include <stdio.h>

#include "alarm.h"
#include "driver/gpio.h"
#include "driver/uart.h"

void serial_init() {
    uart_config_t uart_config;
    memset(&uart_config, 0, sizeof(uart_config));
    uart_config.baud_rate = ALARM_BAUD;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 256, 0, 20, NULL, 0));
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, SERIAL_TX, SERIAL_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}
int serial_get_event(serial_event_t* out_event) {
    uint8_t payload[2];
    if (out_event && sizeof(payload) == uart_read_bytes(UART_NUM_1, &payload,
                                                        sizeof(payload), 0)) {
        out_event->cmd = payload[0];
        out_event->arg = payload[1];
        return 0;
    }
    return -1;
}
void serial_send_alarm(size_t i) {
    if (i >= alarm_count) return;
    //printf("%s alarm #%d\n", alarm_values[i] ? "setting" : "clearing",
    //       (int)i + 1);
    uint8_t payload[2];
    payload[0] = alarm_values[i] ? SET_ALARM : CLEAR_ALARM;
    payload[1] = i;
    uart_write_bytes(UART_NUM_1, payload, sizeof(payload));
}
