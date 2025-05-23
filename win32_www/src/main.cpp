#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include "ws_server.h"
constexpr const size_t alarm_count = 4;
bool alarm_values[alarm_count] = {false, false, false, false};
HANDLE alarm_sync = nullptr;
void alarm_lock() {
    WaitForSingleObject( 
            alarm_sync,    // handle to mutex
            INFINITE);  // no time-out interval
}
void alarm_unlock() {
    ReleaseMutex(alarm_sync);
}

typedef void (*httpd_handler_func_t)(void *);

typedef struct {
    httpd_handler_func_t handler;
    char method[32];
    char path_and_query[512];
    char ws_key[29];
    int length;
    int error;
    SOCKET sock;
} httpd_context_t;
#define MAX_WEB_SOCKETS 10

SOCKET socket_fds[MAX_WEB_SOCKETS] = {0,0,0,0,0,
                    0,0,0,0,0};

static void httpd_get_context(SOCKET, httpd_context_t *);
static void httpd_send_response(httpd_context_t *);
static void httpd_ws_handshake(httpd_context_t *);
static void httpd_send_block(const char *data, size_t len, void *arg);
static void httpd_error_live(const char *);
static void httpd_error_die(const char *);

#define HTTPD_CONTENT_IMPLEMENTATION
#include ",,/../../include/httpd_content.h"

#define REQUEST_SIZE 4096
#define DEFAULT_PORT 8080

enum { RQ_UNDEF, GET, POST, PUT } response_types;

char enc_rfc3986[256] = {0};
char enc_html5[256] = {0};
static int my_strnicmp(const char *lhs, const char *rhs, size_t length) {
    int result = 0;
    while (--length && !result && *lhs && *rhs) {
        result = tolower(*lhs++) - tolower(*rhs++);
    }
    if (!result) {
        if (length) {
            if (*lhs) {
                return 1;
            } else if (*rhs) {
                return -1;
            }
        }
        return 0;
    }
    return result;
}
static uint32_t httpd_pack_alarm_values(bool* values) {
    // pack the alarm values into the buffer
    uint32_t accum = 0;
    size_t i = alarm_count;
    while(i) {
        accum <<= 1;
        accum |= (int)values[i-1];
        --i;
    }
    return accum;
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
        alarm_lock();
        for (size_t i = 0; i < alarm_count; ++i) {
            alarm_values[i]= req_values[i];
        }
        alarm_unlock();
    }
}
static volatile int socket_task_exit = 0;
// these are globals we use in the page
static DWORD WINAPI httpd_socket_task(void* arg) {
    bool old_values[alarm_count];
    memcpy(old_values,alarm_values,sizeof(bool)*alarm_count);
    while(!socket_task_exit) {
        alarm_lock();
        if(0!=memcmp(old_values,alarm_values,sizeof(bool)*alarm_count)) {
            memcpy(old_values,alarm_values,sizeof(bool)*alarm_count);
            alarm_unlock();
            uint8_t buf[5];
            buf[0]=alarm_count;
            uint32_t vals = httpd_pack_alarm_values(old_values);
            vals = htonl(vals);
            memcpy(buf+1,&vals,sizeof(uint32_t));
            ws_srv_frame_t frame;
            frame.final = 1;
            frame.fragmented = 0;
            frame.len = 5;
            frame.payload = buf;
            frame.type = WS_SRV_TYPE_BINARY;
            alarm_lock();
            for(int i = 0;i<MAX_WEB_SOCKETS;++i) {
                SOCKET fd = socket_fds[i];
                if(fd>0) {
                    if(0!=ws_srv_send_frame(&frame,[](void*data,size_t length,void*state) -> int {
                        
                        return (int)SOCKET_ERROR==send((SOCKET)state,(const char*)data,length,0);
                    },(void*)fd)) {
                        socket_fds[i]=0;
                        closesocket(fd);
                    }
                }
            }
            alarm_unlock();
        } else {
            alarm_unlock();
        }
    }
    return TRUE;
}
int main(int argc, char **argv) {
    for (int i = 0; i < 256; i++) {
        enc_rfc3986[i] =
            isalnum(i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
        enc_html5[i] =
            isalnum(i) || i == '*' || i == '-' || i == '.' || i == '_' ? i
            : (i == ' ')                                               ? '+'
                                                                       : 0;
    }
    alarm_sync = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

    int addr_len;
    struct sockaddr_in local, client_addr;

    SOCKET sock, msg_sock;
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR)
        httpd_error_die("WSAStartup()");

    // Fill in the address structure
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(DEFAULT_PORT);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) httpd_error_die("socket()");

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR)
        httpd_error_die("bind()");
    DWORD ws_thread;
    CreateThread(NULL,       // default security attributes
                     0,          // default stack size
                     (LPTHREAD_START_ROUTINE) httpd_socket_task, 
                     NULL,       // no thread function arguments
                     0,          // default creation flags
                     &ws_thread);
