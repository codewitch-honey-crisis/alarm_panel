#ifndef HTTPD_H
#define HTTPD_H
#include <stdint.h>
#include "ws_server.h"

#ifdef __cplusplus
extern "C" {
#endif
void httpd_send_block(const char *data, size_t len, void *arg);
const char *httpd_crack_query(const char *url_part, char *name, char *value);
int httpd_init(uint16_t port, size_t max_handler_count);
void httpd_end();
int httpd_register_handler(const char* path,void(*on_request_callback)(const char* method, const char* path_and_query, void*arg, void*state), void* on_request_callback_state);
int httpd_register_websocket(const char* path,void(*on_connect_callback)(const char* path_and_query, void*state),void* on_connect_callback_state,void(*on_receive_callback)(const ws_srv_frame_t* frame, void* state), void* on_receive_callback_state);
int httpd_broadcast_ws_frame(const char* path_and_query, const ws_srv_frame_t* frame);
int httpd_send_ws_frame(const ws_srv_frame_t* frame, void* arg);

#ifdef __cplusplus
}
#endif

#endif // HTTPD_H
