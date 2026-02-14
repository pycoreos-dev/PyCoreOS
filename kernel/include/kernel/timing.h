#ifndef KERNEL_TIMING_H
#define KERNEL_TIMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void timing_init_from_frame_cycles(uint32_t frame_cycles_60hz);
void timing_sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
