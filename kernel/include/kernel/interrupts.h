#ifndef KERNEL_INTERRUPTS_H
#define KERNEL_INTERRUPTS_H

#ifdef __cplusplus
extern "C" {
#endif

void idt_init(void);
void desktop_tick_user(void);

#ifdef __cplusplus
}
#endif

#endif
