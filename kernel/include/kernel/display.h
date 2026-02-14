#ifndef KERNEL_DISPLAY_H
#define KERNEL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void display_init(void);
bool display_ready(void);
uint32_t display_width(void);
uint32_t display_height(void);
uint32_t display_bpp(void);
uint32_t display_pitch(void);

#ifdef __cplusplus
}
#endif

#endif
