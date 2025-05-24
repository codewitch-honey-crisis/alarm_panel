#include <stdio.h>  
#include "httpd.h"
#define WM_SOCKET (WM_USER+1)

typedef struct httpd_sock_info {
    SOCKET sock;
    WSABUF wsa;
    uint8_t *recv_buf;
    size_t recv_size;
    size_t recv_cap;
    char ws_state;
    struct httpd_sock_info *next;
} httpd_sock_info_t;

static httpd_sock_info_t *httpd_sock_list = NULL;
static HANDLE httpd_sync = NULL;
static HWND httpd_wnd = NULL;
static void(*httpd_on_data_callback)(const ws_srv_frame_t* frame, void* arg, void* state) = NULL;
static void* httpd_on_data_callback_state = NULL;
static void(*httpd_on_req_callback)(httpd_context_t* context, void* state)=NULL;
static void* httpd_on_req_callback_state = NULL;
static void httpd_get_context(httpd_sock_info_t* info,const char* buf, httpd_context_t *out_context);
static void httpd_ws_handshake(httpd_context_t *);
static void httpd_error_die(const char *);
static void httpd_make_event_window(HWND *out_hwnd);
static void httpd_destroy_sock_info(SOCKET sock);

static char enc_rfc3986[256] = {0};
static char enc_html5[256] = {0};
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

static void httpd_error_die(const char *s) {
    fputs("Error: ",stdout);
    puts(s);
    exit(-1);
}
static void httpd_create_sock_info(SOCKET s)

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
    if (SI->recv_buf == NULL) {
        httpd_error_die("malloc()");
    }
    httpd_sock_list = SI;
}

static httpd_sock_info_t *httpd_find_sock_info(SOCKET s) {
    httpd_sock_info_t *SI = httpd_sock_list;

    while (SI) {
        if (SI->sock == s) {
            return SI;
        }

        SI = SI->next;
    }
    return NULL;
}

