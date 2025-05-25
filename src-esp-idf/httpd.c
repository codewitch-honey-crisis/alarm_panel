#include "httpd.h"

#include <stddef.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"

static httpd_handle_t httpd_handle = NULL;
typedef struct {
    httpd_handle_t hd;
    int fd;
    char path_and_query[513];
    void(*on_request_callback)(const char* method, const char* path_and_query, void*arg, void*state);
    void*on_request_callback_state;
} httpd_async_resp_arg_t;
typedef enum {
    HTYPE_NONE=0,
    HTYPE_HTTP,
    HTYPE_WS
} httpd_handler_type_t;
typedef struct {
    httpd_handler_type_t type;
    union {
        // either
        struct { // http
            void(*on_request)(const char* method, const char* path_and_query, void*arg, void*state);
            void* on_request_state;
        } http;
        // or
        struct { // websocket
            void(*on_connect)(const char* path_and_query, void*state);
            void* on_connect_state;
            void(*on_receive)(const ws_srv_frame_t* frame, void* state);
            void* on_receive_state;
        } websocket;
    };
} httpd_handler_context_t;
static httpd_handler_context_t* httpd_handler_contexts = NULL;
static size_t httpd_max_handlers;

const char* httpd_crack_query(const char* url_part, char* name,
                                     char* value) {
    if (url_part == NULL || !*url_part) return NULL;
    const char start = *url_part;
    if (start == '&' || start == '?') {
        ++url_part;
    }
    size_t i = 0;
    char* name_cur = name;
    while (*url_part && *url_part != '=' && *url_part != '&') {
        if (i < 64) {
            *name_cur++ = *url_part;
        }
        ++url_part;
        ++i;
    }
    *name_cur = '\0';
    if (!*url_part || *url_part == '&') {
        *value = '\0';
        return url_part;
    }
    ++url_part;
    i = 0;
    char* value_cur = value;
    while (*url_part && *url_part != '&') {
        if (i < 64) {
            *value_cur++ = *url_part;
        }
        ++url_part;
        ++i;
    }
    *value_cur = '\0';
    return url_part;
}

void httpd_send_block(const char* data, size_t len, void* arg) {
    if(!httpd_handle) {
        return;
    }
    if (!data || !*data || !len) {
        return;
    }
    httpd_async_resp_arg_t* resp_arg = (httpd_async_resp_arg_t*)arg;
    httpd_socket_send(resp_arg->hd, resp_arg->fd, data, len, 0);
}

#define DHND (httpd_descs_sync)
static SemaphoreHandle_t httpd_descs_sync=NULL;
static int httpd_descs[CONFIG_LWIP_MAX_SOCKETS];

static esp_err_t httpd_socket_handler(httpd_req_t* req) {
    int ctxi = (int)req->user_ctx;
    httpd_handler_context_t* ctx = &httpd_handler_contexts[ctxi];
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    esp_err_t ret;
    if(req->method != HTTP_GET) {
        ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
        if (ret != ESP_OK) {
            puts("httpd_ws_recv_frame get length failed");
            return ret;
        }
        ws_pkt.payload = (uint8_t*)malloc(ws_pkt.len);
        if (ws_pkt.payload == NULL) {
            return ESP_ERR_NO_MEM;
        }
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            puts("httpd_ws_recv_frame failed");
            free(ws_pkt.payload);
            return ret;
        }
        ws_srv_frame_t frame;
        ws_srv_esp32_to_frame(&ws_pkt,&frame);
        ctx->websocket.on_receive(&frame,ctx->websocket.on_receive_state);
        
    } else {
        int fd = httpd_req_to_sockfd(req);
        if(fd>-1) {
            xSemaphoreTake(DHND,portMAX_DELAY);
            for(int i = 0;i<CONFIG_LWIP_MAX_SOCKETS;++i) {
                if(httpd_descs[i]<0) {
                    httpd_descs[i]=fd;
                    break;
                }
            }
            xSemaphoreGive(DHND);
        }
    }
    return ESP_OK;
}

