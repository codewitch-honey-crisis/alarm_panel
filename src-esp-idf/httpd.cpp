#include "httpd.hpp"

#include <stddef.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "alarm_common.hpp"
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"
#include "ui.hpp"
#define SHND ((SemaphoreHandle_t)alarm_sync)
static void httpd_send_block(const char* data, size_t len, void* arg);
static void httpd_send_expr(int expr, void* arg);
static void httpd_send_expr(const char* expr, void* arg);

// include the generated react content
#define HTTPD_CONTENT_IMPLEMENTATION
#include "httpd_content.h"

static httpd_handle_t httpd_handle = nullptr;
struct httpd_async_resp_arg {
    httpd_handle_t hd;
    int fd;
};
static void httpd_send_chunked(httpd_async_resp_arg* resp_arg,
                               const char* buffer, size_t buffer_len) {
    char buf[64];
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    if (buffer && buffer_len) {
        itoa(buffer_len, buf, 16);
        strcat(buf, "\r\n");
        httpd_socket_send(hd, fd, buf, strlen(buf), 0);
        httpd_socket_send(hd, fd, buffer, buffer_len, 0);
        httpd_socket_send(hd, fd, "\r\n", 2, 0);
        return;
    }
    httpd_socket_send(hd, fd, "0\r\n\r\n", 5, 0);
}

static const char* httpd_crack_query(const char* url_part, char* name,
                                     char* value) {
    if (url_part == nullptr || !*url_part) return nullptr;
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
static void httpd_parse_url_and_apply_alarms(const char* url) {
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
        xSemaphoreTake(SHND,portMAX_DELAY);
        for (size_t i = 0; i < alarm_count; ++i) {
            alarm_enable(i, req_values[i]);
        }
        xSemaphoreGive(SHND);
        ui_update_switches();
    }
}
static void httpd_send_block(const char* data, size_t len, void* arg) {
    if (!data || !*data || !len) {
        return;
    }
    httpd_async_resp_arg* resp_arg = (httpd_async_resp_arg*)arg;
    httpd_socket_send(resp_arg->hd, resp_arg->fd, data, len, 0);
}
static void httpd_send_expr(int expr, void* arg) {
    httpd_async_resp_arg* resp_arg = (httpd_async_resp_arg*)arg;
    char buf[64];
    itoa(expr, buf, 10);
    httpd_send_chunked(resp_arg, buf, strlen(buf));
}
static void httpd_send_expr(const char* expr, void* arg) {
    httpd_async_resp_arg* resp_arg = (httpd_async_resp_arg*)arg;
    if (!expr || !*expr) {
        return;
    }
    httpd_send_chunked(resp_arg, expr, strlen(expr));
}

