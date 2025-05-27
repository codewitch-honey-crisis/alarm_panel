#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>

#include "httpd.h"

typedef struct {
    char method[32];
    char path_and_query[512];
    char ws_key[29];
    int error;
    void* sinfo;
} httpd_context_t;

#define WM_SOCKET (WM_USER + 1)

// HTTP/1.1 404 Not found
// Content-Type: text/html
// Content-Length: 178
// Content-Encoding: deflate
//
static const unsigned char http_404_response_data[] = {
    0x48, 0x54, 0x54, 0x50, 0x2F, 0x31, 0x2E, 0x31, 0x20, 0x34, 0x30, 0x34, 0x20, 0x4E, 0x6F, 0x74, 0x20, 0x66, 0x6F, 0x75,
    0x6E, 0x64, 0x0D, 0x0A, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2D, 0x54, 0x79, 0x70, 0x65, 0x3A, 0x20, 0x74, 0x65,
    0x78, 0x74, 0x2F, 0x68, 0x74, 0x6D, 0x6C, 0x0D, 0x0A, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2D, 0x4C, 0x65, 0x6E,
    0x67, 0x74, 0x68, 0x3A, 0x20, 0x31, 0x37, 0x38, 0x0D, 0x0A, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2D, 0x45, 0x6E,
    0x63, 0x6F, 0x64, 0x69, 0x6E, 0x67, 0x3A, 0x20, 0x64, 0x65, 0x66, 0x6C, 0x61, 0x74, 0x65, 0x0D, 0x0A, 0x0D, 0x0A, 0x5C,
    0x50, 0x4D, 0x0F, 0x82, 0x30, 0x0C, 0xBD, 0x9B, 0xF8, 0x1F, 0xEA, 0xCE, 0xF2, 0x95, 0x70, 0x1C, 0x5C, 0xD4, 0xAB, 0x7A,
    0xE0, 0xE2, 0x71, 0xB0, 0x9A, 0x2D, 0x19, 0x1B, 0x81, 0x02, 0xF1, 0xDF, 0x3B, 0x81, 0xE8, 0x62, 0x2F, 0xED, 0x6B, 0xFB,
    0x5E, 0x5F, 0xCA, 0x0F, 0xE7, 0xDB, 0xA9, 0x7A, 0xDC, 0x2F, 0xA0, 0xA8, 0x35, 0xE5, 0x7E, 0xC7, 0xB7, 0x0C, 0x3E, 0xB8,
    0x42, 0x21, 0xB7, 0x7A, 0xC1, 0x2D, 0x92, 0x00, 0x2B, 0x5A, 0x2C, 0xD8, 0xA4, 0x71, 0xEE, 0x5C, 0x4F, 0x0C, 0x1A, 0x67,
    0x09, 0x2D, 0x15, 0x6C, 0xD6, 0x92, 0x54, 0x21, 0x71, 0xD2, 0x0D, 0x46, 0x0B, 0x38, 0x82, 0xB6, 0x9A, 0xB4, 0x30, 0xD1,
    0xD0, 0x08, 0x83, 0x45, 0x16, 0xA7, 0x0C, 0x92, 0x50, 0x91, 0x34, 0x19, 0x2C, 0xF3, 0x34, 0x87, 0xAB, 0x23, 0x78, 0xBA,
    0xD1, 0x4A, 0x9E, 0xAC, 0xCD, 0xCD, 0x43, 0x12, 0x98, 0xE0, 0xB5, 0x93, 0xAF, 0x90, 0xAE, 0xB2, 0x7F, 0xAE, 0xEF, 0x04,
    0xF3, 0xAE, 0xAC, 0x14, 0x42, 0x8F, 0x83, 0x1B, 0xFB, 0x06, 0xBD, 0xD5, 0xD1, 0x48, 0xB0, 0x7E, 0xBB, 0xC6, 0x95, 0x10,
    0xF3, 0xA4, 0xFB, 0x5E, 0xDA, 0xD4, 0xBD, 0xC8, 0xEF, 0x07, 0x9F, 0x78, 0x03, 0x00, 0x00, 0xFF, 0xFF};

