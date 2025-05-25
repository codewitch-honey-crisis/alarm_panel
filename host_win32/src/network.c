#include "network.h"
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winsock2.h>
static char net_initialized = 0;
int net_init() {
    if(net_initialized) {
        return 0;
    }
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR) {
        return -1;
    }
    net_initialized =1;
    return 0;

}
void net_end() {
    if(!net_initialized) {
        return;
    }
    WSACleanup();
    net_initialized = 0;
}
net_status_t net_status() {
    if(net_initialized) {
        return NET_CONNECTED;
    } 
    return NET_WAITING;
}
int net_address(char* out_address,size_t out_address_length) {
    if(!net_initialized) {
        return -1;
    }
    strncpy(out_address,"localhost",out_address_length);
    return 0;
}