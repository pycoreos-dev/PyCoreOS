#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
ISO_ROOT = ROOT / "iso_root"


def run(cmd: list[str]) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def resolve_env_or_tool(env_name: str, primary: str, fallback: str | None = None) -> str:
    env_value = os.environ.get(env_name, "").strip()
    if env_value:
        return env_value
    return resolve_tool(primary, fallback)


def resolve_tool(primary: str, fallback: str | None = None) -> str:
    if shutil.which(primary):
        return primary
    if fallback and shutil.which(fallback):
        return fallback
    raise FileNotFoundError(f"Required tool not found: {primary}" + (f" or {fallback}" if fallback else ""))


def toolchain() -> dict[str, str]:
    gcc = resolve_env_or_tool("PYCOREOS_GCC", "i686-elf-gcc", "gcc")
    gxx = resolve_env_or_tool("PYCOREOS_GXX", "i686-elf-g++", "g++")
    ld = resolve_env_or_tool("PYCOREOS_LD", "i686-elf-ld", "ld")
    as_ = resolve_env_or_tool("PYCOREOS_AS", "i686-elf-as", "as")
    return {"gcc": gcc, "gxx": gxx, "ld": ld, "as": as_}


def common_flags(cc: str) -> list[str]:
    flags = [
        "-ffreestanding",
        "-fno-pic",
        "-fno-pie",
        "-O3",
        "-DNDEBUG",
        "-fomit-frame-pointer",
        "-Wall",
        "-Wextra",
        "-I",
        "kernel/include",
        "-I",
        "drivers/include",
        "-I",
        "gui/include",
        "-I",
        "doom/include",
    ]
    if os.path.basename(cc) in {"gcc", "g++"}:
        flags.insert(0, "-m32")
    return flags


def doom_flags(cc: str) -> list[str]:
    """Flags for compiling original DOOM source (third_party/doom/)."""
    flags = [
        "-ffreestanding",
        "-fno-pic",
        "-fno-pie",
        "-O2",
        "-fno-strict-aliasing",
        "-DNORMALUNIX",
        "-fomit-frame-pointer",
        "-I", "doom/include",       # Our shim headers take priority
        "-I", "third_party/doom",   # DOOM's own headers
        "-I", "kernel/include",
        "-I", "drivers/include",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-missing-field-initializers",
        "-Wno-sign-compare",
        "-Wno-implicit-function-declaration",
        "-Wno-pointer-to-int-cast",
        "-Wno-int-to-pointer-cast",
        "-Wno-implicit-int",
        "-Wno-format",
        "-Wno-parentheses",
        "-w",  # Suppress all warnings for upstream DOOM code
    ]
    if os.path.basename(cc) in {"gcc", "g++"}:
        flags.insert(0, "-m32")
    return flags


def doom_pal_flags(cc: str) -> list[str]:
    """Flags for compiling our PyCoreOS DOOM platform layer (doom/src/)."""
    flags = [
        "-ffreestanding",
        "-fno-pic",
        "-fno-pie",
        "-O2",
        "-fno-strict-aliasing",
        "-DNORMALUNIX",
        "-fomit-frame-pointer",
        "-Wall",
        "-I", "doom/include",
        "-I", "third_party/doom",
        "-I", "kernel/include",
        "-I", "drivers/include",
        "-I", "gui/include",
        "-Wno-unused-parameter",
    ]
    if os.path.basename(cc) in {"gcc", "g++"}:
        flags.insert(0, "-m32")
    return flags


DOOM_SRC_FILES = [
    "am_map.c", "d_items.c", "d_main.c", "d_net.c", "doomdef.c",
    "doomstat.c", "dstrings.c", "f_finale.c", "f_wipe.c", "g_game.c",
    "hu_lib.c", "hu_stuff.c", "info.c", "m_argv.c", "m_bbox.c",
    "m_cheat.c", "m_fixed.c", "m_menu.c", "m_misc.c", "m_random.c",
    "m_swap.c", "p_ceilng.c", "p_doors.c", "p_enemy.c", "p_floor.c",
    "p_inter.c", "p_lights.c", "p_map.c", "p_maputl.c", "p_mobj.c",
    "p_plats.c", "p_pspr.c", "p_saveg.c", "p_setup.c", "p_sight.c",
    "p_spec.c", "p_switch.c", "p_telept.c", "p_tick.c", "p_user.c",
    "r_bsp.c", "r_data.c", "r_draw.c", "r_main.c", "r_plane.c",
    "r_segs.c", "r_sky.c", "r_things.c", "s_sound.c", "sounds.c",
    "st_lib.c", "st_stuff.c", "tables.c", "v_video.c", "w_wad.c",
    "wi_stuff.c", "z_zone.c",
]