typedef struct httpd_handler_entry {
    char path[512];
    void (*on_request)(const char* method, const char* path_and_query, void* arg, void* state);
    void* on_request_state;
    void (*on_ws_connect)(const char* path_and_query, void* state);
    void* on_ws_connect_state;
    void (*on_ws_receive)(const ws_srv_frame_t* frame, void* arg, void* state);
    void* on_ws_receive_state;
    struct httpd_handler_entry* next;
} httpd_handler_entry_t;
typedef struct httpd_sock_info {
    SOCKET sock;
    WSABUF wsa;
    uint8_t* recv_buf;
    size_t recv_size;
    size_t recv_cap;
    char ws_state;
    httpd_handler_entry_t* handler;
    struct httpd_sock_info* next;
} httpd_sock_info_t;

static SOCKET httpd_listen_socket = INVALID_SOCKET;
static httpd_sock_info_t* httpd_sock_list = NULL;
static httpd_handler_entry_t* httpd_handler_list = NULL;
static HANDLE httpd_sync = NULL;
static HWND httpd_wnd = NULL;
static void httpd_get_context(httpd_sock_info_t* info, const char* buf, httpd_context_t* out_context);
static void httpd_ws_handshake(httpd_context_t*);
static void httpd_error_die(const char*);
static void httpd_make_event_window(HWND* out_hwnd);
static void httpd_destroy_sock_info(SOCKET sock);

