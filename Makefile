# SPDX-License-Identifier: MIT

QEMU_DIR := external/sf2000_qemu
QEMU_ORACLE_DIR := $(abspath ../sf2000_qemu)
HCLINUX_DIR := external/hclinux/2024.02.y.2
BUILD_DIR := build
INITRAMFS := $(BUILD_DIR)/initramfs.cpio
INITRAMFS_LIST := $(BUILD_DIR)/initramfs.list
INIT_BIN := $(BUILD_DIR)/initramfs-init
INIT_RAW := $(BUILD_DIR)/initramfs-init.raw
GEN_INIT_CPIO := $(BUILD_DIR)/gen_init_cpio
ASDPACK := $(BUILD_DIR)/asdpack
BFLTPACK := $(BUILD_DIR)/bfltpack
QEMU_BIN := $(QEMU_DIR)/.cache/qemu-10.2.2/build/qemu-system-mipsel
QEMU_MKSD := $(QEMU_DIR)/build/mksf2000sd
QEMU_CPU ?= 4Km
QEMU_CPU_ARGS := $(if $(QEMU_CPU),-cpu $(QEMU_CPU),)
QEMU_ROM_CPU ?=
QEMU_ROM_CPU_ARGS := $(if $(QEMU_ROM_CPU),-cpu $(QEMU_ROM_CPU),)
QEMU_DEBUG ?= guest_errors,unimp
QEMU_SD_ARGS ?=
HOSTCC ?= cc
TOOLCHAIN_DIR ?= /opt/gdb-mips-toolchain
CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/bin/mipsel-mti-elf-
CC_MIPS := $(CROSS_COMPILE)gcc
LD_MIPS := $(CROSS_COMPILE)ld
OBJCOPY_MIPS := $(CROSS_COMPILE)objcopy
STRIP_MIPS := $(CROSS_COMPILE)strip
JOBS ?= $(shell nproc 2>/dev/null || echo 1)
ROOTFS ?= tiny
LINUX_VERSION := 5.12.4
LINUX_TARBALL := linux-$(LINUX_VERSION).tar.xz
LINUX_URL := https://cdn.kernel.org/pub/linux/kernel/v5.x/$(LINUX_TARBALL)
LINUX_SRC ?= /tmp/sf2000_linux-kernel-$(LINUX_VERSION)
BUILDROOT_VERSION := 2024.02.12
BUILDROOT_TARBALL := buildroot-$(BUILDROOT_VERSION).tar.xz
BUILDROOT_URL := https://buildroot.org/downloads/$(BUILDROOT_TARBALL)
BUILDROOT_SRC := $(BUILD_DIR)/buildroot-$(BUILDROOT_VERSION)
BUILDROOT_WORK ?= /tmp/sf2000_linux-buildroot
BUILDROOT_OUT ?= $(BUILDROOT_WORK)/buildroot-sf2000
BUILDROOT_DEFCONFIG := buildroot/sf2000_defconfig
BUILDROOT_BUSYBOX_CONFIG := buildroot/sf2000_busybox.config
BUILDROOT_OVERLAY := buildroot/sf2000-rootfs-overlay
BUILDROOT_GENERATED_OVERLAY := $(BUILD_DIR)/buildroot-generated-overlay
BUILDROOT_GENERATED_OVERLAY_STAMP := $(BUILD_DIR)/.stamp-buildroot-generated-overlay
BUILDROOT_INIT_SRC := buildroot/sf2000-init.c
BUILDROOT_INIT_ENTRY := buildroot/sf2000-init-entry.S
BUILDROOT_INIT_CLONE := buildroot/sf2000-init-clone.S
BUILDROOT_INIT := $(BUILDROOT_GENERATED_OVERLAY)/init
BUILDROOT_SUPERVISOR := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-init
BUILDROOT_PAD_SRC := buildroot/sf2000-pad.c
BUILDROOT_PAD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-pad
BUILDROOT_HEARTBEAT_SRC := buildroot/sf2000-heartbeat.c
BUILDROOT_HEARTBEAT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-heartbeat
BUILDROOT_SCREEN_SRC := buildroot/sf2000-screen.c
BUILDROOT_SCREEN_ENTRY := buildroot/sf2000-screen-entry.S
BUILDROOT_SCREEN_CFLAGS :=
BUILDROOT_SCREEN := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-screen
BUILDROOT_PANEL_PROBE_LINK := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-probe
BUILDROOT_PANEL_PROBE_TARGET ?= sf2000-screen
BUILDROOT_PANEL_INIT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-init
BUILDROOT_PANEL_INIT_ENTRY := buildroot/sf2000-panel-init-entry.S
BUILDROOT_PANEL_INIT_SRC := buildroot/sf2000-panel-launcher.S
BUILDROOT_PANEL_INIT_CFLAGS := $(BUILDROOT_HELPER_CFLAGS)
BUILDROOT_PANEL_FASTPROBE_SRC := buildroot/sf2000-panel-fastprobe.c
BUILDROOT_PANEL_FASTPROBE_ENTRY := buildroot/sf2000-panel-fastprobe-entry.S
BUILDROOT_PANEL_FASTPROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-fastprobe
BUILDROOT_PANEL_FASTPROBE_LDFLAGS = $(BUILDROOT_SCREEN_LDFLAGS)
BUILDROOT_STORAGE_PROBE_SRC := buildroot/sf2000-storage-probe.c
BUILDROOT_STORAGE_PROBE_ENTRY := buildroot/sf2000-storage-probe-entry.S
BUILDROOT_STORAGE_PROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-storage-probe
BUILDROOT_STORAGE_PROBE_CFLAGS := -Os -Wall -Wextra -ffreestanding -fno-builtin \
	-mno-abicalls -fno-pic -mno-gpopt -ffunction-sections -fdata-sections
BUILDROOT_STORAGE_PROBE_LDFLAGS = $(BUILDROOT_SCREEN_LDFLAGS) -Wl,--gc-sections
BUILDROOT_STORAGE_FASTPROBE_SRC := buildroot/sf2000-storage-fastprobe.c
BUILDROOT_STORAGE_FASTPROBE_ENTRY := buildroot/sf2000-storage-fastprobe-entry.S
BUILDROOT_STORAGE_FASTPROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-storage-fastprobe
BUILDROOT_STORAGE_FASTPROBE_CFLAGS := $(BUILDROOT_HELPER_CFLAGS) -ffunction-sections -fdata-sections
BUILDROOT_STORAGE_FASTPROBE_LDFLAGS = $(BUILDROOT_SCREEN_LDFLAGS) -Wl,--gc-sections
BUILDROOT_RESET_FASTPROBE_SRC := buildroot/sf2000-reset-fastprobe.c
BUILDROOT_RESET_FASTPROBE_ENTRY := buildroot/sf2000-reset-fastprobe-entry.S
	BUILDROOT_RESET_FASTPROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-reset-fastprobe
BUILDROOT_RESET_FASTPROBE_LDFLAGS = $(BUILDROOT_SCREEN_LDFLAGS)
BUILDROOT_RESET_RESTORE_SCRIPT := scripts/qmp_restore_smoke.py
BUILDROOT_RESET_RESTORE_STATE := $(BUILD_DIR)/state/sf2000-reset-fastprobe.migration
BUILDROOT_RESET_RESTORE_SOCKET := $(BUILD_DIR)/qmp/sf2000-reset-fastprobe.qmp
BUILDROOT_RESET_RESTORE_SOCKET_DEST := $(BUILD_DIR)/qmp/sf2000-reset-fastprobe-restore.qmp
BUILDROOT_RESET_RESTORE_PREFIX := $(BUILD_DIR)/logs/linux-buildroot-reset-restore
BUILDROOT_DEVICE_TABLE := buildroot/sf2000_device_table.txt
BUILDROOT_PATCHES := buildroot/patches
BUILDROOT_OVERLAY_FILES := $(shell find '$(BUILDROOT_OVERLAY)' -type f 2>/dev/null)
BUILDROOT_PATCH_FILES := $(shell find '$(BUILDROOT_PATCHES)' -type f 2>/dev/null)
BUILDROOT_CORE_PATCHES := $(wildcard buildroot/patches/buildroot/*.patch)
BUILDROOT_CPIO := $(BUILD_DIR)/rootfs-buildroot.cpio
BUILDROOT_TOOLCHAIN_STAMP := $(BUILDROOT_OUT)/.stamp-toolchain
BUILDROOT_TARGET_STAMP := $(BUILDROOT_OUT)/.stamp-target
BUILDROOT_REPACK_DIR := $(BUILD_DIR)/buildroot-repack-root
BUILDROOT_DEVICE_CPIO_LIST := $(BUILD_DIR)/buildroot-device-nodes.list
BUILDROOT_CC := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-uclinux-uclibc-gcc
BUILDROOT_FLTHDR := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-uclinux-uclibc-flthdr
BUILDROOT_STRIP := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-uclinux-uclibc-strip
BUILDROOT_HELPER_CFLAGS := -Os -Wall -Wextra -ffreestanding -fno-builtin \
	-mno-abicalls -fno-pic -mno-gpopt
BUILDROOT_FLAT_LDFLAGS := -Wl,-elf2flt=-r -static
BUILDROOT_HELPER_STACK_SIZE := 65536
BUILDROOT_SCREEN_STACK_SIZE := $(BUILDROOT_HELPER_STACK_SIZE)
BUILDROOT_SUPERVISOR_STACK_SIZE := $(BUILDROOT_HELPER_STACK_SIZE)
BUILDROOT_SUPERVISOR_CFLAGS := $(BUILDROOT_HELPER_CFLAGS)
BUILDROOT_SUPERVISOR_LDFLAGS := -nostdlib -static -Wl,-elf2flt=-r \
	-Wl,--section-start=.text=0 -Wl,-e,_start
BUILDROOT_INIT_SOURCE ?= $(INIT_BIN)
INIT_CFLAGS ?=
BUILDROOT_SCREEN_LDFLAGS := -nostartfiles -static -Wl,-elf2flt=-r \
	-Wl,--section-start=.text=0 -Wl,-e,_start
BUILDROOT_HOST_CFLAGS := -O2 -std=gnu17 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
BUILDROOT_HOST_CXXFLAGS := -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
BUILDROOT_MAKE = env -u MAKEFLAGS -u MFLAGS -u ROOTFS $(MAKE) -C '$(BUILDROOT_SRC)' O='$(abspath $(BUILDROOT_OUT))'
ifeq ($(ROOTFS),tiny)
ROOTFS_SUFFIX :=
ROOTFS_CPIO := $(INITRAMFS)
LINUX_OUT := $(BUILD_DIR)/linux-sf2000
else ifeq ($(ROOTFS),buildroot)
ROOTFS_SUFFIX := -buildroot
ROOTFS_CPIO := $(BUILDROOT_CPIO)
LINUX_OUT := $(BUILD_DIR)/linux-sf2000-buildroot
else
$(error unsupported ROOTFS '$(ROOTFS)', expected tiny or buildroot)
endif
LINUX_VMLINUX := $(LINUX_OUT)/vmlinux
LINUX_CONFIG_STAMP := $(LINUX_OUT)/.stamp-config
LINUX_CMDLINE_STAMP := $(LINUX_OUT)/.stamp-cmdline
LINUX_DEFCONFIG ?= 32r1el_defconfig
LINUX_PATCHES := $(wildcard patches/linux-$(LINUX_VERSION)/*.patch)
SF2000_DTB := $(BUILD_DIR)/sf2000.dtb
LINUX_CMDLINE ?= console=ttyS0,115200 earlycon init=/init
LINUX_LOADER_BLOBS_S := $(BUILD_DIR)/linux-loader$(ROOTFS_SUFFIX)-blobs.S
LINUX_LOADER_ENTRY_OBJ := $(BUILD_DIR)/linux-loader$(ROOTFS_SUFFIX)-entry.o
LINUX_LOADER_OBJ := $(BUILD_DIR)/linux-loader$(ROOTFS_SUFFIX).o
LINUX_LOADER_BLOBS_OBJ := $(BUILD_DIR)/linux-loader$(ROOTFS_SUFFIX)-blobs.o
LINUX_LOADER_ELF := $(BUILD_DIR)/linux-loader$(ROOTFS_SUFFIX).elf
LINUX_LOADER_BIN := $(BUILD_DIR)/linux-loader$(ROOTFS_SUFFIX).bin
LINUX_ASD := $(BUILD_DIR)/sf2000-linux$(ROOTFS_SUFFIX).asd
SDCARD_LINUX_ASD := $(BUILD_DIR)/sdcard/firmware/linux.asd
SDCARD_FASTBOOT_BIN := $(BUILD_DIR)/sdcard/firmware/unifrog.bin
SDCARD_BIOS_ASD := $(BUILD_DIR)/sdcard/bios/bisrv.asd
SDCARD_BOOT_OPTIONS := $(BUILD_DIR)/sdcard/BOOT-OPTIONS.txt
SDCARD_LOG_TXT := $(BUILD_DIR)/sdcard/log.txt
LINUX_ROM_SD_IMAGE := $(BUILD_DIR)/sf2000-linux$(ROOTFS_SUFFIX)-rom.sd.img
LINUX_ROM_SD_IMAGE_OFFSET := 1048576
BOOTROM_BUGFIX ?= /root/host-frogdev/universal/orig_firmware/UpdateFirmware/SF2000_XMC_XM25QH40B_4mbit_bugfix.bin
QEMU_BOOT_TIMEOUT ?= 90s
SMOKE_INIT_PATTERN ?= binfmt_flat: SF2000 NOMMU FLAT entry
LOADER_CFLAGS := -Os -ffreestanding -fno-builtin -nostdlib \
	-march=mips32 -mabi=32 -msoft-float -mno-abicalls -fno-pic -mno-gpopt -G 0 \
	-Wall -Wextra

.PHONY: all status qemu rootfs buildroot buildroot-reconfigure linux linux-reextract linux-reconfigure linux-asd linux-buildroot \
	linux-buildroot-asd sdcard-linux sdcard-buildroot linux-rom-sd \
	linux-buildroot-rom-sd run-linux smoke-linux run-linux-asd \
	smoke-linux-asd run-linux-rom smoke-linux-rom run-linux-buildroot-asd \
	smoke-linux-buildroot-asd run-linux-buildroot-storage \
	smoke-linux-buildroot-storage run-linux-buildroot-storage-fast \
	smoke-linux-buildroot-storage-fast run-linux-buildroot-storage-writeback \
smoke-linux-buildroot-storage-writeback run-linux-buildroot-storage-probe-writeback \
smoke-linux-buildroot-storage-probe-writeback run-linux-buildroot-storage-enumeration \
smoke-linux-buildroot-storage-enumeration run-linux-buildroot-rom \
run-linux-buildroot-storage-launch smoke-linux-buildroot-storage-launch \
run-qemu-stock-fatfs-writeback smoke-qemu-stock-fatfs-writeback \
smoke-linux-buildroot-rom run-linux-buildroot-display \
	smoke-linux-buildroot-display run-linux-buildroot-panel \
	smoke-linux-buildroot-panel run-linux-buildroot-panel-fast \
	smoke-linux-buildroot-panel-fast buildroot-panel-probe-link run-linux-input smoke-linux-input \
	run-linux-buildroot-input smoke-linux-buildroot-input \
	run-linux-reboot smoke-linux-reboot run-linux-buildroot-reboot \
	smoke-linux-buildroot-reboot run-linux-buildroot-reset-snapshot \
	smoke-linux-buildroot-reset-snapshot run-linux-buildroot-reset-restore \
	smoke-linux-buildroot-reset-restore clean

all: status

status:
	@printf 'sf2000_linux workspace\n'
	@printf '  qemu:    %s\n' '$(QEMU_DIR)'
	@printf '  hclinux: %s\n' '$(HCLINUX_DIR)'
	@printf '  host cc: %s\n' '$(HOSTCC)'
	@printf '  toolchain: %s\n' '$(CROSS_COMPILE)'
	@printf '  rootfs:  %s\n' '$(ROOTFS)'
	@printf '  buildroot out: %s\n' '$(BUILDROOT_OUT)'
	@printf '  jobs:    %s\n' '$(JOBS)'
	@printf '  qemu bin exists: '
	@test -x '$(QEMU_BIN)' && printf 'yes\n' || printf 'no\n'
	@printf '  qemu cpu: %s\n' '$(QEMU_CPU)'
	@printf '  qemu rom cpu: %s\n' '$(if $(QEMU_ROM_CPU),$(QEMU_ROM_CPU),default)'
	@printf '  initramfs exists: '
	@test -f '$(INITRAMFS)' && printf 'yes\n' || printf 'no\n'
	@printf '  linux vmlinux exists: '
	@test -f '$(LINUX_VMLINUX)' && printf 'yes\n' || printf 'no\n'
	@printf '  linux asd exists: '
	@test -f '$(LINUX_ASD)' && printf 'yes\n' || printf 'no\n'
	@printf '  buildroot cpio exists: '
	@test -f '$(BUILDROOT_CPIO)' && printf 'yes\n' || printf 'no\n'
	@printf '  bugfix ROM exists: '
	@test -f '$(BOOTROM_BUGFIX)' && printf 'yes\n' || printf 'no\n'

qemu:
	$(MAKE) -C '$(QEMU_DIR)' build

$(QEMU_MKSD): $(QEMU_DIR)/tools/mksf2000sd.c
	$(MAKE) -C '$(QEMU_DIR)' build/mksf2000sd

rootfs: $(ROOTFS_CPIO)
buildroot: $(BUILDROOT_CPIO)

buildroot-reconfigure:
	rm -f '$(BUILDROOT_OUT)/.config'
	$(MAKE) ROOTFS='$(ROOTFS)' buildroot

$(BFLTPACK): tools/bfltpack.c Makefile
	mkdir -p '$(dir $@)'
	'$(HOSTCC)' -O2 -Wall -Wextra -o '$@' '$<'

$(INIT_RAW): init/sf2000-flat-init.S Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' $(INIT_CFLAGS) -Os -nostdlib -ffreestanding \
		-march=mips32 -mabi=32 -msoft-float -mno-abicalls \
		-fno-pic -mno-gpopt -G 0 -Wl,-Ttext=0 -Wl,-e,_start \
		-Wl,--gc-sections -Wl,-z,noexecstack -o '$@.elf' '$<'
	'$(OBJCOPY_MIPS)' -O binary -j .text '$@.elf' '$@'

$(INIT_BIN): $(INIT_RAW) $(BFLTPACK)
	'$(BFLTPACK)' '$(INIT_RAW)' '$@'

$(GEN_INIT_CPIO): $(LINUX_SRC)/.patched
	mkdir -p '$(dir $@)'
	'$(HOSTCC)' -O2 -o '$@' '$(LINUX_SRC)'/usr/gen_init_cpio.c

$(INITRAMFS): $(INIT_BIN) $(GEN_INIT_CPIO) Makefile
	mkdir -p '$(dir $@)'
	{ \
		printf 'dir /dev 0755 0 0\n'; \
		printf 'dir /proc 0555 0 0\n'; \
		printf 'dir /sys 0555 0 0\n'; \
		printf 'nod /dev/console 0600 0 0 c 5 1\n'; \
		printf 'nod /dev/null 0666 0 0 c 1 3\n'; \
		printf 'nod /dev/tty 0666 0 0 c 5 0\n'; \
		printf 'nod /dev/kmsg 0600 0 0 c 1 11\n'; \
		printf 'file /init %s 0755 0 0\n' '$(abspath $(INIT_BIN))'; \
	} > '$(INITRAMFS_LIST)'
	'$(GEN_INIT_CPIO)' '$(INITRAMFS_LIST)' > '$@'

$(BUILDROOT_SRC)/Makefile:
	mkdir -p '$(BUILD_DIR)' .cache
	test -f '.cache/$(BUILDROOT_TARBALL)' || curl -L -o '.cache/$(BUILDROOT_TARBALL)' '$(BUILDROOT_URL)'
	rm -rf '$(BUILDROOT_SRC)'
	mkdir -p '$(BUILDROOT_SRC)'
	tar -xf '.cache/$(BUILDROOT_TARBALL)' -C '$(BUILDROOT_SRC)' --strip-components=1

$(BUILDROOT_GENERATED_OVERLAY_STAMP):
	mkdir -p '$(dir $@)' '$(BUILDROOT_GENERATED_OVERLAY)'
	touch '$@'

$(BUILDROOT_SRC)/.patched: $(BUILDROOT_SRC)/Makefile $(BUILDROOT_CORE_PATCHES)
	@for patch in $(BUILDROOT_CORE_PATCHES); do \
		if test -e '$@' && ! test "$$patch" -nt '$@'; then \
			continue; \
		fi; \
		if patch -d '$(BUILDROOT_SRC)' --dry-run -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'applying %s\n' "$$patch"; \
			patch -d '$(BUILDROOT_SRC)' -p1 < "$$patch"; \
		elif patch -d '$(BUILDROOT_SRC)' --dry-run -R -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'already applied %s\n' "$$patch"; \
		else \
			printf 'cannot apply %s; remove %s for a clean Buildroot tree\n' "$$patch" '$(BUILDROOT_SRC)' >&2; \
			exit 1; \
		fi; \
	done
	touch '$@'

$(BUILDROOT_OUT)/.config: $(BUILDROOT_SRC)/.patched $(BUILDROOT_DEFCONFIG) $(BUILDROOT_BUSYBOX_CONFIG) $(BUILDROOT_DEVICE_TABLE) $(BUILDROOT_PATCH_FILES) | $(BUILDROOT_GENERATED_OVERLAY_STAMP)
	mkdir -p '$(BUILDROOT_OUT)'
	$(BUILDROOT_MAKE) BR2_DEFCONFIG='$(abspath $(BUILDROOT_DEFCONFIG))' defconfig
	'$(BUILDROOT_SRC)'/utils/config --file '$@' \
		--set-str GLOBAL_PATCH_DIR '$(abspath $(BUILDROOT_PATCHES))' \
		--set-str PACKAGE_BUSYBOX_CONFIG '$(abspath $(BUILDROOT_BUSYBOX_CONFIG))' \
		--set-str ROOTFS_OVERLAY '$(abspath $(BUILDROOT_OVERLAY)) $(abspath $(BUILDROOT_GENERATED_OVERLAY))' \
		--set-str ROOTFS_DEVICE_TABLE 'system/device_table.txt $(abspath $(BUILDROOT_DEVICE_TABLE))'
	$(BUILDROOT_MAKE) olddefconfig

$(BUILDROOT_TOOLCHAIN_STAMP): $(BUILDROOT_OUT)/.config
	FORCE_UNSAFE_CONFIGURE=1 $(BUILDROOT_MAKE) -j'$(JOBS)' toolchain \
		HOST_CFLAGS='$(BUILDROOT_HOST_CFLAGS)' \
		HOST_CXXFLAGS='$(BUILDROOT_HOST_CXXFLAGS)'
	touch '$@'

$(BUILDROOT_INIT): $(BUILDROOT_INIT_SOURCE) Makefile
	mkdir -p '$(dir $@)'
	cp '$(BUILDROOT_INIT_SOURCE)' '$@'
	chmod 0755 '$@'

$(BUILDROOT_SUPERVISOR): $(BUILDROOT_INIT_SRC) $(BUILDROOT_INIT_ENTRY) $(BUILDROOT_INIT_CLONE) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_SUPERVISOR_CFLAGS) $(BUILDROOT_SUPERVISOR_LDFLAGS) \
		-o '$@' '$(BUILDROOT_INIT_ENTRY)' '$(BUILDROOT_INIT_CLONE)' '$(BUILDROOT_INIT_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_SUPERVISOR_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_PAD): $(BUILDROOT_PAD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_FLAT_LDFLAGS) -o '$@' '$<'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_HEARTBEAT): $(BUILDROOT_HEARTBEAT_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_FLAT_LDFLAGS) -o '$@' '$<'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_SCREEN): $(BUILDROOT_SCREEN_SRC) $(BUILDROOT_SCREEN_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_SCREEN_CFLAGS) $(BUILDROOT_SCREEN_LDFLAGS) \
		-o '$@' '$(BUILDROOT_SCREEN_ENTRY)' '$(BUILDROOT_SCREEN_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_SCREEN_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

buildroot-panel-probe-link: $(BUILDROOT_PANEL_FASTPROBE) Makefile
	mkdir -p '$(dir $(BUILDROOT_PANEL_PROBE_LINK))'
	ln -sf '$(BUILDROOT_PANEL_PROBE_TARGET)' '$(BUILDROOT_PANEL_PROBE_LINK)'

$(BUILDROOT_PANEL_INIT): $(BUILDROOT_PANEL_INIT_SRC) $(BUILDROOT_PANEL_INIT_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_PANEL_INIT_CFLAGS) $(BUILDROOT_SCREEN_LDFLAGS) \
		-o '$@' '$(BUILDROOT_PANEL_INIT_ENTRY)' '$(BUILDROOT_PANEL_INIT_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_SCREEN_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_PANEL_FASTPROBE): $(BUILDROOT_PANEL_FASTPROBE_SRC) $(BUILDROOT_PANEL_FASTPROBE_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_PANEL_FASTPROBE_LDFLAGS) \
		-o '$@' '$(BUILDROOT_PANEL_FASTPROBE_ENTRY)' '$(BUILDROOT_PANEL_FASTPROBE_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_STORAGE_PROBE): $(BUILDROOT_STORAGE_PROBE_SRC) $(BUILDROOT_STORAGE_PROBE_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_STORAGE_PROBE_CFLAGS) $(BUILDROOT_STORAGE_PROBE_LDFLAGS) \
		-o '$@' '$(BUILDROOT_STORAGE_PROBE_ENTRY)' '$(BUILDROOT_STORAGE_PROBE_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_STORAGE_FASTPROBE): $(BUILDROOT_STORAGE_FASTPROBE_SRC) $(BUILDROOT_STORAGE_FASTPROBE_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_STORAGE_FASTPROBE_CFLAGS) $(BUILDROOT_STORAGE_FASTPROBE_LDFLAGS) \
		-o '$@' '$(BUILDROOT_STORAGE_FASTPROBE_ENTRY)' '$(BUILDROOT_STORAGE_FASTPROBE_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_RESET_FASTPROBE): $(BUILDROOT_RESET_FASTPROBE_SRC) $(BUILDROOT_RESET_FASTPROBE_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_RESET_FASTPROBE_LDFLAGS) \
		-o '$@' '$(BUILDROOT_RESET_FASTPROBE_ENTRY)' '$(BUILDROOT_RESET_FASTPROBE_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_TARGET_STAMP): $(BUILDROOT_OUT)/.config $(BUILDROOT_TOOLCHAIN_STAMP) | $(BUILDROOT_GENERATED_OVERLAY_STAMP)
	FORCE_UNSAFE_CONFIGURE=1 $(BUILDROOT_MAKE) -j'$(JOBS)' \
		HOST_CFLAGS='$(BUILDROOT_HOST_CFLAGS)' \
		HOST_CXXFLAGS='$(BUILDROOT_HOST_CXXFLAGS)'
	touch '$@'

$(BUILDROOT_CPIO): $(BUILDROOT_TARGET_STAMP) $(BUILDROOT_INIT) $(BUILDROOT_SUPERVISOR) $(BUILDROOT_PAD) $(BUILDROOT_HEARTBEAT) $(BUILDROOT_SCREEN) $(BUILDROOT_PANEL_INIT) $(BUILDROOT_PANEL_FASTPROBE) $(BUILDROOT_STORAGE_PROBE) $(BUILDROOT_STORAGE_FASTPROBE) $(BUILDROOT_RESET_FASTPROBE) $(BUILDROOT_OVERLAY_FILES) $(BUILDROOT_DEVICE_TABLE) $(GEN_INIT_CPIO)
	mkdir -p '$(dir $@)'
	rm -rf '$(BUILDROOT_REPACK_DIR)'
	mkdir -p '$(BUILDROOT_REPACK_DIR)'
	rsync -a --delete --exclude=/THIS_IS_NOT_YOUR_ROOT_FILESYSTEM \
		'$(BUILDROOT_OUT)'/target/ '$(BUILDROOT_REPACK_DIR)'/
	rsync -a '$(BUILDROOT_OVERLAY)'/ '$(BUILDROOT_REPACK_DIR)'/
	rsync -a '$(BUILDROOT_GENERATED_OVERLAY)'/ '$(BUILDROOT_REPACK_DIR)'/
	rm -rf '$(BUILDROOT_REPACK_DIR)'/run/* '$(BUILDROOT_REPACK_DIR)'/tmp/*
	mkdir -p '$(BUILDROOT_REPACK_DIR)'/dev/input \
		'$(BUILDROOT_REPACK_DIR)'/dev/pts '$(BUILDROOT_REPACK_DIR)'/dev/shm
	{ \
		printf 'nod /dev/console 0622 0 0 c 5 1\n'; \
		printf 'nod /dev/null 0666 0 0 c 1 3\n'; \
		printf 'nod /dev/mem 0640 0 0 c 1 1\n'; \
		printf 'nod /dev/tty 0666 0 0 c 5 0\n'; \
		printf 'nod /dev/ttyS0 0660 0 5 c 4 64\n'; \
		printf 'nod /dev/kmsg 0600 0 0 c 1 11\n'; \
		printf 'nod /dev/mmcblk0 0600 0 0 b 179 0\n'; \
		printf 'nod /dev/uinput 0660 0 0 c 10 223\n'; \
		printf 'nod /dev/input/event0 0660 0 0 c 13 64\n'; \
		printf 'nod /dev/input/event1 0660 0 0 c 13 65\n'; \
		printf 'nod /dev/input/event2 0660 0 0 c 13 66\n'; \
		printf 'nod /dev/input/event3 0660 0 0 c 13 67\n'; \
	} > '$(BUILDROOT_DEVICE_CPIO_LIST)'
	mkdir -p '$(BUILD_DIR)'/usr
	ln -sf ../gen_init_cpio '$(BUILD_DIR)'/usr/gen_init_cpio
	cd '$(BUILD_DIR)' && '$(abspath $(LINUX_SRC))'/usr/gen_initramfs.sh \
		-o '$(abspath $@)' -u squash -g squash \
		'$(abspath $(BUILDROOT_REPACK_DIR))' \
		'$(abspath $(BUILDROOT_DEVICE_CPIO_LIST))'

$(LINUX_SRC)/Makefile:
	mkdir -p '$(BUILD_DIR)' .cache
	test -f '.cache/$(LINUX_TARBALL)' || curl -L -o '.cache/$(LINUX_TARBALL)' '$(LINUX_URL)'
	rm -rf '$(LINUX_SRC)'
	mkdir -p '$(LINUX_SRC)'
	tar -xf '.cache/$(LINUX_TARBALL)' -C '$(LINUX_SRC)' --strip-components=1

$(LINUX_SRC)/.patched: $(LINUX_SRC)/Makefile $(LINUX_PATCHES)
	@if test -e '$@'; then \
		printf 'linux patch series changed; applying incrementally in %s\n' '$(LINUX_SRC)'; \
	fi
	@for patch in $(LINUX_PATCHES); do \
		if test -e '$@' && ! test "$$patch" -nt '$@'; then \
			continue; \
		fi; \
		if patch -d '$(LINUX_SRC)' --dry-run -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'applying %s\n' "$$patch"; \
			patch -d '$(LINUX_SRC)' -p1 < "$$patch"; \
		elif patch -d '$(LINUX_SRC)' --dry-run -R -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'already applied %s\n' "$$patch"; \
		else \
			printf 'cannot apply %s; run make linux-reextract for a clean kernel tree\n' "$$patch" >&2; \
			exit 1; \
		fi; \
	done
	touch '$@'

$(SF2000_DTB): linux/sf2000.dts
	mkdir -p '$(dir $@)'
	dtc -I dts -O dtb -o '$@' '$<'

FORCE:

$(LINUX_CMDLINE_STAMP): Makefile FORCE | $(LINUX_SRC)/.patched
	mkdir -p '$(dir $@)'
	printf '%s\n' '$(LINUX_CMDLINE)' > '$@.tmp'
	if ! cmp -s '$@.tmp' '$@' 2>/dev/null; then mv '$@.tmp' '$@'; else rm -f '$@.tmp'; fi

$(LINUX_CONFIG_STAMP): $(LINUX_SRC)/Makefile Makefile $(LINUX_CMDLINE_STAMP) | $(LINUX_SRC)/.patched
	mkdir -p '$(LINUX_OUT)'
	$(MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' '$(LINUX_DEFCONFIG)'
	'$(LINUX_SRC)'/scripts/config --file '$(LINUX_OUT)/.config' \
		--enable BLK_DEV_INITRD \
		--set-str INITRAMFS_SOURCE '$(abspath $(ROOTFS_CPIO))' \
		--enable CMDLINE_BOOL \
		--set-str CMDLINE '$(LINUX_CMDLINE)' \
		--disable MMU \
		--disable BINFMT_ELF \
		--disable COMPAT_BINFMT_ELF \
		--enable BINFMT_FLAT \
		--disable BINFMT_FLAT_ARGVP_ENVP_ON_STACK \
		--enable BINFMT_SCRIPT \
		--disable COREDUMP \
		--disable DEVMEM \
		--disable DEVKMEM \
		--disable MIPS_GENERIC_KERNEL \
		--enable MIPS_SF2000 \
		--disable MIPS_CMDLINE_FROM_DTB \
		--enable MIPS_CMDLINE_BUILTIN_EXTEND \
		--disable MODULES \
		--disable DEBUG_INFO \
		--disable DEBUG_INFO_REDUCED \
		--disable IKCONFIG \
		--disable IKCONFIG_PROC \
		--disable KALLSYMS \
		--disable KALLSYMS_ALL \
		--disable KALLSYMS_BASE_RELATIVE \
		--disable INITRAMFS_COMPRESSION_GZIP \
		--enable INITRAMFS_COMPRESSION_NONE \
		--enable DEVTMPFS \
		--enable DEVTMPFS_MOUNT \
		--enable PROC_FS \
		--enable SYSFS \
		--enable TMPFS \
		--enable TTY \
		--enable SERIAL_8250 \
		--enable SERIAL_8250_CONSOLE \
		--enable SERIAL_OF_PLATFORM \
		--disable SERIAL_MCTRL_GPIO \
		--disable SERIAL_8250_DMA \
		--disable SERIAL_8250_PCI \
		--disable SMP \
		--disable SMP_UP \
		--disable HIGHMEM \
		--disable JUMP_LABEL \
		--disable HARDWARE_WATCHPOINTS \
		--disable MIPS_CPS \
		--disable MIPS_CPS_PM \
		--disable MIPS_CM \
		--disable MIPS_CPC \
		--disable MIPS_CMP \
		--disable MIPS_CPU_SCACHE \
		--disable MIPS_GIC \
		--disable CLKSRC_MIPS_GIC \
		--disable CPU_MIPSR2_IRQ_VI \
		--disable CPU_MIPSR2_IRQ_EI \
		--disable SWAP_IO_SPACE \
		--disable MIPS_MT \
		--disable MIPS_MT_SMP \
		--disable MIPS_MT_FPAFF \
		--disable BLOCK \
		--disable BLK_DEV \
		--disable BLK_DEV_BSG \
		--disable PCI \
		--disable PCI_MSI \
		--disable SCSI \
		--disable ATA \
		--disable SATA_AHCI \
		--disable MD \
		--disable BLK_DEV_SD \
		--disable NET \
		--disable INET \
		--disable NETFILTER \
		--disable NETDEVICES \
		--disable BPF \
		--disable BPF_SYSCALL \
		--disable SCHED_AUTOGROUP \
		--disable MEMCG \
		--disable MEMCG_SWAP \
		--disable MEMCG_KMEM \
		--disable BLK_CGROUP \
		--enable BLOCK \
		--enable BLK_DEV \
		--enable PARTITION_ADVANCED \
		--enable MSDOS_PARTITION \
		--disable CGROUP_SCHED \
		--disable FAIR_GROUP_SCHED \
		--disable CGROUP_PIDS \
		--disable CGROUP_FREEZER \
		--disable CPUSETS \
		--disable CGROUP_DEVICE \
		--disable CGROUP_CPUACCT \
		--disable CGROUPS \
		--disable NAMESPACES \
		--disable SYSVIPC \
		--disable POSIX_MQUEUE \
		--disable KEYS \
		--enable INPUT \
		--enable INPUT_EVDEV \
		--enable INPUT_MISC \
		--enable INPUT_UINPUT \
		--disable HID \
		--enable USB_SUPPORT \
		--enable USB_COMMON \
		--enable USB \
		--enable USB_ANNOUNCE_NEW_DEVICES \
		--enable USB_DEFAULT_PERSIST \
		--enable USB_EHCI_HCD \
		--enable USB_EHCI_HCD_PLATFORM \
		--enable USB_OHCI_HCD \
		--enable USB_OHCI_HCD_PLATFORM \
		--enable USB_MUSB_HDRC \
		--enable USB_MUSB_HOST \
		--enable USB_MUSB_SF2000 \
		--enable NOP_USB_XCEIV \
		--enable MUSB_PIO_ONLY \
		--enable USB_STORAGE \
		--enable HID \
		--enable HID_GENERIC \
		--enable USB_HID \
		--enable HIDRAW \
		--disable VIRTIO \
		--disable VIRTIO_MENU \
		--disable VIRTIO_BLK \
		--disable VIRTIO_CONSOLE \
		--disable VIRTIO_MMIO \
		--disable HW_RANDOM \
		--enable RANDOM_TRUST_BOOTLOADER \
		--disable I2C \
		--disable SPI \
		--disable GPIOLIB \
		--disable GPIO_SYSFS \
		--disable GPIO_CDEV \
		--disable POWER_SUPPLY \
		--enable MMC \
		--enable MMC_BLOCK \
		--set-val MMC_BLOCK_MINORS 8 \
		--disable MMC_DW \
		--disable MMC_DW_HICHIP \
		--enable MMC_HC15 \
		--enable HICHIP_SYS_INTC \
		--disable MMC_SDHCI \
		--disable MMC_CQHCI \
		--disable MMC_SPI \
		--disable LEDS_CLASS \
		--disable LEDS_SYSCON \
		--disable RTC_CLASS \
		--disable RTC_DRV_M41T80 \
		--disable RTC_DRV_GOLDFISH \
		--disable DMADEVICES \
		--disable MFD_SYSCON \
		--disable NVMEM \
		--disable NVMEM_SYSFS \
		--disable COMMON_CLK_BOSTON \
		--disable MTD \
		--disable MTD_BLOCK \
		--disable MTD_UBI \
		--disable VT \
		--enable SCSI \
		--enable BLK_DEV_SD \
		--enable FAT_FS \
		--enable MSDOS_FS \
		--enable VFAT_FS \
		--enable NLS \
		--enable NLS_CODEPAGE_437 \
		--enable NLS_ISO8859_1 \
		--disable EXT4_FS \
		--disable EXPORTFS \
		--disable FUSE_FS \
		--disable FS_ENCRYPTION \
		--disable FS_VERITY \
		--disable FSNOTIFY \
		--disable DNOTIFY \
		--disable INOTIFY_USER \
		--disable FANOTIFY \
		--disable OVERLAY_FS \
		--disable NETWORK_FILESYSTEMS \
		--disable NFS_FS \
		--disable 9P_FS \
		--disable AIO \
		--disable IO_URING \
		--disable TMPFS_POSIX_ACL \
		--disable TMPFS_XATTR \
		--disable CRYPTO \
		--disable SOUND \
		--disable MEDIA_SUPPORT \
		--disable DRM \
		--disable FB
	$(MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' olddefconfig
	touch '$@'

$(LINUX_VMLINUX): $(LINUX_SRC)/.patched $(LINUX_CONFIG_STAMP) $(ROOTFS_CPIO)
	$(MAKE) -j'$(JOBS)' -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' vmlinux

linux: $(LINUX_VMLINUX) $(SF2000_DTB)

linux-reextract:
	rm -rf '$(LINUX_SRC)' '$(LINUX_OUT)'

linux-reconfigure:
	rm -f '$(LINUX_OUT)/.config' '$(LINUX_CONFIG_STAMP)'
	$(MAKE) ROOTFS='$(ROOTFS)' linux

$(ASDPACK): tools/asdpack.c Makefile
	mkdir -p '$(dir $@)'
	'$(HOSTCC)' -O2 -Wall -Wextra -o '$@' '$<'

$(LINUX_LOADER_ENTRY_OBJ): boot/linux-loader-entry.S Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' $(LOADER_CFLAGS) -c -o '$@' '$<'

$(LINUX_LOADER_OBJ): boot/linux-loader.c Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' $(LOADER_CFLAGS) -c -o '$@' '$<'

$(LINUX_LOADER_BLOBS_S): $(LINUX_VMLINUX) $(SF2000_DTB) Makefile
	mkdir -p '$(dir $@)'
	{ \
		printf '.section .rodata.blobs, "a"\n'; \
		printf '.balign 16\n'; \
		printf '.globl linux_vmlinux_start\n'; \
		printf 'linux_vmlinux_start:\n'; \
		printf '.incbin "%s"\n' '$(abspath $(LINUX_VMLINUX))'; \
		printf '.balign 16\n'; \
		printf '.globl linux_vmlinux_end\n'; \
		printf 'linux_vmlinux_end:\n'; \
		printf '.globl linux_dtb_start\n'; \
		printf 'linux_dtb_start:\n'; \
		printf '.incbin "%s"\n' '$(abspath $(SF2000_DTB))'; \
		printf '.balign 16\n'; \
		printf '.globl linux_dtb_end\n'; \
		printf 'linux_dtb_end:\n'; \
	} > '$@'

$(LINUX_LOADER_BLOBS_OBJ): $(LINUX_LOADER_BLOBS_S)
	'$(CC_MIPS)' $(LOADER_CFLAGS) -c -o '$@' '$<'

$(LINUX_LOADER_ELF): $(LINUX_LOADER_ENTRY_OBJ) $(LINUX_LOADER_OBJ) \
		$(LINUX_LOADER_BLOBS_OBJ) boot/linux-loader.ld
	'$(LD_MIPS)' -EL -T boot/linux-loader.ld -o '$@' \
		'$(LINUX_LOADER_ENTRY_OBJ)' '$(LINUX_LOADER_OBJ)' \
		'$(LINUX_LOADER_BLOBS_OBJ)'

$(LINUX_LOADER_BIN): $(LINUX_LOADER_ELF)
	'$(OBJCOPY_MIPS)' -O binary '$<' '$@'

$(LINUX_ASD): $(LINUX_LOADER_BIN) $(ASDPACK)
	'$(ASDPACK)' '$(LINUX_LOADER_BIN)' '$@'
	'$(ASDPACK)' --check '$@'

$(SDCARD_LINUX_ASD): $(LINUX_ASD)
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(SDCARD_BIOS_ASD): $(LINUX_ASD)
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(SDCARD_FASTBOOT_BIN): $(LINUX_LOADER_BIN)
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(SDCARD_BOOT_OPTIONS): Makefile
	mkdir -p '$(dir $@)'
	{ \
		printf 'SF2000 Linux boot artifacts for ROOTFS=%s\n\n' '$(ROOTFS)'; \
		printf '/bios/bisrv.asd\n'; \
		printf '  Direct ROM boot. This is an ASD image with a valid LCFG CRC.\n\n'; \
		printf '/firmware/unifrog.bin\n'; \
		printf '  Existing fastboot auto-load path. This is the raw loader binary, not an ASD.\n\n'; \
		printf '/firmware/linux.asd\n'; \
		printf '  Unifrog menu handoff path. Boot Unifrog first, then select linux.asd.\n\n'; \
		printf '/log.txt\n'; \
		printf '  Preallocated early boot log. Keep this fixed-size file in the SD root.\n'; \
		printf '  The ROM has disk_write but no f_write, so Linux overwrites this file in place.\n\n'; \
		printf 'RAM progress recorder:\n'; \
		printf '  Early Linux records named progress entries at uncached 0xa13f0000.\n'; \
		printf '  On the next boot, the loader dumps the previous run to log.txt before clearing it.\n'; \
		printf '  This helps replace manual blink counting when RAM survives reset/reboot.\n\n'; \
		printf 'Early watchdog:\n'; \
		printf '  Linux arms WDT0 during setup_arch and pets it near overflow whenever it records progress.\n'; \
		printf '  /init disables WDT0 after userspace is alive. A pre-userspace hang should reboot.\n'; \
		printf '  After that reboot, inspect log.txt for the previous RAM progress dump.\n\n'; \
		printf 'Runtime controls:\n'; \
		printf '  Press SELECT in Linux userspace to request a watchdog reboot.\n\n'; \
		printf 'Visible stages use counted backlight-off pulses plus L25 status LED flashes:\n'; \
		printf '  Direct QEMU blank-ROM runs compress these delays automatically.\n'; \
		printf '  1 pulse: Linux loader entered\n'; \
		printf '  2 pulses: loader is jumping to the kernel\n'; \
		printf '  log.txt records CP0 EBase before/after the loader normalizes it for Linux.\n'; \
		printf '  log.txt records the SYSIO mapping register before/after the Linux mapping bit.\n'; \
		printf '  log.txt also records the handoff words used to decide whether ROM helpers are present.\n'; \
		printf '  If the LCD marker appears, large center digits are the monotonic visible event number.\n'; \
		printf '  Top-left digit 1 means counted stage, 2 means tick; lower-right digits are stage pulses.\n'; \
		printf '  3 pulses: kernel entered MIPS setup_arch\n'; \
		printf '  after 3 pulses, count single ticks inside setup_arch:\n'; \
		printf '    1 cpu_probe, 2 mips_cm_probe skipped/done, 3 prom_init\n'; \
		printf '    4 early console, 5 cpu_report/check_bugs, 6 arch_mem_init\n'; \
		printf '    7 resource/smp map, 8 cpu_cache_init, 9 paging_init\n'; \
		printf '    10 memblock_dump_all\n'; \
		printf '  4 pulses: kernel finished MIPS setup_arch\n'; \
		printf '  5 pulses: start_kernel resumed after setup_arch\n'; \
		printf '  after 5 pulses, count single ticks for the post-setup_arch ladder:\n'; \
		printf '    1 setup_boot_config, 2 setup_command_line, 3 setup_nr_cpu_ids\n'; \
		printf '    4 setup_per_cpu_areas, 5 smp_prepare_boot_cpu, 6 cpu hotplug init\n'; \
		printf '    7 build_all_zonelists, 8 page_alloc_init, 9 jump_label_init\n'; \
		printf '    10 parse_early_param, 11 parse_args, 12 setup_log_buf\n'; \
		printf '    13 vfs_caches_init_early, 14 sort_main_extable\n'; \
		printf '  after the 14th post-setup tick, trap_init begins and emits its own ticks:\n'; \
		printf '    1 check_wait skipped/done, 2 ebase selected, 3 CP0 EBase normalized/skipped\n'; \
		printf '    4 generic vector preinstalled, then per_cpu_trap_init emits sub-ticks:\n'; \
		printf '      1 entry, 2 hwrena, 3 exception vector, 4 IRQ select, 5 ASID\n'; \
		printf '      6 mm context, 7 lazy TLB, 8 before tlb_init, 9 after tlb_init\n'; \
		printf '      10 TLBMISS setup, 11 Status/BEV normalized\n'; \
		printf '    inside tlb_init, physical TLB ticks use bare GPIO without printk/GMA.\n'; \
		printf '    ticks are: entry, config entry, pagemask write/read\n'; \
		printf '      wired zero, before flush, flush entry, irq save, entrylo clear/defer\n'; \
		printf '      wired skip/read, SF2000 TLBWI skip, config done\n'; \
		printf '      refill build before/after\n'; \
		printf '    next trap ticks: 5 per_cpu returned, 6 generic vector copied, 7 default vectors\n'; \
		printf '    8 watch vector, 9 parity setup, 10 board bus-error setup\n'; \
		printf '    11 main exception vectors, 12 icache flush, 13 DBE extable sort, 14 CU2 notifier\n'; \
		printf '  post-setup tick 15 means trap_init returned and mm_init is about to run.\n'; \
		printf '  6 pulses: mm_init completed\n'; \
		printf '  7 pulses: time_init completed\n'; \
		printf '  8 pulses: kernel is about to enable IRQs; RAM log records CP0 IRQ state\n'; \
		printf '  9 pulses: kernel is about to exec /init\n'; \
		printf '  10 pulses: bFLT /init reached userspace\n'; \
	} > '$@'

$(SDCARD_LOG_TXT): Makefile
	mkdir -p '$(dir $@)'
	dd if=/dev/zero of='$@' bs=262144 count=1 >/dev/null 2>&1

$(LINUX_ROM_SD_IMAGE): $(LINUX_ASD) $(QEMU_MKSD)
	'$(QEMU_MKSD)' '$(LINUX_ASD)' '$@' fat32

linux-asd: $(LINUX_ASD)

linux-buildroot:
	$(MAKE) ROOTFS=buildroot linux

linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot linux-asd

sdcard-linux: $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_BOOT_OPTIONS) $(SDCARD_LOG_TXT)

sdcard-buildroot:
	$(MAKE) ROOTFS=buildroot sdcard-linux

linux-rom-sd: $(LINUX_ROM_SD_IMAGE)

linux-buildroot-rom-sd:
	$(MAKE) ROOTFS=buildroot linux-rom-sd

run-linux: qemu linux
	mkdir -p '$(BUILD_DIR)'/logs
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(LINUX_VMLINUX)' \
		-dtb '$(SF2000_DTB)' -append '$(LINUX_CMDLINE)' \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux.log \
		> '$(BUILD_DIR)'/logs/linux.console 2>&1 || test $$? -eq 124

smoke-linux: run-linux
	grep -q 'sf2000: loaded Linux ELF' '$(BUILD_DIR)'/logs/linux.console
	grep -q 'sf2000: uart: .*Linux version' '$(BUILD_DIR)'/logs/linux.log
	grep -q '$(SMOKE_INIT_PATTERN)' '$(BUILD_DIR)'/logs/linux.log

run-linux-asd: qemu linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	SF2000_TRACE_PC='$(SF2000_TRACE_PC)' \
	SF2000_TRACE_SDIO='$(SF2000_TRACE_SDIO)' \
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(LINUX_ASD)' \
		-append '$(LINUX_CMDLINE)' \
		$(QEMU_SD_ARGS) \
		-display none -serial none -monitor none \
		-d '$(QEMU_DEBUG)' -D '$(BUILD_DIR)'/logs/linux-asd.log \
		> '$(BUILD_DIR)'/logs/linux-asd.console 2>&1 || test $$? -eq 124

smoke-linux-asd: run-linux-asd
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-asd.console
	grep -q 'sf2000: uart: .*linux-loader: jump entry=' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q '$(SMOKE_INIT_PATTERN)' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-input: qemu linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 35; printf 'sendkey right 1000\n'; sleep 2; \
		printf 'sendkey x 1000\n'; sleep 2; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(LINUX_ASD)' \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-input.log \
		> '$(BUILD_DIR)'/logs/linux-input.console 2>&1

smoke-linux-input: run-linux-input
	grep -q 'sf2000-pad: userspace input bridge ready' '$(BUILD_DIR)'/logs/linux-input.log
	grep -q 'sf2000-pad: state=.*RIGHT' '$(BUILD_DIR)'/logs/linux-input.log
	grep -q 'sf2000-pad: state=.*A' '$(BUILD_DIR)'/logs/linux-input.log

run-linux-reboot: qemu linux-rom-sd
	test -f '$(BOOTROM_BUGFIX)'
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 35; printf 'sendkey backspace 1000\n'; sleep 10; \
		printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_ROM_CPU_ARGS) -bios '$(BOOTROM_BUGFIX)' \
		-drive if=none,id=sd0,file='$(LINUX_ROM_SD_IMAGE)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-reboot.log \
		> '$(BUILD_DIR)'/logs/linux-reboot.console 2>&1

smoke-linux-reboot: run-linux-reboot
	grep -q 'sf2000-pad: SELECT pressed, rebooting' '$(BUILD_DIR)'/logs/linux-reboot.log
	grep -q 'sf2000: watchdog restart' '$(BUILD_DIR)'/logs/linux-reboot.log
	test "$$(grep -c 'sf2000: uart:  Hichip Bootloader' '$(BUILD_DIR)'/logs/linux-reboot.log)" -ge 2

run-linux-rom: qemu linux-rom-sd
	test -f '$(BOOTROM_BUGFIX)'
	mkdir -p '$(BUILD_DIR)'/logs
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_ROM_CPU_ARGS) -bios '$(BOOTROM_BUGFIX)' \
		-drive if=none,id=sd0,file='$(LINUX_ROM_SD_IMAGE)',format=raw \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-rom.log \
		> '$(BUILD_DIR)'/logs/linux-rom.console 2>&1 || test $$? -eq 124

smoke-linux-rom: run-linux-rom
	grep -q 'sf2000: uart:  Hichip Bootloader' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'CRC check pass !' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000: uart: .*linux-loader: jump entry=' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q '$(SMOKE_INIT_PATTERN)' '$(BUILD_DIR)'/logs/linux-rom.log

run-linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='binfmt_flat: SF2000 NOMMU FLAT entry' run-linux-asd

smoke-linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='binfmt_flat: SF2000 NOMMU FLAT entry' smoke-linux-asd

run-linux-buildroot-storage:
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-buildroot-storage-fast

smoke-linux-buildroot-storage:
	$(MAKE) ROOTFS=buildroot smoke-linux-buildroot-storage-writeback

run-linux-buildroot-storage-fast:
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-asd

smoke-linux-buildroot-storage-fast:
	$(MAKE) ROOTFS=buildroot \
		SF2000_TRACE_SDIO='1' \
		QEMU_DEBUG='guest_errors,unimp' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-buildroot-storage-fast
	grep -q 'Run /usr/sbin/sf2000-storage-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'hc15-probe' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'HC15 SD/MMC host registered' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sdio-access write addr=0x1884c004' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sdio-access write addr=0x1884c002' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-buildroot-storage-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback QEMU_JOBS='$(JOBS)'

smoke-linux-buildroot-storage-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback QEMU_JOBS='$(JOBS)'

run-linux-buildroot-storage-probe-writeback:
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-asd

smoke-linux-buildroot-storage-probe-writeback:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-storage-probe-writeback.XXXXXX.img); \
	trap 'rm -f $$tmp_sd' EXIT; \
	truncate -s 16M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	$(MAKE) ROOTFS=buildroot \
		SF2000_TRACE_SDIO='1' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-buildroot-storage-probe-writeback; \
	grep -q 'Run /usr/sbin/sf2000-storage-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'mmcblk0: mmc0:' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-access write addr=0x1884c024' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-access write addr=0x1884c02c' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-dma-write lba=16 .*len=4096 copied=4096' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'value=0x00000000 name=exit-code' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -a -q 'sf2000 linux sd write test 0239' "$$tmp_sd"

run-linux-buildroot-storage-enumeration:
	$(MAKE) ROOTFS=buildroot \
		SF2000_TRACE_SDIO='1' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-asd

smoke-linux-buildroot-storage-enumeration:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-storage-enumeration.XXXXXX.img); \
	trap 'rm -f $$tmp_sd' EXIT; \
	truncate -s 16M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	$(MAKE) ROOTFS=buildroot \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-buildroot-storage-enumeration; \
	grep -q 'sdio-dma-scr .*len=8 result=0' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'mmc0: new SDXC card' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'mmcblk0: mmc0:' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-dma-read .*len=4096 copied=4096' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-buildroot-storage-launch:
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-buildroot-storage-fast

smoke-linux-buildroot-storage-launch: run-linux-buildroot-storage-launch
	grep -q 'Run /usr/sbin/sf2000-storage-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-asd.log

run-qemu-stock-fatfs-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback QEMU_JOBS='$(JOBS)'

smoke-qemu-stock-fatfs-writeback: run-qemu-stock-fatfs-writeback

run-qemu-board-contract:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-board-contract QEMU_JOBS='$(JOBS)'

smoke-qemu-board-contract: run-qemu-board-contract

run-qemu-display:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-display QEMU_JOBS='$(JOBS)' && \
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-gb300-display QEMU_JOBS='$(JOBS)'

smoke-qemu-display: run-qemu-display

run-linux-buildroot-rom:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' run-linux-rom

smoke-linux-buildroot-rom:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' smoke-linux-rom
	grep -q 'sf2000: uart: .*sf2000: early watchdog armed' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000_buildroot: early watchdog disabled' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000-heartbeat: backlight heartbeat ready' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000-screen: panel init done' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000-screen: gma console ready' '$(BUILD_DIR)'/logs/linux-rom.log

run-linux-buildroot-display: qemu
	$(MAKE) ROOTFS=buildroot linux-asd
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/screenshots/linux-buildroot-gma
	rm -f '$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-*.ppm \
		'$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-latest.ppm
	SF2000_TRACE_GMA=1 \
	SF2000_GMA_DUMP_DIR='$(BUILD_DIR)'/screenshots/linux-buildroot-gma \
	SF2000_GMA_DUMP_LIMIT=8 \
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-buildroot-display.log \
		> '$(BUILD_DIR)'/logs/linux-buildroot-display.console 2>&1 || test $$? -eq 124

smoke-linux-buildroot-display: run-linux-buildroot-display
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-buildroot-display.console
	grep -q 'sf2000-screen: panel init done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'panel-pixel n=1' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'sf2000-screen: gma console ready' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'gma-present .*mode=6' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	test -s '$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-latest.ppm

run-linux-buildroot-panel: qemu
	$(MAKE) ROOTFS=buildroot BUILDROOT_INIT_SOURCE='$(BUILDROOT_SCREEN)' \
		BUILDROOT_SCREEN_CFLAGS='-DPANEL_PROBE_INIT' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon SF2000_PANEL_PROBE=1' linux-asd
	mkdir -p '$(BUILD_DIR)'/logs; \
	SF2000_TRACE_PC='1' timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-append 'console=ttyS0,115200 earlycon SF2000_PANEL_PROBE=1' \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-buildroot-panel.log \
		> '$(BUILD_DIR)'/logs/linux-buildroot-panel.console 2>&1 || test $$? -eq 124

smoke-linux-buildroot-panel: run-linux-buildroot-panel
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-buildroot-panel.console
	grep -q 'Run /init as init process' '$(BUILD_DIR)'/logs/linux-buildroot-panel.log
	grep -q 'sf2000-screen: main entry' '$(BUILD_DIR)'/logs/linux-buildroot-panel.log
	grep -q 'sf2000-screen: panel probe begin' '$(BUILD_DIR)'/logs/linux-buildroot-panel.log
	grep -q 'sf2000: panel-read-id start panel-id=0x009306' '$(BUILD_DIR)'/logs/linux-buildroot-panel.log
	grep -q 'sf2000-screen: panel init done id=0x009306' '$(BUILD_DIR)'/logs/linux-buildroot-panel.log
	grep -q 'sf2000-screen: panel probe done' '$(BUILD_DIR)'/logs/linux-buildroot-panel.log

run-linux-buildroot-panel-fast: qemu
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-panel-fastprobe' \
		SF2000_TRACE_PC='1' run-linux-asd
	mv '$(BUILD_DIR)'/logs/linux-asd.log '$(BUILD_DIR)'/logs/linux-buildroot-panel-fast.log
	mv '$(BUILD_DIR)'/logs/linux-asd.console '$(BUILD_DIR)'/logs/linux-buildroot-panel-fast.console

smoke-linux-buildroot-panel-fast: run-linux-buildroot-panel-fast
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-buildroot-panel-fast.console
	grep -q 'Run /usr/sbin/sf2000-panel-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-buildroot-panel-fast.log
	grep -q 'ret-syscall-exit' '$(BUILD_DIR)'/logs/linux-buildroot-panel-fast.log

run-linux-buildroot-input:
	$(MAKE) ROOTFS=buildroot run-linux-input

smoke-linux-buildroot-input:
	$(MAKE) ROOTFS=buildroot smoke-linux-input

run-linux-buildroot-reboot:
	$(MAKE) ROOTFS=buildroot run-linux-reboot

smoke-linux-buildroot-reboot:
	$(MAKE) ROOTFS=buildroot smoke-linux-reboot
	grep -q 'diag-fast-reset-begin' '$(BUILD_DIR)'/logs/linux-reboot.log
	grep -q 'diag-fast-mmc-done' '$(BUILD_DIR)'/logs/linux-reboot.log
	grep -q 'diag-fast-reset-done' '$(BUILD_DIR)'/logs/linux-reboot.log

run-linux-buildroot-reset-snapshot:
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		linux-asd
	mkdir -p '$(BUILD_DIR)'/logs; \
	SF2000_TRACE_PC='1' timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-append 'console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-buildroot-reset-snapshot.log \
		> '$(BUILD_DIR)'/logs/linux-buildroot-reset-snapshot.console 2>&1 || test $$? -eq 124

smoke-linux-buildroot-reset-snapshot: run-linux-buildroot-reset-snapshot
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-buildroot-reset-snapshot.console
	grep -q 'Run /usr/sbin/sf2000-reset-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-buildroot-reset-snapshot.log
	grep -q 'ret-syscall-exit' '$(BUILD_DIR)'/logs/linux-buildroot-reset-snapshot.log

run-linux-buildroot-reset-restore: qemu
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		linux-asd
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/qmp '$(BUILD_DIR)'/state
	python3 '$(BUILDROOT_RESET_RESTORE_SCRIPT)' \
		--qemu '$(QEMU_BIN)' \
		--cpu '$(QEMU_CPU)' \
		--kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		--append 'console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		--state '$(BUILDROOT_RESET_RESTORE_STATE)' \
		--source-console '$(BUILDROOT_RESET_RESTORE_PREFIX).source.console' \
		--source-log '$(BUILDROOT_RESET_RESTORE_PREFIX).source.log' \
		--restore-console '$(BUILDROOT_RESET_RESTORE_PREFIX).restore.console' \
		--restore-log '$(BUILDROOT_RESET_RESTORE_PREFIX).restore.log' \
		--socket '$(BUILDROOT_RESET_RESTORE_SOCKET)' \
		--restore-socket '$(BUILDROOT_RESET_RESTORE_SOCKET_DEST)' \
		--timeout '$(QEMU_BOOT_TIMEOUT)'

smoke-linux-buildroot-reset-restore: run-linux-buildroot-reset-restore
	grep -q 'sf2000: loaded ASD' '$(BUILDROOT_RESET_RESTORE_PREFIX).source.console'
	grep -q 'sf2000: entry-bytes storage_probe_entry pc=0x047c0050' '$(BUILDROOT_RESET_RESTORE_PREFIX).restore.log'

clean:
	rm -rf '$(BUILD_DIR)' '$(LINUX_SRC)'