static void httpd_destroy_sock_info(SOCKET s) {
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

void httpd_on_request_callback(void(*on_request_callback)(httpd_context_t* context, void* state), void* state) {
    httpd_on_req_callback = on_request_callback;
    httpd_on_req_callback_state = state;
}
void httpd_on_ws_data_callback(void(*on_data_callback)(const ws_srv_frame_t* frame, void* arg, void* state), void* state) {
    httpd_on_data_callback = on_data_callback;
    httpd_on_data_callback_state = state;
}
const char *httpd_crack_query(const char *url_part, char *name,
                                     char *value) {
    if (url_part == NULL || !*url_part) return NULL;
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
}

void httpd_ws_handshake(httpd_context_t *context) {
    static const char *http_response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Accept: ";
    static const size_t http_response_len = 97;
    puts("Begin websocket handshake");
    httpd_send_block(http_response, http_response_len, context);
    httpd_send_block(context->ws_key, strlen(context->ws_key), context);
    httpd_send_block("\r\n\r\n", 4, context);
    free(context);
    return;
}

static int httpd_ws_write(const void*data,size_t length,void*state) {
    httpd_sock_info_t* inf = (httpd_sock_info_t* )state;
    DWORD cb;
    inf->wsa.buf = (char*)data;
    inf->wsa.len = length;
    if(SOCKET_ERROR==WSASend(inf->sock,&inf->wsa,1,&cb,0,NULL,NULL)) {
        return -1;
    }
    return 0;
}
static void httpd_ws_control_message(const ws_srv_frame_t* frame, httpd_sock_info_t* info) {
    uint8_t* data = (uint8_t*)frame->send_payload;
    if(frame->masked) {
        for(int i = 0;i<frame->len;++i) {
            data[i]^=frame->mask_key[i%4];
        }
    }
    ws_srv_frame_t newframe;
    switch(frame->type) {
        case WS_SRV_TYPE_CLOSE:
            // puts("websocket close request from client");
            httpd_destroy_sock_info(info->sock);
            break;
        case WS_SRV_TYPE_PING:
            // puts("websocket ping request from client");
            newframe.final = 1;
            newframe.fragmented=0;
            newframe.len=frame->len;
            newframe.send_payload = data;
            newframe.masked = 0;
            newframe.type = WS_SRV_TYPE_PONG;
            ws_srv_send_frame(&newframe,httpd_ws_write,info);
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
    while(bd->remaining>0 && inlen>0) {
        *(p++)=bd->buffer[bd->cursor++];
        --bd->remaining;
        --inlen;
        ++(*in_out_len);
    }
    return 0;
}
void httpd_send_block(const char *data, size_t len, void *arg) {
    if (!data || !len) {
        return;
    }
    httpd_context_t *ctx = (httpd_context_t *)arg;
    DWORD sent;
    httpd_sock_info_t* sinfo = (httpd_sock_info_t*)ctx->sinfo;
    sinfo->wsa.buf = (char*)data;
    sinfo->wsa .len = len;
    WSASend(sinfo->sock,&sinfo->wsa,1,&sent,0,NULL,NULL);
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
        if (WSAGETSELECTERROR(lParam)) {
            printf("Socket failed with error %d\n", WSAGETSELECTERROR(lParam));
            httpd_destroy_sock_info(wParam);
        }

        else {
            
            switch (WSAGETSELECTEVENT(lParam))

            {
                case FD_ACCEPT:

                    if ((Accept = accept(wParam, NULL, NULL)) == INVALID_SOCKET)

               {                   

                  printf("accept() failed with error %d\n", WSAGetLastError());

                  break;

               }

               else


               // Create a socket information structure to associate with the socket for processing I/O

               httpd_create_sock_info(Accept);


               WSAAsyncSelect(Accept, hwnd, WM_SOCKET, FD_READ|FD_WRITE|FD_CLOSE);
               break;
                case FD_READ:
                    info = httpd_find_sock_info(wParam);
                    if(info==NULL) {
                        puts("Aborted read on closing socket");
                        break;
                    }
                    if (info->ws_state==1) {
                        puts("Receiving websocket data");
                        ws_srv_frame_t frame;
                        if(info->recv_buf==NULL) {
                            info->recv_cap = 8192;
                            info->recv_size = 0;
                            info->recv_buf=(uint8_t*)malloc(info->recv_cap);
                            if(info->recv_buf ==NULL) {
                                httpd_error_die("malloc()");
                            }
                        } else {
                            if(info->recv_size!=0)   {
                                puts("Warning: recv_size nonzero");
                            }
                        }
                        
                        DWORD cb;
                        DWORD dwFlags=0;
                        info->wsa.buf=(char*)info->recv_buf;
                        info->wsa.len = info->recv_cap;
                        if(SOCKET_ERROR==WSARecv(info->sock,&info->wsa,1,&cb,&dwFlags,NULL,NULL)) {
                            int err =WSAGetLastError() ;
                            if (err!= WSAEWOULDBLOCK) {
                                puts("Socket error in incoming websocket frame read");
                                httpd_destroy_sock_info(info->sock);
                                return -1;
                            } 
                        }
                        
                        httpd_buf_data_t st = {(size_t)cb,0,(uint8_t*)info->recv_buf};
                        
                        if(0!=ws_srv_recv_frame(httpd_read_buf_data,&st,&frame)) {
                            puts("Error in incoming websocket frame");
                        }
                        frame.send_payload = st.buffer+st.cursor;
                        if(frame.type==WS_SRV_TYPE_BINARY||frame.type==WS_SRV_TYPE_TEXT && frame.final && frame.len>0) {
                            if(httpd_on_data_callback!=NULL) {
                                httpd_on_data_callback(&frame,info,httpd_on_data_callback_state);
                            }
                        } else {
                            httpd_ws_control_message(&frame,info);
                        } 
                        info->recv_size = 0;
                    } else {
                        // Read data only if the receive buffer is empty

                        if (info->recv_size >= info->recv_cap) {
                            info->recv_buf = (uint8_t *)realloc(info->recv_buf, info->recv_cap * 2);
                            if (info->recv_buf == NULL) {
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
                        if (ctx == NULL) {
                            httpd_error_die("malloc()");
                        }
                        httpd_get_context(info, (const char*)info->recv_buf, ctx);
                        if(ctx->ws_key[0]!=0) {
                            info->ws_state = 1; // handshake
                            httpd_ws_handshake(ctx);
                        } else {
                            info->ws_state = 0;
                            if(httpd_on_req_callback!=NULL) {
                                httpd_on_req_callback(ctx,httpd_on_req_callback_state);
                            }                        
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

int httpd_send_ws_frame(const ws_srv_frame_t* frame, void* arg) {
    if(frame!=NULL||arg!=NULL) {
        httpd_sock_info_t* info = (httpd_sock_info_t*)arg;
        if (info->ws_state > 0) {
            if (0 !=
                ws_srv_send_frame(
                    frame,httpd_ws_write,info)) {
                        httpd_destroy_sock_info(info->sock);
                        return -1;
                
            }
        }
    }
    return -1;
}
void httpd_broadcast_ws_frame(const ws_srv_frame_t* frame) {
    WaitForSingleObject(httpd_sync,  // handle to mutex
                        INFINITE);   // no time-out int
    httpd_sock_info_t * info = httpd_sock_list;
    while(info!=NULL) {
        if (info->ws_state > 0) {
            if (0 !=
                ws_srv_send_frame(
                    frame,httpd_ws_write,info)) {
                        info = info->next;
                        httpd_destroy_sock_info(info->sock);
                        continue;
                
            }
        }
        info = info->next;
    }   
    ReleaseMutex(httpd_sync);
}
int httpd_initialize(uint16_t port) {
    if(httpd_sync!=NULL) {
        return 0;
    }
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR)
        httpd_error_die("WSAStartup()");

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

    if(httpd_sync==NULL) {
        return -1;
    }
    int addr_len;
    struct sockaddr_in local, client_addr;
    httpd_make_event_window(&httpd_wnd);
    if(httpd_wnd==NULL) {
        CloseHandle(httpd_sync);
        httpd_sync=NULL;
        WSACleanup();
        return -1;
    }
    SOCKET sock, msg_sock;

    // Fill in the address structure
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        DestroyWindow(httpd_wnd);
        CloseHandle(httpd_sync);
        httpd_sync=NULL;
        WSACleanup();
        return -1;
    }

    if (WSAAsyncSelect(sock, httpd_wnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE) != 0 ||
        bind(sock, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
        closesocket(sock);
        DestroyWindow(httpd_wnd);
        CloseHandle(httpd_sync);
        httpd_sync=NULL;
        WSACleanup();
        return -1;
    }
    if (listen(sock, 10) == SOCKET_ERROR) {
        closesocket(sock);
        DestroyWindow(httpd_wnd);
        CloseHandle(httpd_sync);
        httpd_sync=NULL;
        WSACleanup();
        return -1;
    }
    return 0;

}
void httpd_deinitialize() {
    
}