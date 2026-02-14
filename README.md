# PyCoreOS

PyCoreOS is a 32-bit educational operating system project with a custom kernel, desktop UI, shell, filesystem, networking stack, and integrated DOOM bridge.

## Release

- Version: `0.1.7-beta.8`
- Channel: `public-beta`
- Codename: `project-11176`

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
- `boot/` bootloader configuration and linker script
- `scripts/` build, test, and release tooling
- `assets/` runtime assets (for example `DOOM1.WAD`)

## Build And Run

Follow the exact setup and command order in:

- `build-instructions.md`

## Notes

- The ISO build expects `assets/DOOM1.WAD`; if missing, a placeholder WAD is generated.
- Audio output is currently not implemented; `audio/bootchime.voc` remains as an asset only.
- This repo is an educational beta and may include unfinished components.
