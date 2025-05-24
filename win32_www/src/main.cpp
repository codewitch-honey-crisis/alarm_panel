#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include "httpd.h"
#include "ws_server.h"

constexpr const size_t alarm_count = 4;
bool alarm_values[alarm_count] = {false, false, false, false};
HANDLE alarm_sync = nullptr;
void alarm_lock() {
    WaitForSingleObject(alarm_sync,  // handle to mutex
                        INFINITE);   // no time-out interval
}
void alarm_unlock() { ReleaseMutex(alarm_sync); }
void alarm_enable(size_t index, bool value) {
    if(index>=alarm_count) return;
    alarm_values[index]=value;
}
static void alarm_unpack_values(uint32_t data, size_t length, bool* out_values) {
    if(length<1 || length>32 || out_values==nullptr) {return;}
    // unpack the alarm values into the buffer
    for (int i = 0 ; i < length; ++i) {
        out_values[i]=(data&1);
        data>>=1;
    }
}

#define HTTPD_CONTENT_IMPLEMENTATION
#include ",,/../../include/httpd_content.h"

#define DEFAULT_PORT 8080


static uint32_t alarm_pack_values(bool *values) {
    // pack the alarm values into the buffer
    uint32_t accum = 0;
    size_t i = alarm_count;
    while (i) {
        accum <<= 1;
        accum |= (int)values[i - 1];
        --i;
    }
    return accum;
}


static void parse_url_and_apply_alarms(const char *url) {
    const char *query = strchr(url, '?');
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
                char *endsz;
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
static void on_request(httpd_context_t* ctx,void* state) {
    int hi = httpd_response_handler_match(ctx->path_and_query);
    if(hi==-1) {
        httpd_content_404_clasp(ctx);
        return;
    }
    parse_url_and_apply_alarms(ctx->path_and_query);
    httpd_response_handlers[hi].handler(ctx);
}
static volatile int socket_task_exit = 0;
// these are globals we use in the page
static DWORD WINAPI alarm_notify_task(void *arg) {
    bool old_values[alarm_count];
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
            frame.send_payload = buf;
            frame.type = WS_SRV_TYPE_BINARY;
            httpd_broadcast_ws_frame(&frame);
        } else {
            alarm_unlock();
        }
    }
    return TRUE;
}
static void on_ws_data(const ws_srv_frame_t* frame, void* arg, void* state) {
    uint8_t* data = (uint8_t*)frame->send_payload;
    if(frame->masked) {
        for(int i = 0;i<frame->len;++i) {
            data[i]^=frame->mask_key[i%4];
        }
    }
    if(frame->len==5) {
        if(data[0]==alarm_count) {
                uint32_t vals;
                memcpy(&vals,data+1,sizeof(uint32_t));
                vals = htonl(vals);
                bool new_values[alarm_count];
                alarm_unpack_values(vals,alarm_count,new_values);
                alarm_lock();
                for(int i = 0;i<alarm_count;++i) {
                    alarm_enable(i,new_values[i]);
                }
                alarm_unlock();
                //ui_update_switches();
            } else {
                puts("alarm count doesn't match");
            }
    }
}


int main(int argc, char **argv) {
    httpd_initialize(DEFAULT_PORT);
    httpd_on_ws_data_callback(on_ws_data,nullptr);
    httpd_on_request_callback(on_request,nullptr);
    alarm_sync = CreateMutex(NULL,   // default security attributes
                             FALSE,  // initially not owned
                             NULL);  // unnamed mutex
    
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
        if (ret == -1)

        {
            
            return -1;
        }

        TranslateMessage(&msg);

        DispatchMessage(&msg);
        if(msg.message==WM_QUIT) {
            socket_task_exit = 1;
            break;
        }
    }
    httpd_deinitialize();
    WSACleanup();
    return 0;
}

