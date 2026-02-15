SHELL := /bin/bash

.PHONY: all help show-config check-tools check-limine build kernel iso run test beta release clean distclean install-deps install-deps-scons

ROOT := $(abspath .)
BUILD_DIR ?= build
OBJ_DIR ?= $(BUILD_DIR)/obj
ISO_ROOT ?= iso_root
RELEASE_DIR ?= $(BUILD_DIR)/releases
TEST_TIMEOUT_SEC ?= 20

find-tool = $(if $(shell command -v $(1) 2>/dev/null),$(1),$(2))

ifeq ($(origin CC),default)
CC := $(call find-tool,i686-elf-gcc,gcc)
endif
ifeq ($(origin CXX),default)
CXX := $(call find-tool,i686-elf-g++,g++)
endif
ifeq ($(origin LD),default)
LD := $(call find-tool,i686-elf-ld,ld)
endif
ifeq ($(origin AS),default)
AS := $(call find-tool,i686-elf-as,as)
endif
ifeq ($(origin HOST_CC),undefined)
HOST_CC := $(call find-tool,cc,gcc)
endif
ifeq ($(origin XORRISO),undefined)
XORRISO := $(call find-tool,xorriso,xorriso)
endif
ifeq ($(origin QEMU),undefined)
QEMU := $(call find-tool,qemu-system-i386,qemu-system-x86_64)
endif

ifneq ($(strip $(PYCOREOS_LIMINE_DIR)),)
LIMINE_DIR ?= $(PYCOREOS_LIMINE_DIR)
else
LIMINE_DIR ?= third_party/limine
endif

CC_BASENAME := $(notdir $(CC))
CXX_BASENAME := $(notdir $(CXX))
LD_BASENAME := $(notdir $(LD))
AS_BASENAME := $(notdir $(AS))

CC_ARCH_FLAGS :=
ifneq ($(filter gcc clang,$(CC_BASENAME)),)
CC_ARCH_FLAGS += -m32
endif

CXX_ARCH_FLAGS :=
ifneq ($(filter g++ clang++,$(CXX_BASENAME)),)
CXX_ARCH_FLAGS += -m32
endif

LD_ARCH_FLAGS :=
ifneq ($(filter ld,$(LD_BASENAME)),)
LD_ARCH_FLAGS += -m elf_i386
endif
KERNEL_LD_FLAGS := $(LD_ARCH_FLAGS) -z noexecstack

