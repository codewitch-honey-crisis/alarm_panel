#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include "ws_server.h"

#define WM_SOCKET (WM_USER + 1)

constexpr const size_t alarm_count = 4;
bool alarm_values[alarm_count] = {false, false, false, false};
HANDLE alarm_sync = nullptr;
void alarm_lock() {
    WaitForSingleObject(alarm_sync,  // handle to mutex
                        INFINITE);   // no time-out interval
}
void alarm_unlock() { ReleaseMutex(alarm_sync); }

typedef void (*httpd_handler_func_t)(void *);

typedef struct httpd_sock_info {
    SOCKET sock;
    WSABUF wsa;
    uint8_t *recv_buf;
    size_t recv_size;
    size_t recv_cap;
    char ws_state;
    struct httpd_sock_info *next;
} httpd_sock_info_t;

typedef struct {
    httpd_handler_func_t handler;
    char method[32];
    char path_and_query[512];
    char ws_key[29];
    int error;
    httpd_sock_info_t* sinfo;
} httpd_context_t;
#define MAX_WEB_SOCKETS 10


httpd_sock_info_t *httpd_sock_list = nullptr;
SOCKET socket_fds[MAX_WEB_SOCKETS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void httpd_get_context(httpd_sock_info_t* info,const char* buf, httpd_context_t *out_context);
static void httpd_send_response(httpd_context_t *);
static void httpd_ws_handshake(httpd_context_t *);
static void httpd_send_block(const char *data, size_t len, void *arg);
static void httpd_error_live(const char *);
static void httpd_error_die(const char *);
static void httpd_make_event_window(HWND *out_hwnd);
static void httpd_destroy_sock_info(SOCKET sock);
#define HTTPD_CONTENT_IMPLEMENTATION
#include ",,/../../include/httpd_content.h"

#define REQUEST_SIZE 4096
#define DEFAULT_PORT 8080

enum { RQ_UNDEF,
       GET,
       POST,
       PUT } response_types;

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
static uint32_t httpd_pack_alarm_values(bool *values) {
    // pack the alarm values into the buffer
    uint32_t accum = 0;
    size_t i = alarm_count;
    while (i) {
        accum <<= 1;
        accum |= (int)values[i - 1];
        --i;
    }
    return accum;
}
static const char *httpd_crack_query(const char *url_part, char *name,
                                     char *value) {
    if (url_part == nullptr || !*url_part) return nullptr;
    const char start = *url_part;
    if (start == '&' || start == '?') {
        ++url_part;
    }
    size_t i = 0;
    char *name_cur = name;
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
    char *value_cur = value;
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
static void httpd_parse_url_and_apply_alarms(const char *url) {
    const char *query = strchr(url, '?');
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
                char *endsz;
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
            alarm_values[i] = req_values[i];
        }
        alarm_unlock();
    }
}
static volatile int socket_task_exit = 0;
// these are globals we use in the page
static DWORD WINAPI httpd_socket_task(void *arg) {
    bool old_values[alarm_count];
    memcpy(old_values, alarm_values, sizeof(bool) * alarm_count);
    while (!socket_task_exit) {
        alarm_lock();
        if (0 != memcmp(old_values, alarm_values, sizeof(bool) * alarm_count)) {
            memcpy(old_values, alarm_values, sizeof(bool) * alarm_count);
            alarm_unlock();
            uint8_t buf[5];
            buf[0] = alarm_count;
            uint32_t vals = httpd_pack_alarm_values(old_values);
            vals = htonl(vals);
            memcpy(buf + 1, &vals, sizeof(uint32_t));
            ws_srv_frame_t frame;
            frame.final = 1;
            frame.fragmented = 0;
            frame.len = 5;
            frame.payload = buf;
            frame.type = WS_SRV_TYPE_BINARY;
            alarm_lock();
            httpd_sock_info_t * info = httpd_sock_list;
            while(info!=nullptr) {
                if (info->ws_state > 0) {
                    if (0 !=
                        ws_srv_send_frame(
                            &frame,
                            [](void *data, size_t length, void *state) -> int {
                                httpd_sock_info_t* si = (httpd_sock_info_t*)state;
                                si->wsa.buf = (char*)data;
                                si->wsa.len = length;
                                DWORD l;
                                return (int)SOCKET_ERROR ==
                                       WSASend(si->sock,&si->wsa,1,&l,0,NULL,NULL);
                            },
                            info)) {
                                info = info->next;
                                httpd_destroy_sock_info(info->sock);
                                continue;
                        
                    }
                }
                info = info->next;
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
    alarm_sync = CreateMutex(NULL,   // default security attributes
                             FALSE,  // initially not owned
                             NULL);  // unnamed mutex

    int addr_len;
    struct sockaddr_in local, client_addr;
    HWND evt_wnd;
    httpd_make_event_window(&evt_wnd);
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

    if (WSAAsyncSelect(sock, evt_wnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE) != 0) {
        httpd_error_die("WSAAsyncSelect");
    }
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR)
        httpd_error_die("bind()");
    DWORD ws_thread;
    CreateThread(NULL,  // default security attributes
                 0,     // default stack size
                 (LPTHREAD_START_ROUTINE)httpd_socket_task,
                 NULL,  // no thread function arguments
                 0,     // default creation flags
                 &ws_thread);
    if (listen(sock, 10) == SOCKET_ERROR) httpd_error_die("listen()");
    puts("Waiting for connection...");
    fflush(stdout);
    BOOL ret;
    MSG msg;
    while (ret = GetMessage(&msg, NULL, 0, 0)) {
        if (ret == -1)

        {
            httpd_error_die("GetMessage()");

            return 1;
        }

        TranslateMessage(&msg);

        DispatchMessage(&msg);
        if(msg.message==WM_QUIT) {
            socket_task_exit = 1;
            break;
        }
    }

    // int count = 0;
    // while (1) {
    //     addr_len = sizeof(client_addr);
    //     msg_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
    //     if (msg_sock == INVALID_SOCKET || msg_sock == -1)
    //         httpd_error_die("accept()");

    //     printf("Connected to %s:%d\n", inet_ntoa(client_addr.sin_addr),
    //            htons(client_addr.sin_port));

    //     httpd_context_t *context =
    //         (httpd_context_t *)malloc(sizeof(httpd_context_t));
    //     if (context == nullptr) {
    //         closesocket(msg_sock);
    //         socket_task_exit = 1;
    //         WSACleanup();
    //         return -1;
    //     }
    //     httpd_get_context(msg_sock, context);
    //     printf("Client requested %s\n", context->path_and_query);

    //     if (context->length == 0) continue;

    //     if (0 == my_strnicmp("GET", context->method, 3) &&
    //             !context->method[3] && context->ws_key[0] != 0 &&
    //             0 == strcmp("/socket", context->path_and_query) ||
    //         0 == strcmp("/socket/", context->path_and_query)) {
    //         // this is a websocket handshake
    //         httpd_ws_handshake(context);
    //         WSAAsyncSelect(msg_sock, evt_wnd, WM_SOCKET,
    //                        FD_READ | FD_WRITE | FD_CLOSE);
    //     } else {
    //         httpd_parse_url_and_apply_alarms(context->path_and_query);
    //         httpd_send_response(context);
    //         closesocket(msg_sock);
    //     }
    //     goto listen_goto;
    // }
    // socket_task_exit = 1;
    WSACleanup();
    DestroyWindow(evt_wnd);
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

void httpd_get_context(httpd_sock_info_t* info, const char *buf, httpd_context_t *out_context) {
    
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
}

void httpd_ws_handshake(httpd_context_t *context) {
    static const char *http_response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: ";
    static const size_t http_response_len = strlen(http_response);
    puts("Begin websocket handshake");
    httpd_send_block(http_response, http_response_len, context);
    httpd_send_block(context->ws_key, strlen(context->ws_key), context);
    httpd_send_block("\r\n\r\n", 4, context);
    return;
}

static void httpd_send_block(const char *data, size_t len, void *arg) {
    if (!data || !len) {
        return;
    }
    httpd_context_t *ctx = (httpd_context_t *)arg;
    DWORD sent;
    ctx->sinfo->wsa.buf = (char*)data;
    ctx->sinfo->wsa .len = len;
    WSASend(ctx->sinfo->sock,&ctx->sinfo->wsa,1,&sent,0,NULL,NULL);
}

void httpd_error_live(const char *s) {
    fprintf(stderr, "Error: %s failed with error %d\n", s, WSAGetLastError());
    WSACleanup();
}

void httpd_error_die(const char *s) {
    socket_task_exit = 1;
    httpd_error_live(s);
    exit(EXIT_FAILURE);
}
void httpd_create_sock_info(SOCKET s)

{
    httpd_sock_info_t *SI;

    if ((SI = (httpd_sock_info_t *)GlobalAlloc(
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
    SI->ws_state = 0; // not a websocket or not negotiated
    SI->next = httpd_sock_list;
    SI->recv_buf = (uint8_t *)malloc(SI->recv_cap);
    if (SI->recv_buf == nullptr) {
        httpd_error_die("malloc()");
    }
    httpd_sock_list = SI;
}

httpd_sock_info_t *httpd_find_sock_info(SOCKET s) {
    httpd_sock_info_t *SI = httpd_sock_list;

    while (SI) {
        if (SI->sock == s) {
            return SI;
        }

        SI = SI->next;
    }
    return nullptr;
}

void httpd_destroy_sock_info(SOCKET s) {
    httpd_sock_info_t *SI = httpd_sock_list;

    httpd_sock_info_t *PrevSI = NULL;

    while (SI)

    {
        if (SI->sock == s)

        {
            if (PrevSI)

                PrevSI->next = SI->next;

            else

                httpd_sock_list = SI->next;
            if (SI->recv_buf != nullptr) {
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

static LRESULT CALLBACK httpd_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                          LPARAM lParam)

{
    SOCKET Accept;

    DWORD RecvBytes;

    DWORD SendBytes;

    DWORD Flags;
    httpd_sock_info_t *info;
    if (uMsg == WM_SOCKET) {
        puts("Incoming socket event");
        if (WSAGETSELECTERROR(lParam)) {
            printf("Socket failed with error %d\n", WSAGETSELECTERROR(lParam));
            httpd_destroy_sock_info(wParam);
        }

        else {
            printf("Socket looks fine!\n");

            switch (WSAGETSELECTEVENT(lParam))

            {
                case FD_ACCEPT:

                    if ((Accept = accept(wParam, NULL, NULL)) == INVALID_SOCKET)

               {                   

                  printf("accept() failed with error %d\n", WSAGetLastError());

                  break;

               }

               else

                  printf("accept() is OK!\n");

 


 

               // Create a socket information structure to associate with the socket for processing I/O

               httpd_create_sock_info(Accept);

               printf("Socket number %d connected\n", Accept);

               WSAAsyncSelect(Accept, hwnd, WM_SOCKET, FD_READ|FD_WRITE|FD_CLOSE);
               break;
                case FD_READ:
                    info = httpd_find_sock_info(wParam);
                    if (info->ws_state==1) {
                        puts("Receiving websocket handshake");

                    } else {
                        // Read data only if the receive buffer is empty

                        if (info->recv_size >= info->recv_cap) {
                            info->recv_buf = (uint8_t *)realloc(info->recv_buf, info->recv_cap * 2);
                            if (info->recv_buf == nullptr) {
                                httpd_error_die("realloc()");
                            }
                            info->recv_cap *= 2;
                        }
                        info->wsa.buf = (char *)(info->recv_buf + info->recv_size);
                        info->wsa.len = info->recv_cap - info->recv_size;
                        Flags = 0;

                        if (WSARecv(info->sock,
                                    &(info->wsa), 1, &RecvBytes,

                                    &Flags, NULL, NULL) == SOCKET_ERROR)

                        {
                            if (WSAGetLastError() != WSAEWOULDBLOCK)

                            {
                                printf("WSARecv() failed with error %d\n",
                                       WSAGetLastError());

                                httpd_destroy_sock_info(wParam);

                                return 0;
                            }

                        }

                        else  // No error so update the byte count

                        {
                            info->recv_size += RecvBytes;
                        }
                        httpd_context_t *ctx = (httpd_context_t *)malloc(sizeof(httpd_context_t));
                        if (ctx == nullptr) {
                            httpd_error_die("malloc()");
                        }
                        httpd_get_context(info, (const char*)info->recv_buf, ctx);
                        if(ctx->ws_key[0]!=0) {
                            info->ws_state = 1; // handshake
                            puts("sending HTTP WS handshake");
                            httpd_ws_handshake(ctx);
                        } else {
                            info->ws_state = 0;
                            httpd_parse_url_and_apply_alarms(ctx->path_and_query);
                            puts("sending HTTP response");
                            httpd_send_response(ctx);
                        
                        }
                        info->recv_size = 0;

                        //httpd_destroy_sock_info(ctx->sock);
                        //closesocket(ctx->sock);
                        
                    }
                    break;

                case FD_WRITE:

                        puts("FD_WRITE");
                    break;

                case FD_CLOSE:

                    printf("Closing socket %d\n", wParam);

                    httpd_destroy_sock_info(wParam);

                    break;
            }
        }

        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
void httpd_make_event_window(HWND *out_hwnd) {
    WNDCLASS wndclass;

    CHAR *ProviderClass = "AsyncSelect";

    HWND Window;

    wndclass.style = CS_HREDRAW | CS_VREDRAW;

    wndclass.lpfnWndProc = (WNDPROC)httpd_window_proc;

    wndclass.cbClsExtra = 0;

    wndclass.cbWndExtra = 0;

    wndclass.hInstance = NULL;

    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);

    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    wndclass.lpszMenuName = NULL;

    wndclass.lpszClassName = (LPCWSTR)ProviderClass;

    if (RegisterClass(&wndclass) == 0)

    {
        printf("RegisterClass() failed with error %d\n", GetLastError());

        *out_hwnd = NULL;
        return;
    }

    else

        printf("RegisterClass() is OK!\n");

    // Create a window

    if ((Window = CreateWindow(

             (LPCWSTR)ProviderClass,

             L"",

             WS_OVERLAPPEDWINDOW,

             CW_USEDEFAULT,

             CW_USEDEFAULT,

             CW_USEDEFAULT,

             CW_USEDEFAULT,

             NULL,

             NULL,

             NULL,

             NULL)) == NULL)

    {
        printf("CreateWindow() failed with error %d\n", GetLastError());

        *out_hwnd = NULL;
        return;

    }

    else

        printf("CreateWindow() is OK!\n");

    *out_hwnd = Window;
    return;
}