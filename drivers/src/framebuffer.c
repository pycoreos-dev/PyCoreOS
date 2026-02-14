#include "drivers/framebuffer.h"

#include "kernel/multiboot.h"

#include <stddef.h>
#include <stdint.h>

static uint8_t* s_fb = NULL;
static uint32_t s_width = 0;
static uint32_t s_height = 0;
static uint32_t s_pitch = 0;
static uint8_t s_bpp = 0;
static bool s_ready = false;

enum {
    kScreenWidth = 1280,
    kScreenHeight = 720,
    kScreenBpp = 32,
};

static inline void copy_u32_words(uint32_t* dst, const uint32_t* src, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
}

struct vbe_mode_info_block {
    uint16_t attributes;
    uint8_t win_a;
    uint8_t win_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t w_char;
    uint8_t y_char;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;
    uint32_t framebuffer;
} __attribute__((packed));

static bool init_from_multiboot_framebuffer(const struct multiboot_info* mb) {
    if ((mb->flags & MULTIBOOT_INFO_FRAMEBUFFER) == 0) {
        return false;
    }
    if (mb->framebuffer_type != 1) {
        return false;
    }
    if (mb->framebuffer_bpp != kScreenBpp) {
        return false;
    }
    if (mb->framebuffer_width != kScreenWidth || mb->framebuffer_height != kScreenHeight) {
        return false;
    }

    s_fb = (uint8_t*)(uintptr_t)mb->framebuffer_addr;
    s_width = kScreenWidth;
    s_height = kScreenHeight;
    s_pitch = mb->framebuffer_pitch;
    s_bpp = mb->framebuffer_bpp;
    return true;
}

static bool init_from_vbe_mode_info(const struct multiboot_info* mb) {
    if ((mb->flags & MULTIBOOT_INFO_VBE_INFO) == 0) {
        return false;
    }
    if (mb->vbe_mode_info == 0) {
        return false;
    }

    const struct vbe_mode_info_block* vbe = (const struct vbe_mode_info_block*)(uintptr_t)mb->vbe_mode_info;
    if ((vbe->attributes & 0x80U) == 0) {
        return false;
    }
    if (vbe->framebuffer == 0 || vbe->width == 0 || vbe->height == 0 || vbe->pitch == 0) {
        return false;
    }
    if (vbe->bpp != kScreenBpp) {
        return false;
    }
    if ((uint32_t)vbe->width != kScreenWidth || (uint32_t)vbe->height != kScreenHeight) {
        return false;
    }

    s_fb = (uint8_t*)(uintptr_t)vbe->framebuffer;
    s_width = kScreenWidth;
    s_height = kScreenHeight;
    s_pitch = vbe->pitch;
    s_bpp = vbe->bpp;
    return true;
}

bool framebuffer_init(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    s_ready = false;

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        return false;
    }

    const struct multiboot_info* mb = (const struct multiboot_info*)(uintptr_t)multiboot_info_addr;
    if (!init_from_multiboot_framebuffer(mb) && !init_from_vbe_mode_info(mb)) {
        return false;
    }

    if (s_fb == NULL || s_width == 0 || s_height == 0 || s_pitch == 0) {
        return false;
    }

    s_ready = true;
    return true;
}

bool framebuffer_ready(void) {
    return s_ready;
}

uint32_t framebuffer_width(void) {
    return s_width;
}

uint32_t framebuffer_height(void) {
    return s_height;
}

uint32_t framebuffer_bpp(void) {
    return (uint32_t)s_bpp;
}

uint32_t framebuffer_pitch(void) {
    return s_pitch;
}

void framebuffer_draw_pixel(int x, int y, uint32_t color) {
    if (!s_ready) {
        return;
    }
    if (x < 0 || y < 0) {
        return;
    }
    if ((uint32_t)x >= s_width || (uint32_t)y >= s_height) {
        return;
    }

    uint8_t* p = s_fb + (size_t)y * s_pitch + (size_t)x * (s_bpp / 8U);
    if (s_bpp == 32) {
        *(uint32_t*)p = color;
    } else if (s_bpp == 16) {
        uint16_t r = (uint16_t)((color >> 19U) & 0x1FU);
        uint16_t g = (uint16_t)((color >> 10U) & 0x3FU);
        uint16_t b = (uint16_t)((color >> 3U) & 0x1FU);
        *(uint16_t*)p = (uint16_t)((r << 11U) | (g << 5U) | b);
    } else {
        p[0] = (uint8_t)(color & 0xFFU);
        p[1] = (uint8_t)((color >> 8U) & 0xFFU);
        p[2] = (uint8_t)((color >> 16U) & 0xFFU);
    }
}

static inline uint16_t rgb888_to_565(uint32_t color) {
    const uint16_t r = (uint16_t)((color >> 19U) & 0x1FU);
    const uint16_t g = (uint16_t)((color >> 10U) & 0x3FU);
    const uint16_t b = (uint16_t)((color >> 3U) & 0x1FU);
    return (uint16_t)((r << 11U) | (g << 5U) | b);
}

void framebuffer_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!s_ready || w <= 0 || h <= 0) {
        return;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)s_width) x1 = (int)s_width;
    if (y1 > (int)s_height) y1 = (int)s_height;

    const int width = x1 - x0;
    const int height = y1 - y0;
    if (width <= 0 || height <= 0) {
        return;
    }

    if (s_bpp == 32) {
        for (int py = 0; py < height; ++py) {
            uint32_t* dst = (uint32_t*)(s_fb + (size_t)(y0 + py) * s_pitch + (size_t)x0 * 4U);
            for (int px = 0; px < width; ++px) {
                dst[px] = color;
            }
        }
        return;
    }

    if (s_bpp == 16) {
        const uint16_t c16 = rgb888_to_565(color);
        for (int py = 0; py < height; ++py) {
            uint16_t* dst = (uint16_t*)(s_fb + (size_t)(y0 + py) * s_pitch + (size_t)x0 * 2U);
            for (int px = 0; px < width; ++px) {
                dst[px] = c16;
            }
        }
        return;
    }

    const uint8_t blue = (uint8_t)(color & 0xFFU);
    const uint8_t green = (uint8_t)((color >> 8U) & 0xFFU);
    const uint8_t red = (uint8_t)((color >> 16U) & 0xFFU);
    for (int py = y0; py < y1; ++py) {
        uint8_t* dst = s_fb + (size_t)py * s_pitch + (size_t)x0 * 3U;
        for (int px = 0; px < width; ++px) {
            dst[0] = blue;
            dst[1] = green;
            dst[2] = red;
            dst += 3;
        }
    }
}

void framebuffer_clear(uint32_t color) {
    framebuffer_fill_rect(0, 0, (int)s_width, (int)s_height, color);
}

void framebuffer_present_argb8888(const uint32_t* src, uint32_t src_pitch_pixels) {
    if (!s_ready || src == NULL || src_pitch_pixels == 0) {
        return;
    }

    if (s_bpp == 32) {
        if (src_pitch_pixels == s_width && s_pitch == s_width * 4U) {
            copy_u32_words((uint32_t*)s_fb, src, s_width * s_height);
            return;
        }
        for (uint32_t y = 0; y < s_height; ++y) {
            const uint32_t* src_row = src + (size_t)y * src_pitch_pixels;
            uint32_t* dst_row = (uint32_t*)(s_fb + (size_t)y * s_pitch);
            copy_u32_words(dst_row, src_row, s_width);
        }
        return;
    }

    if (s_bpp == 16) {
        for (uint32_t y = 0; y < s_height; ++y) {
            const uint32_t* src_row = src + (size_t)y * src_pitch_pixels;
            uint16_t* dst_row = (uint16_t*)(s_fb + (size_t)y * s_pitch);
            for (uint32_t x = 0; x < s_width; ++x) {
                dst_row[x] = rgb888_to_565(src_row[x]);
            }
        }
        return;
    }

    for (uint32_t y = 0; y < s_height; ++y) {
        const uint32_t* src_row = src + (size_t)y * src_pitch_pixels;
        uint8_t* dst_row = s_fb + (size_t)y * s_pitch;
        for (uint32_t x = 0; x < s_width; ++x) {
            const uint32_t color = src_row[x];
            dst_row[0] = (uint8_t)(color & 0xFFU);
            dst_row[1] = (uint8_t)((color >> 8U) & 0xFFU);
            dst_row[2] = (uint8_t)((color >> 16U) & 0xFFU);
            dst_row += 3;
        }
    }
}

void framebuffer_present_argb8888_rect(const uint32_t* src, uint32_t src_pitch_pixels, int x, int y, int w, int h) {
    if (!s_ready || src == NULL || src_pitch_pixels == 0 || w <= 0 || h <= 0) {
        return;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)s_width) x1 = (int)s_width;
    if (y1 > (int)s_height) y1 = (int)s_height;

    const int width = x1 - x0;
    const int height = y1 - y0;
    if (width <= 0 || height <= 0) {
        return;
    }

    if (s_bpp == 32) {
        for (int row = 0; row < height; ++row) {
            const uint32_t* src_row = src + (size_t)(y0 + row) * src_pitch_pixels + (size_t)x0;
            uint32_t* dst_row = (uint32_t*)(s_fb + (size_t)(y0 + row) * s_pitch + (size_t)x0 * 4U);
            copy_u32_words(dst_row, src_row, (uint32_t)width);
        }
        return;
    }

    if (s_bpp == 16) {
        for (int row = 0; row < height; ++row) {
            const uint32_t* src_row = src + (size_t)(y0 + row) * src_pitch_pixels + (size_t)x0;
            uint16_t* dst_row = (uint16_t*)(s_fb + (size_t)(y0 + row) * s_pitch + (size_t)x0 * 2U);
            for (int col = 0; col < width; ++col) {
                dst_row[col] = rgb888_to_565(src_row[col]);
            }
        }
        return;
    }

    for (int row = 0; row < height; ++row) {
        const uint32_t* src_row = src + (size_t)(y0 + row) * src_pitch_pixels + (size_t)x0;
        uint8_t* dst_row = s_fb + (size_t)(y0 + row) * s_pitch + (size_t)x0 * 3U;
        for (int col = 0; col < width; ++col) {
            const uint32_t color = src_row[col];
            dst_row[0] = (uint8_t)(color & 0xFFU);
            dst_row[1] = (uint8_t)((color >> 8U) & 0xFFU);
            dst_row[2] = (uint8_t)((color >> 16U) & 0xFFU);
            dst_row += 3;
        }
    }
}