static char enc_rfc3986[256] = {0};
static char enc_html5[256] = {0};
static int my_strnicmp(const char* lhs, const char* rhs, size_t length) {
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

static void httpd_error_die(const char* s) {
    fputs("Error: ", stdout);
    puts(s);
    exit(-1);
}
static void httpd_create_sock_info(SOCKET s)

{
    httpd_sock_info_t* SI;

    if ((SI = (httpd_sock_info_t*)GlobalAlloc(
             GPTR, sizeof(httpd_sock_info_t))) == NULL)

    {
        httpd_error_die("GlobalAlloc()");

        return;

    }

    else

        // Prepare SocketInfo structure for use

        SI->sock = s;
    SI->recv_cap = 8192;
    SI->recv_size = 0;
    SI->ws_state = 0;  // not a websocket or not negotiated
    SI->next = httpd_sock_list;
    SI->handler = NULL;
    SI->recv_buf = (uint8_t*)malloc(SI->recv_cap);
    if (SI->recv_buf == NULL) {
        httpd_error_die("malloc()");
    }
    httpd_sock_list = SI;
}

static httpd_sock_info_t* httpd_find_sock_info(SOCKET s) {
    httpd_sock_info_t* SI = httpd_sock_list;

    while (SI) {
        if (SI->sock == s) {
            return SI;
        }

        SI = SI->next;
    }
    return NULL;
}

static void httpd_destroy_sock_info(SOCKET s) {
    httpd_sock_info_t* SI = httpd_sock_list;

    httpd_sock_info_t* PrevSI = NULL;

    while (SI) {
        if (SI->sock == s) {
            if (PrevSI) {
                PrevSI->next = SI->next;
            } else {
                httpd_sock_list = SI->next;
            }
            if (SI->recv_buf != NULL) {
                free(SI->recv_buf);
            }
            closesocket(SI->sock);
            GlobalFree(SI);
            return;
        }
        PrevSI = SI;
        SI = SI->next;
    }
}

static void httpd_create_handler(const char* path, void (*on_request_callback)(const char* method, const char* path_and_query, void* arg, void* state),
                                 void* on_request_callback_state,
                                 void (*on_connect_callback)(const char* path_and_querty, void* state),
                                 void* on_connect_callback_state,
                                 void (*on_receive_callback)(const ws_srv_frame_t* frame, void* arg, void* state),
                                 void* on_receive_callback_state) {
    if (on_connect_callback != NULL && on_request_callback != NULL && on_receive_callback == NULL) {
        return;
    }
    httpd_handler_entry_t* e;

    if ((e = (httpd_handler_entry_t*)malloc(sizeof(httpd_handler_entry_t))) == NULL) {
        httpd_error_die("malloc()");
        return;
    } else {
        strncpy(e->path, path, sizeof(e->path));
        e->on_request = on_request_callback;
        e->on_request_state = on_request_callback_state;
        e->on_ws_connect = on_connect_callback;
        e->on_ws_connect_state = on_connect_callback_state;
        e->on_ws_receive = on_receive_callback;
        e->on_ws_receive_state = on_receive_callback_state;
    }
    e->next = httpd_handler_list;
    httpd_handler_list = e;
}
static httpd_path_cmp(const char* path, const char* cmp) {
    int result = 0;
    int length = 0;
    while (!result && *path != '\0' && *path != '?' && *cmp != '\0') {
        result = tolower(*path++) - tolower(*cmp++);
        ++length;
    }
    if (!result) {
        if (length) {
            if (*path == '?') {
                return 0;
            } else if (*cmp) {
                return -1;
            } else {
                return *path != '\0';
            }
        }
        return 0;
    }
    return result;
}
static httpd_handler_entry_t* httpd_find_handler(const char* path) {
    httpd_handler_entry_t* e = httpd_handler_list;
    while (e) {
        if (0 == httpd_path_cmp(path, e->path)) {
            return e;
        }
        e = e->next;
    }
    return NULL;
}

static void httpd_destroy_handler(const char* path) {
    httpd_handler_entry_t* e = httpd_handler_list;
    httpd_handler_entry_t* prev = NULL;

    while (e) {
        if (0 == strcmp(path, e->path)) {
            if (prev) {
                prev->next = e->next;
            } else {
                httpd_handler_list = e->next;
            }
            free(e);
            return;
        }
        prev = e;
        e = e->next;
    }
}

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

static char* httpd_url_encode(char* enc, size_t size, const char* s,
                              const char* table) {
    char* result = enc;
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

void httpd_get_context(httpd_sock_info_t* info, const char* buf, httpd_context_t* out_context) {
    sscanf(buf, "%s %s ", out_context->method, out_context->path_and_query);
    out_context->ws_key[0] = '\0';
    const char* hdcursor = strchr(buf, '\n') + 1;  // skip the status line
    while (*hdcursor && *hdcursor != '\r') {
        if (0 == my_strnicmp("Sec-WebSocket-Key:", hdcursor, 18)) {
            // parse the value
            const char* hdvalbegin = hdcursor + 18;
            while (*hdvalbegin == ' ') {
                ++hdvalbegin;
            }
            const char* hdvalend = strchr(hdvalbegin, '\r');
            ws_srv_compute_sec(hdvalbegin, hdvalend - hdvalbegin,
                               out_context->ws_key,
                               sizeof(out_context->ws_key));
            hdcursor = hdvalend + 2;  // next header
        } else {
            hdcursor = strchr(hdcursor, '\n') + 1;  // next header
        }
    }
    out_context->sinfo = info;
    out_context->error = 0;
}

void httpd_ws_handshake(httpd_context_t* context) {
    static const char* http_upg_response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: ";
    static const size_t http_upg_response_len = 97;
    puts("Begin websocket handshake");
    httpd_handler_entry_t* he = ((httpd_sock_info_t*)context->sinfo)->handler;
    if (he != NULL) {
        httpd_send_block(http_upg_response, http_upg_response_len, context);
        httpd_send_block(context->ws_key, strlen(context->ws_key), context);
        httpd_send_block("\r\n\r\n", 4, context);
        if (he->on_ws_connect != NULL) {
            he->on_ws_connect(context->path_and_query, he->on_ws_connect_state);
        }
    } else {
        httpd_send_block((const char*)http_404_response_data, sizeof(http_404_response_data), context);
    }
    free(context);
    return;
}

static int httpd_ws_write(const void* data, size_t length, void* state) {
    httpd_sock_info_t* inf = (httpd_sock_info_t*)state;
    DWORD cb;
    inf->wsa.buf = (char*)data;
    inf->wsa.len = length;
    if (SOCKET_ERROR == WSASend(inf->sock, &inf->wsa, 1, &cb, 0, NULL, NULL)) {
        return -1;
    }
    return 0;
}
static void httpd_ws_control_message(const ws_srv_frame_t* frame, httpd_sock_info_t* info) {
    uint8_t* data = (uint8_t*)frame->payload;
    if (frame->masked) {
        for (int i = 0; i < frame->len; ++i) {
            data[i] ^= frame->mask_key[i % 4];
        }
    }
    ws_srv_frame_t newframe;
    switch (frame->type) {
        case WS_SRV_TYPE_CLOSE:
            // puts("websocket close request from client");
            httpd_destroy_sock_info(info->sock);
            break;
        case WS_SRV_TYPE_PING:
            // puts("websocket ping request from client");
            newframe.final = 1;
            newframe.fragmented = 0;
            newframe.len = frame->len;
            ws_srv_unmask_payload(frame,data);
            newframe.payload = data;
            newframe.masked = 0;
            newframe.type = WS_SRV_TYPE_PONG;
            ws_srv_send_frame(&newframe, httpd_ws_write, info);
    }
}
typedef struct {
    size_t remaining;
    size_t cursor;
    uint8_t* buffer;
} httpd_buf_data_t;
static int httpd_read_buf_data(void* out_data, size_t* in_out_len, void* state) {
    httpd_buf_data_t* bd = (httpd_buf_data_t*)state;
    uint8_t* p = (uint8_t*)out_data;
    size_t inlen = *in_out_len;
    *in_out_len = 0;
    while (bd->remaining > 0 && inlen > 0) {
        *(p++) = bd->buffer[bd->cursor++];
        --bd->remaining;
        --inlen;
        ++(*in_out_len);
    }
    return 0;
}
void httpd_send_block(const char* data, size_t len, void* arg) {
    if (!data || !len) {
        return;
    }
    httpd_context_t* ctx = (httpd_context_t*)arg;
    DWORD sent;
    httpd_sock_info_t* sinfo = (httpd_sock_info_t*)ctx->sinfo;
    sinfo->wsa.buf = (char*)data;
    sinfo->wsa.len = len;
    WSASend(sinfo->sock, &sinfo->wsa, 1, &sent, 0, NULL, NULL);
}
static LRESULT CALLBACK httpd_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                          LPARAM lParam)

{
    SOCKET accept_sock;
    DWORD recv_bytes;
    DWORD flags;
    httpd_sock_info_t* info;
    if (uMsg == WM_SOCKET) {
        if (WSAGETSELECTERROR(lParam)) {
            printf("Socket failed with error %d\n", WSAGETSELECTERROR(lParam));
            httpd_destroy_sock_info(wParam);
        } else {
            switch (WSAGETSELECTEVENT(lParam)) {
                case FD_ACCEPT:
                    if ((accept_sock = accept(wParam, NULL, NULL)) == INVALID_SOCKET) {
                        printf("accept() failed with error %d\n", WSAGetLastError());
                        break;
                    }
                    // Create a socket information structure to associate with the socket for processing I/O
                    httpd_create_sock_info(accept_sock);
                    WSAAsyncSelect(accept_sock, hwnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE);
                    break;
                case FD_READ:
                    info = httpd_find_sock_info(wParam);
                    if (info == NULL) {
                        puts("Aborted read on closing socket");
                        break;
                    }
                    if (info->ws_state == 1) {
                        puts("Receiving websocket data");
                        ws_srv_frame_t frame;
                        if (info->recv_buf == NULL) {
                            info->recv_cap = 8192;
                            info->recv_size = 0;
                            info->recv_buf = (uint8_t*)malloc(info->recv_cap);
                            if (info->recv_buf == NULL) {
                                httpd_error_die("malloc()");
                            }
                        } else {
                            if (info->recv_size != 0) {
                                puts("Warning: recv_size nonzero");
                            }
                        }
                        DWORD cb;
                        DWORD dwFlags = 0;
                        info->wsa.buf = (char*)info->recv_buf;
                        info->wsa.len = info->recv_cap;
                        if (SOCKET_ERROR == WSARecv(info->sock, &info->wsa, 1, &cb, &dwFlags, NULL, NULL)) {
                            int err = WSAGetLastError();
                            if (err != WSAEWOULDBLOCK) {
                                puts("Socket error in incoming websocket frame read");
                                httpd_destroy_sock_info(info->sock);
                                return -1;
                            }
                        }

                        httpd_buf_data_t st = {(size_t)cb, 0, (uint8_t*)info->recv_buf};

                        if (0 != ws_srv_recv_frame(httpd_read_buf_data, &st, &frame)) {
                            puts("Error in incoming websocket frame");
                        }
                        frame.payload = st.buffer + st.cursor;
                        if (frame.type == WS_SRV_TYPE_BINARY || frame.type == WS_SRV_TYPE_TEXT && frame.final && frame.len > 0) {
                            if (info->handler->on_ws_receive != NULL) {
                                info->handler->on_ws_receive(&frame, info, info->handler->on_ws_receive_state);
                            }
                        } else {
                            httpd_ws_control_message(&frame, info);
                        }
                        info->recv_size = 0;
                    } else {
                        // Read data only if the receive buffer is empty

                        if (info->recv_size >= info->recv_cap) {
                            info->recv_buf = (uint8_t*)realloc(info->recv_buf, info->recv_cap * 2);
                            if (info->recv_buf == NULL) {
                                httpd_error_die("realloc()");
                            }
                            info->recv_cap *= 2;
                        }
                        info->wsa.buf = (char*)(info->recv_buf + info->recv_size);
                        info->wsa.len = info->recv_cap - info->recv_size;
                        flags = 0;

                        if (WSARecv(info->sock, &(info->wsa), 1, &recv_bytes,
                                    &flags, NULL, NULL) == SOCKET_ERROR) {
                            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                                printf("WSARecv() failed with error %d\n",
                                       WSAGetLastError());
                                httpd_destroy_sock_info(wParam);
                                return 0;
                            }
                        }

                        else  { // No error so update the byte count
                            info->recv_size += recv_bytes;
                        }
                        httpd_context_t* ctx = (httpd_context_t*)malloc(sizeof(httpd_context_t));
                        if (ctx == NULL) {
                            httpd_error_die("malloc()");
                        }
                        httpd_get_context(info, (const char*)info->recv_buf, ctx);
                        const char* qp = strchr(ctx->path_and_query, '?');
                        if (qp == NULL) qp = ctx->path_and_query + strlen(ctx->path_and_query);
                        httpd_handler_entry_t* he = httpd_find_handler(ctx->path_and_query);
                        if (he == NULL) {
                            httpd_send_block((const char*)http_404_response_data, sizeof(http_404_response_data), ctx);
                            free(ctx);
                            break;
                        }
                        info->handler = he;
                        if (he->on_request == NULL) {
                            info->ws_state = 1;  // handshake
                            httpd_ws_handshake(ctx);
                        } else {
                            info->ws_state = 0;
                            he->on_request(ctx->method, ctx->path_and_query, ctx, he->on_request_state);
                        }
                        info->recv_size = 0;
                    }
                    break;

                case FD_WRITE:
                    break;

                case FD_CLOSE:
                    httpd_destroy_sock_info(wParam);

                    break;
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void httpd_make_event_window(HWND* out_hwnd) {
    WNDCLASSW wndclass;
    WCHAR* clsname = L"soecket_event_window";
    HWND hwnd;
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = (WNDPROC)httpd_window_proc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = NULL;
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = (LPCWSTR)clsname;
    if (RegisterClassW(&wndclass) == 0) {
        printf("RegisterClass() failed with error %d\n", GetLastError());
        *out_hwnd = NULL;
        return;
    }
    // Create a window

    if ((hwnd = CreateWindowW(

             (LPCWSTR)clsname,
             L"",
             WS_OVERLAPPEDWINDOW,
             CW_USEDEFAULT,
             CW_USEDEFAULT,
             CW_USEDEFAULT,
             CW_USEDEFAULT,
             NULL,
             NULL,
             NULL,
             NULL)) == NULL) {
        printf("CreateWindow() failed with error %d\n", GetLastError());

        *out_hwnd = NULL;
        return;
    }
    *out_hwnd = hwnd;
    return;
}

int httpd_send_ws_frame(const ws_srv_frame_t* frame, void* arg) {
    if (frame != NULL || arg != NULL) {
        httpd_sock_info_t* info = (httpd_sock_info_t*)arg;
        if (info->ws_state > 0) {
            if (0 !=
                ws_srv_send_frame(
                    frame, httpd_ws_write, info)) {
                httpd_destroy_sock_info(info->sock);
                return -1;
            }
        }
    }
    return -1;
}
int httpd_broadcast_ws_frame(const char* path, const ws_srv_frame_t* frame) {
    if (httpd_sync == NULL || frame == NULL) {
        return -1;
    }
    WaitForSingleObject(httpd_sync,  // handle to mutex
                        INFINITE);   // no time-out int
    httpd_sock_info_t* info = httpd_sock_list;
    while (info != NULL) {
        if (info->ws_state > 0) {
            if (NULL == path || (info->handler != NULL && 0 == strcmp(path, info->handler->path))) {
                if (0 !=
                    ws_srv_send_frame(
                        frame, httpd_ws_write, info)) {
                    info = info->next;
                    httpd_destroy_sock_info(info->sock);
                    continue;
                }
            }
        }
        info = info->next;
    }
    ReleaseMutex(httpd_sync);
    return 0;
}

int httpd_init(uint16_t port, size_t max_handlers) {
    if (httpd_sync != NULL) {
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        enc_rfc3986[i] =
            isalnum(i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
        enc_html5[i] =
            isalnum(i) || i == '*' || i == '-' || i == '.' || i == '_' ? i
            : (i == ' ')                                               ? '+'
                                                                       : 0;
    }
    httpd_sync = CreateMutex(NULL,   // default security attributes
                             FALSE,  // initially not owned
                             NULL);  // unnamed mutex

    if (httpd_sync == NULL) {
        return -1;
    }
    int addr_len;
    struct sockaddr_in local, client_addr;
    httpd_make_event_window(&httpd_wnd);
    if (httpd_wnd == NULL) {
        CloseHandle(httpd_sync);
        httpd_sync = NULL;
        return -1;
    }
    SOCKET msg_sock;

    // Fill in the address structure
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    httpd_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd_listen_socket == INVALID_SOCKET) {
        DestroyWindow(httpd_wnd);
        httpd_wnd = NULL;
        CloseHandle(httpd_sync);
        httpd_sync = NULL;
        return -1;
    }

    if (WSAAsyncSelect(httpd_listen_socket, httpd_wnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE) != 0 ||
        bind(httpd_listen_socket, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        closesocket(httpd_listen_socket);
        httpd_listen_socket = INVALID_SOCKET;
        DestroyWindow(httpd_wnd);
        httpd_wnd = NULL;
        CloseHandle(httpd_sync);
        httpd_sync = NULL;
        return -1;
    }
    if (listen(httpd_listen_socket, 10) == SOCKET_ERROR) {
        closesocket(httpd_listen_socket);
        httpd_listen_socket = INVALID_SOCKET;
        DestroyWindow(httpd_wnd);
        httpd_wnd = NULL;
        CloseHandle(httpd_sync);
        httpd_sync = NULL;
        return -1;
    }
    return 0;
}
void httpd_end() {
    if (httpd_sync == NULL) {
        return;
    }
    closesocket(httpd_listen_socket);
    httpd_listen_socket = INVALID_SOCKET;
    DestroyWindow(httpd_wnd);
    httpd_wnd = NULL;
    CloseHandle(httpd_sync);
    httpd_sync = NULL;
    httpd_handler_entry_t* he = httpd_handler_list;
    while (he) {
        httpd_handler_entry_t* next = he->next;
        free(he);
        he = next;
    }
    httpd_handler_list = NULL;
    httpd_sock_info_t* si = httpd_sock_list;
    while (si) {
        httpd_sock_info_t* next = si->next;
        if (si->recv_buf != NULL) {
            free(si->recv_buf);
        }
        if (si->sock != INVALID_SOCKET) {
            closesocket(si->sock);
        }
        GlobalFree(he);
        si = next;
    }
    httpd_sock_list = NULL;
}
int httpd_register_handler(const char* path, void (*on_request_callback)(const char* method, const char* path_and_query, void* arg, void* state), void* on_request_callback_state) {
    httpd_create_handler(path, on_request_callback, on_request_callback_state, NULL, NULL, NULL, NULL);
    return 0;
}
int httpd_register_websocket(const char* path, void (*on_connect_callback)(const char* path_and_query, void* state), void* on_connect_callback_state, void (*on_receive_callback)(const ws_srv_frame_t* frame, void* arg, void* state), void* on_receive_callback_state) {
    httpd_create_handler(path, NULL, NULL, on_connect_callback, on_connect_callback_state, on_receive_callback, on_receive_callback_state);
    return 0;
}
