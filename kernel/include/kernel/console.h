#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void console_init(void);
void console_clear(uint8_t color);
void console_putc(char c, uint8_t color);
void console_write(const char* s, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif
