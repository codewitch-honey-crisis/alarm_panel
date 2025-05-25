#ifndef NETWORK_H
#define NETWORK_H
#include <stddef.h>
typedef enum { NET_WAITING, NET_CONNECTED, NET_CONNECT_FAILED } net_status_t;
#ifdef __cplusplus
extern "C" {
#endif
int net_init();
void net_end();
net_status_t net_status();
int net_address(char* out_address,size_t out_address_length);
#ifdef __cplusplus
}
#endif
#endif