#include "drivers/net_rtl8139.h"

#include <stddef.h>
#include <stdint.h>

enum {
    kPciCfgAddr = 0xCF8,
    kPciCfgData = 0xCFC,

    kRtlVendor = 0x10EC,
    kRtlDevice = 0x8139,

    kRegIdr0 = 0x00,
    kRegTsd0 = 0x10,
    kRegTsad0 = 0x20,
    kRegRbstart = 0x30,
    kRegCr = 0x37,
    kRegCapr = 0x38,
    kRegCbr = 0x3A,
    kRegImr = 0x3C,
    kRegIsr = 0x3E,
    kRegTcr = 0x40,
    kRegRcr = 0x44,
    kRegConfig1 = 0x52,

    kCrRe = 0x08,
    kCrTe = 0x04,
    kCrReset = 0x10,
    kCrBufEmpty = 0x01,

    kRxRingBytes = 8192,
    kRxRingAlloc = kRxRingBytes + 16 + 1500,
    kTxSlots = 4,
    kTxBufBytes = 2048,
};

static bool s_ready = false;
static uint16_t s_io_base = 0;
static uint8_t s_rx_ring[kRxRingAlloc] __attribute__((aligned(16)));
static uint8_t s_tx_buf[kTxSlots][kTxBufBytes] __attribute__((aligned(4)));
static uint8_t s_tx_next = 0;
static uint16_t s_rx_read = 0;
static uint8_t s_mac[6] = {0, 0, 0, 0, 0, 0};

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    const uint32_t addr = 0x80000000U |
                          ((uint32_t)bus << 16U) |
                          ((uint32_t)slot << 11U) |
                          ((uint32_t)func << 8U) |
                          (uint32_t)(offset & 0xFCU);
    outl(kPciCfgAddr, addr);
    return inl(kPciCfgData);
}

static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    const uint32_t addr = 0x80000000U |
                          ((uint32_t)bus << 16U) |
                          ((uint32_t)slot << 11U) |
                          ((uint32_t)func << 8U) |
                          (uint32_t)(offset & 0xFCU);
    outl(kPciCfgAddr, addr);
    outl(kPciCfgData, value);
}

static uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    const uint32_t value = pci_read32(bus, slot, func, offset);
    return (uint16_t)((value >> ((offset & 2U) * 8U)) & 0xFFFFU);
}

static void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    const uint8_t aligned = (uint8_t)(offset & 0xFCU);
    uint32_t reg = pci_read32(bus, slot, func, aligned);
    const uint32_t shift = (uint32_t)(offset & 2U) * 8U;
    reg &= ~(0xFFFFU << shift);
    reg |= (uint32_t)value << shift;
    pci_write32(bus, slot, func, aligned, reg);
}

static bool rtl_find_pci(uint8_t* out_bus, uint8_t* out_slot, uint8_t* out_func) {
    for (uint16_t bus = 0; bus < 256U; ++bus) {
        for (uint8_t slot = 0; slot < 32U; ++slot) {
            for (uint8_t func = 0; func < 8U; ++func) {
                const uint16_t vendor = pci_read16((uint8_t)bus, slot, func, 0x00);
                if (vendor == 0xFFFFU) {
                    if (func == 0U) {
                        break;
                    }
                    continue;
                }
                const uint16_t device = pci_read16((uint8_t)bus, slot, func, 0x02);
                if (vendor == kRtlVendor && device == kRtlDevice) {
                    *out_bus = (uint8_t)bus;
                    *out_slot = slot;
                    *out_func = func;
                    return true;
                }
            }
        }
    }
    return false;
}

static void rtl_reset(void) {
    outb((uint16_t)(s_io_base + kRegCr), kCrReset);
    for (uint32_t i = 0; i < 200000U; ++i) {
        if ((inb((uint16_t)(s_io_base + kRegCr)) & kCrReset) == 0U) {
            break;
        }
    }
}

static uint16_t read_ring16(uint16_t offset) {
    const uint16_t a = (uint16_t)(offset % kRxRingBytes);
    const uint16_t b = (uint16_t)((offset + 1U) % kRxRingBytes);
    return (uint16_t)s_rx_ring[a] | ((uint16_t)s_rx_ring[b] << 8U);
}

static uint8_t read_ring8(uint16_t offset) {
    return s_rx_ring[offset % kRxRingBytes];
}

