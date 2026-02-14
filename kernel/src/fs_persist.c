#include "kernel/fs_persist.h"

#include "drivers/ata.h"
#include "kernel/filesystem.h"

#include <stddef.h>
#include <stdint.h>

enum {
    kFsPersistStartLba = 2048U,
    kFsPersistMaxBytes = 300000U,
    kFsPersistHeaderSectors = 1U,
};

static bool s_available = false;

static uint32_t checksum32(const uint8_t* data, size_t size) {
    uint32_t acc = 0xC0DEC0DEU;
    for (size_t i = 0; i < size; ++i) {
        acc ^= (uint32_t)data[i];
        acc = (acc << 5U) | (acc >> 27U);
        acc += 0x9E3779B9U;
    }
    return acc;
}

void fs_persist_init(void) {
    s_available = ata_ready();
}

bool fs_persist_available(void) {
    return s_available;
}

bool fs_persist_save_now(void) {
    if (!s_available) {
        return false;
    }

    static uint8_t image[kFsPersistMaxBytes];
    const size_t image_size = fs_serialize_ramdisk(image, sizeof(image));
    if (image_size == 0) {
        return false;
    }

    uint8_t header[512];
    for (size_t i = 0; i < sizeof(header); ++i) {
        header[i] = 0;
    }

    header[0] = 'P';
    header[1] = 'Y';
    header[2] = 'F';
    header[3] = 'S';
    header[4] = 'I';
    header[5] = 'M';
    header[6] = 'G';
    header[7] = '1';
    header[8] = (uint8_t)(image_size & 0xFFU);
    header[9] = (uint8_t)((image_size >> 8U) & 0xFFU);
    header[10] = (uint8_t)((image_size >> 16U) & 0xFFU);
    header[11] = (uint8_t)((image_size >> 24U) & 0xFFU);
    const uint32_t sum = checksum32(image, image_size);
    header[12] = (uint8_t)(sum & 0xFFU);
    header[13] = (uint8_t)((sum >> 8U) & 0xFFU);
    header[14] = (uint8_t)((sum >> 16U) & 0xFFU);
    header[15] = (uint8_t)((sum >> 24U) & 0xFFU);

    if (!ata_write_sector28(kFsPersistStartLba, header)) {
        return false;
    }

    const size_t data_sectors = (image_size + 511U) / 512U;
    for (size_t s = 0; s < data_sectors; ++s) {
        uint8_t sector[512];
        for (size_t i = 0; i < sizeof(sector); ++i) {
            sector[i] = 0;
        }

        const size_t offset = s * 512U;
        size_t chunk = image_size - offset;
        if (chunk > sizeof(sector)) {
            chunk = sizeof(sector);
        }
        for (size_t i = 0; i < chunk; ++i) {
            sector[i] = image[offset + i];
        }

        if (!ata_write_sector28(kFsPersistStartLba + kFsPersistHeaderSectors + (uint32_t)s, sector)) {
            return false;
        }
    }

    return true;
}

bool fs_persist_load_now(void) {
    if (!s_available) {
        return false;
    }

    uint8_t header[512];
    if (!ata_read_sector28(kFsPersistStartLba, header)) {
        return false;
    }
    if (header[0] != 'P' || header[1] != 'Y' || header[2] != 'F' || header[3] != 'S' ||
        header[4] != 'I' || header[5] != 'M' || header[6] != 'G' || header[7] != '1') {
        return false;
    }

    const size_t image_size = (size_t)header[8] |
                              ((size_t)header[9] << 8U) |
                              ((size_t)header[10] << 16U) |
                              ((size_t)header[11] << 24U);
    const uint32_t expected_sum = (uint32_t)header[12] |
                                  ((uint32_t)header[13] << 8U) |
                                  ((uint32_t)header[14] << 16U) |
                                  ((uint32_t)header[15] << 24U);
    if (image_size == 0 || image_size > kFsPersistMaxBytes) {
        return false;
    }

    static uint8_t image[kFsPersistMaxBytes];
    const size_t data_sectors = (image_size + 511U) / 512U;
    for (size_t s = 0; s < data_sectors; ++s) {
        uint8_t sector[512];
        if (!ata_read_sector28(kFsPersistStartLba + kFsPersistHeaderSectors + (uint32_t)s, sector)) {
            return false;
        }

        const size_t offset = s * 512U;
        size_t chunk = image_size - offset;
        if (chunk > sizeof(sector)) {
            chunk = sizeof(sector);
        }
        for (size_t i = 0; i < chunk; ++i) {
            image[offset + i] = sector[i];
        }
    }

    if (checksum32(image, image_size) != expected_sum) {
        return false;
    }
    return fs_deserialize_ramdisk(image, image_size);
}

bool fs_save_to_disk(void) {
    return fs_persist_save_now();
}

bool fs_load_from_disk(void) {
    return fs_persist_load_now();
}
