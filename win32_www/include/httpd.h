#ifndef HTTPD_H
#define HTTPD_H
#include <stdint.h>
#include "ws_server.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>

//typedef void (*httpd_handler_func_t)(void *);

typedef struct {
    //httpd_handler_func_t handler;
    char method[32];
    char path_and_query[512];
    char ws_key[29];
    int error;
    void* sinfo;
} httpd_context_t;

#ifdef __cplusplus
extern "C" {
#endif
void httpd_send_block(const char *data, size_t len, void *arg);
void httpd_broadcast_ws_frame(const ws_srv_frame_t* frame);
int httpd_send_ws_frame(const ws_srv_frame_t* frame, void* arg);
const char *httpd_crack_query(const char *url_part, char *name, char *value);
int httpd_initialize(uint16_t port);
void httpd_deinitialize();
void httpd_on_ws_data_callback(void(*on_data_callback)(const ws_srv_frame_t* frame, void* arg, void* state), void* state);
void httpd_on_request_callback(void(*on_request_callback)(httpd_context_t* context, void* state), void* state);
#ifdef __cplusplus
}
#endif

#endif // HTTPD_H

