#include "kernel/net_stack.h"

#include "drivers/net_rtl8139.h"

#include <stddef.h>
#include <stdint.h>

static bool s_ready = false;
static uint16_t s_ip_id = 1;
static uint16_t s_icmp_seq = 1;
static uint8_t s_local_mac[6] = {0x02, 0x50, 0x79, 0x43, 0x4F, 0x53};
static const uint8_t kLocalIp[4] = {10, 0, 2, 15};

static uint16_t read_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8U) | (uint16_t)p[1]);
}

static void write_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8U);
    p[1] = (uint8_t)(v & 0xFFU);
}

static void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24U) & 0xFFU);
    p[1] = (uint8_t)((v >> 16U) & 0xFFU);
    p[2] = (uint8_t)((v >> 8U) & 0xFFU);
    p[3] = (uint8_t)(v & 0xFFU);
}

static bool ipv4_eq_local(const uint8_t* ip4) {
    return ip4[0] == kLocalIp[0] && ip4[1] == kLocalIp[1] && ip4[2] == kLocalIp[2] && ip4[3] == kLocalIp[3];
}

static uint16_t checksum16(const uint8_t* data, uint32_t len) {
    uint32_t sum = 0;
    uint32_t i = 0;
    while (i + 1U < len) {
        const uint16_t word = (uint16_t)((uint16_t)data[i] << 8U) | (uint16_t)data[i + 1U];
        sum += (uint32_t)word;
        i += 2U;
    }
    if (i < len) {
        sum += (uint32_t)((uint16_t)data[i] << 8U);
    }

    while ((sum >> 16U) != 0U) {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return (uint16_t)~sum;
}

static void handle_arp(const uint8_t* frame, size_t len) {
    if (len < 14U + 28U) {
        return;
    }

    const uint8_t* arp = frame + 14;
    const uint16_t htype = read_be16(arp + 0);
    const uint16_t ptype = read_be16(arp + 2);
    const uint8_t hlen = arp[4];
    const uint8_t plen = arp[5];
    const uint16_t op = read_be16(arp + 6);
    const uint8_t* sender_mac = arp + 8;
    const uint8_t* sender_ip = arp + 14;
    const uint8_t* target_ip = arp + 24;

    if (htype != 1U || ptype != 0x0800U || hlen != 6U || plen != 4U || op != 1U) {
        return;
    }
    if (!ipv4_eq_local(target_ip)) {
        return;
    }

    uint8_t reply[42];
    for (int i = 0; i < 6; ++i) {
        reply[i] = sender_mac[i];
        reply[6 + i] = s_local_mac[i];
    }
    reply[12] = 0x08;
    reply[13] = 0x06;

    uint8_t* rarp = reply + 14;
    write_be16(rarp + 0, 1U);
    write_be16(rarp + 2, 0x0800U);
    rarp[4] = 6U;
    rarp[5] = 4U;
    write_be16(rarp + 6, 2U);
    for (int i = 0; i < 6; ++i) {
        rarp[8 + i] = s_local_mac[i];
        rarp[18 + i] = sender_mac[i];
    }
    for (int i = 0; i < 4; ++i) {
        rarp[14 + i] = kLocalIp[i];
        rarp[24 + i] = sender_ip[i];
    }

    (void)rtl8139_send(reply, sizeof(reply));
}

static void handle_ipv4(const uint8_t* frame, size_t len) {
    if (len < 14U + 20U) {
        return;
    }

    const uint8_t* ip = frame + 14;
    const uint8_t ihl = (uint8_t)((ip[0] & 0x0FU) * 4U);
    if (ihl < 20U || len < 14U + ihl) {
        return;
    }

    const uint16_t total_len = read_be16(ip + 2);
    if (total_len < ihl || len < 14U + total_len) {
        return;
    }
    if (!ipv4_eq_local(ip + 16)) {
        return;
    }

    if (ip[9] != 1U) {
        return;
    }

    const uint8_t* icmp = ip + ihl;
    const uint16_t icmp_len = (uint16_t)(total_len - ihl);
    if (icmp_len < 8U || icmp[0] != 8U || icmp[1] != 0U) {
        return;
    }

    uint8_t reply[1514];
    const size_t frame_len = (size_t)14U + (size_t)total_len;
    if (frame_len > sizeof(reply)) {
        return;
    }

    for (size_t i = 0; i < frame_len; ++i) {
        reply[i] = frame[i];
    }

    for (int i = 0; i < 6; ++i) {
        reply[i] = frame[6 + i];
        reply[6 + i] = s_local_mac[i];
    }

    uint8_t* rip = reply + 14;
    for (int i = 0; i < 4; ++i) {
        rip[16 + i] = rip[12 + i];
        rip[12 + i] = kLocalIp[i];
    }
    rip[8] = 64U;
    rip[10] = 0U;
    rip[11] = 0U;
    const uint16_t ip_sum = checksum16(rip, ihl);
    write_be16(rip + 10, ip_sum);

    uint8_t* ricmp = rip + ihl;
    ricmp[0] = 0U;
    ricmp[1] = 0U;
    ricmp[2] = 0U;
    ricmp[3] = 0U;
    const uint16_t icmp_sum = checksum16(ricmp, icmp_len);
    write_be16(ricmp + 2, icmp_sum);

    (void)rtl8139_send(reply, frame_len);
}

void net_stack_init(void) {
    s_ready = rtl8139_ready();
    if (!s_ready) {
        return;
    }
    (void)rtl8139_get_mac(s_local_mac);
}

bool net_stack_ready(void) {
    return s_ready;
}

void net_stack_poll(void) {
    if (!s_ready) {
        return;
    }

    uint8_t frame[1600];
    size_t len = 0;

    int budget = 6;
    while (budget-- > 0 && rtl8139_receive(frame, sizeof(frame), &len)) {
        if (len < 14U) {
            continue;
        }

        const uint16_t eth_type = read_be16(frame + 12);
        if (eth_type == 0x0806U) {
            handle_arp(frame, len);
            continue;
        }
        if (eth_type == 0x0800U) {
            handle_ipv4(frame, len);
            continue;
        }
    }
}

bool net_stack_send_ping(uint32_t ipv4_be) {
    if (!s_ready) {
        return false;
    }

    uint8_t frame[64];
    for (uint32_t i = 0; i < sizeof(frame); ++i) {
        frame[i] = 0;
    }

    /* Ethernet header. */
    for (int i = 0; i < 6; ++i) {
        frame[i] = 0xFFU;
        frame[6 + i] = s_local_mac[i];
    }
    frame[12] = 0x08U;
    frame[13] = 0x00U;

    /* IPv4 header. */
    uint8_t* ip = &frame[14];
    ip[0] = 0x45U;
    ip[1] = 0x00U;
    ip[2] = 0x00U;
    ip[3] = 28U;
    const uint16_t id = s_ip_id++;
    write_be16(ip + 4, id);
    ip[6] = 0x00U;
    ip[7] = 0x00U;
    ip[8] = 64U;
    ip[9] = 1U;
    ip[10] = 0x00U;
    ip[11] = 0x00U;
    for (int i = 0; i < 4; ++i) {
        ip[12 + i] = kLocalIp[i];
    }
    write_be32(ip + 16, ipv4_be);
    const uint16_t ip_sum = checksum16(ip, 20U);
    write_be16(ip + 10, ip_sum);

    /* ICMP echo request, empty payload. */
    uint8_t* icmp = &frame[34];
    icmp[0] = 8U;
    icmp[1] = 0U;
    icmp[2] = 0U;
    icmp[3] = 0U;
    write_be16(icmp + 4, 0xC0DEU);
    write_be16(icmp + 6, s_icmp_seq++);
    const uint16_t icmp_sum = checksum16(icmp, 8U);
    write_be16(icmp + 2, icmp_sum);

    return rtl8139_send(frame, 42U);
}
