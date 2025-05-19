#include "httpd.hpp"
#include <stddef.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "esp_http_server.h"
#include "esp_netif_ip_addr.h"
#include "alarm_common.hpp"
#include "ui.hpp"
static void httpd_send_block(const char* data, size_t len, void* arg);
static void httpd_send_expr(int expr, void* arg);
static void httpd_send_expr(const char* expr, void* arg);

// include the generated react content
#define HTTPD_CONTENT_IMPLEMENTATION
#include "httpd_content.h"

static httpd_handle_t httpd_handle = nullptr;
static SemaphoreHandle_t httpd_ui_sync = nullptr;
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
    while (*url_part && *url_part != '=' && *url_part!='&') {
        if (i < 64) {
            *name_cur++ = *url_part;
        }
        ++url_part;
        ++i;
    }
    *name_cur = '\0';
    if (!*url_part || *url_part=='&') {
        *value = '\0';
        return url_part;
    }
    ++url_part;
    i = 0;
    char* value_cur = value;
    while (*url_part && *url_part != '&') {
        if(i<64) {
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
        for (size_t i = 0; i < alarm_count; ++i) {
            alarm_enable(i, req_values[i]);
        }
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
        return ESP_FAIL;
    }
    httpd_work_fn_t h;
    if(handler_index==-1) {
        h = httpd_content_404_clasp;
    } else {
        h = httpd_response_handlers[handler_index].handler;
    }
    httpd_queue_work(req->handle, h, resp_arg);
    return ESP_OK;
}
static bool httpd_match(const char* str1, const char* str2, size_t length) {
    return true;
}
void httpd_init() {
    httpd_ui_sync = xSemaphoreCreateMutex();
    if (httpd_ui_sync == nullptr) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = HTTPD_RESPONSE_HANDLER_COUNT;
    config.server_port = 80;
    config.max_open_sockets = (CONFIG_LWIP_MAX_SOCKETS - 3);
    config.uri_match_fn = httpd_match;
    ESP_ERROR_CHECK(httpd_start(&httpd_handle, &config));
    httpd_uri_t handler = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = httpd_request_handler,
        .user_ctx = NULL
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &handler));
}
void httpd_end() {
    if (httpd_handle == nullptr) {
        return;
    }
    ESP_ERROR_CHECK(httpd_stop(httpd_handle));
    httpd_handle = nullptr;
    vSemaphoreDelete(httpd_ui_sync);
    httpd_ui_sync = nullptr;
}