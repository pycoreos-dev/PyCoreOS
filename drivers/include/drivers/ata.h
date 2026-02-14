#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ata_init(void);
bool ata_ready(void);
bool ata_read_sector28(uint32_t lba, uint8_t* out512);
bool ata_write_sector28(uint32_t lba, const uint8_t* in512);

#ifdef __cplusplus
}
#endif

#endif
