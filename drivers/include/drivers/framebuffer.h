#ifndef DRIVERS_FRAMEBUFFER_H
#define DRIVERS_FRAMEBUFFER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool framebuffer_init(uint32_t multiboot_magic, uint32_t multiboot_info_addr);
bool framebuffer_ready(void);
uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
uint32_t framebuffer_bpp(void);
uint32_t framebuffer_pitch(void);
void framebuffer_clear(uint32_t color);
void framebuffer_fill_rect(int x, int y, int w, int h, uint32_t color);
void framebuffer_draw_pixel(int x, int y, uint32_t color);
void framebuffer_present_argb8888(const uint32_t* src, uint32_t src_pitch_pixels);
void framebuffer_present_argb8888_rect(const uint32_t* src, uint32_t src_pitch_pixels, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