void rtl8139_init(void) {
    s_ready = false;
    s_io_base = 0;
    s_tx_next = 0;
    s_rx_read = 0;

    uint8_t bus = 0;
    uint8_t slot = 0;
    uint8_t func = 0;
    if (!rtl_find_pci(&bus, &slot, &func)) {
        return;
    }

    uint16_t cmd = pci_read16(bus, slot, func, 0x04);
    cmd |= 0x0005U;
    pci_write16(bus, slot, func, 0x04, cmd);

    const uint32_t bar0 = pci_read32(bus, slot, func, 0x10);
    if ((bar0 & 0x1U) == 0U) {
        return;
    }
    s_io_base = (uint16_t)(bar0 & 0xFFFCU);
    if (s_io_base == 0U) {
        return;
    }

    outb((uint16_t)(s_io_base + kRegConfig1), 0x00);
    rtl_reset();

    outl((uint16_t)(s_io_base + kRegRbstart), (uint32_t)(uintptr_t)s_rx_ring);
    outw((uint16_t)(s_io_base + kRegImr), 0x0005U);
    outw((uint16_t)(s_io_base + kRegIsr), 0xFFFFU);

    outl((uint16_t)(s_io_base + kRegRcr), 0x0000000FU | (1U << 7));
    outl((uint16_t)(s_io_base + kRegTcr), 0x03000600U);
    outb((uint16_t)(s_io_base + kRegCr), (uint8_t)(kCrRe | kCrTe));

    for (int i = 0; i < 6; ++i) {
        s_mac[i] = inb((uint16_t)(s_io_base + kRegIdr0 + i));
    }

    s_ready = true;
}

bool rtl8139_ready(void) {
    return s_ready;
}

bool rtl8139_get_mac(uint8_t out_mac[6]) {
    if (!s_ready || out_mac == NULL) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        out_mac[i] = s_mac[i];
    }
    return true;
}

bool rtl8139_send(const void* packet, size_t len) {
    if (!s_ready || packet == NULL || len == 0 || len > 1514U) {
        return false;
    }

    const uint8_t slot = s_tx_next;
    s_tx_next = (uint8_t)((s_tx_next + 1U) % kTxSlots);

    const uint8_t* src = (const uint8_t*)packet;
    for (size_t i = 0; i < len; ++i) {
        s_tx_buf[slot][i] = src[i];
    }

    outl((uint16_t)(s_io_base + kRegTsad0 + slot * 4U), (uint32_t)(uintptr_t)s_tx_buf[slot]);
    outl((uint16_t)(s_io_base + kRegTsd0 + slot * 4U), (uint32_t)len);
    return true;
}

bool rtl8139_receive(void* out_packet, size_t out_cap, size_t* out_len) {
    if (out_len != NULL) {
        *out_len = 0;
    }
    if (!s_ready || out_packet == NULL || out_cap == 0U) {
        return false;
    }

    const uint8_t cr = inb((uint16_t)(s_io_base + kRegCr));
    if ((cr & kCrBufEmpty) != 0U) {
        return false;
    }

    const uint16_t status = read_ring16(s_rx_read);
    const uint16_t frame_len_raw = read_ring16((uint16_t)(s_rx_read + 2U));
    if ((status & 0x1U) == 0U || frame_len_raw < 4U || frame_len_raw > 1792U) {
        rtl_reset();
        return false;
    }

    size_t frame_len = (size_t)frame_len_raw - 4U;
    if (frame_len > out_cap) {
        frame_len = out_cap;
    }

    uint8_t* dst = (uint8_t*)out_packet;
    const uint16_t payload = (uint16_t)(s_rx_read + 4U);
    for (size_t i = 0; i < frame_len; ++i) {
        dst[i] = read_ring8((uint16_t)(payload + (uint16_t)i));
    }

    uint16_t next = (uint16_t)(s_rx_read + frame_len_raw + 4U);
    next = (uint16_t)((next + 3U) & ~3U);
    next %= kRxRingBytes;
    s_rx_read = next;

    outw((uint16_t)(s_io_base + kRegCapr), (uint16_t)(s_rx_read - 16U));
    outw((uint16_t)(s_io_base + kRegIsr), 0xFFFFU);

    if (out_len != NULL) {
        *out_len = frame_len;
    }
    return true;
}
