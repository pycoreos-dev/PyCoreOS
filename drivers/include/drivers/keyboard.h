#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void keyboard_init(void);
bool keyboard_read_char(char* out);

#ifdef __cplusplus
}
#endif

#endif
