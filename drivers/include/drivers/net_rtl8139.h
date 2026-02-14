#ifndef DRIVERS_NET_RTL8139_H
#define DRIVERS_NET_RTL8139_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void rtl8139_init(void);
bool rtl8139_ready(void);
bool rtl8139_get_mac(uint8_t out_mac[6]);
bool rtl8139_send(const void* packet, size_t len);
bool rtl8139_receive(void* out_packet, size_t out_cap, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif
