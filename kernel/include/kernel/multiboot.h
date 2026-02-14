#ifndef KERNEL_MULTIBOOT_H
#define KERNEL_MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002U

#define MULTIBOOT_INFO_MEMORY        (1U << 0)
#define MULTIBOOT_INFO_BOOTDEV       (1U << 1)
#define MULTIBOOT_INFO_CMDLINE       (1U << 2)
#define MULTIBOOT_INFO_MODS          (1U << 3)
#define MULTIBOOT_INFO_AOUT_SYMS     (1U << 4)
#define MULTIBOOT_INFO_ELF_SHDR      (1U << 5)
#define MULTIBOOT_INFO_MMAP          (1U << 6)
#define MULTIBOOT_INFO_DRIVES        (1U << 7)
#define MULTIBOOT_INFO_CONFIG_TABLE  (1U << 8)
#define MULTIBOOT_INFO_BOOT_LOADER   (1U << 9)
#define MULTIBOOT_INFO_APM_TABLE     (1U << 10)
#define MULTIBOOT_INFO_VBE_INFO      (1U << 11)
#define MULTIBOOT_INFO_FRAMEBUFFER   (1U << 12)

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;

    union {
        struct {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } aout_sym;
        struct {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } elf_sec;
    } u;

    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t color_info;
} __attribute__((packed));

struct multiboot_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} __attribute__((packed));

#endif
