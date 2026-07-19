# SPDX-License-Identifier: MIT

QEMU_DIR ?= $(abspath ../sf2000_qemu)
QEMU_ORACLE_DIR ?= $(abspath $(QEMU_DIR))
HCLINUX_DIR := external/hclinux/2024.02.y.2
BUILD_DIR := build
INITRAMFS := $(BUILD_DIR)/initramfs.cpio
INITRAMFS_LIST := $(BUILD_DIR)/initramfs.list
INIT_BIN := $(BUILD_DIR)/initramfs-init
INIT_RAW := $(BUILD_DIR)/initramfs-init.raw
GEN_INIT_CPIO := $(BUILD_DIR)/gen_init_cpio
ASDPACK := $(BUILD_DIR)/asdpack
BFLTPACK := $(BUILD_DIR)/bfltpack
QEMU_BIN := /tmp/sf2000-qemu/qemu-10.2.2/build/qemu-system-mipsel
QEMU_MKSD := $(QEMU_DIR)/build/mksf2000sd
QEMU_CPU ?= 4Km
QEMU_CPU_ARGS := $(if $(QEMU_CPU),-cpu $(QEMU_CPU),)
QEMU_ROM_CPU ?=
QEMU_ROM_CPU_ARGS := $(if $(QEMU_ROM_CPU),-cpu $(QEMU_ROM_CPU),)
QEMU_DEBUG ?= guest_errors,unimp
QEMU_SD_ARGS ?=
HOSTCC ?= cc
TOOLCHAIN_DIR ?= $(BUILDROOT_OUT)/host
CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/bin/mipsel-buildroot-uclinux-uclibc-
CC_MIPS = $(CROSS_COMPILE)gcc
LD_MIPS = $(CROSS_COMPILE)ld
OBJCOPY_MIPS = $(CROSS_COMPILE)objcopy
STRIP_MIPS = $(CROSS_COMPILE)strip
OBJDUMP_MIPS = $(CROSS_COMPILE)objdump
READELF_MIPS = $(CROSS_COMPILE)readelf
NM_MIPS = $(CROSS_COMPILE)nm
JOBS ?= $(shell nproc 2>/dev/null || echo 1)
ROOTFS ?= tiny
LINUX_VERSION := 7.1.4
LINUX_TARBALL := linux-$(LINUX_VERSION).tar.xz
LINUX_URL := https://cdn.kernel.org/pub/linux/kernel/v7.x/$(LINUX_TARBALL)
LINUX_SRC ?= /tmp/sf2000-linux-next-kernel-$(LINUX_VERSION)
BUILDROOT_VERSION := 2026.05.1
BUILDROOT_TARBALL := buildroot-$(BUILDROOT_VERSION).tar.xz
BUILDROOT_URL := https://buildroot.org/downloads/$(BUILDROOT_TARBALL)
BUILDROOT_WORK ?= /tmp/sf2000-linux-next-buildroot
BUILDROOT_SRC ?= $(BUILDROOT_WORK)/buildroot-$(BUILDROOT_VERSION)
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
BUILDROOT_PAD_ENTRY := buildroot/sf2000-pad-entry.S
BUILDROOT_PAD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-pad
BUILDROOT_POWERD_SRC := buildroot/sf2000-powerd.c
BUILDROOT_POWERD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-powerd
FRONTEND_PROJECT ?= ../sf2000_linux_frontend
BUILDROOT_FRONTEND := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/sf2000-frontend
BUILDROOT_AUDIO_SRC := buildroot/sf2000-audio.c
BUILDROOT_AUDIO_ENTRY := buildroot/sf2000-audio-entry.S
BUILDROOT_AUDIO := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-audio
BUILDROOT_HEARTBEAT_SRC := buildroot/sf2000-heartbeat.c
BUILDROOT_HEARTBEAT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-heartbeat
BUILDROOT_LOGD_SRC := buildroot/sf2000-logd.c
BUILDROOT_LOGD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-logd
BUILDROOT_MOUNT_SRC := buildroot/sf2000-mount.c
BUILDROOT_MOUNT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-mount
BUILDROOT_SCREEN_SRC := buildroot/sf2000-screen.c
BUILDROOT_SCREEN_ENTRY := buildroot/sf2000-screen-entry.S
BUILDROOT_SCREEN_CFLAGS :=
BUILDROOT_SCREEN := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-screen
BUILDROOT_SCREEN_SOURCE_STAMP := $(BUILD_DIR)/.stamp-buildroot-screen-source
BUILDROOT_PANEL_PROBE_LINK := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-probe
BUILDROOT_PANEL_PROBE_TARGET ?= sf2000-screen
BUILDROOT_PANEL_INIT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-init
BUILDROOT_PANEL_INIT_ENTRY := buildroot/sf2000-panel-init-entry.S
BUILDROOT_PANEL_INIT_SRC := buildroot/sf2000-panel-launcher.S
BUILDROOT_PANEL_INIT_CFLAGS = $(BUILDROOT_HELPER_CFLAGS)
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
BUILDROOT_STORAGE_FASTPROBE_CFLAGS = $(BUILDROOT_HELPER_CFLAGS) -ffunction-sections -fdata-sections
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
	-mno-abicalls -fno-pic -fno-pie -mno-gpopt
BUILDROOT_FLAT_LDFLAGS := -Wl,-elf2flt=-r -static -no-pie
BUILDROOT_HELPER_STACK_SIZE := 65536
BUILDROOT_SCREEN_STACK_SIZE := $(BUILDROOT_HELPER_STACK_SIZE)
BUILDROOT_SUPERVISOR_STACK_SIZE := $(BUILDROOT_HELPER_STACK_SIZE)
BUILDROOT_SUPERVISOR_CFLAGS := $(BUILDROOT_HELPER_CFLAGS)
BUILDROOT_SUPERVISOR_LDFLAGS := -nostdlib -static -no-pie -Wl,-elf2flt=-r \
	-Wl,--section-start=.text=0 -Wl,-e,_start
BUILDROOT_INIT_SOURCE ?= $(INIT_BIN)
INIT_CFLAGS ?=
BUILDROOT_SCREEN_LDFLAGS := -nostartfiles -static -no-pie -Wl,-elf2flt=-r \
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
SF2000_BOOT_VISUAL ?= console
SF2000_BOOT_COLOR ?= 0x0000
SF2000_BOOT_HOLD_MS ?= 750
LINUX_CMDLINE ?= console=ttyS0,115200 earlycon init=/init \
	SF2000_BOOT_VISUAL=$(SF2000_BOOT_VISUAL) \
	SF2000_BOOT_COLOR=$(SF2000_BOOT_COLOR) \
	SF2000_BOOT_HOLD_MS=$(SF2000_BOOT_HOLD_MS)
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
SDCARD_CHECKSUMS := $(BUILD_DIR)/sdcard/SHA256SUMS
LINUX_ROM_SD_IMAGE := $(BUILD_DIR)/sf2000-linux$(ROOTFS_SUFFIX)-rom.sd.img
LINUX_ROM_SD_IMAGE_OFFSET := 1048576
BOOTROM_BUGFIX ?= /root/host-frogdev/universal/orig_firmware/UpdateFirmware/SF2000_XMC_XM25QH40B_4mbit_bugfix.bin
STOCK_ASD ?= /root/host-frogdev/universal/orig_firmware/bisrv_08_03.asd
QEMU_ORACLE_ARGS = QEMU_JOBS='$(JOBS)' FIRMWARE_BUGFIX='$(BOOTROM_BUGFIX)' ASD='$(STOCK_ASD)'
UNIFROG_DIR ?= $(abspath ../unifrog)
MUFROG_DIR ?= $(if $(wildcard $(abspath ../mufrog-commandc)),$(abspath ../mufrog-commandc),$(abspath ../mufrog))
UNIFROG_ASD ?= $(UNIFROG_DIR)/bisrv.asd
MUFROG_ASD ?= $(MUFROG_DIR)/bisrv.asd
UNIFROG_SD_ROOT ?= $(UNIFROG_DIR)/output/sdcard
MUFROG_SD_ROOT ?= $(MUFROG_DIR)/output/sdcard
UNIFROG_QEMU_SD := $(BUILD_DIR)/unifrog-qemu.sd.img
MUFROG_QEMU_SD := $(BUILD_DIR)/mufrog-qemu.sd.img
QEMU_BOOT_TIMEOUT ?= 90s
METRICS_LOG ?= /root/host-frogdev/universal/latest_log/sf2000_linux/loglinux0027.txt
PHYSICAL_CONTRACT_LOG ?= /root/host-frogdev/universal/latest_log/sf2000_linux/log92.txt
QEMU_CONTRACT_LOG ?= $(BUILD_DIR)/logs/linux-buildroot-display.log
QEMU_BENCH_SECONDS ?= 15
QEMU_DISPLAY_ARGS ?=
QEMU_FIDELITY_ARGS ?= -icount shift=1,sleep=on,align=on
GE_VENDOR_ARCHIVE ?= /root/host-frogdev/universal/temp/mufrog-commandc/unifrog-hcrtos-sdk/lib/vendor/libge.a
GE_REVERSE_DIR := $(BUILD_DIR)/reverse-ge
GE_NODE_TEST := $(BUILD_DIR)/hcge-node-test
GE_VENDOR_NODE_TEST := $(BUILD_DIR)/hcge-vendor-node-test
GE_LINUX_OBJ := $(BUILD_DIR)/hcge-linux.o
GE_VENDOR_CAPTURE := $(BUILD_DIR)/hcge-vendor-capture
GE_SOURCE_CAPTURE := $(BUILD_DIR)/hcge-source-capture
GE_VENDOR_CAPTURE_GOLDEN := ge/hcge_vendor_capture.golden
GE_SOURCE_CAPTURE_GOLDEN := ge/hcge_source_capture.golden
GE_ELF_CC := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-uclinux-uclibc-gcc.br_real
SMOKE_INIT_PATTERN ?= binfmt_flat: SF2000 NOMMU FLAT entry
LOADER_CFLAGS := -Os -ffreestanding -fno-builtin -nostdlib \
	-march=mips32 -mabi=32 -msoft-float -mno-abicalls -fno-pic -mno-gpopt -G 0 \
	-Wall -Wextra

