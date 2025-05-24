#ifndef HTCW_WS_SERVER_H
#define HTCW_WS_SERVER_H
#include <stdint.h>
#include <stddef.h>
typedef enum {
    WS_SRV_TYPE_CONTINUE   = 0x0,
    WS_SRV_TYPE_TEXT       = 0x1,
    WS_SRV_TYPE_BINARY     = 0x2,
    WS_SRV_TYPE_CLOSE      = 0x8,
    WS_SRV_TYPE_PING       = 0x9,
    WS_SRV_TYPE_PONG       = 0xA
} ws_srv_msg_type_t;

typedef enum {
    WS_SRV_RECV_ERROR = -1,
    WS_SRV_RECV_DONE = 0,
    WS_SRC_RECV_MORE = 1
} ws_srv_recv_result_t;

typedef struct {
    char final;                     /*!< Final frame:
                                     For received frames this field indicates whether the `FIN` flag was set.
                                     For frames to be transmitted, this field is only used if the `fragmented`
                                         option is set as well. If `fragmented` is false, the `FIN` flag is set
                                         by default, marking the ws_frame as a complete/unfragmented message
                                         (esp_http_server doesn't automatically fragment messages) */
    char fragmented;            /*!< Indication that the frame allocated for transmission is a message fragment,
                                     so the `FIN` flag is set manually according to the `final` option.
                                     This flag is never set for received messages */
    ws_srv_msg_type_t type;       /*!< WebSocket frame type */
    char masked;
    uint8_t mask_key[4];
    uint64_t len;                 /*!< Length of the WebSocket data */
    const uint8_t *send_payload;           /*!< Pre-allocated data buffer */  
} ws_srv_frame_t;

#ifdef __cplusplus
extern "C" {
#endif
int ws_srv_compute_sec(const char* ws_sec, size_t ws_sec_length, char* out_buffer, size_t out_buffer_length);
int ws_srv_send_frame(const ws_srv_frame_t* frame,int(*socket_send)(const void* data,size_t length, void*state), void* socket_send_state);
int ws_srv_recv_frame(int(*socket_recv)(void* out_data,size_t* in_out_data_length, void*state),void* socket_recv_state,ws_srv_frame_t* out_frame);
#ifdef __cplusplus
}
#endif
#endif // HTCW_WS_SERVER_H