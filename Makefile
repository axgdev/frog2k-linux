# SPDX-License-Identifier: MIT

QEMU_DIR ?= $(abspath ../sf2000_qemu)
QEMU_ORACLE_DIR ?= $(abspath $(QEMU_DIR))
HCLINUX_DIR := external/hclinux/2024.02.y.2
BUILD_DIR := build
INITRAMFS := $(BUILD_DIR)/initramfs.cpio
INITRAMFS_LIST := $(BUILD_DIR)/initramfs.list
INIT_BIN := $(BUILD_DIR)/initramfs-init
GEN_INIT_CPIO := $(BUILD_DIR)/gen_init_cpio
ASDPACK := $(BUILD_DIR)/asdpack
QEMU_BIN := /tmp/sf2000-qemu/qemu-10.2.2/build/qemu-system-mipsel
QEMU_MKSD := $(QEMU_DIR)/build/mksf2000sd
QEMU_CPU ?= 4Km
QEMU_CPU_ARGS := $(if $(QEMU_CPU),-cpu $(QEMU_CPU),)
QEMU_ROM_CPU ?=
QEMU_ROM_CPU_ARGS := $(if $(QEMU_ROM_CPU),-cpu $(QEMU_ROM_CPU),)
QEMU_DEBUG ?= guest_errors,unimp
QEMU_SD_ARGS ?=
HOSTCC ?= cc
AUDIO_TEST := $(BUILD_DIR)/hc15xx-audio-test
AUDIO_AVSYNC_TEST := $(BUILD_DIR)/hc15xx-avsync-test
AUDIO_RESAMPLER_TEST := $(BUILD_DIR)/hc15xx-resampler-test
RETAINED_TEST := $(BUILD_DIR)/hc15xx-retained-test
RETAINED_SRC := platform/hc15xx_retained.c
RETAINED_HEADER := include/hc15xx_retained.h
EFUSE_TEST := $(BUILD_DIR)/hc15xx-efuse-test
VDEC_TEST := $(BUILD_DIR)/hc15xx-vdec-test
VDEC_CODEC_TEST := $(BUILD_DIR)/hc15xx-vdec-codec-test
DSC_TEST := $(BUILD_DIR)/hc15xx-dsc-test
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
BUILDROOT_INIT_CLONE := buildroot/sf2000-init-clone.S
BUILDROOT_INIT := $(BUILDROOT_GENERATED_OVERLAY)/init
BUILDROOT_SUPERVISOR := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-init
BUILDROOT_PAD_SRC := buildroot/sf2000-pad.c
BUILDROOT_PAD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-pad
BUILDROOT_POWERD_SRC := buildroot/sf2000-powerd.c
BUILDROOT_POWERD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-powerd
FRONTEND_PROJECT ?= ../sf2000_linux_frontend
BUILDROOT_FRONTEND := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/sf2000-frontend
BUILDROOT_GAMBATTE := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/sf2000-gambatte
BUILDROOT_GPSP := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/sf2000-gpsp
BUILDROOT_AUDIO_SRC := buildroot/sf2000-audio.c
BUILDROOT_AUDIO := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-audio
BUILDROOT_HEARTBEAT_SRC := buildroot/sf2000-heartbeat.c
BUILDROOT_HEARTBEAT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-heartbeat
BUILDROOT_LOGD_SRC := buildroot/sf2000-logd.c
BUILDROOT_LOGD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-logd
BUILDROOT_MOUNT_SRC := buildroot/sf2000-mount.c
BUILDROOT_MOUNT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-mount
BUILDROOT_SCREEN_SRC := buildroot/sf2000-screen.c
BUILDROOT_SCREEN_CFLAGS :=
BUILDROOT_SCREEN := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-screen
BUILDROOT_SCREEN_SOURCE_STAMP := $(BUILD_DIR)/.stamp-buildroot-screen-source
BUILDROOT_PANEL_PROBE_LINK := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-probe
BUILDROOT_PANEL_PROBE_TARGET ?= sf2000-screen
BUILDROOT_PANEL_INIT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-init
BUILDROOT_PANEL_INIT_SRC := buildroot/sf2000-panel-init.c
BUILDROOT_PANEL_FASTPROBE_SRC := buildroot/sf2000-panel-fastprobe.c
BUILDROOT_PANEL_FASTPROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-fastprobe
BUILDROOT_STORAGE_PROBE_SRC := buildroot/sf2000-storage-probe.c
BUILDROOT_STORAGE_PROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-storage-probe
BUILDROOT_STORAGE_FASTPROBE_SRC := buildroot/sf2000-storage-fastprobe.c
BUILDROOT_STORAGE_FASTPROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-storage-fastprobe
BUILDROOT_RESET_FASTPROBE_SRC := buildroot/sf2000-reset-fastprobe.c
BUILDROOT_RESET_FASTPROBE := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-reset-fastprobe
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
BUILDROOT_STRIP := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-uclinux-uclibc-strip
BUILDROOT_INIT_SOURCE ?= $(INIT_BIN)
INIT_CFLAGS ?=
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
LINUX_CMDLINE ?= console=ttyS0,115200 earlycon init=/init initramfs_async=0 \
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
BROWSER_TEST_SD := $(BUILD_DIR)/browser-test.sd.img
BROWSER_TEST_ROM := $(BUILD_DIR)/browser-test.gb
BROWSER_TEST_ROM_TOOL := $(BUILD_DIR)/mkgbtest
GPSP_TEST_SD := $(BUILD_DIR)/gpsp-test.sd.img
GPSP_TEST_ROM := $(BUILD_DIR)/gpsp-test.gba
GPSP_TEST_ROM_TOOL := $(BUILD_DIR)/mkgabatest
GPSP_SMC_TEST_MODES := smc-ab smc-range smc-isa smc-mirror smc-dma-epoch smc-block
GPSP_SMC_TEST_ROMS := $(addprefix $(BUILD_DIR)/gpsp-,$(addsuffix .gba,$(GPSP_SMC_TEST_MODES)))
GPSP_SMC_MODE ?= smc-ab
GPSP_REAL_ROM ?=
GPSP_REAL_TEST_SD := $(BUILD_DIR)/gpsp-real-test.sd.img
FRONTEND_LIFECYCLE_TEST_SD := $(BUILD_DIR)/frontend-lifecycle-test.sd.img
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