.PHONY: all status qemu rootfs toolchain buildroot buildroot-reconfigure linux linux-reextract linux-reconfigure linux-asd linux-buildroot \
	linux-buildroot-asd sdcard-linux sdcard-buildroot linux-rom-sd \
	linux-buildroot-rom-sd run-linux smoke-linux run-linux-asd \
	smoke-linux-asd run-linux-rom smoke-linux-rom run-linux-buildroot-asd \
	smoke-linux-buildroot-asd run-linux-buildroot-storage \
	smoke-linux-buildroot-storage run-linux-buildroot-storage-fast \
	smoke-linux-buildroot-storage-fast run-linux-buildroot-storage-writeback \
smoke-linux-buildroot-storage-writeback run-linux-buildroot-storage-probe-writeback \
smoke-linux-buildroot-storage-probe-writeback run-linux-buildroot-storage-enumeration \
smoke-linux-buildroot-storage-enumeration smoke-linux-buildroot-persistent-storage \
run-linux-buildroot-rom \
run-linux-buildroot-storage-launch smoke-linux-buildroot-storage-launch \
run-qemu-stock-fatfs-writeback smoke-qemu-stock-fatfs-writeback \
	smoke-linux-buildroot-rom run-linux-buildroot-display \
	smoke-linux-buildroot-display \
	smoke-linux-buildroot-fb-test run-linux-buildroot-panel \
	smoke-linux-buildroot-panel run-linux-buildroot-panel-fast \
	smoke-linux-buildroot-panel-fast buildroot-panel-probe-link run-linux-input smoke-linux-input \
	run-linux-power smoke-linux-power \
	run-linux-frontend smoke-linux-frontend \
	run-linux-buildroot-input smoke-linux-buildroot-input \
	metrics-linux metrics-qemu-fidelity benchmark-qemu-linux \
	run-linux-buildroot-fidelity smoke-linux-buildroot-fidelity \
	smoke-linux-physical-contract metrics-qemu-timing \
	run-linux-reboot smoke-linux-reboot run-linux-buildroot-reboot \
	smoke-linux-buildroot-reboot run-linux-buildroot-reset-snapshot \
	run-linux-buildroot-audio smoke-linux-buildroot-audio \
	run-qemu-unifrog smoke-qemu-unifrog run-qemu-mufrog smoke-qemu-mufrog \
	run-qemu-unifrog-display smoke-qemu-unifrog-display \
	run-qemu-mufrog-display smoke-qemu-mufrog-display \
	smoke-linux-buildroot-reset-snapshot run-linux-buildroot-reset-restore \
	smoke-linux-buildroot-reset-restore reverse-ge test-ge-node \
	test-ge-node-vendor capture-ge-vendor test-ge-vendor-capture \
	test-ge-source-capture test-ge-formats test-ge-effects \
	test-ge-mask test-ge-custom-keys test-ge-utils test-ge-matrix \
	test-ge-queue test-ge-batch test-ge-filter-extract \
	test-ge-symbol-coverage clean

GE_BATCH_TEST := $(BUILD_DIR)/hcge-batch-test

GE_UTILS_TEST := $(BUILD_DIR)/hcge-utils-test
GE_VENDOR_UTILS_TEST := $(BUILD_DIR)/hcge-vendor-utils-test
GE_MATRIX_TEST := $(BUILD_DIR)/hcge-matrix-test
GE_VENDOR_MATRIX_TEST := $(BUILD_DIR)/hcge-vendor-matrix-test
GE_QUEUE_TEST := $(BUILD_DIR)/hcge-queue-test
GE_VENDOR_QUEUE_TEST := $(BUILD_DIR)/hcge-vendor-queue-test
GE_FILTER_TEST := $(BUILD_DIR)/hcge-filter-test
GE_VENDOR_FILTER_TEST := $(BUILD_DIR)/hcge-vendor-filter-test

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

