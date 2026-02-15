# How PyCoreOS Works

This document explains how the OS works and what each project file does.

## Runtime flow

1. Limine loads the Multiboot1 kernel using `boot/limine.conf` and jumps into `boot/boot.s`.
2. `boot/boot.s` sets CPU state and calls `kernel_main` in `kernel/src/main.cpp`.
3. `kernel_main` initializes core services and devices (interrupts, input, display, storage, network, filesystem, desktop, CLI).
4. The kernel enters the main loop and repeatedly:
   - polls keyboard, mouse, and network,
   - executes queued CLI commands,
   - enters ring 3 to tick the desktop/UI, then returns to ring 0.
5. The desktop (`gui/src/desktop.c`) renders windows, apps, and terminal output from a ring-3 trampoline (`desktop_tick_user`).
6. CLI commands (`kernel/src/cli.c`) operate on the in-memory filesystem and system services.
7. Persistence (`kernel/src/fs_persist.c`) can save/load the RAM filesystem image to ATA sectors.
8. DOOM can be launched via `doom/src/doom_bridge.c`, which runs DOOM and returns to desktop.

## Privilege model

- PyCoreOS now defines kernel and user segments in `kernel/src/interrupts.c` (GDT + TSS setup).
- The desktop frame tick path is entered in ring 3 via `desktop_tick_user()` from `kernel/src/main.cpp`.
- The ring-3 desktop path returns to kernel mode via `int 0x80` (DPL3 gate), handled in `kernel/src/interrupts.c`.
- This is a single-address-space design today: GUI code runs at CPL3, but full process/address-space isolation is not implemented yet.

## File-by-file map

### Root

- `Makefile` primary build system with direct compile/link/ISO/run/test/release targets.
- `SConstruct` optional SCons frontend that exposes the same targets as Make.
- `README.md` project overview and contributor-facing orientation.
- `build-instructions.md` exact step-by-step local build/run commands.
- `howitworks.md` this architecture and file map document.

### Boot

- `boot/boot.s` low-level bootstrap and kernel entry handoff.
- `boot/linker.ld` kernel memory layout and section placement.
- `boot/limine.conf` Limine boot entry and kernel path (Multiboot1 kernel-only handoff).

### Build orchestration

- `Makefile` handles object compilation, kernel link, Limine ISO packaging, QEMU run/test, and beta bundle generation.
- `third_party/limine/` provides vendored Limine boot binaries and `limine.c` for building the host-side install tool.
- `Makefile` is the primary path and does not require Python.
- `SConstruct` offers an optional command surface (`scons build`, `scons iso`, etc.) while reusing the Makefile logic.

### Kernel headers

- `kernel/include/kernel/types.h` basic shared type definitions.
- `kernel/include/kernel/multiboot.h` Multiboot structures/constants from bootloader.
- `kernel/include/kernel/interrupts.h` interrupt setup and ring-3 desktop tick entry interface.
- `kernel/include/kernel/console.h` text console output interface.
- `kernel/include/kernel/display.h` framebuffer presentation abstraction.
- `kernel/include/kernel/serial.h` serial logging interface.
- `kernel/include/kernel/timing.h` timing/sleep/frame pacing API.
- `kernel/include/kernel/filesystem.h` in-memory filesystem and serialization API.
- `kernel/include/kernel/fs_persist.h` RAM filesystem persistence API.
- `kernel/include/kernel/cli.h` command execution interface and CLI actions.
- `kernel/include/kernel/net_stack.h` minimal network stack API.
- `kernel/include/kernel/release.h` version/channel/codename constants and getters.

### Kernel sources

- `kernel/src/main.cpp` system bring-up, main event loop, and ring-3 desktop tick dispatch.
- `kernel/src/main.cpp` also imports embedded `DOOM1.WAD` from linked binary symbols into the virtual filesystem.
- `kernel/src/interrupts.c` GDT/IDT/TSS setup, interrupt plumbing, and ring-3 trampoline/return path.
- `kernel/src/console.c` VGA text-mode console rendering.
- `kernel/src/display.c` display backend selection and framebuffer draw path.
- `kernel/src/serial.c` COM serial initialization and writes.
- `kernel/src/timing.c` timing calibration and sleep/tick helpers.
- `kernel/src/filesystem.c` RAM filesystem, optional boot-module import, and serialization.
- `kernel/src/fs_persist.c` save/load serialized filesystem image via ATA sectors.
- `kernel/src/cli.c` shell command parser and implementations.
- `kernel/src/net_stack.c` small ARP/IPv4/ICMP stack over RTL8139 driver.
- `kernel/src/release.c` runtime accessors for release metadata.