int httpd_init(uint16_t port, size_t max_handler_count) {
    if (httpd_handle != NULL) {
        return 0;
    }
    if(max_handler_count<1) {
        return -1;
    }
    httpd_max_handlers=max_handler_count;
    for(int i = 0;i<CONFIG_LWIP_MAX_SOCKETS;++i) {
        httpd_descs[i]=-1;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = max_handler_count;
    config.server_port = port;
    config.max_open_sockets = (CONFIG_LWIP_MAX_SOCKETS - 3);
    httpd_handler_contexts = (httpd_handler_context_t*)malloc(sizeof(httpd_handler_context_t)*max_handler_count);
    memset(httpd_handler_contexts,0,sizeof(httpd_handler_context_t)*max_handler_count);
    if(httpd_handler_contexts==NULL) {
        return -1;
    }
    httpd_descs_sync = xSemaphoreCreateMutex();
    if(httpd_descs_sync==NULL) {
        free(httpd_handler_contexts);
        httpd_handler_contexts = NULL;
        return -1;
    }
    if(ESP_OK!=httpd_start(&httpd_handle, &config)) {
        free(httpd_handler_contexts);
        httpd_handler_contexts = NULL;
        return -1;
    }
    return 0;
}
void httpd_end() {
    if (httpd_handle == NULL) {
        return;
    }
    ESP_ERROR_CHECK(httpd_stop(httpd_handle));
    if(httpd_descs_sync!=NULL) {
        vSemaphoreDelete(httpd_descs_sync);
        httpd_descs_sync=NULL;
    }
    httpd_handle = NULL;  
    free(httpd_handler_contexts);
    httpd_handler_contexts=NULL;
}

void httpd_async_response(void* arg) {
    httpd_async_resp_arg_t* resp_arg = (httpd_async_resp_arg_t*)arg;
    resp_arg->on_request_callback("GET",resp_arg->path_and_query,resp_arg,resp_arg->on_request_callback_state);
}
esp_err_t httpd_request_thunk(httpd_req_t* req) {
    typedef void(*on_request_callback_t)(const char* method, const char* path_and_query, void*arg, void*state);
    int ctxi = (int)req->user_ctx;
    httpd_handler_context_t* ctx = &httpd_handler_contexts[ctxi];
    
    on_request_callback_t cb = ctx->http.on_request;
    if(cb==NULL) return ESP_FAIL;
    httpd_async_resp_arg_t* resp_arg = (httpd_async_resp_arg_t*)malloc(sizeof(httpd_async_resp_arg_t));
    if(resp_arg==NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->fd = httpd_req_to_sockfd(req);
    if(resp_arg->fd==-1) {
        free(resp_arg);
        return ESP_FAIL;
    }
    resp_arg->hd = httpd_handle;
    resp_arg->on_request_callback = cb;
    resp_arg->on_request_callback_state = ctx->http.on_request_state;
    strncpy(resp_arg->path_and_query,req->uri,sizeof(req->uri));
    return httpd_queue_work(httpd_handle,httpd_async_response,resp_arg);
}

int httpd_register_handler(const char* path,void(*on_request_callback)(const char* method, const char* path_and_query, void*arg, void*state), void* on_request_callback_state) {
    if(!httpd_handle) {
        return -1;
    }
    httpd_uri_t h;
    memset(&h,0,sizeof(httpd_uri_t));
    // TODO: add more methods
    h.method = HTTP_GET;
    h.uri = path;
    int i = 0;
    for(;i<httpd_max_handlers;++i) {
        if(httpd_handler_contexts[i].type==HTYPE_NONE) {
            httpd_handler_context_t* hctx = &httpd_handler_contexts[i];
            hctx->type = HTYPE_HTTP;
            hctx->http.on_request = on_request_callback;
            hctx->http.on_request_state = on_request_callback_state;
            break;
        }
    }
    h.user_ctx = (void*)i;
    h.handler = httpd_request_thunk;
    if(ESP_OK!=httpd_register_uri_handler(httpd_handle,&h)) {
        return -1;
    }
    return 0;
}
int httpd_register_websocket(const char* path,void(*on_connect_callback)(const char* path_and_query, void*state),void* on_connect_callback_state,void(*on_receive_callback)(const ws_srv_frame_t* frame, void* state), void* on_receive_callback_state) {
#ifdef CONFIG_HTTPD_WS_SUPPORT
    if(!httpd_handle) {
        return -1;
    }
    httpd_uri_t h;
    memset(&h,0,sizeof(httpd_uri_t));
    h.method = HTTP_GET;
    h.uri = path;
    h.is_websocket = 1;
    int i = 0;
    for(;i<httpd_max_handlers;++i) {
        if(httpd_handler_contexts[i].type==HTYPE_NONE) {
            httpd_handler_context_t* hctx = &httpd_handler_contexts[i];
            hctx->type = HTYPE_WS;
            hctx->websocket.on_connect = on_connect_callback;
            hctx->websocket.on_connect_state = on_connect_callback_state;
            hctx->websocket.on_receive = on_receive_callback;
            hctx->websocket.on_receive_state = on_connect_callback_state;
            break;
        }
    }
    h.user_ctx = (void*)i;
    h.handler = httpd_socket_handler;
    if(ESP_OK!=httpd_register_uri_handler(httpd_handle,&h)) {
        return -1;
    }
    return 0;
#else
    return -1; // not supported
#endif
}
int httpd_broadcast_ws_frame(const ws_srv_frame_t* frame) {
    if(!httpd_handle) {
        return -1;
    }
    int fds[CONFIG_LWIP_MAX_SOCKETS];
    esp_err_t ret;
    xSemaphoreTake(DHND,portMAX_DELAY);
    memcpy(fds,httpd_descs,sizeof(int)*CONFIG_LWIP_MAX_SOCKETS);
    xSemaphoreGive(DHND);
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));    
    ws_srv_frame_to_esp32(frame,&ws_pkt);
    for(int i = 0;i<CONFIG_LWIP_MAX_SOCKETS;++i) {
        int fd = fds[i];
        if(fd!=-1) {
            ret = httpd_ws_send_frame_async(httpd_handle,fd, &ws_pkt);
            if (ret != ESP_OK) {
                // Client likely disconnected - close the socket
                httpd_sess_trigger_close(httpd_handle, fd);
                fds[i] = -1;
                xSemaphoreTake(DHND,portMAX_DELAY);
                httpd_descs[i]=-1;
                xSemaphoreGive(DHND);
            }
        }
    }
    return 0;
}
int httpd_send_ws_frame(const ws_srv_frame_t* frame, void* arg) {
    if(!httpd_handle) {
        return -1;
    }
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));    
    ws_srv_frame_to_esp32(frame,&ws_pkt);
    esp_err_t ret = httpd_ws_send_frame_async(httpd_handle,(int)arg, &ws_pkt);
    if (ret != ESP_OK) {
        // Client likely disconnected - close the socket
        httpd_sess_trigger_close(httpd_handle, (int)arg);
        xSemaphoreTake(DHND,portMAX_DELAY);
        for(int i = 0;i<CONFIG_LWIP_MAX_SOCKETS;++i) {
            if(httpd_descs[i]==(int)arg) {
                httpd_descs[i]=-1;
                break;
            }
        }
        xSemaphoreGive(DHND);
        return -1;
    }
    return 0;
}