reverse-ge:
	test -f '$(GE_VENDOR_ARCHIVE)'
	rm -rf '$(GE_REVERSE_DIR)'
	mkdir -p '$(GE_REVERSE_DIR)'
	cd '$(GE_REVERSE_DIR)' && ar x '$(GE_VENDOR_ARCHIVE)'
	for obj in '$(GE_REVERSE_DIR)'/*.o; do \
		'$(OBJDUMP_MIPS)' -dr "$$obj" > "$$obj.dis"; \
		'$(READELF_MIPS)' --debug-dump=info "$$obj" > "$$obj.dwarf"; \
		'$(NM_MIPS)' -C --defined-only "$$obj" > "$$obj.symbols"; \
	done

$(GE_NODE_TEST): ge/hcge_node.c ge/hcge_node.h ge/hcge_node_test.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -std=c99 -O2 -Wall -Wextra -Werror -Ige \
		-o '$@' ge/hcge_node.c ge/hcge_node_test.c

test-ge-node: $(GE_NODE_TEST)
	'$(GE_NODE_TEST)'

$(GE_UTILS_TEST): ge/hcge_utils.c ge/hcge_utils_test.c ge/ge_api.h
	mkdir -p '$(dir $@)'
	$(HOSTCC) -std=c99 -O2 -Wall -Wextra -Werror -Ige \
		-o '$@' ge/hcge_utils.c ge/hcge_utils_test.c

$(GE_VENDOR_UTILS_TEST): ge/hcge_utils_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_utils_test.c '$(GE_VENDOR_ARCHIVE)'

test-ge-utils: $(GE_UTILS_TEST) $(GE_VENDOR_UTILS_TEST)
	qemu-mipsel '$(GE_VENDOR_UTILS_TEST)' > '$(BUILD_DIR)/hcge-utils.vendor'
	'$(GE_UTILS_TEST)' | cmp - '$(BUILD_DIR)/hcge-utils.vendor'

$(GE_MATRIX_TEST): ge/hcge_matrix.c ge/hcge_matrix_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_matrix.c ge/hcge_matrix_test.c -lm

$(GE_VENDOR_MATRIX_TEST): ge/hcge_matrix_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_matrix_test.c '$(GE_VENDOR_ARCHIVE)' -lm

test-ge-matrix: $(GE_MATRIX_TEST) $(GE_VENDOR_MATRIX_TEST)
	qemu-mipsel '$(GE_VENDOR_MATRIX_TEST)' > '$(BUILD_DIR)/hcge-matrix.vendor'
	qemu-mipsel '$(GE_MATRIX_TEST)' | cmp - '$(BUILD_DIR)/hcge-matrix.vendor'

$(GE_QUEUE_TEST): ge/hcge_linux.c ge/hcge_node.c ge/hcge_queue_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -ffunction-sections \
		-Wl,--gc-sections -o '$@' ge/hcge_linux.c ge/hcge_node.c \
		ge/hcge_queue_test.c

$(GE_VENDOR_QUEUE_TEST): ge/hcge_queue_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_queue_test.c '$(GE_VENDOR_ARCHIVE)'

test-ge-queue: $(GE_QUEUE_TEST) $(GE_VENDOR_QUEUE_TEST)
	qemu-mipsel '$(GE_VENDOR_QUEUE_TEST)' > '$(BUILD_DIR)/hcge-queue.vendor'
	qemu-mipsel '$(GE_QUEUE_TEST)' | cmp - '$(BUILD_DIR)/hcge-queue.vendor'

$(GE_BATCH_TEST): ge/hcge_linux.c ge/hcge_node.c ge/hcge_batch_test.c \
		ge/ge_api.h $(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -ffunction-sections \
		-Wl,--gc-sections -Wl,--wrap=ioctl -o '$@' ge/hcge_linux.c \
		ge/hcge_node.c ge/hcge_batch_test.c

test-ge-batch: $(GE_BATCH_TEST)
	qemu-mipsel '$(GE_BATCH_TEST)' | grep -q \
		'^ret=0 calls=2 words=7 data=1,2,3,4,5,6,7$$'

$(GE_FILTER_TEST): ge/hcge_filter.c ge/hcge_filter_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_filter.c ge/hcge_filter_test.c -lm

$(GE_VENDOR_FILTER_TEST): ge/hcge_filter_test.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_filter_test.c '$(GE_VENDOR_ARCHIVE)' -lm

test-ge-filter-extract: $(GE_FILTER_TEST) $(GE_VENDOR_FILTER_TEST)
	qemu-mipsel '$(GE_VENDOR_FILTER_TEST)' > '$(BUILD_DIR)/hcge-filter.vendor'
	qemu-mipsel '$(GE_FILTER_TEST)' | cmp - '$(BUILD_DIR)/hcge-filter.vendor'

test-ge-symbol-coverage: reverse-ge
	rm -rf '$(BUILD_DIR)/ge-symbol-audit'
	mkdir -p '$(BUILD_DIR)/ge-symbol-audit'
	for src in ge/hcge_linux.c ge/hcge_utils.c ge/hcge_matrix.c \
		ge/hcge_filter.c ge/hcge_node.c; do \
		$(HOSTCC) -std=c99 -O2 -Ige -c "$$src" -o \
			"$(BUILD_DIR)/ge-symbol-audit/$$(basename "$$src" .c).o"; \
	done
	{ for symbols in '$(GE_REVERSE_DIR)'/*.symbols; do \
		awk '$$2 == "T" { print $$3 }' "$$symbols"; done; } | sort -u > \
		'$(BUILD_DIR)/ge-symbol-audit/vendor'
	nm -g --defined-only '$(BUILD_DIR)'/ge-symbol-audit/*.o | \
		awk '$$2 == "T" { print $$3 }' | sort -u > \
		'$(BUILD_DIR)/ge-symbol-audit/source'
	comm -23 '$(BUILD_DIR)/ge-symbol-audit/vendor' \
		'$(BUILD_DIR)/ge-symbol-audit/source' > \
		'$(BUILD_DIR)/ge-symbol-audit/missing'
	test ! -s '$(BUILD_DIR)/ge-symbol-audit/missing' || \
		{ printf 'missing vendor exports:\n'; cat '$(BUILD_DIR)/ge-symbol-audit/missing'; false; }

$(GE_VENDOR_NODE_TEST): ge/hcge_node.c ge/hcge_node.h \
		ge/hcge_vendor_compare.c $(BUILDROOT_TOOLCHAIN_STAMP) reverse-ge
	'$(GE_ELF_CC)' -std=c99 -O2 -ffunction-sections -fdata-sections \
		-Wl,--gc-sections -static -Ige -o '$@' ge/hcge_node.c \
		ge/hcge_vendor_compare.c '$(GE_REVERSE_DIR)'/hcge_node_ctx.c.o

test-ge-node-vendor: $(GE_VENDOR_NODE_TEST)
	qemu-mipsel '$(GE_VENDOR_NODE_TEST)'

$(GE_LINUX_OBJ): ge/hcge_linux.c ge/ge_api.h $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' -std=c99 -Os -Wall -Wextra -Werror -Ige -c -o '$@' '$<'

$(GE_VENDOR_CAPTURE): ge/hcge_vendor_capture.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -o '$@' '$<' \
		'$(GE_VENDOR_ARCHIVE)' -lm -Wl,--wrap=open -Wl,--wrap=close \
		-Wl,--wrap=ioctl -Wl,--wrap=mmap -Wl,--wrap=munmap \
		-Wl,--wrap=usleep

capture-ge-vendor: $(GE_VENDOR_CAPTURE)
	qemu-mipsel '$(GE_VENDOR_CAPTURE)'

test-ge-vendor-capture: $(GE_VENDOR_CAPTURE) $(GE_VENDOR_CAPTURE_GOLDEN)
	qemu-mipsel '$(GE_VENDOR_CAPTURE)' | cmp - '$(GE_VENDOR_CAPTURE_GOLDEN)'

$(GE_SOURCE_CAPTURE): ge/hcge_vendor_capture.c ge/hcge_linux.c ge/hcge_node.c ge/ge_api.h \
		$(BUILDROOT_TOOLCHAIN_STAMP)
	'$(GE_ELF_CC)' -std=c99 -O2 -static -Ige -DHCGE_SOURCE_CAPTURE \
		-o '$@' ge/hcge_vendor_capture.c ge/hcge_linux.c ge/hcge_node.c \
		-Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl \
		-Wl,--wrap=mmap -Wl,--wrap=munmap -Wl,--wrap=usleep

test-ge-source-capture: $(GE_SOURCE_CAPTURE) $(GE_SOURCE_CAPTURE_GOLDEN)
	qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 | \
		cmp - '$(GE_SOURCE_CAPTURE_GOLDEN)'

test-ge-formats: $(GE_VENDOR_CAPTURE) $(GE_SOURCE_CAPTURE)
	set -e; for format in 0 1 3 4 7; do \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			"$$format" > '$(BUILD_DIR)/.hcge-vendor-format'; \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			"$$format" | cmp - '$(BUILD_DIR)/.hcge-vendor-format'; \
	done; rm -f '$(BUILD_DIR)/.hcge-vendor-format'

test-ge-effects: $(GE_VENDOR_CAPTURE) $(GE_SOURCE_CAPTURE)
	set -e; for flags in 0 1 2 3 4 5 6 7 8 16 24 32 64 128 512 1024 \
		65536 103 119 1167 16777216 33554432; do \
		HCGE_CAPTURE_SRC_KEY=0xf81f HCGE_CAPTURE_DST_KEY=0x07e0 \
		HCGE_CAPTURE_SRC_BLEND=2 HCGE_CAPTURE_DST_BLEND=6 \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 "$$flags" 0 > '$(BUILD_DIR)/.hcge-vendor-effect'; \
		HCGE_CAPTURE_SRC_KEY=0xf81f HCGE_CAPTURE_DST_KEY=0x07e0 \
		HCGE_CAPTURE_SRC_BLEND=2 HCGE_CAPTURE_DST_BLEND=6 \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 "$$flags" 0 | cmp - '$(BUILD_DIR)/.hcge-vendor-effect'; \
	done; for format in 0 1 3 4 7; do \
		HCGE_CAPTURE_SRC_KEY=0x89abcdef HCGE_CAPTURE_DST_KEY=0x12345678 \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			"$$format" 24 0 > '$(BUILD_DIR)/.hcge-vendor-effect'; \
		HCGE_CAPTURE_SRC_KEY=0x89abcdef HCGE_CAPTURE_DST_KEY=0x12345678 \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			"$$format" 24 0 | cmp - '$(BUILD_DIR)/.hcge-vendor-effect'; \
	done; for flags in 0 1 2 3 4 7 8 16 32 63; do \
		HCGE_CAPTURE_COLOR=0x80123456 HCGE_CAPTURE_DST_KEY=0x07e0 \
		HCGE_CAPTURE_SRC_BLEND=2 HCGE_CAPTURE_DST_BLEND=6 \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 0 "$$flags" | sed -n '1p' > '$(BUILD_DIR)/.hcge-vendor-effect'; \
		HCGE_CAPTURE_COLOR=0x80123456 HCGE_CAPTURE_DST_KEY=0x07e0 \
		HCGE_CAPTURE_SRC_BLEND=2 HCGE_CAPTURE_DST_BLEND=6 \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 0 "$$flags" | sed -n '1p' | \
			cmp - '$(BUILD_DIR)/.hcge-vendor-effect'; \
	done; for flags in 0x1000 0x2000 0x4000; do \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 "$$flags" 0 | sed -n '2p' > '$(BUILD_DIR)/.hcge-vendor-effect'; \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 "$$flags" 0 | sed -n '2p' | \
			cmp - '$(BUILD_DIR)/.hcge-vendor-effect'; \
	done; rm -f '$(BUILD_DIR)/.hcge-vendor-effect'

test-ge-mask: $(GE_VENDOR_CAPTURE) $(GE_SOURCE_CAPTURE)
	set -e; for flags in 1048576 1048577 1048578 1048579 1048580 \
		1048608 1048640 1049088; do \
		HCGE_CAPTURE_SRC_BLEND=2 HCGE_CAPTURE_DST_BLEND=6 \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 "$$flags" 0 2>/dev/null | \
			grep -E '^(fill|blit|stretch)-' > '$(BUILD_DIR)/.hcge-vendor-mask'; \
		HCGE_CAPTURE_SRC_BLEND=2 HCGE_CAPTURE_DST_BLEND=6 \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 "$$flags" 0 2>/dev/null | \
			grep -E '^(fill|blit|stretch)-' | \
			cmp - '$(BUILD_DIR)/.hcge-vendor-mask'; \
	done; for setup in stencil padded; do \
		unset HCGE_CAPTURE_MASK_X HCGE_CAPTURE_MASK_Y HCGE_CAPTURE_MASK_FLAGS \
			HCGE_CAPTURE_MASK_WIDTH HCGE_CAPTURE_MASK_HEIGHT \
			HCGE_CAPTURE_MASK_PITCH; \
		if test "$$setup" = stencil; then \
			export HCGE_CAPTURE_MASK_X=7 HCGE_CAPTURE_MASK_Y=9 \
				HCGE_CAPTURE_MASK_FLAGS=1; \
		else \
			export HCGE_CAPTURE_MASK_WIDTH=100 HCGE_CAPTURE_MASK_HEIGHT=70 \
				HCGE_CAPTURE_MASK_PITCH=112; \
		fi; \
		qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 1048576 0 2>/dev/null | grep -E '^(fill|blit|stretch)-' \
			> '$(BUILD_DIR)/.hcge-vendor-mask'; \
		qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
			1 1048576 0 2>/dev/null | grep -E '^(fill|blit|stretch)-' | \
			cmp - '$(BUILD_DIR)/.hcge-vendor-mask'; \
	done; rm -f '$(BUILD_DIR)/.hcge-vendor-mask'

test-ge-custom-keys: $(GE_VENDOR_CAPTURE) $(GE_SOURCE_CAPTURE)
	set -e; for flags in 536870912 1073741824 1610612736; do \
		for operation in 0 1 2 3 4 5; do \
			HCGE_CAPTURE_SRC_KEY=0xf81f HCGE_CAPTURE_DST_KEY=0x07e0 \
			HCGE_CAPTURE_SRC_KEY_OP="$$operation" \
			HCGE_CAPTURE_DST_KEY_OP="$$operation" \
			qemu-mipsel '$(GE_VENDOR_CAPTURE)' 0 0 64 48 0 0 160 120 \
				1 "$$flags" 0 2>/dev/null | \
				grep -E '^(fill|blit|stretch)-' \
				> '$(BUILD_DIR)/.hcge-vendor-key'; \
			HCGE_CAPTURE_SRC_KEY=0xf81f HCGE_CAPTURE_DST_KEY=0x07e0 \
			HCGE_CAPTURE_SRC_KEY_OP="$$operation" \
			HCGE_CAPTURE_DST_KEY_OP="$$operation" \
			qemu-mipsel '$(GE_SOURCE_CAPTURE)' 0 0 64 48 0 0 160 120 \
				1 "$$flags" 0 2>/dev/null | \
				grep -E '^(fill|blit|stretch)-' | \
				cmp - '$(BUILD_DIR)/.hcge-vendor-key'; \
		done; \
	done; rm -f '$(BUILD_DIR)/.hcge-vendor-key'

qemu:
	$(MAKE) -C '$(QEMU_DIR)' build

$(QEMU_MKSD): $(QEMU_DIR)/tools/mksf2000sd.c
	$(MAKE) -C '$(QEMU_DIR)' build/mksf2000sd

rootfs: $(ROOTFS_CPIO)
toolchain: $(BUILDROOT_TOOLCHAIN_STAMP)
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
		-fno-pic -fno-pie -no-pie -mno-gpopt -G 0 -Wl,-Ttext=0 -Wl,-e,_start \
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


# The panel-probe targets deliberately override BUILDROOT_INIT_SOURCE to run
# sf2000-screen as /init.  A later normal build must restore the real init
# binary; without a content check, make sees the same destination and silently
# leaves the probe executable in the rootfs (and therefore in the ASD image).
$(BUILDROOT_INIT): FORCE $(BUILDROOT_INIT_SOURCE) Makefile
	mkdir -p '$(dir $@)'
	if ! cmp -s '$(BUILDROOT_INIT_SOURCE)' '$@' 2>/dev/null; then \
		cp '$(BUILDROOT_INIT_SOURCE)' '$@'; \
		chmod 0755 '$@'; \
	fi

$(BUILDROOT_SUPERVISOR): $(BUILDROOT_INIT_SRC) $(BUILDROOT_INIT_ENTRY) $(BUILDROOT_INIT_CLONE) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_SUPERVISOR_CFLAGS) $(BUILDROOT_SUPERVISOR_LDFLAGS) \
		-o '$@' '$(BUILDROOT_INIT_ENTRY)' '$(BUILDROOT_INIT_CLONE)' '$(BUILDROOT_INIT_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_SUPERVISOR_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_PAD): $(BUILDROOT_PAD_SRC) $(BUILDROOT_PAD_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_SCREEN_LDFLAGS) \
		-o '$@' '$(BUILDROOT_PAD_ENTRY)' '$(BUILDROOT_PAD_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_POWERD): $(BUILDROOT_POWERD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_FLAT_LDFLAGS) -o '$@' '$<'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_FRONTEND): $(shell find '$(FRONTEND_PROJECT)'/src '$(FRONTEND_PROJECT)'/include '$(FRONTEND_PROJECT)'/tests -type f 2>/dev/null) $(FRONTEND_PROJECT)/Makefile $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	$(MAKE) -C '$(FRONTEND_PROJECT)' frogui \
		CROSS_COMPILE='$(patsubst %gcc,%,$(BUILDROOT_CC))'
	mkdir -p '$(dir $@)'
	cp '$(FRONTEND_PROJECT)'/build/sf2000-frontend-frogui '$@'

$(BUILDROOT_AUDIO): $(BUILDROOT_AUDIO_SRC) $(BUILDROOT_AUDIO_ENTRY) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_SCREEN_LDFLAGS) \
		-o '$@' '$(BUILDROOT_AUDIO_ENTRY)' '$(BUILDROOT_AUDIO_SRC)'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_HEARTBEAT): $(BUILDROOT_HEARTBEAT_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_FLAT_LDFLAGS) -o '$@' '$<'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_LOGD): $(BUILDROOT_LOGD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_FLAT_LDFLAGS) -o '$@' '$<'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_MOUNT): $(BUILDROOT_MOUNT_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_FLAT_LDFLAGS) -o '$@' '$<'
	'$(BUILDROOT_FLTHDR)' -s '$(BUILDROOT_HELPER_STACK_SIZE)' '$@'
	rm -f '$@.gdb'

$(BUILDROOT_SCREEN_SOURCE_STAMP): FORCE $(BUILDROOT_SCREEN_SRC) \
		$(BUILDROOT_SCREEN_ENTRY) ge/hcge_linux.c ge/hcge_node.c \
		ge/hcge_node.h ge/ge_api.h Makefile
	mkdir -p '$(dir $@)'
	{ sha256sum '$(BUILDROOT_SCREEN_SRC)' '$(BUILDROOT_SCREEN_ENTRY)' \
		ge/hcge_linux.c ge/hcge_node.c ge/hcge_node.h ge/ge_api.h; \
		printf '%s\n' '$(BUILDROOT_HELPER_CFLAGS)' '$(BUILDROOT_SCREEN_CFLAGS)' \
			'$(BUILDROOT_SCREEN_LDFLAGS)' '$(BUILDROOT_SCREEN_STACK_SIZE)'; \
	} > '$@.tmp'
	if cmp -s '$@.tmp' '$@'; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

$(BUILDROOT_SCREEN): $(BUILDROOT_SCREEN_SOURCE_STAMP) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	SOURCE_DATE_EPOCH=0 '$(BUILDROOT_CC)' $(BUILDROOT_HELPER_CFLAGS) $(BUILDROOT_SCREEN_CFLAGS) \
		-Ige $(BUILDROOT_SCREEN_LDFLAGS) -o '$@' \
		'$(BUILDROOT_SCREEN_ENTRY)' '$(BUILDROOT_SCREEN_SRC)' \
		ge/hcge_linux.c ge/hcge_node.c
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

$(BUILDROOT_CPIO): $(BUILDROOT_TARGET_STAMP) $(BUILDROOT_INIT) $(BUILDROOT_SUPERVISOR) $(BUILDROOT_PAD) $(BUILDROOT_POWERD) $(BUILDROOT_FRONTEND) $(BUILDROOT_AUDIO) $(BUILDROOT_HEARTBEAT) $(BUILDROOT_LOGD) $(BUILDROOT_MOUNT) $(BUILDROOT_SCREEN) $(BUILDROOT_PANEL_INIT) $(BUILDROOT_PANEL_FASTPROBE) $(BUILDROOT_STORAGE_PROBE) $(BUILDROOT_STORAGE_FASTPROBE) $(BUILDROOT_RESET_FASTPROBE) $(BUILDROOT_OVERLAY_FILES) $(BUILDROOT_DEVICE_TABLE) $(GEN_INIT_CPIO)
	mkdir -p '$(dir $@)'
	rm -rf '$(BUILDROOT_REPACK_DIR)'
	mkdir -p '$(BUILDROOT_REPACK_DIR)'
	rsync -a --delete --exclude=/THIS_IS_NOT_YOUR_ROOT_FILESYSTEM \
		'$(BUILDROOT_OUT)'/target/ '$(BUILDROOT_REPACK_DIR)'/
	rsync -a '$(BUILDROOT_OVERLAY)'/ '$(BUILDROOT_REPACK_DIR)'/
	rsync -a '$(BUILDROOT_GENERATED_OVERLAY)'/ '$(BUILDROOT_REPACK_DIR)'/
	rm -rf '$(BUILDROOT_REPACK_DIR)'/run/* '$(BUILDROOT_REPACK_DIR)'/tmp/*
	mkdir -p '$(BUILDROOT_REPACK_DIR)'/dev/input '$(BUILDROOT_REPACK_DIR)'/dev/snd \
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
		printf 'nod /dev/ge 0660 0 0 c 10 243\n'; \
		printf 'nod /dev/fb0 0660 0 0 c 29 0\n'; \
		printf 'nod /dev/snd/pcmC0D0p 0660 0 0 c 116 16\n'; \
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
		--enable DEBUG_INFO_NONE \
		--disable DEBUG_INFO_REDUCED \
		--disable DEBUG_KERNEL \
		--disable DEBUG_MISC \
		--disable DEBUG_FS \
		--disable BLK_DEBUG_FS \
		--disable IKCONFIG \
		--disable IKCONFIG_PROC \
		--disable KALLSYMS \
		--disable KALLSYMS_ALL \
		--disable KALLSYMS_BASE_RELATIVE \
		--enable INITRAMFS_COMPRESSION_GZIP \
		--disable INITRAMFS_COMPRESSION_NONE \
		--disable INITRAMFS_COMPRESSION_LZ4 \
		--enable RD_GZIP \
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
		--disable EFI_PARTITION \
		--disable MQ_IOSCHED_DEADLINE \
		--disable MQ_IOSCHED_KYBER \
		--disable IOSCHED_BFQ \
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
		--disable INPUT_KEYBOARD \
		--disable INPUT_MOUSE \
		--disable SERIO \
		--enable INPUT_MISC \
		--enable INPUT_UINPUT \
		--disable HID \
		--disable HID_SUPPORT \
		--disable HIDRAW \
		--disable USB_SUPPORT \
		--disable USB_COMMON \
		--disable USB \
		--disable USB_ANNOUNCE_NEW_DEVICES \
		--disable USB_DEFAULT_PERSIST \
		--disable USB_XHCI_HCD \
		--disable USB_EHCI_HCD \
		--disable USB_EHCI_HCD_PLATFORM \
		--disable USB_OHCI_HCD \
		--disable USB_OHCI_HCD_PLATFORM \
		--disable USB_DWC2 \
		--disable USB_MUSB_HDRC \
		--disable USB_MUSB_HOST \
		--disable USB_MUSB_SF2000 \
		--disable NOP_USB_XCEIV \
		--disable MUSB_PIO_ONLY \
		--disable USB_STORAGE \
		--disable HID_GENERIC \
		--disable USB_HID \
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
		--disable PM \
		--disable SUSPEND \
		--disable FW_LOADER \
		--disable MAGIC_SYSRQ \
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
		--disable SCSI \
		--disable SCSI_MOD \
		--disable BLK_DEV_SD \
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
		--enable SOUND \
		--enable SND \
		--enable SND_PCM \
		--enable SND_DRIVERS \
		--enable SND_SF2000 \
		--enable SF2000_GE \
		--disable SF2000_PANEL_SYNC \
		--disable SND_SEQUENCER \
		--disable SND_MIXER_OSS \
		--disable SND_PCM_OSS \
		--disable SND_HDA \
		--disable SND_USB_AUDIO \
		--disable MEDIA_SUPPORT \
		--disable DRM \
		--disable GOLDFISH \
		--disable GOLDFISH_PIPE \
		--disable GOLDFISH_TTY \
		--disable FB_GOLDFISH \
		--disable KEYBOARD_GOLDFISH_EVENTS \
		--disable MMC_LITEX \
		--disable LITEX \
		--disable LITEX_SOC_CONTROLLER \
		--disable COMMON_CLK_PISTACHIO \
		--disable CLKSRC_PISTACHIO \
		--disable RESET_PISTACHIO \
		--disable PHY_PISTACHIO_USB \
		--disable REGULATOR \
		--disable GENERIC_PHY \
		--enable COMMON_CLK \
		--disable DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT \
		--disable DEBUG_INFO_DWARF4 \
		--disable DEBUG_INFO_DWARF5 \
		--enable DEBUG_INFO_NONE \
		--enable FB \
		--enable FB_PROVIDE_GET_FB_UNMAPPED_AREA \
		--enable FB_SIMPLE
	$(MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' olddefconfig
	touch '$@'

$(LINUX_VMLINUX): $(LINUX_SRC)/.patched $(LINUX_CONFIG_STAMP) $(ROOTFS_CPIO)
	$(MAKE) -j'$(JOBS)' -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' vmlinux
	# The loader consumes only allocated ELF sections.  Keeping DWARF in the
	# embedded image wastes most of RAM and makes relocation overlap needlessly
	# large; the kernel build's vmlinux.unstripped remains available for symbols.
	'$(STRIP_MIPS)' --strip-debug '$(LINUX_VMLINUX)'

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
		printf '.section .payload, "a"\n'; \
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
	cp '$<' '$@.tmp'
	cmp '$<' '$@.tmp'
	'$(ASDPACK)' --check '$@.tmp'
	mv '$@.tmp' '$@'

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
		printf 'Display handoff:\n'; \
		printf '  Normal boot emits one loader health blink, then keeps the backlight dark.\n'; \
		printf '  The display service enables it only after a complete controlled frame is in panel GRAM.\n'; \
		printf '  Boot visual: %s, RGB565 color: %s, hold: %s ms.\n\n' \
			'$(SF2000_BOOT_VISUAL)' '$(SF2000_BOOT_COLOR)' '$(SF2000_BOOT_HOLD_MS)'; \
		printf 'Runtime controls:\n'; \
		printf '  Press START+SELECT for a logger flush, sync, unmount, and clean restart.\n'; \
		printf '  A bounded watchdog restart is used only if the orderly path stalls.\n'; \
	} > '$@'

$(SDCARD_LOG_TXT): Makefile
	mkdir -p '$(dir $@)'
	dd if=/dev/zero of='$@' bs=262144 count=1 >/dev/null 2>&1

$(SDCARD_CHECKSUMS): $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN)
	cd '$(BUILD_DIR)/sdcard' && sha256sum bios/bisrv.asd \
		firmware/linux.asd firmware/unifrog.bin > SHA256SUMS

$(LINUX_ROM_SD_IMAGE): $(LINUX_ASD) $(QEMU_MKSD)
	'$(QEMU_MKSD)' '$(LINUX_ASD)' '$@' fat32

linux-asd: $(LINUX_ASD) $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_CHECKSUMS)
	cmp '$(LINUX_ASD)' '$(SDCARD_LINUX_ASD)'
	cmp '$(LINUX_ASD)' '$(SDCARD_BIOS_ASD)'
	cd '$(BUILD_DIR)/sdcard' && sha256sum -c SHA256SUMS

linux-buildroot:
	$(MAKE) ROOTFS=buildroot linux

linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot linux-asd

sdcard-linux: $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_BOOT_OPTIONS) $(SDCARD_LOG_TXT) \
		$(SDCARD_CHECKSUMS)

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
	(sleep 5; printf 'sendkey right 1000\n'; sleep 1; \
		printf 'sendkey x 1000\n'; sleep 2; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(LINUX_ASD)' \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-input.log \
		> '$(BUILD_DIR)'/logs/linux-input.console 2>&1

smoke-linux-input: run-linux-input
	grep -q 'sf2000-pad: userspace input bridge ready' '$(BUILD_DIR)'/logs/linux-input.log
	grep -q 'sf2000-pad: state=.*RIGHT' '$(BUILD_DIR)'/logs/linux-input.log
	grep -q 'sf2000-pad: state=.*A' '$(BUILD_DIR)'/logs/linux-input.log

run-linux-power: qemu linux-buildroot-asd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey ret-a 500\n'; sleep 4; \
		printf 'sendkey x 2500\n'; sleep 4; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-power.log \
		> '$(BUILD_DIR)'/logs/linux-power.console 2>&1

smoke-linux-power: run-linux-power
	grep -q 'screen-ready-done' '$(BUILD_DIR)'/logs/linux-power.log
	grep -q 'sf2000-powerd: display standby entering' '$(BUILD_DIR)'/logs/linux-power.log
	grep -q 'sf2000-powerd: display standby resumed' '$(BUILD_DIR)'/logs/linux-power.log
	! grep -Eq 'reloc outside program|Kernel panic' '$(BUILD_DIR)'/logs/linux-power.log

run-linux-frontend: qemu linux-buildroot-asd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey ret-w 500\n'; sleep 4; \
		printf 'sendkey ret-q 500\n'; sleep 3; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-frontend.log \
		> '$(BUILD_DIR)'/logs/linux-frontend.console 2>&1

smoke-linux-frontend: run-linux-frontend
	grep -q 'screen-ready-done' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-powerd: frontend launch START+R' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: frontend running START+L exits' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: first frame 320x240 pitch=640 format=2' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-powerd: frontend first frame visible' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-powerd: frontend returned to console' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Eq 'sf2000-powerd: discarded [1-9][0-9]* stale frontend input events' '$(BUILD_DIR)'/logs/linux-frontend.log
	! grep -Eq 'reloc outside program|Kernel panic' '$(BUILD_DIR)'/logs/linux-frontend.log

run-linux-reboot: qemu linux-rom-sd
	test -f '$(BOOTROM_BUGFIX)'
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 10; printf 'sendkey backspace 3000\n'; sleep 8; \
		printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_ROM_CPU_ARGS) -bios '$(BOOTROM_BUGFIX)' \
		-drive if=none,id=sd0,file='$(LINUX_ROM_SD_IMAGE)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-reboot.log \
		> '$(BUILD_DIR)'/logs/linux-reboot.console 2>&1

smoke-linux-reboot: run-linux-reboot
	grep -q 'sf2000_linux: flat init alive' '$(BUILD_DIR)'/logs/linux-reboot.log
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
	grep -q 'sf2000_buildroot: userspace alive' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000-ge .*HC15xx GE queue at' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000_buildroot: graphics engine ready /dev/ge' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'new USB bus registered' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-asd.log

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
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback $(QEMU_ORACLE_ARGS)

smoke-linux-buildroot-storage-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback $(QEMU_ORACLE_ARGS)

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

smoke-linux-buildroot-persistent-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-persistent-storage.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-loglinux.XXXXXX.txt); \
	tmp_test=$$(mktemp '$(BUILD_DIR)'/sf2000-storage-test.XXXXXX.bin); \
	trap 'rm -f $$tmp_sd $$tmp_log $$tmp_test' EXIT; \
	truncate -s 64M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	$(MAKE) ROOTFS=buildroot QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	mcopy -i "$$tmp_sd" ::loglinux.txt "$$tmp_log"; \
	mcopy -i "$$tmp_sd" ::sf2000-storage-test.bin "$$tmp_test"; \
	grep -q 'source=storage storage-test=pass bytes=262144 hash=ec55efc5' "$$tmp_log"; \
	grep -q 'source=kmsg .*sf2000-mount: mount ok' "$$tmp_log"; \
	grep -q 'source=kmsg ' "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux pre-mount profile begin ---' "$$tmp_log"; \
	grep -q 'source=proc-stat ' "$$tmp_log"; \
	grep -q 'source=proc-meminfo ' "$$tmp_log"; \
	grep -q 'source=proc-interrupts ' "$$tmp_log"; \
	grep -q 'source=heartbeat alive' "$$tmp_log"; \
	test "$$(wc -c < "$$tmp_test")" -eq 262144; \
	echo '61825158a601440496406cded7107a985e4201366c379dccb78f8b8a67398ec4  '"$$tmp_test" | sha256sum -c -; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-buildroot-storage-launch:
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-buildroot-storage-fast

smoke-linux-buildroot-storage-launch: run-linux-buildroot-storage-launch
	grep -q 'Run /usr/sbin/sf2000-storage-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-asd.log

run-qemu-stock-fatfs-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback $(QEMU_ORACLE_ARGS)

smoke-qemu-stock-fatfs-writeback: run-qemu-stock-fatfs-writeback

run-qemu-board-contract:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-board-contract $(QEMU_ORACLE_ARGS)

smoke-qemu-board-contract: run-qemu-board-contract

run-qemu-display:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-display $(QEMU_ORACLE_ARGS) && \
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-gb300-display $(QEMU_ORACLE_ARGS)

smoke-qemu-display: run-qemu-display

run-linux-buildroot-rom:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' run-linux-rom

smoke-linux-buildroot-rom:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' smoke-linux-rom
	grep -q 'sf2000: uart: .*sf2000: early watchdog armed' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000_buildroot: early watchdog disabled' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'name=screen-after-gma-desc' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-rom.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-rom.log

run-linux-buildroot-display: qemu
	$(MAKE) ROOTFS=buildroot linux-asd
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/screenshots/linux-buildroot-gma
	rm -f '$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-*.ppm \
		'$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-latest.ppm
	SF2000_TRACE_GMA=1 \
	SF2000_GMA_DUMP_DIR='$(BUILD_DIR)'/screenshots/linux-buildroot-gma \
	SF2000_GMA_DUMP_LIMIT=8 \
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) $(QEMU_DISPLAY_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-buildroot-display.log \
		> '$(BUILD_DIR)'/logs/linux-buildroot-display.console 2>&1 || test $$? -eq 124

smoke-linux-buildroot-display: run-linux-buildroot-display
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-buildroot-display.console
	grep -q 'sf2000: VOU RGB compositor latch complete' '$(BUILD_DIR)'/logs/linux-buildroot-display.console
	grep -q 'simple-framebuffer .*fb0: simplefb registered' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'sf2000_buildroot: framebuffer ready /dev/fb0' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'sf2000-screen: /dev/fb0 RGB565 write ready' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-ge-console-fill-ok' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'sf2000: reserved diag memory gma=0xf00000+0x100000' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-after-backlight' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-after-gma-desc' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-loop-present-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-cached-render-ready' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00f00000 name=screen-gma-present-desc' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00f00280 name=screen-gma-present-desc' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-ge-scanout-init-begin' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-ge-scanout-clear-ok' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-ge-scanout-init-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-ge-scanout-init-fail' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-vou-latch-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-rgb-prime-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-rgb-prime2-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-rgb-engine-ready' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-ge-mcu-visible' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-gma-probe-begin' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00137002 name=screen-rgb-vou-connect-ctrl' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000015 name=screen-rgb-vou-connect-mode' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'VOU raster disconnected from PRGB' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00040001 name=screen-raster-ctl-hw' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00f00280 name=screen-raster-expected' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00f00000 name=screen-raster-expected' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	test "$$(grep -c 'name=screen-raster-wait-ok' '$(BUILD_DIR)'/logs/linux-buildroot-display.log)" -ge 2
	! grep -q 'name=screen-raster-wait-fail' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-rgb-handoff-abort' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-post-gma-dmba-hw' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000004 name=screen-native-hold-begin' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000004 name=screen-native-hold-count' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000004 name=screen-native-hold-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-native-present' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-native-hold-ms' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x01300378 name=screen-native-hold-vou' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00040001 name=screen-native-hold-gma' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-native-present-fail' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-te-conditioning-done' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-te-stream-start' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-te-rearm-edge' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	gate1="$$(sed -n 's/.*value=\(0x[0-9a-fA-F]*\) name=screen-post-gate1.*/\1/p' '$(BUILD_DIR)'/logs/linux-buildroot-display.log | tail -n 1)"; \
		test $$((gate1 & 0x600)) -eq 1536
	grep -q 'name=screen-panel-id' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=screen-panel-aux' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-probe-restore-present' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'name=screen-native-hold-fail' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000000 name=screen-rgb-source' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x01300378 name=screen-vou-total' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x028e000a name=screen-vou-hactive' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x011e002e name=screen-vou-vactive' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00060600 name=screen-rgb-vsync' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0xb6060606 name=screen-rgb-pad-clock' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000029 name=screen-panel-command-final' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'panel-cmd cmd=0x2c' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'panel-data cmd=0x36 index=0 value=0x0070' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'GMA doorbell before VOU RGB latch' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'GMA scanout with panel VSYNC disconnected' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'GMA scanout with panel pixel clock disconnected' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'GMA scanout while panel remains in MCU RAMWR state' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'GMA scanout while panel RAMCTRL remains MCU-owned' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'panel entered RGB mode before VOU/GMA raster was active' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'assert(common.c' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	! grep -q 'Invalid argument\|No such device' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	test -s '$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-latest.ppm

metrics-linux:
	@awk '\
	function ktime(   p,n,a) { \
		p = index($$0, "source=kmsg "); \
		if (!p) return -1; \
		n = split(substr($$0, p + 12), a, ","); \
		return n >= 3 ? a[3] + 0 : -1; \
	} \
	/source=kmsg .*sf2000_buildroot: starting screen/ { screen_start = ktime() } \
	/source=kmsg .*sf2000-screen: main entry/ && !screen_main { screen_main = ktime() } \
	/source=kmsg .*guarded panel init begin/ { panel_begin = ktime() } \
	/source=kmsg .*guarded panel init done/ { panel_done = ktime() } \
	/source=kmsg .*sf2000_buildroot: screen ready/ { screen_ready = ktime() } \
	/source=logd --- SF2000 Linux storage mounted ---/ { mount_us = $$2; sub("mono_us=", "", mount_us) } \
	/source=storage storage-test=pass/ { storage_us = $$2; sub("mono_us=", "", storage_us) } \
	/source=proc-stat cpu  / { \
		p = index($$0, "source=proc-stat cpu  "); \
		n = split(substr($$0, p + 22), c, " "); total = idle = field = 0; \
		for (i = 1; i <= n; i++) if (c[i] != "") { total += c[i]; if (++field == 4) idle = c[i] } \
		if (!stat_seen++) { total0 = total; idle0 = idle } \
		total1 = total; idle1 = idle; \
	} \
	/name=screen-panel-init-us/ { for (i = 1; i <= NF; i++) if ($$i ~ /^value=/) { panel_metric = $$i; sub(/^value=/, "", panel_metric) } } \
	/name=screen-panel-push-us/ { for (i = 1; i <= NF; i++) if ($$i ~ /^value=/) { push_metric = $$i; sub(/^value=/, "", push_metric) } } \
	/name=screen-rgb-ready-us/ { for (i = 1; i <= NF; i++) if ($$i ~ /^value=/) { rgb_metric = $$i; sub(/^value=/, "", rgb_metric) } } \
	/name=screen-service-ready-us/ { for (i = 1; i <= NF; i++) if ($$i ~ /^value=/) { ready_metric = $$i; sub(/^value=/, "", ready_metric) } } \
	END { \
		printf "linux.screen_exec_to_main_ms=%.3f\n", (screen_main-screen_start)/1000; \
		printf "linux.panel_init_ms=%.3f\n", (panel_done-panel_begin)/1000; \
		printf "linux.screen_start_to_ready_ms=%.3f\n", (screen_ready-screen_start)/1000; \
		printf "linux.storage_256k_write_verify_ms=%.3f\n", (storage_us-mount_us)/1000; \
		if (total1 > total0) printf "linux.sampled_cpu_busy_pct=%.2f\n", 100*(1-(idle1-idle0)/(total1-total0)); \
		if (panel_metric != "") printf "linux.instrumented_panel_init_us=%s\n", panel_metric; \
		if (push_metric != "") printf "linux.instrumented_panel_push_us=%s\n", push_metric; \
		if (rgb_metric != "") printf "linux.instrumented_rgb_ready_us=%s\n", rgb_metric; \
		if (ready_metric != "") printf "linux.instrumented_service_ready_us=%s\n", ready_metric; \
	}' '$(METRICS_LOG)'

benchmark-qemu-linux: qemu linux-buildroot-asd
	mkdir -p '$(BUILD_DIR)'/metrics
	/usr/bin/time -f 'qemu.wall_s=%e\nqemu.user_s=%U\nqemu.sys_s=%S\nqemu.host_cpu_pct=%P\nqemu.max_rss_kb=%M' \
		-o '$(BUILD_DIR)'/metrics/qemu-linux.txt \
		timeout '$(QEMU_BENCH_SECONDS)s' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-display none -serial none -monitor none \
		-D '$(BUILD_DIR)'/metrics/qemu-linux.log \
		>/dev/null 2>&1 || test $$? -eq 124
	cat '$(BUILD_DIR)'/metrics/qemu-linux.txt

metrics-qemu-fidelity:
	@awk '\
	BEGIN { \
		want["screen-panel-id"]; want["screen-panel-aux"]; \
		want["screen-rgb-source"]; want["screen-vou-total"]; \
		want["screen-vou-hactive"]; want["screen-vou-vactive"]; \
		want["screen-rgb-vsync"]; want["screen-rgb-pad-clock"]; \
		want["screen-rgb-vou-connect-ctrl"]; \
		want["screen-rgb-vou-connect-mode"]; \
		want["screen-panel-command-final"]; want["screen-native-hold-count"]; \
		want["screen-native-hold-gma"]; want["screen-native-hold-vou"]; \
		want["screen-te-conditioning-done"]; \
	} \
	FNR == 1 { source++ } \
	{ \
		name = value = ""; \
		for (i = 1; i <= NF; i++) { \
			if ($$i ~ /^name=/) { name = $$i; sub(/^name=/, "", name) } \
			if ($$i ~ /^value=/) { value = $$i; sub(/^value=/, "", value) } \
		} \
		if (name in want && value != "") { if (source == 1) physical[name] = value; else qemu[name] = value } \
	} \
	END { \
		for (name in want) { \
			compared++; \
			if (physical[name] != "" && physical[name] == qemu[name]) matched++; \
			else printf "fidelity.mismatch.%s=physical:%s,qemu:%s\n", name, physical[name], qemu[name]; \
		} \
		printf "fidelity.contract_matched=%u\n", matched; \
		printf "fidelity.contract_compared=%u\n", compared; \
		printf "fidelity.contract_pct=%.2f\n", 100 * matched / compared; \
	}' '$(PHYSICAL_CONTRACT_LOG)' '$(QEMU_CONTRACT_LOG)'

run-linux-buildroot-fidelity:
	$(MAKE) QEMU_DISPLAY_ARGS='$(QEMU_FIDELITY_ARGS)' \
		QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' run-linux-buildroot-display

smoke-linux-physical-contract: linux-buildroot-asd
	grep -A1 'if (!IS_ENABLED(CONFIG_MIPS_SF2000))' \
		'$(LINUX_SRC)/arch/mips/kernel/traps.c' | \
		grep -q 'check_wait()'

metrics-qemu-timing:
	@awk '\
	function physical_time(   p,n,a) { \
		p = index($$0, "source=kmsg "); if (!p) return -1; \
		n = split(substr($$0, p + 12), a, ","); return n >= 3 ? a[3] / 1000000 : -1; \
	} \
	function qemu_time(   s) { \
		if (match($$0, /\[[ ]*[0-9]+\.[0-9]+\]/)) { s = substr($$0, RSTART + 1, RLENGTH - 2); return s + 0 } \
		return -1; \
	} \
	FNR == 1 { source++ } \
	/Calibrating delay loop/ { \
		if (match($$0, /[0-9]+\.[0-9]+ BogoMIPS/)) { v = substr($$0, RSTART, RLENGTH); sub(/ BogoMIPS/, "", v); bogo[source] = v + 0 } \
	} \
	/sf2000_buildroot: starting screen/ { start[source] = source == 1 ? physical_time() : qemu_time() } \
	/sf2000-screen: main entry/ && !main[source] { main[source] = source == 1 ? physical_time() : qemu_time() } \
	/guarded panel init begin/ { panel0[source] = source == 1 ? physical_time() : qemu_time() } \
	/guarded panel init done/ { panel1[source] = source == 1 ? physical_time() : qemu_time() } \
	/sf2000_buildroot: screen ready/ { ready[source] = source == 1 ? physical_time() : qemu_time() } \
	END { \
		printf "timing.physical_bogomips=%.2f\n", bogo[1]; printf "timing.qemu_bogomips=%.2f\n", bogo[2]; \
		printf "timing.bogomips_ratio=%.3f\n", bogo[2]/bogo[1]; \
		printf "timing.physical_exec_ms=%.3f\n", 1000*(main[1]-start[1]); \
		printf "timing.qemu_exec_ms=%.3f\n", 1000*(main[2]-start[2]); \
		printf "timing.physical_panel_ms=%.3f\n", 1000*(panel1[1]-panel0[1]); \
		printf "timing.qemu_panel_ms=%.3f\n", 1000*(panel1[2]-panel0[2]); \
		printf "timing.physical_ready_ms=%.3f\n", 1000*(ready[1]-start[1]); \
		printf "timing.qemu_ready_ms=%.3f\n", 1000*(ready[2]-start[2]); \
		if (bogo[2]/bogo[1] < .75 || bogo[2]/bogo[1] > 1.25) exit 1; \
		if ((panel1[2]-panel0[2])/(panel1[1]-panel0[1]) < .75 || (panel1[2]-panel0[2])/(panel1[1]-panel0[1]) > 1.25) exit 1; \
	}' '$(METRICS_LOG)' '$(QEMU_CONTRACT_LOG)'

smoke-linux-buildroot-fidelity:
	$(MAKE) QEMU_DISPLAY_ARGS='$(QEMU_FIDELITY_ARGS)' \
		QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' smoke-linux-buildroot-display
	$(MAKE) smoke-linux-physical-contract
	$(MAKE) metrics-qemu-fidelity
	$(MAKE) metrics-qemu-timing

smoke-linux-buildroot-fb-test:
	$(MAKE) ROOTFS=buildroot QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon init=/init SF2000_FB_TEST=1' \
		run-linux-buildroot-display
	grep -q 'sf2000_buildroot: stopping screen for framebuffer test' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'name=init-screen-stop-wait' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'sf2000_buildroot: exec /usr/bin/fb-test -p 0' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'fb-test 1.1.1' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'fb res 320x240 virtual 320x240, line_len 640, bpp 16' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'sf2000_buildroot: framebuffer test complete' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'value=0x00000000 name=init-fb-test-exit' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	grep -q 'gma-present .*mode=6' '$(BUILD_DIR)'/logs/linux-buildroot-display.log
	test -s '$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-latest.ppm
	pixel() { \
		dd if='$(BUILD_DIR)'/screenshots/linux-buildroot-gma/sf2000-gma-latest.ppm \
			bs=1 skip=$$((15 + 3 * ($$2 * 320 + $$1))) count=3 2>/dev/null | \
			od -An -tx1 | tr -d ' \n'; \
	}; \
	test "$$(pixel 0 0)" = ffffff; \
	test "$$(pixel 100 0)" = 00ff00; \
	test "$$(pixel 0 100)" = 0000ff; \
	test "$$(pixel 319 100)" = ff0000; \
	test "$$(pixel 100 239)" = ffff00
run-linux-buildroot-audio: qemu
	$(MAKE) ROOTFS=buildroot \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1' linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	rm -f '$(BUILD_DIR)'/sf2000-audio.wav
	(sleep 6; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000,audiodev=sf2000wav $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
		-append 'console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1' \
		-audiodev wav,id=sf2000wav,path='$(BUILD_DIR)'/sf2000-audio.wav \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-buildroot-audio.log \
		> '$(BUILD_DIR)'/logs/linux-buildroot-audio.console 2>&1

smoke-linux-buildroot-audio: run-linux-buildroot-audio
	grep -q 'sf2000-pcm .*PCM playback ready' '$(BUILD_DIR)'/logs/linux-buildroot-audio.log
	grep -q 'sf2000-audio: ALSA PCM DMA tone active' '$(BUILD_DIR)'/logs/linux-buildroot-audio.log
	grep -q 'sf2000: audio guest DMA active' '$(BUILD_DIR)'/logs/linux-buildroot-audio.console
	test -s '$(BUILD_DIR)'/sf2000-audio.wav

$(UNIFROG_QEMU_SD): $(UNIFROG_ASD) $(UNIFROG_SD_ROOT) Makefile
	test -d '$(UNIFROG_SD_ROOT)'
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n UNIFROG '$@' >/dev/null
	mcopy -i '$@' -s '$(UNIFROG_SD_ROOT)'/* ::/

$(MUFROG_QEMU_SD): $(MUFROG_ASD) $(MUFROG_SD_ROOT) Makefile
	test -d '$(MUFROG_SD_ROOT)'
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n MUFROG '$@' >/dev/null
	mcopy -i '$@' -s '$(MUFROG_SD_ROOT)'/* ::/
	# MuFrog deliberately rejects a package-only template as a user SD card.
	# A normal card has at least one user directory; keep the fixture empty but
	# representative so this smoke test exercises the mounted block device.
	mmd -i '$@' ::/ROMS

run-qemu-unifrog: qemu $(UNIFROG_QEMU_SD)
	test -f '$(UNIFROG_ASD)'
	mkdir -p '$(BUILD_DIR)'/logs
	timeout 30s '$(QEMU_BIN)' -M sf2000 -kernel '$(UNIFROG_ASD)' \
		-drive if=none,id=sd0,file='$(UNIFROG_QEMU_SD)',format=raw \
		-display none -serial none -monitor none -d guest_errors,unimp \
		-D '$(BUILD_DIR)'/logs/qemu-unifrog.log \
		> '$(BUILD_DIR)'/logs/qemu-unifrog.console 2>&1 || test $$? -eq 124

smoke-qemu-unifrog: run-qemu-unifrog
	grep -q 'name=unifrog.storage.done .*arg2=0x00000000' '$(BUILD_DIR)'/logs/qemu-unifrog.log
	grep -q 'name=unifrog.js.begin' '$(BUILD_DIR)'/logs/qemu-unifrog.log
	grep -q 'name=unifrog.boot_logo.done' '$(BUILD_DIR)'/logs/qemu-unifrog.log

run-qemu-unifrog-display: qemu $(UNIFROG_QEMU_SD)
	test -f '$(UNIFROG_ASD)'
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/screenshots
	rm -f '$(BUILD_DIR)'/screenshots/qemu-unifrog.ppm \
		'$(BUILD_DIR)'/screenshots/qemu-unifrog-after-input.ppm
	(sleep 8; printf 'screendump $(BUILD_DIR)/screenshots/qemu-unifrog.ppm\nsendkey down 1000\n'; \
		sleep 2; printf 'screendump $(BUILD_DIR)/screenshots/qemu-unifrog-after-input.ppm\nquit\n') | \
		'$(QEMU_BIN)' -M sf2000 -kernel '$(UNIFROG_ASD)' \
		-drive if=none,id=sd0,file='$(UNIFROG_QEMU_SD)',format=raw \
		-display none -serial none -monitor stdio -d guest_errors,unimp \
		-D '$(BUILD_DIR)'/logs/qemu-unifrog-display.log \
		> '$(BUILD_DIR)'/logs/qemu-unifrog-display.console 2>&1

smoke-qemu-unifrog-display: run-qemu-unifrog-display
	grep -q 'name=unifrog.storage.done .*arg2=0x00000000' '$(BUILD_DIR)'/logs/qemu-unifrog-display.log
	grep -q 'name=unifrog.js.begin' '$(BUILD_DIR)'/logs/qemu-unifrog-display.log
	! grep -q 'GMA scanout with panel sync/DE disconnected' '$(BUILD_DIR)'/logs/qemu-unifrog-display.log
	! grep -q 'VOU raster disconnected from PRGB' '$(BUILD_DIR)'/logs/qemu-unifrog-display.log
	test -s '$(BUILD_DIR)'/screenshots/qemu-unifrog.ppm
	od -An -tu1 -j 15 '$(BUILD_DIR)'/screenshots/qemu-unifrog.ppm | \
		awk '{ for (i = 1; i <= NF; i++) if ($$i) { found = 1; exit } } END { exit !found }'
	test -s '$(BUILD_DIR)'/screenshots/qemu-unifrog-after-input.ppm
	! cmp -s '$(BUILD_DIR)'/screenshots/qemu-unifrog.ppm \
		'$(BUILD_DIR)'/screenshots/qemu-unifrog-after-input.ppm

run-qemu-mufrog: qemu $(MUFROG_QEMU_SD)
	test -f '$(MUFROG_ASD)'
	mkdir -p '$(BUILD_DIR)'/logs
	timeout 20s '$(QEMU_BIN)' -M sf2000 -kernel '$(MUFROG_ASD)' \
		-drive if=none,id=sd0,file='$(MUFROG_QEMU_SD)',format=raw \
		-display none -serial none -monitor none -d guest_errors,unimp \
		-D '$(BUILD_DIR)'/logs/qemu-mufrog.log \
		> '$(BUILD_DIR)'/logs/qemu-mufrog.console 2>&1 || test $$? -eq 124

smoke-qemu-mufrog: run-qemu-mufrog
	grep -q 'name=unifrog.module_init.done' '$(BUILD_DIR)'/logs/qemu-mufrog.log
	grep -q 'name=unifrog.storage.done .*arg2=0x00000000' '$(BUILD_DIR)'/logs/qemu-mufrog.log
	grep -q 'name=unifrog.js.begin' '$(BUILD_DIR)'/logs/qemu-mufrog.log
	grep -q 'name=unifrog.boot_logo.done' '$(BUILD_DIR)'/logs/qemu-mufrog.log

run-qemu-mufrog-display: qemu $(MUFROG_QEMU_SD)
	test -f '$(MUFROG_ASD)'
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/screenshots
	rm -f '$(BUILD_DIR)'/screenshots/qemu-mufrog.ppm \
		'$(BUILD_DIR)'/screenshots/qemu-mufrog-after-input.ppm
	(sleep 9; printf 'screendump $(BUILD_DIR)/screenshots/qemu-mufrog.ppm\nsendkey down 1000\n'; \
		sleep 2; printf 'screendump $(BUILD_DIR)/screenshots/qemu-mufrog-after-input.ppm\nquit\n') | \
		'$(QEMU_BIN)' -M sf2000 -kernel '$(MUFROG_ASD)' \
		-drive if=none,id=sd0,file='$(MUFROG_QEMU_SD)',format=raw \
		-display none -serial none -monitor stdio -d guest_errors,unimp \
		-D '$(BUILD_DIR)'/logs/qemu-mufrog-display.log \
		> '$(BUILD_DIR)'/logs/qemu-mufrog-display.console 2>&1

smoke-qemu-mufrog-display: run-qemu-mufrog-display
	grep -q 'name=unifrog.storage.done .*arg2=0x00000000' '$(BUILD_DIR)'/logs/qemu-mufrog-display.log
	grep -q 'name=unifrog.js.begin' '$(BUILD_DIR)'/logs/qemu-mufrog-display.log
	! grep -q 'GMA scanout with panel sync/DE disconnected' '$(BUILD_DIR)'/logs/qemu-mufrog-display.log
	! grep -q 'VOU raster disconnected from PRGB' '$(BUILD_DIR)'/logs/qemu-mufrog-display.log
	test -s '$(BUILD_DIR)'/screenshots/qemu-mufrog.ppm
	od -An -tu1 -j 15 '$(BUILD_DIR)'/screenshots/qemu-mufrog.ppm | \
		awk '{ for (i = 1; i <= NF; i++) if ($$i) { found = 1; exit } } END { exit !found }'
	test -s '$(BUILD_DIR)'/screenshots/qemu-mufrog-after-input.ppm
	! cmp -s '$(BUILD_DIR)'/screenshots/qemu-mufrog.ppm \
		'$(BUILD_DIR)'/screenshots/qemu-mufrog-after-input.ppm

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