static esp_err_t httpd_request_handler(httpd_req_t* req) {
    // match the handler
    int handler_index = httpd_response_handler_match(req->uri);

    httpd_async_resp_arg* resp_arg =
        (httpd_async_resp_arg*)malloc(sizeof(httpd_async_resp_arg));
    if (resp_arg == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    httpd_parse_url_and_apply_alarms(req->uri);
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    if (resp_arg->fd < 0) {
        free(resp_arg);
        return ESP_FAIL;
    }
    httpd_work_fn_t h;
    if (handler_index == -1) {
        free(resp_arg);
        return ESP_FAIL; // shouldn't get here
    } else {
        h = httpd_response_handlers[handler_index].handler;
    }
    httpd_queue_work(req->handle, h, resp_arg);
    return ESP_OK;
}
#define DHND (httpd_descs_sync)
static SemaphoreHandle_t httpd_descs_sync=nullptr;
static TaskHandle_t httpd_socket_task_handle=nullptr;
static int httpd_descs[CONFIG_LWIP_MAX_SOCKETS];
static uint32_t pack_alarm_values(bool* values) {
    // pack the alarm values into the buffer
    uint32_t accum = 0;
    for (int i = alarm_count - 1; i >= 0; --i) {
        accum <<= 1;
        accum |= values[i];
    }
    return accum;
}
static void httpd_socket_task(void* arg) {
    bool old_values[alarm_count];
    int fds[CONFIG_LWIP_MAX_SOCKETS];
    esp_err_t ret;
    xSemaphoreTake(SHND,portMAX_DELAY);
    memcpy(old_values,alarm_values,sizeof(bool)*alarm_count);
    xSemaphoreGive(SHND);
    while(1) {
        xSemaphoreTake(SHND,portMAX_DELAY);
        if(0!=memcmp(alarm_values,old_values,sizeof(bool)*alarm_count)) {
            memcpy(old_values,alarm_values,sizeof(bool)*alarm_count);
            xSemaphoreGive(SHND);
            xSemaphoreTake(DHND,portMAX_DELAY);
            memcpy(fds,httpd_descs,sizeof(int)*CONFIG_LWIP_MAX_SOCKETS);
            xSemaphoreGive(DHND);
            uint32_t vals = pack_alarm_values(old_values);
            uint8_t buf[5];
            buf[0] = alarm_count;
            vals = __bswap32(vals);
            memcpy(buf + 1, &vals, sizeof(vals));        
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.type = HTTPD_WS_TYPE_BINARY;
            ws_pkt.len = sizeof(buf);
            ws_pkt.payload = buf;
            ws_pkt.fragmented = false;
            ws_pkt.final = true;
            for(int i = 0;i<CONFIG_LWIP_MAX_SOCKETS;++i) {
                int fd = fds[i];
                if(fd!=-1) {
                    ret = httpd_ws_send_frame_async(httpd_handle,fd, &ws_pkt);
                    if (ret != ESP_OK) {
                        // Client likely disconnected - close the socket
                        puts("websocket disconnected");
                        httpd_sess_trigger_close(httpd_handle, fd);
                        fds[i] = -1;
                        xSemaphoreTake(DHND,portMAX_DELAY);
                        httpd_descs[i]=-1;
                        xSemaphoreGive(DHND);
                    }
                }
            }
        } else {
            xSemaphoreGive(SHND);
        }
    }
}
static esp_err_t httpd_socket_handler(httpd_req_t* req) {
    httpd_ws_frame_t ws_pkt;
    uint8_t buf[5];
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    uint8_t* data = nullptr;
    esp_err_t ret;
    if(req->method != HTTP_GET) {
        ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
        if (ret != ESP_OK) {
            puts("httpd_ws_recv_frame get length failed");
            return ret;
        }
        if (ws_pkt.len >= 1) {
            // this SUCKS but we have no choice. This API is grrrr
            // we don't actually need any of this data.
            // would be nice if we could pass null into ws_pkt.payload
            ws_pkt.payload = (uint8_t*)malloc(ws_pkt.len);
            if (ws_pkt.payload == nullptr) {
                return ESP_ERR_NO_MEM;
            }
            ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
            if (ret != ESP_OK) {
                puts("httpd_ws_recv_frame failed");
                return ret;
            }
            // we don't actually use it
            free(ws_pkt.payload);
        } else {
            puts("httpd_ws: no data recieved");
            return ESP_OK;
        }
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
    xSemaphoreTake(SHND,portMAX_DELAY);
    uint32_t vals = pack_alarm_values(alarm_values);
    buf[0] = alarm_count;
    xSemaphoreGive(SHND);
    vals = __bswap32(vals);
    memcpy(buf + 1, &vals, sizeof(vals));
    ws_pkt.payload = buf;
    ws_pkt.len = sizeof(buf);
    ws_pkt.fragmented = false;
    ws_pkt.final = true;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;
    ret = httpd_ws_send_frame(req, &ws_pkt);
    return ret;
}
void httpd_init() {
    for(int i = 0;i<CONFIG_LWIP_MAX_SOCKETS;++i) {
        httpd_descs[i]=-1;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = HTTPD_RESPONSE_HANDLER_COUNT + 1;
    config.server_port = 80;
    config.max_open_sockets = (CONFIG_LWIP_MAX_SOCKETS - 3);
    ESP_ERROR_CHECK(httpd_start(&httpd_handle, &config));
    httpd_uri_t handler = {.uri = "/",
                           .method = HTTP_GET,
                           .handler = httpd_request_handler,
                           .user_ctx = NULL};
    for (int i = 0; i < HTTPD_RESPONSE_HANDLER_COUNT; ++i) {
        handler.uri = httpd_response_handlers[i].path_encoded;
        ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &handler));
    }
    handler.uri = "/socket";
    handler.is_websocket = true;
    handler.method = HTTP_GET;
    handler.supported_subprotocol = nullptr;
    handler.handle_ws_control_frames = false;
    handler.handler = httpd_socket_handler;
    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &handler));
    httpd_descs_sync = xSemaphoreCreateMutex();
    if(httpd_descs_sync==nullptr) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    xTaskCreate(httpd_socket_task,"httpd_socket_task",2048,NULL,10,&httpd_socket_task_handle);
    if(httpd_socket_task_handle==nullptr) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}
void httpd_end() {
    if (httpd_handle == nullptr) {
        return;
    }
    if(httpd_socket_task_handle!=nullptr) {
        vTaskDelete(httpd_socket_task_handle);
        httpd_socket_task_handle=nullptr;
    }
    ESP_ERROR_CHECK(httpd_stop(httpd_handle));
    if(httpd_descs_sync!=nullptr) {
        vSemaphoreDelete(httpd_descs_sync);
        httpd_descs_sync=nullptr;
    }
    httpd_handle = nullptr;
    
}