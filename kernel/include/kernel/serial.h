#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);
void serial_write(const char* text);

#ifdef __cplusplus
}
#endif

#endif
