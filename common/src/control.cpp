#ifdef LOCAL_PC
#define DEFAULT_PORT 8080
#else
#define DEFAULT_PORT 80
#endif
#define MAX_WEB_SOCKET_URIS 1
#define MAX_HANDLERS (HTTPD_RESPONSE_HANDLER_COUNT + MAX_WEB_SOCKET_URIS)
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "alarm.h"
#include "config.h"
#include "display.h"
#include "httpd.h"
#include "network.h"
#include "power.h"
#include "serial.h"
#include "i2c.h"
#include "spi.h"
#include "task.h"
#include "ui.h"
#define HTTPD_CONTENT_IMPLEMENTATION
#include "httpd_content.h"

// swap byte order from little<->big endian
static uint32_t swap_endian_32(uint32_t number) {
  return ((number & 0xFF) << 24) | ((number & 0xFF00) << 8) | ((number & 0xFF0000) >> 8) | (number >> 24);
}

// parse the query parameters and apply them to the alarms
static void parse_url_and_apply(const char* url) {
    const char* query = strchr(url, '?');
    bool has_set = false;
    char name[64];
    char value[64];
    bool req_values[alarm_count];
    if (query != nullptr) {
        memset(req_values, 0, sizeof(req_values));
        while (1) {
            query = httpd_crack_query(query, name, value);
            if (!query) {
                break;
            }
            if (!strcmp("set", name)) {
                has_set = true;
            } else if (!strcmp("a", name)) {
                char* endsz;
                
                long l = strtol(value, &endsz, 10);
                if (l >= 0 && l < alarm_count) {
                    req_values[l] = true;
                }
            }
        }
    }
    if (has_set) {
        alarm_lock();
        for (size_t i = 0; i < alarm_count; ++i) {
            alarm_values[i] = req_values[i];
        }
        ui_update_switches(false);
        alarm_unlock();
        
    }
}

static void on_ws_connect(const char* path_and_query, void* state) {
    puts("Websocket connected");
}
static void on_ws_receive(const ws_srv_frame_t* frame, void* arg, void* state) {
    if(frame->len==1) { // request a refresh
        char old_values[alarm_count];
        alarm_lock();
        memcpy(old_values, alarm_values, sizeof(char) * ALARM_COUNT);
        alarm_unlock();
        uint32_t vals = alarm_pack_values(old_values,alarm_count);
        uint8_t buf[5];
        buf[0] = alarm_count;
        vals = swap_endian_32(vals);
        memcpy(buf + 1, &vals, sizeof(vals));
        ws_srv_frame_t frame;
        frame.final = 1;
        frame.fragmented = 0;
        frame.len = sizeof(buf);
        frame.payload = buf;
        frame.masked = 0;
        frame.type = WS_SRV_TYPE_BINARY;
        httpd_send_ws_frame(&frame,arg);
    } else if (frame->len == 5) { // set the alarms
        ws_srv_unmask_payload(frame, frame->payload);
        if (frame->payload[0] == alarm_count) {
            uint32_t data;
            memcpy(&data, frame->payload + 1, sizeof(uint32_t));
            data = swap_endian_32(data);
            char new_values[alarm_count];
            alarm_unpack_values(data, new_values, alarm_count);
            alarm_lock();
            for (int i = 0; i < alarm_count; ++i) {
                alarm_enable(i, new_values[i]);
            }
            ui_update_switches(false);
            alarm_unlock();
        } else {
            puts("alarm count doesn't match");
        }
    } else {
        puts("Unknown data received");
    }
}

static void on_request(const char* method, const char* path_and_query, void* arg, void* state) {
    typedef void (*handler_fn_t)(void*);
    handler_fn_t resp_handler = (handler_fn_t)state;
    parse_url_and_apply(path_and_query);
    resp_handler(arg);
}

static void register_handlers() {
    for (size_t i = 0; i < HTTPD_RESPONSE_HANDLER_COUNT; ++i) {
        if (0 != httpd_register_handler(httpd_response_handlers[i].path_encoded, on_request, (void*)httpd_response_handlers[i].handler)) {
            puts("Error registering handler");
            return;
        }
    }
    if (0 != httpd_register_websocket("/socket", on_ws_connect, nullptr, on_ws_receive, nullptr)) {
        puts("Error registering websocket");
    }
}

static void alarms_changed_socket_task(void* arg) {
    char old_values[alarm_count];
    alarm_lock();
    memcpy(old_values, alarm_values, sizeof(char) * ALARM_COUNT);
    alarm_unlock();
    while (1) {
        alarm_lock();
        if (0 != memcmp(alarm_values, old_values, sizeof(char) * ALARM_COUNT)) {
            memcpy(old_values, alarm_values, sizeof(char) * ALARM_COUNT);
            alarm_unlock();
            uint32_t vals = alarm_pack_values(old_values,alarm_count);
            uint8_t buf[5];
            buf[0] = alarm_count;
            vals = swap_endian_32(vals);
            memcpy(buf + 1, &vals, sizeof(vals));
            ws_srv_frame_t frame;
            frame.final = 1;
            frame.fragmented = 0;
            frame.len = sizeof(buf);
            frame.payload = buf;
            frame.masked = 0;
            frame.type = WS_SRV_TYPE_BINARY;
            httpd_broadcast_ws_frame("/socket",&frame);
        } else {
            alarm_unlock();
        }
        task_delay(0);
    }
}

extern "C" void loop() {
    ui_update();
    serial_event_t evt;
    if (0 == serial_get_event(&evt)) {
        switch (evt.cmd) {
            case ALARM_THROWN:
                alarm_lock();
                alarm_enable(evt.arg, true);
                alarm_unlock();
                break;
            default:
                puts("Unknown event received");
                break;
        }
    }
    if (!ui_web_link_visible()) {  // not connected yet
        if (net_status() == NET_CONNECTED) {
            puts("Connected");
            // initialize the web server
            puts("Starting web server");
            httpd_init(DEFAULT_PORT, MAX_HANDLERS);
            register_handlers();
            // set the QR text to our website
            static char qr_text[256];
            strncpy(qr_text, "http://", sizeof(qr_text) - 1);
            ;
            char* qr_next = qr_text + 7;
            net_address(qr_next, 249);
            if(DEFAULT_PORT!=80) {
                qr_next = qr_text + strlen(qr_text);
                strncat(qr_next, ":", 2);
                qr_next += 1;
                snprintf(qr_next, 256 - (qr_next - qr_text) + 1, "%d", (int)DEFAULT_PORT);
            }
            puts(qr_text);
            ui_web_link(qr_text);
        }
    } else {
        if (net_status() == NET_CONNECT_FAILED) {
            httpd_end();
            net_end();
            ui_web_link(nullptr);
            puts("A network failure was encountered. Retrying.");
            net_init();
        }
    }
}
extern "C" void run() {
    assert(!i2c_master_init());
    assert(!power_init());  // do this first
    assert(!spi_init());    // used by the LCD and SD reader
    assert(!display_init());
    assert(!serial_init());
    assert(!alarm_init());
    assert(!ui_init());
    assert(!net_init());
    assert(task_init(alarms_changed_socket_task, TASK_STACK_DEFAULT, TASK_AFFINITY_ANY, NULL));
    // clear any junk from the second serial:
    serial_event_t evt;
    while (0 == serial_get_event(&evt));
}
