#ifndef KERNEL_NET_STACK_H
#define KERNEL_NET_STACK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void net_stack_init(void);
bool net_stack_ready(void);
void net_stack_poll(void);
bool net_stack_send_ping(uint32_t ipv4_be);

#ifdef __cplusplus
}
#endif

#endif