DOOM_PAL_FILES = [
    "libc_shim.c",
    "i_system_pcos.c",
    "i_video_pcos.c",
    "i_sound_pcos.c",
    "i_net_pcos.c",
    "i_main_pcos.c",
    "doom_bridge.c",
]


def build_kernel() -> Path:
    BUILD.mkdir(exist_ok=True)
    tc = toolchain()

    cflags = common_flags(tc["gcc"])
    cxxflags = common_flags(tc["gxx"]) + ["-fno-exceptions", "-fno-rtti"]
    dflags = doom_flags(tc["gcc"])
    dpflags = doom_pal_flags(tc["gcc"])

    objs = {
        "boot": BUILD / "boot.o",
        "console": BUILD / "console.o",
        "filesystem": BUILD / "filesystem.o",
        "display": BUILD / "display.o",
        "serial": BUILD / "serial.o",
        "release": BUILD / "release.o",
        "timing": BUILD / "timing.o",
        "fs_persist": BUILD / "fs_persist.o",
        "cli": BUILD / "cli.o",
        "interrupts": BUILD / "interrupts.o",
        "keyboard": BUILD / "keyboard.o",
        "mouse": BUILD / "mouse.o",
        "ata": BUILD / "ata.o",
        "net": BUILD / "net_rtl8139.o",
        "framebuffer": BUILD / "framebuffer.o",
        "net_stack": BUILD / "net_stack.o",
        "font": BUILD / "font5x7.o",
        "image_loader": BUILD / "image_loader.o",
        "cursor": BUILD / "cursor_manager.o",
        "desktop": BUILD / "desktop.o",
        "main": BUILD / "main.o",
    }

    as_cmd = [tc["as"], "--32", "boot/boot.s", "-o", str(objs["boot"])]
    if os.path.basename(tc["as"]) != "as":
        as_cmd = [tc["gcc"], "-m32", "-c", "boot/boot.s", "-o", str(objs["boot"])]
    run(as_cmd)

    run([tc["gcc"], *cflags, "-c", "kernel/src/console.c", "-o", str(objs["console"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/filesystem.c", "-o", str(objs["filesystem"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/display.c", "-o", str(objs["display"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/serial.c", "-o", str(objs["serial"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/release.c", "-o", str(objs["release"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/timing.c", "-o", str(objs["timing"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/fs_persist.c", "-o", str(objs["fs_persist"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/cli.c", "-o", str(objs["cli"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/interrupts.c", "-o", str(objs["interrupts"])])
    run([tc["gcc"], *cflags, "-c", "drivers/src/keyboard.c", "-o", str(objs["keyboard"])])
    run([tc["gcc"], *cflags, "-c", "drivers/src/mouse.c", "-o", str(objs["mouse"])])
    run([tc["gcc"], *cflags, "-c", "drivers/src/ata.c", "-o", str(objs["ata"])])
    run([tc["gcc"], *cflags, "-c", "drivers/src/net_rtl8139.c", "-o", str(objs["net"])])
    run([tc["gcc"], *cflags, "-c", "drivers/src/framebuffer.c", "-o", str(objs["framebuffer"])])
    run([tc["gcc"], *cflags, "-c", "kernel/src/net_stack.c", "-o", str(objs["net_stack"])])
    run([tc["gcc"], *cflags, "-c", "gui/src/font5x7.c", "-o", str(objs["font"])])
    run([tc["gcc"], *cflags, "-c", "gui/src/image_loader.c", "-o", str(objs["image_loader"])])
    run([tc["gcc"], *cflags, "-c", "gui/src/cursor_manager.c", "-o", str(objs["cursor"])])
    run([tc["gcc"], *cflags, "-c", "gui/src/desktop.c", "-o", str(objs["desktop"])])
    run([tc["gxx"], *cxxflags, "-std=c++17", "-c", "kernel/src/main.cpp", "-o", str(objs["main"])])

    for src in DOOM_SRC_FILES:
        obj_name = f"doom_{src[:-2]}.o"
        obj_path = BUILD / obj_name
        objs[f"doom_{src[:-2]}"] = obj_path
        run([tc["gcc"], *dflags, "-c", f"third_party/doom/{src}", "-o", str(obj_path)])

    for src in DOOM_PAL_FILES:
        obj_name = f"doompal_{src[:-2]}.o"
        obj_path = BUILD / obj_name
        objs[f"doompal_{src[:-2]}"] = obj_path
        run([tc["gcc"], *dpflags, "-c", f"doom/src/{src}", "-o", str(obj_path)])

    kernel_bin = BUILD / "pycoreos.bin"
    ldflags = ["-T", "boot/linker.ld", "-nostdlib", "-m", "elf_i386"]
    run([tc["ld"], *ldflags, "-o", str(kernel_bin), *(str(x) for x in objs.values())])
    return kernel_bin



def make_iso(kernel_bin: Path) -> Path:
    grub_mkrescue = resolve_env_or_tool("PYCOREOS_GRUB_MKRESCUE", "grub-mkrescue", "grub2-mkrescue")

    (ISO_ROOT / "boot" / "grub").mkdir(parents=True, exist_ok=True)
    shutil.copy2(kernel_bin, ISO_ROOT / "boot" / "pycoreos.bin")

    grub_cfg = ROOT / "boot" / "grub" / "grub.cfg"
    grub_dst = ISO_ROOT / "boot" / "grub" / "grub.cfg"
    grub_dst.write_text(grub_cfg.read_text())

    wad_src = ROOT / "assets" / "DOOM1.WAD"
    wad_dst = ISO_ROOT / "boot" / "DOOM1.WAD"
    if wad_src.exists():
        shutil.copy2(wad_src, wad_dst)
    else:
        wad_dst.write_bytes(b"PWAD\\x00\\x00\\x00\\x00PyCoreOS placeholder WAD. Replace assets/DOOM1.WAD for real content.\\n")

    iso = BUILD / "pycoreos.iso"
    run([grub_mkrescue, "-o", str(iso), str(ISO_ROOT)])
    return iso


def run_qemu(iso: Path) -> None:
    qemu = resolve_env_or_tool("PYCOREOS_QEMU", "qemu-system-i386", "qemu-system-x86_64")
    run([qemu, "-cdrom", str(iso), "-m", "1024M", "-vga", "std"])


def run_tests() -> None:
    kernel = build_kernel()
    if not kernel.exists() or kernel.stat().st_size == 0:
        raise RuntimeError("Kernel build failed: output missing or empty")

    iso = make_iso(kernel)
    qemu = resolve_env_or_tool("PYCOREOS_QEMU", "qemu-system-i386", "qemu-system-x86_64")
    cmd = [
        qemu,
        "-cdrom",
        str(iso),
        "-m",
        "256M",
        "-display",
        "none",
        "-monitor",
        "none",
        "-serial",
        "stdio",
        "-no-reboot",
        "-no-shutdown",
    ]

    serial_output = ""
    try:
        completed = subprocess.run(cmd, check=False, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=20)
        serial_output = completed.stdout or ""
    except subprocess.TimeoutExpired as exc:
        if isinstance(exc.stdout, bytes):
            serial_output = exc.stdout.decode("utf-8", errors="replace")
        else:
            serial_output = exc.stdout or ""

    marker = "PYCOREOS_BOOT_OK"
    if marker not in serial_output:
        tail = serial_output[-400:]
        raise RuntimeError("Headless QEMU test did not emit boot marker. Serial tail:\n" + tail)

    print("Kernel headless boot test passed.")


def clean() -> None:
    shutil.rmtree(BUILD, ignore_errors=True)
    if (ISO_ROOT / "boot" / "pycoreos.bin").exists():
        (ISO_ROOT / "boot" / "pycoreos.bin").unlink()


def main() -> int:
    parser = argparse.ArgumentParser(description="Build PyCoreOS")
    parser.add_argument("target", choices=["build", "iso", "run", "test", "clean"])
    args = parser.parse_args()

    if args.target == "clean":
        clean()
        return 0

    if args.target == "test":
        run_tests()
        return 0

    kernel = build_kernel()
    if args.target == "build":
        print(f"Built kernel: {kernel}")
        return 0

    iso = make_iso(kernel)
    print(f"Built ISO: {iso}")

    if args.target == "run":
        run_qemu(iso)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (subprocess.CalledProcessError, FileNotFoundError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
