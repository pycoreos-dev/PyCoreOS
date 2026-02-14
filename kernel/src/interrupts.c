#include "kernel/interrupts.h"

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} gdt_entry;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} table_ptr;

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} idt_entry;

typedef struct __attribute__((packed)) {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} tss_entry;

enum {
    kKernelCs = 0x08,
    kKernelDs = 0x10,
    kUserCs = 0x1B,
    kUserDs = 0x23,
    kTssSel = 0x28,
    kInt80Vector = 0x80
};

extern void desktop_tick(void);
extern void isr_hang_stub(void);
extern void isr_int80_stub(void);
extern void ring3_desktop_entry(void);
extern void ring3_enter_desktop(void);

static gdt_entry s_gdt[6];
static table_ptr s_gdt_ptr;
static idt_entry s_idt[256];
static table_ptr s_idt_ptr;
static tss_entry s_tss;
static uint8_t s_ring0_stack[8192] __attribute__((aligned(16)));
static uint8_t s_ring3_stack[16384] __attribute__((aligned(16)));

volatile uint32_t g_ring3_stack_top = 0;
volatile uint32_t g_ring3_resume_esp = 0;
volatile uint32_t g_ring3_resume_eip = 0;

static void set_gdt_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    s_gdt[idx].limit_low = (uint16_t)(limit & 0xFFFFU);
    s_gdt[idx].base_low = (uint16_t)(base & 0xFFFFU);
    s_gdt[idx].base_mid = (uint8_t)((base >> 16U) & 0xFFU);
    s_gdt[idx].access = access;
    s_gdt[idx].granularity = (uint8_t)(((limit >> 16U) & 0x0FU) | (gran & 0xF0U));
    s_gdt[idx].base_high = (uint8_t)((base >> 24U) & 0xFFU);
}

static void set_idt_entry(uint8_t vector, uintptr_t handler, uint16_t selector, uint8_t type_attr) {
    s_idt[vector].offset_low = (uint16_t)(handler & 0xFFFFU);
    s_idt[vector].selector = selector;
    s_idt[vector].zero = 0;
    s_idt[vector].type_attr = type_attr;
    s_idt[vector].offset_high = (uint16_t)((handler >> 16U) & 0xFFFFU);
}

static void zero_tss(void) {
    uint8_t* bytes = (uint8_t*)&s_tss;
    for (size_t i = 0; i < sizeof(s_tss); ++i) {
        bytes[i] = 0;
    }
}

static void load_gdt_and_segments(void) {
    __asm__ volatile(
        "cli\n"
        "lgdt %0\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        :
        : "m"(s_gdt_ptr)
        : "ax", "memory");
}

void idt_init(void) {
    set_gdt_entry(0, 0, 0, 0, 0);
    set_gdt_entry(1, 0, 0xFFFFFU, 0x9AU, 0xCFU);
    set_gdt_entry(2, 0, 0xFFFFFU, 0x92U, 0xCFU);
    set_gdt_entry(3, 0, 0xFFFFFU, 0xFAU, 0xCFU);
    set_gdt_entry(4, 0, 0xFFFFFU, 0xF2U, 0xCFU);
    set_gdt_entry(5, (uint32_t)(uintptr_t)&s_tss, (uint32_t)(sizeof(s_tss) - 1U), 0x89U, 0x00U);

    s_gdt_ptr.limit = (uint16_t)(sizeof(s_gdt) - 1U);
    s_gdt_ptr.base = (uint32_t)(uintptr_t)&s_gdt[0];
    load_gdt_and_segments();

    zero_tss();
    s_tss.ss0 = kKernelDs;
    s_tss.esp0 = (uint32_t)(uintptr_t)&s_ring0_stack[sizeof(s_ring0_stack)];
    s_tss.iomap_base = (uint16_t)sizeof(s_tss);
    {
        const uint16_t tss_sel = kTssSel;
        __asm__ volatile("ltr %w0" : : "r"(tss_sel) : "memory");
    }

    for (size_t i = 0; i < 256U; ++i) {
        set_idt_entry((uint8_t)i, (uintptr_t)isr_hang_stub, kKernelCs, 0x8EU);
    }
    set_idt_entry(kInt80Vector, (uintptr_t)isr_int80_stub, kKernelCs, 0xEEU);

    s_idt_ptr.limit = (uint16_t)(sizeof(s_idt) - 1U);
    s_idt_ptr.base = (uint32_t)(uintptr_t)&s_idt[0];
    __asm__ volatile("lidt %0" : : "m"(s_idt_ptr) : "memory");

    g_ring3_stack_top = (uint32_t)(uintptr_t)&s_ring3_stack[sizeof(s_ring3_stack)];
}

void desktop_tick_user(void) {
    ring3_enter_desktop();
}

__asm__(
    ".global isr_hang_stub\n"
    "isr_hang_stub:\n"
    "    cli\n"
    "1:\n"
    "    hlt\n"
    "    jmp 1b\n"
);

__asm__(
    ".global isr_int80_stub\n"
    "isr_int80_stub:\n"
    "    cmpl $1, %eax\n"
    "    jne 1f\n"
    "    movl g_ring3_resume_esp, %esp\n"
    "    jmp *g_ring3_resume_eip\n"
    "1:\n"
    "    iret\n"
);

__asm__(
    ".global ring3_desktop_entry\n"
    "ring3_desktop_entry:\n"
    "    movw $0x23, %ax\n"
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movw %ax, %fs\n"
    "    movw %ax, %gs\n"
    "    call desktop_tick\n"
    "    movl $1, %eax\n"
    "    int $0x80\n"
    "1:\n"
    "    jmp 1b\n"
);

__asm__(
    ".global ring3_enter_desktop\n"
    "ring3_enter_desktop:\n"
    "    push %ebp\n"
    "    mov %esp, %ebp\n"
    "    push %ebx\n"
    "    push %esi\n"
    "    push %edi\n"
    "    movl $1f, g_ring3_resume_eip\n"
    "    movl %esp, g_ring3_resume_esp\n"
    "    movl g_ring3_stack_top, %eax\n"
    "    pushl $0x23\n"
    "    pushl %eax\n"
    "    pushfl\n"
    "    pushl $0x1B\n"
    "    pushl $ring3_desktop_entry\n"
    "    iret\n"
    "1:\n"
    "    pop %edi\n"
    "    pop %esi\n"
    "    pop %ebx\n"
    "    leave\n"
    "    ret\n"
);
