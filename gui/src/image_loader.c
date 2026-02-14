#include "gui/image_loader.h"

#include <stddef.h>
#include <stdint.h>

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8U);
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

static bool decode_bmp(const uint8_t* data, size_t size, uint32_t* out, int out_w, int out_h) {
    if (data == NULL || out == NULL || size < 54 || out_w <= 0 || out_h <= 0) {
        return false;
    }
    if (data[0] != 'B' || data[1] != 'M') {
        return false;
    }

    const uint32_t pixel_offset = read_le32(data + 10);
    const uint32_t dib_size = read_le32(data + 14);
    if (dib_size < 40 || pixel_offset >= size) {
        return false;
    }

    const int32_t src_w = (int32_t)read_le32(data + 18);
    const int32_t src_h_raw = (int32_t)read_le32(data + 22);
    const uint16_t planes = read_le16(data + 26);
    const uint16_t bpp = read_le16(data + 28);
    const uint32_t compression = read_le32(data + 30);

    if (planes != 1 || (bpp != 24 && bpp != 32) || compression != 0 || src_w <= 0 || src_h_raw == 0) {
        return false;
    }

    const bool bottom_up = src_h_raw > 0;
    const int src_h = bottom_up ? src_h_raw : -src_h_raw;
    const int bytes_per_pixel = (int)(bpp / 8U);
    const int row_bytes = src_w * bytes_per_pixel;
    const int row_stride = (row_bytes + 3) & ~3;

    if ((size_t)pixel_offset + (size_t)row_stride * (size_t)src_h > size) {
        return false;
    }

    for (int y = 0; y < out_h; ++y) {
        const int sy = (y * src_h) / out_h;
        const int src_y = bottom_up ? (src_h - 1 - sy) : sy;
        const uint8_t* row = data + pixel_offset + (size_t)src_y * (size_t)row_stride;

        for (int x = 0; x < out_w; ++x) {
            const int sx = (x * src_w) / out_w;
            const uint8_t* p = row + (size_t)sx * (size_t)bytes_per_pixel;
            const uint8_t b = p[0];
            const uint8_t g = p[1];
            const uint8_t r = p[2];
            out[(size_t)y * (size_t)out_w + (size_t)x] = ((uint32_t)r << 16U) | ((uint32_t)g << 8U) | (uint32_t)b;
        }
    }

    return true;
}

static bool decode_tga(const uint8_t* data, size_t size, uint32_t* out, int out_w, int out_h) {
    if (data == NULL || out == NULL || size < 18 || out_w <= 0 || out_h <= 0) {
        return false;
    }

    const uint8_t id_len = data[0];
    const uint8_t color_map_type = data[1];
    const uint8_t image_type = data[2];
    const uint16_t src_w = read_le16(data + 12);
    const uint16_t src_h = read_le16(data + 14);
    const uint8_t bpp = data[16];
    const uint8_t descriptor = data[17];

    if (color_map_type != 0 || image_type != 2 || (bpp != 24 && bpp != 32) || src_w == 0 || src_h == 0) {
        return false;
    }

    const size_t pixel_offset = 18U + (size_t)id_len;
    const int bytes_per_pixel = (int)(bpp / 8U);
    const size_t src_bytes = (size_t)src_w * (size_t)src_h * (size_t)bytes_per_pixel;
    if (pixel_offset + src_bytes > size) {
        return false;
    }

    const bool top_origin = (descriptor & 0x20U) != 0;

    for (int y = 0; y < out_h; ++y) {
        const int sy = (y * (int)src_h) / out_h;
        const int src_y = top_origin ? sy : ((int)src_h - 1 - sy);
        const uint8_t* row = data + pixel_offset + (size_t)src_y * (size_t)src_w * (size_t)bytes_per_pixel;

        for (int x = 0; x < out_w; ++x) {
            const int sx = (x * (int)src_w) / out_w;
            const uint8_t* p = row + (size_t)sx * (size_t)bytes_per_pixel;
            const uint8_t b = p[0];
            const uint8_t g = p[1];
            const uint8_t r = p[2];
            out[(size_t)y * (size_t)out_w + (size_t)x] = ((uint32_t)r << 16U) | ((uint32_t)g << 8U) | (uint32_t)b;
        }
    }

    return true;
}

bool image_loader_decode_bmp_or_tga(const uint8_t* data, size_t size,
                                    uint32_t* out_argb8888,
                                    int out_w, int out_h) {
    if (decode_bmp(data, size, out_argb8888, out_w, out_h)) {
        return true;
    }
    return decode_tga(data, size, out_argb8888, out_w, out_h);
}
