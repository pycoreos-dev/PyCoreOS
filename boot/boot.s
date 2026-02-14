.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set VIDEO,    1<<2
.set FLAGS,    ALIGN | MEMINFO | VIDEO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)
.set MODE_TYPE, 0
.set MODE_WIDTH, 1280
.set MODE_HEIGHT, 720
.set MODE_DEPTH, 32

.section .text
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0
.long 0
.long 0
.long 0
.long 0
.long MODE_TYPE
.long MODE_WIDTH
.long MODE_HEIGHT
.long MODE_DEPTH

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.extern kernel_main

_start:
    cli
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
    add $8, %esp

.hang:
    hlt
    jmp .hang

.section .note.GNU-stack,"",@progbits
