# PyCoreOS

PyCoreOS is a 32-bit educational operating system project with a custom kernel, desktop UI, shell, filesystem, networking stack, and integrated DOOM bridge.

## Release

- Version: `0.1.0-beta.1`
- Channel: `public-beta`
- Codename: `first-light`

## What It Includes

- Custom kernel entry, interrupts, timing, and device initialization
- GDT/IDT/TSS setup with kernel/user ring segments and a ring-3 desktop tick path
- Graphical desktop interface with window management and app launcher
- Built-in terminal/CLI with file and system commands
- In-memory filesystem with persistence snapshot support
- Networking stack and RTL8139 driver integration
- DOOM integration through a native bridge

## Project Layout

- `kernel/` kernel runtime, core services, CLI, persistence, networking hooks
- `drivers/` hardware-facing drivers (framebuffer, input, ATA, NIC)
- `gui/` desktop renderer, window manager, app surfaces
- `doom/` DOOM platform layer and bridge
- `third_party/doom/` upstream DOOM source integration
- `boot/` Limine bootloader configuration and linker script
- `third_party/limine/` vendored Limine assets and `limine.c` host utility source
- `Makefile` primary flexible build system (compile, ISO, run, test, release)
- `SConstruct` optional SCons frontend that forwards to the same Make targets
- `assets/` runtime assets (for example `DOOM1.WAD`)

## Build And Run

Makefile-first workflow (no Python required):

```bash
make build
make iso
make run
```

SCons alternative:

```bash
scons build
scons iso
scons run
```

SCons is optional and needs a local `scons` install (which brings Python through SCons itself). The default `make` workflow has no Python dependency.

For full setup, dependency install, and configurable overrides, see:

- `build-instructions.md`

## Notes

- `assets/DOOM1.WAD` is linked into the kernel image at build time and imported into the virtual filesystem as `DOOM1.WAD` during boot.
- The ISO build also copies `assets/DOOM1.WAD` into `iso_root/boot/DOOM1.WAD` (or generates a placeholder if missing) for distribution convenience.
- The ISO build uses Limine (not GRUB) and is self-contained by default via `third_party/limine`.
  You can override this path with `PYCOREOS_LIMINE_DIR=/path/to/limine-assets`.
- Audio output is currently not implemented; `audio/bootchime.voc` remains as an asset only.
- This repo is an educational beta and may include unfinished components.
- Shell command naming follows Linux-style verbs (`clear`, `logout`, etc.); Windows-style aliases are not part of the command surface.

## Compatibility Profile

- Bochs: supported using BIOS boot from `build/pycoreos.iso`.
- VirtualBox: supported with BIOS boot; UEFI boot is available with the vendored Limine EFI binaries.
- VMware: supported with BIOS or UEFI boot from ISO media.
- Lenovo ThinkPad X390: targeted via UEFI x64 boot (`BOOTX64.EFI` from Limine); disable Secure Boot.

Current hardware support notes:

- Display path depends on Multiboot framebuffer/VBE handoff.
- Current default graphics target is `1024x768x32`.
- Input is PS/2 keyboard/mouse oriented.
- Persistence uses ATA PIO; AHCI/NVMe persistence is not implemented yet.
- For VirtualBox input reliability, use PS/2 pointing/keyboard paths (for example `Pointing Device = PS/2 Mouse` and USB controller disabled).
