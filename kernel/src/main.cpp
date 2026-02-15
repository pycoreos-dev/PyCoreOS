#include "doom/doom_bridge.h"
#include "drivers/framebuffer.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/ata.h"
#include "drivers/net_rtl8139.h"
#include "gui/desktop.h"
#include "kernel/cli.h"
#include "kernel/console.h"
#include "kernel/display.h"
#include "kernel/filesystem.h"
#include "kernel/fs_persist.h"
#include "kernel/interrupts.h"
#include "kernel/multiboot.h"
#include "kernel/net_stack.h"
#include "kernel/release.h"
#include "kernel/serial.h"
#include "kernel/timing.h"

extern "C" unsigned char _binary_assets_DOOM1_WAD_start[];
extern "C" unsigned char _binary_assets_DOOM1_WAD_end[];

static void copy_module_name(char* out, unsigned int out_cap, const char* source) {
    if (out == nullptr || out_cap == 0U) {
        return;
    }

    out[0] = '\0';
    if (source == nullptr) {
        const char* fallback = "boot_module.bin";
        unsigned int i = 0;
        while (fallback[i] != '\0' && i + 1U < out_cap) {
            out[i] = fallback[i];
            ++i;
        }
        out[i] = '\0';
        return;
    }

    const char* name = source;
    while (*name == ' ') {
        ++name;
    }
    unsigned int i = 0;
    while (name[i] != '\0' && name[i] != ' ' && i + 1U < out_cap) {
        out[i] = name[i];
        ++i;
    }
    out[i] = '\0';

    if (i == 0U) {
        const char* fallback = "boot_module.bin";
        while (fallback[i] != '\0' && i + 1U < out_cap) {
            out[i] = fallback[i];
            ++i;
        }
        out[i] = '\0';
    }
}

static void import_multiboot_modules(unsigned int multiboot_info_addr) {
    const multiboot_info* mb = (const multiboot_info*)multiboot_info_addr;
    if (mb == nullptr) {
        return;
    }
    if ((mb->flags & MULTIBOOT_INFO_MODS) == 0 || mb->mods_count == 0 || mb->mods_addr == 0) {
        return;
    }

    const multiboot_module* mods = (const multiboot_module*)mb->mods_addr;
    for (unsigned int i = 0; i < mb->mods_count; ++i) {
        const unsigned int mod_start = mods[i].mod_start;
        const unsigned int mod_end = mods[i].mod_end;
        if (mod_end <= mod_start) {
            continue;
        }

        const void* data = (const void*)mod_start;
        const unsigned int size = mod_end - mod_start;
        const char* mod_string = (const char*)mods[i].string;
        char name[64];
        copy_module_name(name, sizeof(name), mod_string);
        (void)fs_import_module(name, data, (size_t)size);
    }
}

static void import_embedded_doom_wad(void) {
    const unsigned char* wad_start = _binary_assets_DOOM1_WAD_start;
    const unsigned char* wad_end = _binary_assets_DOOM1_WAD_end;
    if (wad_end <= wad_start) {
        return;
    }

    const size_t wad_size = (size_t)(wad_end - wad_start);
    (void)fs_import_module("DOOM1.WAD", wad_start, wad_size);
}

static inline unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static unsigned short pit_read_counter0(void) {
    outb(0x43, 0x00);
    const unsigned char lo = inb(0x40);
    const unsigned char hi = inb(0x40);
    return (unsigned short)(((unsigned short)hi << 8U) | (unsigned short)lo);
}

static unsigned int pit_elapsed_counts(unsigned short* inout_last) {
    const unsigned short cur = pit_read_counter0();
    const unsigned short prev = *inout_last;
    *inout_last = cur;
    if (prev >= cur) {
        return (unsigned int)(prev - cur);
    }
    return (unsigned int)prev + (65536U - (unsigned int)cur);
}

static void console_write_hex32(unsigned int v, unsigned char color) {
    const char* hex = "0123456789ABCDEF";
    console_write("0x", color);
    for (int i = 7; i >= 0; --i) {
        unsigned int nib = (v >> (i * 4)) & 0xFU;
        console_putc(hex[nib], color);
    }
}

static void system_restart(void) {
    outb(0x64, 0xFE);
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

static void system_shutdown(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

extern "C" void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_addr) {
    serial_init();
    serial_write("[BOOT] kernel entry\n");
    serial_write("[BOOT] PyCoreOS ");
    serial_write(pycoreos_version());
    serial_write(" (");
    serial_write(pycoreos_channel());
    serial_write(")\n");

    bool fb_ok = framebuffer_init(multiboot_magic, multiboot_info_addr);
    if (!fb_ok) {
        console_init();
        console_write("Framebuffer unavailable, using text mode.\n", 0x1F);
        console_write("mb_magic=", 0x1F);
        console_write_hex32(multiboot_magic, 0x1F);
        console_write("\n", 0x1F);

        const multiboot_info* mb = (const multiboot_info*)multiboot_info_addr;
        if (mb != nullptr) {
            console_write("mb_flags=", 0x1F);
            console_write_hex32(mb->flags, 0x1F);
            console_write("\n", 0x1F);
            console_write("vbe_mode_info=", 0x1F);
            console_write_hex32(mb->vbe_mode_info, 0x1F);
            console_write("\n", 0x1F);
        }
    }

    idt_init();
    keyboard_init();
    display_init();
    mouse_init(display_width(), display_height());
    ata_init();
    rtl8139_init();
    net_stack_init();
    fs_init();
    fs_persist_init();
    (void)fs_load_from_disk();
    import_embedded_doom_wad();
    import_multiboot_modules(multiboot_info_addr);
    doom_bridge_init();
    desktop_init();
    cli_init();
    serial_write("PYCOREOS_BOOT_OK\n");

    timing_init_from_frame_cycles(0U);
    unsigned short pit_last = pit_read_counter0();
    unsigned int pit_accum = 0U;
    unsigned int frame_frac = 0U;
    const unsigned int frame_counts_base = 1193182U / 60U;
    const unsigned int frame_counts_rem = 1193182U % 60U;

    for (;;) {
        unsigned int idle_spins = 0;
        unsigned int frame_target = frame_counts_base;
        frame_frac += frame_counts_rem;
        if (frame_frac >= 60U) {
            frame_frac -= 60U;
            ++frame_target;
        }

        for (;;) {
            pit_accum += pit_elapsed_counts(&pit_last);
            if (pit_accum >= frame_target) {
                pit_accum -= frame_target;
                break;
            }

            char c = 0;
            int key_budget = 12;
            while (key_budget-- > 0 && keyboard_read_char(&c)) {
                desktop_queue_key(c);
            }

            mouse_state ms;
            int mouse_budget = 24;
            while (mouse_budget-- > 0 && mouse_poll(&ms)) {
                desktop_set_mouse(ms.x, ms.y, ms.left, ms.right, ms.middle, ms.wheel_delta);
            }

            net_stack_poll();
            __asm__ volatile("pause");
            ++idle_spins;
        }

        desktop_report_idle_spins(idle_spins);
        desktop_tick_user();
        cli_action action = CLI_ACTION_NONE;
        if (desktop_consume_kernel_action(&action)) {
            if (action == CLI_ACTION_LAUNCH_DOOM) {
                doom_bridge_launch();
            } else if (action == CLI_ACTION_RESTART) {
                system_restart();
            } else if (action == CLI_ACTION_SHUTDOWN) {
                system_shutdown();
            }
        }
    }
}