listen_goto:

    if (listen(sock, 10) == SOCKET_ERROR) httpd_error_die("listen()");

    printf("Waiting for connection...\n");

    int count = 0;
    while (1) {
        addr_len = sizeof(client_addr);
        msg_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);

        if (msg_sock == INVALID_SOCKET || msg_sock == -1)
            httpd_error_die("accept()");

        printf("Connected to %s:%d\n", inet_ntoa(client_addr.sin_addr),
               htons(client_addr.sin_port));

        httpd_context_t *context =
            (httpd_context_t *)malloc(sizeof(httpd_context_t));
        if (context == nullptr) {
            closesocket(msg_sock);
            socket_task_exit=1;
            WSACleanup();
            return -1;
        }
        httpd_get_context(msg_sock, context);
        printf("Client requested %s\n", context->path_and_query);

        if (context->length == 0) continue;
        
        if(0==my_strnicmp("GET",context->method,3) && !context->method[3] && context->ws_key[0]!=0
            && 0==strcmp("/socket",context->path_and_query)||0==strcmp("/socket/",context->path_and_query)) { 
            // this is a websocket handshake
            httpd_ws_handshake(context);
        } else {
            httpd_parse_url_and_apply_alarms(context->path_and_query);
            httpd_send_response(context);
            closesocket(msg_sock);
        }
        goto listen_goto;
    }
    socket_task_exit=1;
    WSACleanup();
    return 0;
}

static char *httpd_url_encode(char *enc, size_t size, const char *s,
                              const char *table) {
    char *result = enc;
    if (table == NULL) table = enc_rfc3986;
    for (; *s; s++) {
        if (table[(int)*s]) {
            *enc++ = table[(int)*s];
            --size;
        } else {
            snprintf(enc, size, "%%%02X", *s);
            while (*++enc) {
                --size;
            }
        }
    }
    return result;
}

void httpd_get_context(SOCKET sock, httpd_context_t *out_context) {
    int len;
    char buf[REQUEST_SIZE];

    len = recv(sock, buf, sizeof(buf), 0);
    if (len == 0) {
        out_context->length = len;
        out_context->sock = sock;
        out_context->error = 0;
        out_context->handler = nullptr;
        return;
    }
    sscanf(buf, "%s %s ", out_context->method, out_context->path_and_query);
    out_context->ws_key[0] = '\0';
    const char *hdcursor = strchr(buf, '\n') + 1;  // skip the status line
    while (*hdcursor && *hdcursor != '\r') {
        if (0 == my_strnicmp("Sec-WebSocket-Key:", hdcursor, 18)) {
            // parse the value
            const char *hdvalbegin = hdcursor + 18;
            while (*hdvalbegin == ' ') {
                ++hdvalbegin;
            }
            const char *hdvalend = strchr(hdvalbegin, '\r');
            ws_srv_compute_sec(hdvalbegin, hdvalend - hdvalbegin, out_context->ws_key,
                               sizeof(out_context->ws_key));
            hdcursor = hdvalend + 2;  // next header
        } else {
            hdcursor = strchr(hdcursor, '\n') + 1;  // next header
        }
    }

    out_context->length = len;
    out_context->sock = sock;
    out_context->error = 0;
    out_context->handler = NULL;
    int hi = httpd_response_handler_match(out_context->path_and_query);
    if (hi > -1) {
        out_context->handler = httpd_response_handlers[hi].handler;
    } else {
        out_context->handler = nullptr;
    }
}

void httpd_send_response(httpd_context_t *context) {
    httpd_handler_func_t h;
    if (context->error || context->handler == NULL) {
        h = httpd_content_404_clasp;
    } else {
        h = context->handler;
    }
    h(context);
    closesocket(context->sock);
}

void httpd_ws_handshake(httpd_context_t *context) {
    static const char* http_response = "HTTP/1.1 101 Switching Protocols\r\n" \
        "Upgrade: websocket\r\n" \
        "Connection: Upgrade\r\nSec-WebSocket-Accept: ";
    static const size_t http_response_len = strlen(http_response);
    puts("Begin websocket handshake");
    httpd_send_block(http_response,http_response_len,context);
    httpd_send_block(context->ws_key,strlen(context->ws_key),context);
    httpd_send_block("\r\n\r\n",4,context);
    alarm_lock();
    for(size_t i = 0;i<MAX_WEB_SOCKETS;++i) {
        SOCKET fd = socket_fds[i];
        if(fd==0) {
            socket_fds[i]=context->sock;
            alarm_unlock();
            puts("Websocket handshake successful");
            return;
        }
    }
    alarm_unlock();
    puts("out of web sockets");
    return;
}

static void httpd_send_block(const char *data, size_t len, void *arg) {
    if (!data || !len) {
        return;
    }
    httpd_context_t *ctx = (httpd_context_t *)arg;
    SOCKET sock = ctx->sock;
    send(sock, data, len, 0);
}

void httpd_error_live(const char *s) {
    fprintf(stderr, "Error: %s failed with error %d\n", s, WSAGetLastError());
    WSACleanup();
}

void httpd_error_die(const char *s) {
    socket_task_exit=1;
    httpd_error_live(s);
    exit(EXIT_FAILURE);
}