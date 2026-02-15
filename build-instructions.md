# PyCoreOS Build Instructions

## 0) Limine assets (repo-local by default)

The build is self-contained and uses vendored Limine assets from:

- `third_party/limine`

If you want to use a different Limine directory, set:

```bash
export PYCOREOS_LIMINE_DIR=/absolute/path/to/limine-assets
```

## 1) Install build tools

```bash
make install-deps
```

This installs:

- `make` (primary build system)
- `xorriso`, `qemu-system-x86`
- `gcc-multilib`, `g++-multilib`, `binutils`

The primary `make` workflow has no Python dependency.

## 2) Optional: install SCons frontend

```bash
make install-deps-scons
```

SCons is an optional alternative command surface. It is not required for normal builds.

## 3) Build with Makefile (primary)

```bash
make build
make iso
make run
make test
make beta
```

WAD packaging behavior:

- `assets/DOOM1.WAD` is linked into `build/pycoreos.bin` and imported at runtime as `DOOM1.WAD`.
- `make iso` also places a copy in `iso_root/boot/DOOM1.WAD` (or writes a placeholder if the asset is missing).

Available Make targets:

- `make build` compile and link kernel (`build/pycoreos.bin`)
- `make iso` build bootable ISO (`build/pycoreos.iso`)
- `make run` run ISO in QEMU
- `make test` headless serial boot check (expects `PYCOREOS_BOOT_OK`)
- `make beta` generate release bundle under `build/releases/`
- `make clean` clean build and generated ISO-root artifacts
- `make show-config` print resolved tools and directories
- `make help` print target/override help

## 4) Build with SCons (alternative)

SCons provides the same workflow by forwarding targets to the Makefile:

```bash
scons build
scons iso
scons run
scons test
scons beta
scons clean
```

## 5) Flexibility and tool overrides

Both Make and SCons accept tool/path overrides.

Make examples:

```bash
make iso CC=gcc CXX=g++ LD=ld AS=as
make run QEMU=qemu-system-x86_64
make iso PYCOREOS_LIMINE_DIR=/opt/limine-assets
```

SCons examples:

```bash
scons iso CC=gcc CXX=g++ LD=ld AS=as
scons run QEMU=qemu-system-x86_64
scons iso PYCOREOS_LIMINE_DIR=/opt/limine-assets
```

## 6) Platform compatibility targets

- Bochs: BIOS boot from the generated ISO is the expected path.
- VirtualBox: BIOS and UEFI boot paths are available.
- VMware: BIOS and UEFI boot paths are available.
- Lenovo ThinkPad X390: UEFI x64 boot path is provided by vendored `BOOTX64.EFI`.

Practical limits in the current OS build:

- Framebuffer output depends on Multiboot framebuffer/VBE handoff.
- Current target mode is `1024x768x32`.
- Input drivers are PS/2 focused.
- Filesystem persistence uses ATA PIO only (no AHCI/NVMe persistence yet).

VirtualBox notes for current input stack:

- Prefer BIOS boot for dev/test workflow.
- Set `Pointing Device` to `PS/2 Mouse`.
- Disable USB controller if keyboard/mouse input is not detected.
