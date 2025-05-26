#ifndef HTTPD_H
#define HTTPD_H
#include <stdint.h>
#include "ws_server.h"

#ifdef __cplusplus
extern "C" {
#endif
/// @brief Sends data over a connected socket
/// @param data The data to send
/// @param len The length of the data
/// @param arg The context argument (contains info like platform specific socket info)
void httpd_send_block(const char *data, size_t len, void *arg);
/// @brief Splits a query string into name/value pairs
/// @param url_part The next chuck of the url to parse
/// @param out_name The buffer to hold the name
/// @param out_value The buffer to hold the value
/// @return The next part of the query string to parse, or NULL if done
const char *httpd_crack_query(const char *url_part, char *out_name, char *out_value);
/// @brief Initialize the HTTPD service
/// @param port The port to listen on
/// @param max_handler_count The maximum number of handlers that may be registered (including websockets)
/// @return 0 if success, non-zero if error
int httpd_init(uint16_t port, size_t max_handler_count);
/// @brief Ends the HTTPD service
void httpd_end(void);
/// @brief Adds an HTTP request handler
/// @param path The path to handle (url encoded)
/// @param on_request_callback The request callback function
/// @param on_request_callback_state User defined state to pass to the callback
/// @return 0 on success, non-zero on error
int httpd_register_handler(const char* path,void(*on_request_callback)(const char* method, const char* path_and_query, void*arg, void*state), void* on_request_callback_state);
/// @brief Adds a websocket listener
/// @param path The path to handle (url encoded)
/// @param on_connect_callback The function called on connect (may be null)
/// @param on_connect_callback_state User defined state to pass to the callback
/// @param on_receive_callback The function called when data is received
/// @param on_receive_callback_state User defined state to pass to the callback
/// @return 
int httpd_register_websocket(const char* path,void(*on_connect_callback)(const char* path_and_query, void*state),void* on_connect_callback_state,void(*on_receive_callback)(const ws_srv_frame_t* frame, void* state), void* on_receive_callback_state);
/// @brief Broadcasts data to all connected web sockets
/// @param path_and_query The path and query string to broadcast to. This must be the same as what was used to connect, or NULL to broadcast to all sockets
/// @param frame The websocket frame to send
/// @return 0 on success, non-zero on error
int httpd_broadcast_ws_frame(const char* path_and_query, const ws_srv_frame_t* frame);
/// @brief Sends a frame to a web socket
/// @param frame The frame to send
/// @param arg The context argument (contains info like platform specific socket info)
/// @return 0 on success, non-zero on error
int httpd_send_ws_frame(const ws_srv_frame_t* frame, void* arg);

#ifdef __cplusplus
}
#endif

#endif // HTTPD_H
