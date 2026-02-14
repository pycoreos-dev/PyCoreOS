ENTRY(_start)

SECTIONS {
    . = 1M;

    .text ALIGN(4K) : {
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }
}
