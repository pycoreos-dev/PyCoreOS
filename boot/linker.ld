ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 2M;

    .text ALIGN(4K) : {
        KEEP(*(.multiboot))
        *(.text*)
    } :text

    .rodata ALIGN(4K) : {
        *(.rodata*)
    } :text

    .data ALIGN(4K) : {
        *(.data*)
    } :data

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss*)
    } :data
}