COMMON_INCLUDES := -Ikernel/include -Idrivers/include -Igui/include -Idoom/include
COMMON_CFLAGS := $(CC_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -O3 -DNDEBUG -fomit-frame-pointer -Wall -Wextra $(COMMON_INCLUDES) $(EXTRA_CFLAGS)
COMMON_CXXFLAGS := $(CXX_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -O3 -DNDEBUG -fomit-frame-pointer -Wall -Wextra -fno-exceptions -fno-rtti $(COMMON_INCLUDES) $(EXTRA_CXXFLAGS)

DOOM_CFLAGS := $(CC_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -O2 -fno-strict-aliasing -DNORMALUNIX -fomit-frame-pointer -Idoom/include -Ithird_party/doom -Ikernel/include -Idrivers/include -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-field-initializers -Wno-sign-compare -Wno-implicit-function-declaration -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-implicit-int -Wno-format -Wno-parentheses -w $(EXTRA_DOOM_CFLAGS)
DOOM_PLATFORM_CFLAGS := $(CC_ARCH_FLAGS) -ffreestanding -fno-pic -fno-pie -O2 -fno-strict-aliasing -DNORMALUNIX -fomit-frame-pointer -Wall -Idoom/include -Ithird_party/doom -Ikernel/include -Idrivers/include -Igui/include -Wno-unused-parameter $(EXTRA_DOOMPAL_CFLAGS)

BOOT_SRC := boot/boot.s
WAD_ASSET := assets/DOOM1.WAD
KERNEL_C_SRCS := $(sort $(wildcard kernel/src/*.c))
DRIVER_C_SRCS := $(sort $(wildcard drivers/src/*.c))
GUI_C_SRCS := $(sort $(wildcard gui/src/*.c))
KERNEL_CPP_SRCS := kernel/src/main.cpp
DOOM_CORE_SRCS := $(sort $(wildcard third_party/doom/*.c))
DOOM_PLATFORM_SRCS := \
	doom/src/libc_shim.c \
	doom/src/i_system_pcos.c \
	doom/src/i_video_pcos.c \
	doom/src/i_sound_pcos.c \
	doom/src/i_net_pcos.c \
	doom/src/i_main_pcos.c \
	doom/src/doom_bridge.c

BOOT_OBJ := $(OBJ_DIR)/boot/boot.o
KERNEL_C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(KERNEL_C_SRCS))
DRIVER_C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(DRIVER_C_SRCS))
GUI_C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(GUI_C_SRCS))
KERNEL_CPP_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(KERNEL_CPP_SRCS))
DOOM_CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(DOOM_CORE_SRCS))
DOOM_PLATFORM_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(DOOM_PLATFORM_SRCS))
WAD_OBJ := $(OBJ_DIR)/assets/DOOM1.WAD.o
ALL_OBJS := $(BOOT_OBJ) $(KERNEL_C_OBJS) $(DRIVER_C_OBJS) $(GUI_C_OBJS) $(KERNEL_CPP_OBJS) $(DOOM_CORE_OBJS) $(DOOM_PLATFORM_OBJS) $(WAD_OBJ)

KERNEL_BIN := $(BUILD_DIR)/pycoreos.bin
ISO_IMAGE := $(BUILD_DIR)/pycoreos.iso
LIMINE_TOOL := $(BUILD_DIR)/tools/limine
LIMINE_REQUIRED := limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin

ifeq ($(AS_BASENAME),as)
BOOT_AS_CMD = $(CC) $(CC_ARCH_FLAGS) -c $< -o $@
else
BOOT_AS_CMD = $(AS) --32 $< -o $@
endif

all: iso

help:
	@echo "PyCoreOS build system (Makefile-first)"
	@echo ""
	@echo "Targets:"
	@echo "  make build         Compile and link the kernel binary"
	@echo "  make iso           Build bootable ISO (default target via 'make all')"
	@echo "  make run           Launch ISO in QEMU"
	@echo "  make test          Headless boot check (expects PYCOREOS_BOOT_OK on serial)"
	@echo "  make beta          Build release bundle under build/releases/"
	@echo "  make clean         Remove build outputs and generated ISO root files"
	@echo "  make install-deps  Install Ubuntu/Debian dependencies for Make-only workflow"
	@echo "  make install-deps-scons  Install optional SCons frontend"
	@echo "  make show-config   Print resolved toolchain and path configuration"
	@echo ""
	@echo "Common overrides:"
	@echo "  CC=... CXX=... LD=... AS=... HOST_CC=..."
	@echo "  XORRISO=... QEMU=... PYCOREOS_LIMINE_DIR=/path/to/limine-assets"
	@echo "  BUILD_DIR=... ISO_ROOT=... TEST_TIMEOUT_SEC=..."
	@echo ""
	@echo "Default build path has no Python dependency."
	@echo "Optional SCons frontend is available: scons build | scons iso | scons run | scons test"

show-config:
	@echo "CC=$(CC)"
	@echo "CXX=$(CXX)"
	@echo "LD=$(LD)"
	@echo "AS=$(AS)"
	@echo "HOST_CC=$(HOST_CC)"
	@echo "XORRISO=$(XORRISO)"
	@echo "QEMU=$(QEMU)"
	@echo "LIMINE_DIR=$(LIMINE_DIR)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "ISO_ROOT=$(ISO_ROOT)"
	@echo "RELEASE_DIR=$(RELEASE_DIR)"
	@echo "TEST_TIMEOUT_SEC=$(TEST_TIMEOUT_SEC)"

check-tools:
	@echo "Checking tools..."

check-limine:
	@for f in $(LIMINE_REQUIRED); do \
		if [ ! -f "$(LIMINE_DIR)/$$f" ]; then \
			echo "error: missing Limine asset: $(LIMINE_DIR)/$$f"; \
			echo "Default path is third_party/limine. Set PYCOREOS_LIMINE_DIR=/absolute/path/to/limine-assets to override."; \
			exit 1; \
		fi; \
	done

build: $(KERNEL_BIN)
kernel: build

$(KERNEL_BIN): check-tools $(ALL_OBJS) boot/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(KERNEL_LD_FLAGS) -T boot/linker.ld -nostdlib -o $@ $(ALL_OBJS)

$(BOOT_OBJ): $(BOOT_SRC)
	@mkdir -p $(dir $@)
	$(BOOT_AS_CMD)

$(OBJ_DIR)/kernel/src/main.o: kernel/src/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON_CXXFLAGS) -std=c++17 -c $< -o $@

$(OBJ_DIR)/kernel/src/%.o: kernel/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(OBJ_DIR)/drivers/src/%.o: drivers/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(OBJ_DIR)/gui/src/%.o: gui/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(OBJ_DIR)/third_party/doom/%.o: third_party/doom/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DOOM_CFLAGS) -c $< -o $@

$(OBJ_DIR)/doom/src/%.o: doom/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(DOOM_PLATFORM_CFLAGS) -c $< -o $@

$(WAD_OBJ): $(WAD_ASSET)
	@mkdir -p $(dir $@)
	$(LD) $(LD_ARCH_FLAGS) -r -b binary $< -o $@

$(LIMINE_TOOL): check-limine
	@mkdir -p $(dir $@)
	@if [ -x "$(LIMINE_DIR)/limine" ]; then \
		cp "$(LIMINE_DIR)/limine" "$@"; \
	elif [ -f "$(LIMINE_DIR)/limine.c" ]; then \
		$(HOST_CC) -std=c99 -O2 -pipe "$(LIMINE_DIR)/limine.c" -o "$@"; \
	else \
		echo "error: missing Limine tool and source in $(LIMINE_DIR)"; \
		echo "need either $(LIMINE_DIR)/limine or $(LIMINE_DIR)/limine.c"; \
		exit 1; \
	fi
	@chmod +x "$@"

iso: $(ISO_IMAGE)

$(ISO_IMAGE): $(KERNEL_BIN) boot/limine.conf check-limine $(LIMINE_TOOL)
	@if ! command -v "$(XORRISO)" >/dev/null 2>&1; then \
		echo "error: required tool not found: $(XORRISO)"; \
		exit 1; \
	fi
	@mkdir -p "$(BUILD_DIR)" "$(ISO_ROOT)/boot/limine" "$(ISO_ROOT)/EFI/BOOT"
	cp "$(KERNEL_BIN)" "$(ISO_ROOT)/boot/pycoreos.bin"
	cp "boot/limine.conf" "$(ISO_ROOT)/limine.conf"
	cp "$(LIMINE_DIR)/limine-bios.sys" "$(ISO_ROOT)/boot/limine/limine-bios.sys"
	cp "$(LIMINE_DIR)/limine-bios-cd.bin" "$(ISO_ROOT)/boot/limine/limine-bios-cd.bin"
	cp "$(LIMINE_DIR)/limine-uefi-cd.bin" "$(ISO_ROOT)/boot/limine/limine-uefi-cd.bin"
	@if [ -f "$(LIMINE_DIR)/BOOTX64.EFI" ]; then cp "$(LIMINE_DIR)/BOOTX64.EFI" "$(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI"; fi
	@if [ -f "$(LIMINE_DIR)/BOOTIA32.EFI" ]; then cp "$(LIMINE_DIR)/BOOTIA32.EFI" "$(ISO_ROOT)/EFI/BOOT/BOOTIA32.EFI"; fi
	@if [ -f "assets/DOOM1.WAD" ]; then \
		cp "assets/DOOM1.WAD" "$(ISO_ROOT)/boot/DOOM1.WAD"; \
	else \
		printf 'PWAD\0\0\0\0PyCoreOS placeholder WAD. Replace assets/DOOM1.WAD for real content.\n' > "$(ISO_ROOT)/boot/DOOM1.WAD"; \
	fi
	$(XORRISO) -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		-o "$(ISO_IMAGE)" "$(ISO_ROOT)"
	"$(LIMINE_TOOL)" bios-install "$(ISO_IMAGE)"
	@echo "Built ISO: $(ISO_IMAGE)"

run: $(ISO_IMAGE)
	@if ! command -v "$(QEMU)" >/dev/null 2>&1; then \
		echo "error: required tool not found: $(QEMU)"; \
		exit 1; \
	fi
	"$(QEMU)" -cdrom "$(ISO_IMAGE)" -m 1024M -vga std

test: $(ISO_IMAGE)
	@if ! command -v "$(QEMU)" >/dev/null 2>&1; then \
		echo "error: required tool not found: $(QEMU)"; \
		exit 1; \
	fi
	@mkdir -p "$(BUILD_DIR)"
	@log_file="$(BUILD_DIR)/test-serial.log"; \
	rm -f "$$log_file"; \
	if command -v timeout >/dev/null 2>&1; then \
		timeout "$(TEST_TIMEOUT_SEC)s" "$(QEMU)" -cdrom "$(ISO_IMAGE)" -m 256M -display none -monitor none -serial stdio -no-reboot -no-shutdown > "$$log_file" 2>&1 || true; \
	else \
		"$(QEMU)" -cdrom "$(ISO_IMAGE)" -m 256M -display none -monitor none -serial stdio -no-reboot -no-shutdown > "$$log_file" 2>&1 || true; \
	fi; \
	if grep -q "PYCOREOS_BOOT_OK" "$$log_file"; then \
		echo "Kernel headless boot test passed."; \
	else \
		echo "error: headless QEMU test did not emit boot marker."; \
		echo "Serial tail:"; \
		tail -c 400 "$$log_file" || true; \
		exit 1; \
	fi

beta: test
	@set -euo pipefail; \
	version=$$(sed -n 's/^[[:space:]]*#define[[:space:]]*PYCOREOS_VERSION[[:space:]]*"\([^"]*\)".*/\1/p' kernel/include/kernel/release.h | head -n 1); \
	channel=$$(sed -n 's/^[[:space:]]*#define[[:space:]]*PYCOREOS_CHANNEL[[:space:]]*"\([^"]*\)".*/\1/p' kernel/include/kernel/release.h | head -n 1); \
	codename=$$(sed -n 's/^[[:space:]]*#define[[:space:]]*PYCOREOS_CODENAME[[:space:]]*"\([^"]*\)".*/\1/p' kernel/include/kernel/release.h | head -n 1); \
	if [ -z "$$version" ] || [ -z "$$channel" ] || [ -z "$$codename" ]; then \
		echo "error: failed to parse release defines from kernel/include/kernel/release.h"; \
		exit 1; \
	fi; \
	tag="pycoreos-$$version"; \
	bundle_dir="$(RELEASE_DIR)/$$tag"; \
	archive="$(RELEASE_DIR)/$$tag-bundle.tar.gz"; \
	mkdir -p "$(RELEASE_DIR)"; \
	rm -rf "$$bundle_dir"; \
	mkdir -p "$$bundle_dir"; \
	cp "$(ISO_IMAGE)" "$$bundle_dir/$$tag.iso"; \
	for doc in README.md CHANGELOG.md BETA_TESTING.md RELEASE_CHECKLIST.md; do \
		if [ -f "$$doc" ]; then cp "$$doc" "$$bundle_dir/"; fi; \
	done; \
	built_utc=$$(date -u +"%Y-%m-%dT%H:%M:%SZ"); \
	release_json="$$bundle_dir/release.json"; \
	{ \
		echo "{"; \
		echo "  \"name\": \"PyCoreOS\","; \
		echo "  \"version\": \"$$version\","; \
		echo "  \"channel\": \"$$channel\","; \
		echo "  \"codename\": \"$$codename\","; \
		echo "  \"built_utc\": \"$$built_utc\","; \
		echo "  \"artifacts\": ["; \
		first=1; \
		while IFS= read -r artifact; do \
			[ "$$artifact" = "release.json" ] && continue; \
			if [ $$first -eq 0 ]; then echo ","; fi; \
			printf "    \"%s\"" "$$artifact"; \
			first=0; \
		done < <(cd "$$bundle_dir" && ls -1 | LC_ALL=C sort); \
		echo ""; \
		echo "  ]"; \
		echo "}"; \
	} > "$$release_json"; \
	( cd "$$bundle_dir" && sha256sum * > SHA256SUMS ); \
	rm -f "$$archive"; \
	tar -C "$(RELEASE_DIR)" -czf "$$archive" "$$tag"; \
	echo "Beta release bundle ready: $$archive"

release: beta

clean:
	rm -rf "$(BUILD_DIR)"
	rm -f "$(ISO_ROOT)/boot/pycoreos.bin" "$(ISO_ROOT)/boot/DOOM1.WAD" "$(ISO_ROOT)/limine.conf"
	rm -rf "$(ISO_ROOT)/boot/limine" "$(ISO_ROOT)/EFI"

distclean: clean

install-deps:
	sudo apt-get update
	sudo apt-get install -y make xorriso qemu-system-x86 gcc-multilib g++-multilib binutils

install-deps-scons:
	sudo apt-get update
	sudo apt-get install -y scons