FFMPEG_VERSION ?= 8.1.2
FFMPEG_URL := https://ffmpeg.org/releases/ffmpeg-$(FFMPEG_VERSION).tar.xz
FFMPEG_SRC ?= /tmp/sf2000-ffmpeg/ffmpeg-$(FFMPEG_VERSION)
FFMPEG_OUT := $(BUILD_DIR)/ffmpeg
FFMPEG_INSTALL := $(abspath $(FFMPEG_OUT)/install)
FFMPEG_STAMP := $(FFMPEG_OUT)/.stamp-built

BUILDROOT_DEVTEST_SRC := buildroot/sf2000-devtest.c
BUILDROOT_DEVTEST := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-devtest
BUILDROOT_EFUSE_DEVICE := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/test_efuse_device
BUILDROOT_VDEC_DEVICE := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/test_vdec_device
BUILDROOT_DSC_DEVICE := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/test_dsc_device
BUILDROOT_PLAYER := $(BUILDROOT_GENERATED_OVERLAY)/usr/bin/sf2000-player

PLAYER_TEST_SD := $(BUILD_DIR)/player-test.sd.img
PLAYER_TEST_WAV := $(BUILD_DIR)/test-tone.wav
MKTESTWAV := $(BUILD_DIR)/mktestwav

SMOKE_INIT_PATTERN ?= sf2000_linux: init alive
LOADER_CFLAGS := -Os -ffreestanding -fno-builtin -nostdlib \
	-march=mips32 -mabi=32 -msoft-float -mno-abicalls -fno-pic -mno-gpopt -G 0 \
	-Wall -Wextra

.PHONY: all status qemu rootfs toolchain buildroot buildroot-reconfigure audio-test linux linux-reextract linux-reconfigure linux-asd linux-buildroot \
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
	run-linux-gpsp smoke-linux-gpsp \
	gpsp-real-test-sd run-linux-gpsp-real smoke-linux-gpsp-real \
	gpsp-smc-test-roms run-linux-gpsp-smc smoke-linux-gpsp-smc \
	run-linux-frontend-lifecycle smoke-linux-frontend-lifecycle \
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
	test-ge-symbol-coverage efuse-test vdec-test vdec-codec-test dsc-test \
	device-tests ffmpeg player \
	run-linux-devtest smoke-linux-devtest \
	run-linux-player smoke-linux-player \
	clean

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

ASDPACK := $(BUILD_DIR)/asdpack

$(ASDPACK): tools/asdpack.c Makefile
	mkdir -p '$(dir $@)'
	'$(HOSTCC)' -O2 -Wall -Wextra -o '$@' '$<'

rootfs: $(ROOTFS_CPIO)
toolchain: $(BUILDROOT_TOOLCHAIN_STAMP)
buildroot: $(BUILDROOT_CPIO)

linux-asd: $(LINUX_ASD)

linux-buildroot:
	$(MAKE) ROOTFS=buildroot linux

linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot linux-asd

