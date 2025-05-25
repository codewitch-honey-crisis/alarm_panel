#define DEFAULT_PORT 8080
#define MAX_HANDLERS (HTTPD_RESPONSE_HANDLER_COUNT+5)
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include "alarm.h"
#include "httpd.h"
#include "ws_server.h"



#include "httpd_content.h"

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
        alarm_unlock();
    }
}


static volatile int socket_task_exit = 0;

static DWORD WINAPI alarm_notify_task(void* arg) {
    char old_values[alarm_count];
    alarm_lock();
    memcpy(old_values, alarm_values, sizeof(bool) * alarm_count);
    alarm_unlock();
    while (!socket_task_exit) {
        alarm_lock();
        if (0 != memcmp(old_values, alarm_values, sizeof(bool) * alarm_count)) {
            memcpy(old_values, alarm_values, sizeof(bool) * alarm_count);
            alarm_unlock();
            uint8_t buf[5];
            buf[0] = alarm_count;
            uint32_t vals = alarm_pack_values(old_values);
            vals = htonl(vals);
            memcpy(buf + 1, &vals, sizeof(uint32_t));
            ws_srv_frame_t frame;
            frame.final = 1;
            frame.fragmented = 0;
            frame.len = 5;
            frame.payload = buf;
            frame.type = WS_SRV_TYPE_BINARY;
            httpd_broadcast_ws_frame(&frame);
        } else {
            alarm_unlock();
        }
    }
    return TRUE;
}
static void on_socket_connect(const char* path_and_query,void* state) {
    puts("on connect");
    fflush(stdout);
}
static void on_socket_receive(const ws_srv_frame_t* frame, void* state) {
    puts("on receive");
    fflush(stdout);
    uint8_t* data = (uint8_t*)frame->payload;
    if (frame->masked) {
        for (int i = 0; i < frame->len; ++i) {
            data[i] ^= frame->mask_key[i % 4];
        }
    }
    if (frame->len == 5) {
        if (data[0] == alarm_count) {
            uint32_t vals;
            memcpy(&vals, data + 1, sizeof(uint32_t));
            vals = htonl(vals);
            char new_values[alarm_count];
            alarm_unpack_values(vals, alarm_count, new_values);
            alarm_lock();
            for (int i = 0; i < alarm_count; ++i) {
                alarm_enable(i, new_values[i]);
            }
            alarm_unlock();
            // ui_update_switches();
        } else {
            puts("alarm count doesn't match");
        }
    }
}
static void on_request(const char* method, const char* path_and_query, void* arg, void* state) {
    typedef void(*handler_fn_t)(void*);
    handler_fn_t h = (handler_fn_t)state;
    parse_url_and_apply(path_and_query);
    h(arg); 
}
int main(int argc, char** argv) {
    httpd_init(DEFAULT_PORT,MAX_HANDLERS); // max handlers is ignored on windows
    for (size_t i = 0; i < HTTPD_RESPONSE_HANDLER_COUNT; ++i) {
        if (httpd_response_handlers[i].handler != nullptr) {
            httpd_register_handler(httpd_response_handlers[i].path_encoded, on_request, httpd_response_handlers[i].handler);
        }
    }
    httpd_register_websocket("/socket", on_socket_connect, NULL, on_socket_receive, NULL);
    DWORD ws_thread;
    CreateThread(NULL,  // default security attributes
                 0,     // default stack size
                 (LPTHREAD_START_ROUTINE)alarm_notify_task,
                 NULL,  // no thread function arguments
                 0,     // default creation flags
                 &ws_thread);

    puts("Waiting for connection...");
    fflush(stdout);
    BOOL ret;
    MSG msg;
    while (ret = GetMessage(&msg, NULL, 0, 0)) {
        if (ret == -1) {
            return -1;
        }

        TranslateMessage(&msg);

        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) {
            socket_task_exit = 1;
            break;
        }
    }
    httpd_end();
    WSACleanup();
    return 0;
}
