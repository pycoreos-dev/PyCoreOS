#ifndef GUI_IMAGE_LOADER_H
#define GUI_IMAGE_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool image_loader_decode_bmp_or_tga(const uint8_t* data, size_t size,
                                    uint32_t* out_argb8888,
                                    int out_w, int out_h);

#ifdef __cplusplus
}
#endif

#endif