$(LINUX_ASD): $(LINUX_LOADER_BIN) $(ASDPACK)
	'$(ASDPACK)' '$(LINUX_LOADER_BIN)' '$@'
	mkdir -p '$(dir $(SDCARD_LINUX_ASD))'
	if test -f '$(SDCARD_LINUX_ASD)'; then \
		cp '$(LINUX_ASD)' '$(SDCARD_LINUX_ASD)'; \
	fi
	mkdir -p '$(dir $(SDCARD_BIOS_ASD))'
	if test -f '$(SDCARD_BIOS_ASD)'; then cp '$(LINUX_ASD)' '$(SDCARD_BIOS_ASD)'; fi

$(LINUX_LOADER_ENTRY_OBJ): boot/linux-loader-entry.S Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' -c -o '$@' '$<'

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
		printf '.balign 16\n'; \
		printf '.globl linux_dtb_start\n'; \
		printf 'linux_dtb_start:\n'; \
		printf '.incbin "%s"\n' '$(abspath $(SF2000_DTB))'; \
		printf '.balign 16\n'; \
		printf '.globl linux_dtb_end\n'; \
		printf 'linux_dtb_end:\n'; \
	} > '$@'

$(LINUX_LOADER_BLOBS_OBJ): $(LINUX_LOADER_BLOBS_S)
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' $(LOADER_CFLAGS) -c -o '$@' '$<'

$(LINUX_LOADER_ELF): $(LINUX_LOADER_ENTRY_OBJ) $(LINUX_LOADER_OBJ) \
		$(LINUX_LOADER_BLOBS_OBJ) boot/linux-loader.ld
	mkdir -p '$(dir $@)'
	'$(LD_MIPS)' -T boot/linux-loader.ld -o '$@' \
		'$(LINUX_LOADER_ENTRY_OBJ)' '$(LINUX_LOADER_OBJ)' \
		'$(LINUX_LOADER_BLOBS_OBJ)'

$(LINUX_LOADER_BIN): $(LINUX_LOADER_ELF)
	mkdir -p '$(dir $@)'
	'$(OBJCOPY_MIPS)' -O binary '$<' '$@'

buildroot-reconfigure:
	rm -f '$(BUILDROOT_OUT)/.config'
	$(MAKE) ROOTFS='$(ROOTFS)' buildroot

$(INIT_BIN): init/sf2000-flat-init.S Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' $(INIT_CFLAGS) -Os -nostdlib -ffreestanding \
		-march=mips32 -mabi=32 -msoft-float -mno-abicalls \
		-fno-pic -fno-pie -no-pie -mno-gpopt -G 0 \
		-Wl,-Ttext-segment=0x85000000 -Wl,-e,_start \
		-Wl,--gc-sections -Wl,-z,noexecstack \
		-Wl,--no-check-sections -o '$@' '$<'

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

$(BUILDROOT_CPIO): $(BUILDROOT_TOOLCHAIN_STAMP) $(BUILDROOT_GENERATED_OVERLAY_STAMP)
	cd '$(BUILDROOT_OUT)' && $(BUILDROOT_MAKE) rootfs-cpio
	cp '$(BUILDROOT_OUT)/images/rootfs.cpio' '$@'
	touch '$@'

$(BUILDROOT_TOOLCHAIN_STAMP): $(BUILDROOT_OUT)/.config
	FORCE_UNSAFE_CONFIGURE=1 $(BUILDROOT_MAKE) -j'$(JOBS)' toolchain \
		HOST_CFLAGS='$(BUILDROOT_HOST_CFLAGS)' \
		HOST_CXXFLAGS='$(BUILDROOT_HOST_CXXFLAGS)'
	touch '$@'

UCLIBC_SRC := $(BUILDROOT_OUT)/build/uclibc-1.0.58

: $(BUILDROOT_TOOLCHAIN_STAMP) $(UCLIBC_PIE_PATCHES) Makefile
	for p in $(UCLIBC_PIE_PATCHES); do \
		patch -d '$(UCLIBC_SRC)' -p1 -N -r /dev/null < "$$p" 2>/dev/null || true; \
	done
	mkdir -p '$(dir $@)'
	cp '$(UCLIBC_SRC)'/.config '$(BUILD_DIR)'/uclibc-pie/.config
	sed -i 's/^# STATIC_PIE is not set/STATIC_PIE=y/' '$(BUILD_DIR)'/uclibc-pie/.config
	sed -i 's/^STATIC_PIE=n/STATIC_PIE=y/' '$(BUILD_DIR)'/uclibc-pie/.config
	grep -q '^STATIC_PIE=y' '$(BUILD_DIR)'/uclibc-pie/.config || \
		echo 'STATIC_PIE=y' >> '$(BUILD_DIR)'/uclibc-pie/.config
	'$(UCLIBC_SRC)'/extra/config/conf --silentoldconfig \
		-Kconfig '$(UCLIBC_SRC)' O='$(abspath $(BUILD_DIR)/uclibc-pie)' 2>/dev/null || true
	$(MAKE) -C '$(UCLIBC_SRC)' \
		O='$(abspath $(BUILD_DIR)/uclibc-pie)' \
		CROSS_COMPILE='$(patsubst %gcc,%,$(BUILDROOT_CC))' \
		UCLIBC_EXTRA_CFLAGS='' \
		-j'$(JOBS)' libs startfiles
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


