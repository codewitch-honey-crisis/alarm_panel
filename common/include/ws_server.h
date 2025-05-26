#ifndef HTCW_WS_SERVER_H
#define HTCW_WS_SERVER_H
#include <stdint.h>
#include <stddef.h>
#ifdef ESP_PLATFORM
#include <stddef.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"
#endif
/// @brief Indicates the type of websocket frame
typedef enum {
    WS_SRV_TYPE_CONTINUE   = 0x0,
    WS_SRV_TYPE_TEXT       = 0x1,
    WS_SRV_TYPE_BINARY     = 0x2,
    WS_SRV_TYPE_CLOSE      = 0x8,
    WS_SRV_TYPE_PING       = 0x9,
    WS_SRV_TYPE_PONG       = 0xA
} ws_srv_msg_type_t;
// ESP32 has built in websocket support
#ifndef ESP_PLATFORM
typedef enum {
    WS_SRV_RECV_ERROR = -1,
    WS_SRV_RECV_DONE = 0,
    WS_SRC_RECV_MORE = 1
} ws_srv_recv_result_t;
#endif
/// @brief A websocket frame
typedef struct {
    /// @brief Non-zero if this is the final frame in a fragmented series or if this a single non-fragmented frame
    char final;
    /// @brief True if this is part of a fragmented series
    char fragmented;
    /// @brief The type of websocket message
    ws_srv_msg_type_t type;
    /// @brief Non-zero if the message is masked. Client messages are always masked
    char masked;
    /// @brief A 4 byte nonce used to mask the data, if masked is set
    uint8_t mask_key[4];
    /// @brief The length of the payload
    uint64_t len;
    /// @brief The payload data
    uint8_t *payload;          
} ws_srv_frame_t;

#ifdef __cplusplus
extern "C" {
#endif
#ifndef ESP_PLATFORM
/// @brief Computes a websocket sec response code
/// @param ws_sec The key from the HTTP header
/// @param ws_sec_length The length of the key
/// @param out_buffer An out buffer to hold the result
/// @param out_buffer_length The length of the out buffer
/// @return 
int ws_srv_compute_sec(const char* ws_sec, size_t ws_sec_length, char* out_buffer, size_t out_buffer_length);
/// @brief Sends a frame over a socket
/// @param frame The frame to send
/// @param socket_send A method used to write to a socket
/// @param socket_send_state The user context to send with the method
/// @return 0 if success, or non-zero if error
int ws_srv_send_frame(const ws_srv_frame_t* frame,int(*socket_send)(const void* data,size_t length, void*state), void* socket_send_state);
/// @brief Receives a frame
/// @param socket_recv The socket receive method
/// @param socket_recv_state The user context to send with the method
/// @param out_frame The frame
/// @return 0 if success, otherwise non-zero
int ws_srv_recv_frame(int(*socket_recv)(void* out_data,size_t* in_out_data_length, void*state),void* socket_recv_state,ws_srv_frame_t* out_frame);
#else
/// @brief Converts a ws_srv_frame_t instance to an httpd_ws_frame_t instance
/// @param frame The ws_srv_frame_t instance
/// @param out_esp32_frame The httpd_ws_frame_t holding the out instance
void ws_srv_frame_to_esp32(const ws_srv_frame_t* frame,httpd_ws_frame_t* out_esp32_frame);
/// @brief Converts a httpd_ws_frame_t instance to a ws_srv_frame_t instance 
/// @param esp32_frame The httpd_ws_Frame_t instance
/// @param out_frame The ws_srv_frame_t holding the out instance
void ws_srv_esp32_to_frame(const httpd_ws_frame_t* esp32_frame, ws_srv_frame_t* out_frame);
#endif
/// @brief Unmaskes a masked payload
/// @param frame The frame to unmask
/// @param payload The payload pointer. This may be the frame's payload pointer, as unmasking inplace is supported
void ws_srv_unmask_payload(const ws_srv_frame_t* frame, uint8_t* payload);

#ifdef __cplusplus
}
#endif
#endif // HTCW_WS_SERVER_H