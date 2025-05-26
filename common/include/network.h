#ifndef NETWORK_H
#define NETWORK_H
#include <stddef.h>
typedef enum { NET_WAITING, NET_CONNECTED, NET_CONNECT_FAILED } net_status_t;
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Begins initializing the network
/// @return 0 on success, non-zero on error
int net_init(void);
/// @brief Ends the network stack
void net_end(void);
/// @brief Reports the status of the network
/// @return One of net_status_t values
net_status_t net_status(void);
/// @brief Retrieves the address of the device
/// @param out_address A buffer to hold the address
/// @param out_address_length The length of the address buffer
/// @return 0 on success, non-zero on error
int net_address(char* out_address,size_t out_address_length);
#ifdef __cplusplus
}
#endif
#endif