# Simple ET_EXEC compilation at 0x85000000 (no PIE needed)
$(BUILDROOT_SUPERVISOR): $(BUILDROOT_INIT_SRC) $(BUILDROOT_INIT_CLONE) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi=32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_INIT_CLONE) $(BUILDROOT_INIT_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(BUILDROOT_PAD): $(BUILDROOT_PAD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_PAD_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(BUILDROOT_POWERD): $(BUILDROOT_POWERD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_POWERD_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(BUILDROOT_FRONTEND): player/browser.c $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra \
		-march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' player/browser.c \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lm -lgcc

$(BUILDROOT_AUDIO): $(BUILDROOT_AUDIO_SRC) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_AUDIO_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(BUILDROOT_HEARTBEAT): $(BUILDROOT_HEARTBEAT_SRC) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_HEARTBEAT_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(BUILDROOT_LOGD): $(BUILDROOT_LOGD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_LOGD_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(BUILDROOT_MOUNT): $(BUILDROOT_MOUNT_SRC) $(BUILDROOT_TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(BUILDROOT_CC) -Os -Wall -march=mips32 -mabi-32 -msoft-float \
		-static -Wl,-Ttext-segment=0x85000000 \
		-o '$@' $(BUILDROOT_MOUNT_SRC) \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/lib' \
		-L'/tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/mipsel-buildroot-uclinux-uclibc/sysroot/usr/lib' \
		-lc -lgcc

$(LINUX_VMLINUX): $(LINUX_SRC)/.patched $(LINUX_CONFIG_STAMP) $(ROOTFS_CPIO)
	$(MAKE) -j'$(JOBS)' -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' vmlinux
	'$(STRIP_MIPS)' --strip-debug '$(LINUX_VMLINUX)'

$(LINUX_CONFIG_STAMP): $(LINUX_SRC)/.patched | $(LINUX_OUT)
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
		--disable BINFMT_FLAT \
		--enable BINFMT_ELF_NOMMU \
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
		--enable UDOS_PARTITION \
		--enable ISCSI_BOOT \
		--enable EFI_PARTITION \
		--enable CFQ_IOSCHED \
		--enable DEFAULT_CFQ \
		--enable IOSCHED_AS \
		--enable SF2000_BLOCK_DEVICE \
		--enable SF2000_USB_STORAGE \
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
		--enable SF2000_ROM_BUFFER \
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
		--enable FB_SIMPLE \
		--disable CONFIG_VDSO \
		--disable CONFIG_TIME_NS
	$(MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' olddefconfig
	touch '$@'

$(LINUX_SRC)/.patched: $(LINUX_SRC)/Makefile Makefile
	@if test -e '$@'; then \
		printf 'linux patch series changed; reapplying in %s\n' '$(LINUX_SRC)'; \
	fi
	@for patch in $(wildcard patches/linux-$(LINUX_VERSION)/*.patch); do \
		if test "$$patch" -nt '$@'; then \
			if patch -d '$(LINUX_SRC)' --dry-run -p1 < "$$patch" >/dev/null 2>&1; then \
				printf 'applying %s\n' "$$patch"; \
				patch -d '$(LINUX_SRC)' -p1 < "$$patch"; \
			elif patch -d '$(LINUX_SRC)' --dry-run -R -p1 < "$$patch" >/dev/null 2>&1; then \
				printf 'already applied %s\n' "$$patch"; \
			else \
				printf 'cannot apply %s; remove %s for a clean kernel tree\n' "$$patch" '$(LINUX_SRC)' >&2; \
				exit 1; \
			fi; \
		fi; \
	done
	touch '$@'

$(LINUX_SRC):
	mkdir -p '$(LINUX_SRC)'
	tar xvf '.cache/linux-$(LINUX_VERSION).tar.xz' -C '$(LINUX_SRC)' --strip-components=1

$(LINUX_OUT):
	mkdir -p '$@'

$(SF2000_DTB): linux/sf2000.dts
	mkdir -p '$(dir $@)'
	dtc -I dts -O dtb -o '$@' '$<'

# Define ROOTFS-dependent variables
ifeq ($(ROOTFS),buildroot)
ROOTFS_CPIO := $(BUILDROOT_CPIO)
else ifeq ($(ROOTFS),tiny)
ROOTFS_CPIO := $(INITRAMFS)
else
ROOTFS_CPIO ?= $(INITRAMFS)
endif

linux: $(LINUX_VMLINUX) $(SF2000_DTB) $(ROOTFS_CPIO)