### Driver headers

- `drivers/include/drivers/framebuffer.h` framebuffer init/query/present API.
- `drivers/include/drivers/keyboard.h` keyboard init and key read API.
- `drivers/include/drivers/mouse.h` mouse init and poll API.
- `drivers/include/drivers/ata.h` ATA PIO disk read/write API.
- `drivers/include/drivers/net_rtl8139.h` RTL8139 NIC API (init/send/receive/MAC).

### Driver sources

- `drivers/src/framebuffer.c` framebuffer detection and setup from multiboot info.
- `drivers/src/keyboard.c` PS/2 keyboard handling.
- `drivers/src/mouse.c` PS/2 mouse packet decode and state updates.
- `drivers/src/ata.c` ATA PIO sector read/write implementation.
- `drivers/src/net_rtl8139.c` PCI discovery and RTL8139 RX/TX ring management.

### GUI headers

- `gui/include/gui/desktop.h` desktop lifecycle/input interfaces.
- `gui/include/gui/font5x7.h` bitmap font access.
- `gui/include/gui/image_loader.h` simple image loading/parsing helpers.
- `gui/include/gui/cursor_manager.h` cursor image/state management API.

### GUI sources

- `gui/src/desktop.c` full desktop environment, window manager, app rendering, terminal UI.
- `gui/src/font5x7.c` builtin 5x7 glyph raster data/functions.
- `gui/src/image_loader.c` image decode and surface conversion helpers.
- `gui/src/cursor_manager.c` cursor theme/pixel handling.

### DOOM bridge headers

- `doom/include/doom/doom_bridge.h` launch/init bridge API between kernel UI and DOOM.
- `doom/include/doom/libc_shim.h` freestanding libc declarations used by DOOM platform layer.
- `doom/include/doom/raycast.h` standalone raycast demo API (not wired into normal boot flow).

### DOOM shim compatibility headers

These headers route DOOM includes to the local freestanding shim layer.

- `doom/include/alloca.h`
- `doom/include/ctype.h`
- `doom/include/errno.h`
- `doom/include/fcntl.h`
- `doom/include/malloc.h`
- `doom/include/signal.h`
- `doom/include/stdio.h`
- `doom/include/stdlib.h`
- `doom/include/string.h`
- `doom/include/unistd.h`
- `doom/include/values.h`
- `doom/include/sys/stat.h`
- `doom/include/sys/time.h`
- `doom/include/sys/types.h`

### DOOM bridge sources

- `doom/src/doom_bridge.c` desktop command bridge to enter/exit DOOM.
- `doom/src/i_main_pcos.c` DOOM entry integration for PyCoreOS target.
- `doom/src/i_system_pcos.c` DOOM system/timing/memory/error hooks.
- `doom/src/i_video_pcos.c` DOOM frame presentation and input glue.
- `doom/src/i_net_pcos.c` DOOM networking shim (single-player oriented).
- `doom/src/i_sound_pcos.c` DOOM sound API stubs for current no-sound build.
- `doom/src/libc_shim.c` freestanding libc implementation used by DOOM.
- `doom/src/raycast.c` experimental software raycaster module.

### Assets and docs

- `assets/DOOM1.WAD` DOOM game data linked into the kernel image at build time; also copied into ISO payload during `make iso`.
- `audio/bootchime.voc` optional asset file; currently unused in runtime.
- `docs/AUDIO.md` current audio status document (audio is not implemented in this build).

## Platform compatibility targets

- Bochs: BIOS ISO boot target.
- VirtualBox: BIOS and UEFI ISO boot targets.
- VMware: BIOS and UEFI ISO boot targets.
- Lenovo ThinkPad X390: UEFI x64 ISO boot target.

Current device support limits:

- Graphics path depends on Multiboot framebuffer/VBE handoff.
- Current default mode target is `1024x768x32`.
- Input stack is PS/2 keyboard/mouse focused.
- Persistence path uses ATA PIO only (no AHCI/NVMe persistence yet).
- CLI command surface is Linux-style (for example `clear`, `logout`) without Windows command aliases.

### External upstream code

- `third_party/doom/*` upstream DOOM engine sources compiled by the Makefile/SCons build path.
  PyCoreOS-specific behavior is added via files under `doom/src/` and `doom/include/`.
