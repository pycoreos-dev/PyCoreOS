#include "drivers/ata.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    kAtaData = 0x1F0,
    kAtaError = 0x1F1,
    kAtaSectorCount = 0x1F2,
    kAtaLbaLow = 0x1F3,
    kAtaLbaMid = 0x1F4,
    kAtaLbaHigh = 0x1F5,
    kAtaDrive = 0x1F6,
    kAtaStatus = 0x1F7,
    kAtaCommand = 0x1F7,

    kAtaStatusErr = 0x01,
    kAtaStatusDrq = 0x08,
    kAtaStatusDf = 0x20,
    kAtaStatusDrdy = 0x40,
    kAtaStatusBusy = 0x80,

    kAtaCmdReadSectors = 0x20,
    kAtaCmdWriteSectors = 0x30,
    kAtaCmdIdentify = 0xEC,

    kAtaPollBudget = 100000,
};

static bool s_ready = false;

static inline void io_wait(void) {
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
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

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static bool ata_poll(bool require_drq) {
    for (int i = 0; i < 4; ++i) {
        (void)inb(kAtaStatus);
    }

    for (uint32_t i = 0; i < kAtaPollBudget; ++i) {
        const uint8_t status = inb(kAtaStatus);
        if ((status & kAtaStatusBusy) != 0) {
            continue;
        }
        if ((status & (kAtaStatusErr | kAtaStatusDf)) != 0) {
            return false;
        }
        if (!require_drq || (status & kAtaStatusDrq) != 0) {
            return true;
        }
    }
    return false;
}

static void ata_select_drive(uint32_t lba) {
    outb(kAtaDrive, (uint8_t)(0xE0U | ((lba >> 24U) & 0x0FU)));
    io_wait();
}

void ata_init(void) {
    s_ready = false;

    ata_select_drive(0);
    outb(kAtaSectorCount, 0);
    outb(kAtaLbaLow, 0);
    outb(kAtaLbaMid, 0);
    outb(kAtaLbaHigh, 0);
    outb(kAtaCommand, kAtaCmdIdentify);

    const uint8_t status = inb(kAtaStatus);
    if (status == 0) {
        return;
    }
    if (!ata_poll(true)) {
        return;
    }

    for (int i = 0; i < 256; ++i) {
        (void)inw(kAtaData);
    }

    s_ready = true;
}

bool ata_ready(void) {
    return s_ready;
}

bool ata_read_sector28(uint32_t lba, uint8_t* out512) {
    if (!s_ready || out512 == NULL) {
        return false;
    }

    ata_select_drive(lba);
    outb(kAtaSectorCount, 1);
    outb(kAtaLbaLow, (uint8_t)(lba & 0xFFU));
    outb(kAtaLbaMid, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(kAtaLbaHigh, (uint8_t)((lba >> 16U) & 0xFFU));
    outb(kAtaCommand, kAtaCmdReadSectors);

    if (!ata_poll(true)) {
        return false;
    }

    for (int i = 0; i < 256; ++i) {
        const uint16_t word = inw(kAtaData);
        out512[i * 2] = (uint8_t)(word & 0xFFU);
        out512[i * 2 + 1] = (uint8_t)((word >> 8U) & 0xFFU);
    }
    return true;
}

bool ata_write_sector28(uint32_t lba, const uint8_t* in512) {
    if (!s_ready || in512 == NULL) {
        return false;
    }

    ata_select_drive(lba);
    outb(kAtaSectorCount, 1);
    outb(kAtaLbaLow, (uint8_t)(lba & 0xFFU));
    outb(kAtaLbaMid, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(kAtaLbaHigh, (uint8_t)((lba >> 16U) & 0xFFU));
    outb(kAtaCommand, kAtaCmdWriteSectors);

    if (!ata_poll(true)) {
        return false;
    }

    for (int i = 0; i < 256; ++i) {
        const uint16_t word = (uint16_t)in512[i * 2] | ((uint16_t)in512[i * 2 + 1] << 8U);
        outw(kAtaData, word);
    }

    io_wait();
    outb(kAtaCommand, 0xE7);
    return ata_poll(false);
}
