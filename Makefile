# SPDX-License-Identifier: MIT

QEMU_DIR ?= $(abspath ../sf2000_qemu)
QEMU_ORACLE_DIR ?= $(abspath $(QEMU_DIR))
QEMU_VERSION ?= 10.2.2
QEMU_WORK ?= /tmp/sf2000-qemu
HCLINUX_DIR := external/hclinux/2024.02.y.2
BUILD_DIR ?= build
INITRAMFS := $(BUILD_DIR)/initramfs.cpio
INITRAMFS_LIST := $(BUILD_DIR)/initramfs.list
INIT_BIN := $(BUILD_DIR)/initramfs-init
GEN_INIT_CPIO := $(BUILD_DIR)/gen_init_cpio
ASDPACK := $(BUILD_DIR)/asdpack
INITRAMFS_DATE ?= 1970-01-01 UTC
INITRAMFS_EPOCH ?= 0
QEMU_BIN ?= $(QEMU_WORK)/qemu-$(QEMU_VERSION)/build/qemu-system-mipsel
QEMU_MKSD := $(QEMU_DIR)/build/mksf2000sd
# The SF2000 kernel is built for MIPS32r1 but deliberately uses MIPS32r2 CP0
# features (IntCtl/EBase select-1, ehb) in per_cpu_trap_init(); the 4Km model
# (r1 only) raises Reserved Instruction on those and traps forever.  The 24Kc
# (r2) is also the sf2000 machine's own default CPU.
QEMU_CPU ?= 24Kc
QEMU_CPU_ARGS := $(if $(QEMU_CPU),-cpu $(QEMU_CPU),)
# Extra -M sf2000,key=val machine options, e.g. ,ge-no-irq=on for the
# no-completion-IRQ regression boot that forces the GE poll path.
QEMU_MACHINE_ARGS ?=
QEMU_ROM_CPU ?=
QEMU_ROM_CPU_ARGS := $(if $(QEMU_ROM_CPU),-cpu $(QEMU_ROM_CPU),)
QEMU_DEBUG ?= guest_errors,unimp
QEMU_SD_ARGS ?=
HOSTCC ?= cc
USE_CCACHE ?= 1
CCACHE ?= $(if $(filter 1,$(USE_CCACHE)),$(shell command -v ccache 2>/dev/null),)
CCACHE_COMPILE := $(if $(strip $(CCACHE)),$(CCACHE) ,)
# Keep the cache outside disposable build outputs so `make clean`, alternate
# BUILD_DIR values, and separate kernel/rootfs builds can reuse it.
CCACHE_DIR ?= $(abspath .cache/ccache)
CCACHE_COMPILERCHECK ?= content
export CCACHE_DIR CCACHE_COMPILERCHECK
# Kernel.org intermittently closes large HTTP/2 streams on hosted runners.
# Keep downloads on HTTP/1.1 and never promote a failed partial transfer from
# the temporary path to the verified archive path.
CURL_DOWNLOAD ?= curl --http1.1 --fail --location --retry 5 --retry-all-errors --retry-delay 2 --connect-timeout 30
AUDIO_TEST := $(BUILD_DIR)/hc15xx-audio-test
AUDIO_AVSYNC_TEST := $(BUILD_DIR)/hc15xx-avsync-test
AUDIO_RESAMPLER_TEST := $(BUILD_DIR)/hc15xx-resampler-test
RETAINED_TEST := $(BUILD_DIR)/hc15xx-retained-test
EFUSE_TEST := $(BUILD_DIR)/hc15xx-efuse-test
VDEC_TEST := $(BUILD_DIR)/hc15xx-vdec-test
VDEC_CODEC_TEST := $(BUILD_DIR)/hc15xx-vdec-codec-test
DSC_TEST := $(BUILD_DIR)/hc15xx-dsc-test
# These values identify the downloaded release and are tied to the checksums
# below.  Alternate mirrors may override FROG_TOOLCHAIN_URL, but changing the
# release components without updating the pinned artifact is unsupported.
FROG_TOOLCHAIN_VERSION := 1.3.2
FROG_TOOLCHAIN_GCC_VERSION := 16.2.0
FROG_TOOLCHAIN_BINUTILS_VERSION := 2.47
FROG_TOOLCHAIN_UCLIBC_VERSION := 1.0.59
FROG_TOOLCHAIN_HOST_ARCH ?= $(shell uname -m)
FROG_TOOLCHAIN_ARCH := $(if $(filter x86_64 amd64,$(FROG_TOOLCHAIN_HOST_ARCH)),x86_64,$(if $(filter aarch64 arm64,$(FROG_TOOLCHAIN_HOST_ARCH)),arm64,))
ifeq ($(strip $(FROG_TOOLCHAIN_ARCH)),)
$(error unsupported host architecture '$(FROG_TOOLCHAIN_HOST_ARCH)'; set FROG_TOOLCHAIN_HOST_ARCH to x86_64 or arm64)
endif
FROG_TOOLCHAIN_NAME := toolchain-uclibc-static-$(FROG_TOOLCHAIN_ARCH)-gcc$(FROG_TOOLCHAIN_GCC_VERSION)-binutils$(FROG_TOOLCHAIN_BINUTILS_VERSION)-uclibc-ng$(FROG_TOOLCHAIN_UCLIBC_VERSION).tar.xz
FROG_TOOLCHAIN_URL ?= https://github.com/axgdev/frog-toolchain/releases/download/v$(FROG_TOOLCHAIN_VERSION)/$(FROG_TOOLCHAIN_NAME)
FROG_TOOLCHAIN_SHA256_arm64 := ff7e9742a9b6fbbfcf58394b92c0805c4a0a7bdf21592a546fb665e24ee60fc4
FROG_TOOLCHAIN_SHA256_x86_64 := 8d4599a27ec2493ba56cc3940025973f86947569eeaed7e95836957649f4d88b
FROG_TOOLCHAIN_SHA256 := $(FROG_TOOLCHAIN_SHA256_$(FROG_TOOLCHAIN_ARCH))
FROG_TOOLCHAIN_ARCHIVE ?= .cache/$(FROG_TOOLCHAIN_NAME)
FROG_TOOLCHAIN_WORK ?= /tmp/sf2000-linux-frog-toolchain-v$(FROG_TOOLCHAIN_VERSION)-$(FROG_TOOLCHAIN_ARCH)
FROG_TOOLCHAIN_PREFIX ?= $(FROG_TOOLCHAIN_WORK)/mipsel-unknown-linux-uclibc
TOOLCHAIN_DIR ?= $(FROG_TOOLCHAIN_PREFIX)
TOOLCHAIN_TUPLE := mipsel-unknown-linux-uclibc
CROSS_COMPILE ?= $(TOOLCHAIN_DIR)/bin/$(TOOLCHAIN_TUPLE)-
CC_MIPS = $(CROSS_COMPILE)gcc
CC_MIPS_RUN = $(CCACHE_COMPILE)$(CC_MIPS)
KERNEL_CC = $(CCACHE_COMPILE)$(CROSS_COMPILE)gcc
LD_MIPS = $(CROSS_COMPILE)ld
OBJCOPY_MIPS = $(CROSS_COMPILE)objcopy
STRIP_MIPS = $(CROSS_COMPILE)strip
OBJDUMP_MIPS = $(CROSS_COMPILE)objdump
READELF_MIPS = $(CROSS_COMPILE)readelf
NM_MIPS = $(CROSS_COMPILE)nm
JOBS ?= $(shell nproc 2>/dev/null || echo 1)
# The kernel make is isolated from the outer jobserver, so it can safely use
# the same parallelism as the other build stages. Override this independently
# on constrained hosts, for example KERNEL_JOBS=1.
KERNEL_JOBS ?= $(JOBS)
ROOTFS ?= tiny
LINUX_VERSION := 7.1.4
LINUX_TARBALL := linux-$(LINUX_VERSION).tar.xz
LINUX_URL := https://cdn.kernel.org/pub/linux/kernel/v7.x/$(LINUX_TARBALL)
LINUX_SHA256 := 1c63922a119675d38e3ae0f8f6ee07f15c41a786ab9ed66563749bb8c9a08e2e
LINUX_SOURCE_METHOD ?= git
LINUX_GIT_URL ?= https://github.com/gregkh/linux.git
LINUX_GIT_TAG := v$(LINUX_VERSION)
LINUX_ARCHIVE := .cache/$(LINUX_TARBALL)
LINUX_ARCHIVE_CHECK := $(LINUX_ARCHIVE).verified
LINUX_SRC ?= /tmp/sf2000-linux-next-kernel-$(LINUX_VERSION)
LINUX_SLIM_SCRIPT_SHA256 := $(shell sha256sum scripts/kernel-slim.sh | awk '{print $$1}')
LINUX_SLIM_ARCHIVE ?= $(patsubst %.tar.xz,%-slim-$(LINUX_GIT_TAG)-$(LINUX_SLIM_SCRIPT_SHA256).tar.xz,$(LINUX_ARCHIVE))
LINUX_SLIM_WORK ?= /tmp/sf2000-linux-slim-$(LINUX_VERSION)-$(LINUX_SLIM_SCRIPT_SHA256)
BUSYBOX_VERSION := 1.38.0
BUSYBOX_TARBALL := busybox-$(BUSYBOX_VERSION).tar.bz2
BUSYBOX_URL := https://www.busybox.net/downloads/$(BUSYBOX_TARBALL)
BUSYBOX_SHA256 := 34f9ea6ff8636f2c9241153b9114eefa9e65674a45318ae1ef95bb5f31c53bb2
BUSYBOX_WORK ?= /tmp/sf2000-linux-busybox-$(BUSYBOX_VERSION)
BUSYBOX_SRC ?= $(BUSYBOX_WORK)/busybox-$(BUSYBOX_VERSION)
BUSYBOX_ARCHIVE ?= .cache/$(BUSYBOX_TARBALL)
BUSYBOX_ARCHIVE_CHECK := $(BUSYBOX_ARCHIVE).verified
BUSYBOX_OUT := $(BUILD_DIR)/busybox
BUSYBOX_ROOT := $(BUILD_DIR)/busybox-root
BUSYBOX_CONFIG := userspace/busybox.config
BUSYBOX_PATCHES := $(wildcard userspace/patches/busybox/*.patch)
BUSYBOX_SOURCE_STAMP := $(BUSYBOX_OUT)/.stamp-source
BUSYBOX_PATCH_STAMP := $(BUSYBOX_OUT)/.stamp-patched
BUSYBOX_STAMP := $(BUSYBOX_OUT)/.stamp-built
ROOTFS_BASE := userspace/rootfs-base
ROOTFS_OVERLAY := userspace/rootfs-overlay
ROOTFS_DIR := $(BUILD_DIR)/rootfs-full
ROOTFS_DEVICE_CPIO_LIST := $(BUILD_DIR)/rootfs-device-nodes.list
ROOTFS_FULL_CPIO := $(BUILD_DIR)/rootfs-full.cpio
ROOTFS_GENERATED_OVERLAY := $(BUILD_DIR)/userspace-generated-overlay
ROOTFS_GENERATED_OVERLAY_STAMP := $(BUILD_DIR)/.stamp-userspace-generated-overlay
ROOTFS_OVERLAY_FILES := $(shell find '$(ROOTFS_OVERLAY)' -type f 2>/dev/null)
ROOTFS_BASE_FILES := $(shell find '$(ROOTFS_BASE)' -type f 2>/dev/null)
FB_TEST_APP_VERSION := 1.1.1
FB_TEST_APP_TARBALL := fb-test-app-$(FB_TEST_APP_VERSION).tar.gz
FB_TEST_APP_URL := https://github.com/andy-shev/fb-test-app/archive/rosetta-$(FB_TEST_APP_VERSION)/$(FB_TEST_APP_TARBALL)
FB_TEST_APP_SHA256 := 45d490ed78a6e4425d9a760e81e99dc503af01704e17ab5bf186b87a31c5e3db
FB_TEST_APP_WORK ?= /tmp/sf2000-linux-fb-test-app-$(FB_TEST_APP_VERSION)
FB_TEST_APP_SRC ?= $(FB_TEST_APP_WORK)/fb-test-app-rosetta-$(FB_TEST_APP_VERSION)
FB_TEST_APP_ARCHIVE ?= .cache/$(FB_TEST_APP_TARBALL)
FB_TEST_APP_ARCHIVE_CHECK := $(FB_TEST_APP_ARCHIVE).verified
FB_TEST_APP_OUT := $(BUILD_DIR)/fb-test-app
FB_TEST_APP_SOURCE_STAMP := $(FB_TEST_APP_OUT)/.stamp-source
FB_TEST_APP_STAMP := $(FB_TEST_APP_OUT)/.stamp-built
ENABLE_EXPERIMENTAL_DEVTESTS ?= 0
FIXED_ET_EXEC ?= 0
FRONTEND_PROJECT ?= ../sf2000_linux_frontend
CI_FRESH_TARGET ?= ci-fresh-js2300
USERSPACE_GENERATED_OVERLAY := $(ROOTFS_GENERATED_OVERLAY)
USERSPACE_INIT_SRC := userspace/sf2000-init.c
USERSPACE_INIT_CLONE := userspace/sf2000-init-clone.S
USERSPACE_INIT := $(USERSPACE_GENERATED_OVERLAY)/init
USERSPACE_SUPERVISOR := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-init
USERSPACE_PAD_SRC := userspace/sf2000-pad.c
USERSPACE_PAD := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-pad
USERSPACE_POWERD_SRC := userspace/sf2000-powerd.c
USERSPACE_POWERD := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-powerd
USERSPACE_FRONTEND := $(USERSPACE_GENERATED_OVERLAY)/usr/bin/sf2000-frontend
USERSPACE_JS2300 := $(USERSPACE_GENERATED_OVERLAY)/usr/bin/sf2000-js2300
USERSPACE_AUDIO_SRC := userspace/sf2000-audio.c
USERSPACE_AUDIO := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-audio
USERSPACE_HEARTBEAT_SRC := userspace/sf2000-heartbeat.c
USERSPACE_HEARTBEAT := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-heartbeat
USERSPACE_LOGD_SRC := userspace/sf2000-logd.c
USERSPACE_LOGD := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-logd
USERSPACE_MOUNT_SRC := userspace/sf2000-mount.c
USERSPACE_MOUNT := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-mount
USERSPACE_SCREEN_SRC := userspace/sf2000-screen.c
USERSPACE_SCREEN_CFLAGS :=
USERSPACE_SCREEN := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-screen
USERSPACE_SCREEN_SOURCE_STAMP := $(BUILD_DIR)/.stamp-userspace-screen-source
USERSPACE_PANEL_PROBE_LINK := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-probe
USERSPACE_PANEL_PROBE_TARGET ?= sf2000-screen
USERSPACE_PANEL_INIT := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-init
USERSPACE_PANEL_INIT_SRC := userspace/sf2000-panel-init.c
USERSPACE_PANEL_FASTPROBE_SRC := userspace/sf2000-panel-fastprobe.c
USERSPACE_PANEL_FASTPROBE := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-panel-fastprobe
USERSPACE_STORAGE_PROBE_SRC := userspace/sf2000-storage-probe.c
USERSPACE_STORAGE_PROBE := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-storage-probe
USERSPACE_STORAGE_FASTPROBE_SRC := userspace/sf2000-storage-fastprobe.c
USERSPACE_STORAGE_FASTPROBE := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-storage-fastprobe
USERSPACE_RESET_FASTPROBE_SRC := userspace/sf2000-reset-fastprobe.c
USERSPACE_RESET_FASTPROBE := $(USERSPACE_GENERATED_OVERLAY)/usr/sbin/sf2000-reset-fastprobe
USERSPACE_RESET_RESTORE_SCRIPT := scripts/qmp_restore_smoke.py
USERSPACE_RESET_RESTORE_STATE := $(BUILD_DIR)/state/sf2000-reset-fastprobe.migration
USERSPACE_RESET_RESTORE_SOCKET := $(BUILD_DIR)/qmp/sf2000-reset-fastprobe.qmp
USERSPACE_RESET_RESTORE_SOCKET_DEST := $(BUILD_DIR)/qmp/sf2000-reset-fastprobe-restore.qmp
USERSPACE_RESET_RESTORE_PREFIX := $(BUILD_DIR)/logs/linux-full-reset-restore
USERSPACE_ELF_LDFLAGS := -static -Wl,-Ttext-segment=0x85000000 \
	-Wl,--no-check-sections -Wl,--gc-sections
PIE_STAMP := $(BUILD_DIR)/uclibc-pie/.stamp-built

# The pinned frog-toolchain is the only supported target compiler. It is
# downloaded as an artifact; no target toolchain is built in this repository.
TOOLCHAIN_STAMP := $(BUILD_DIR)/toolchain/.stamp-verified
TARGET_CC := $(TOOLCHAIN_DIR)/bin/$(TOOLCHAIN_TUPLE)-gcc
TARGET_CXX := $(TOOLCHAIN_DIR)/bin/$(TOOLCHAIN_TUPLE)-g++
TARGET_STRIP := $(TOOLCHAIN_DIR)/bin/$(TOOLCHAIN_TUPLE)-strip
PIE_SYSROOT := $(TOOLCHAIN_DIR)/$(TOOLCHAIN_TUPLE)/sysroot/usr/lib
TARGET_CC_RUN = $(CCACHE_COMPILE)$(TARGET_CC)
# The frog-toolchain GCC startfile spec lists -static before -static-pie, so
# `-static -static-pie` selects the non-PIC crt1.o and the MIPS static-PIE link
# fails with R_MIPS_HI16 against _gp. Generate a specs override once and use it
# for every direct userspace link.
TOOLCHAIN_SPECS := $(BUILD_DIR)/toolchain/static-pie.specs
FROG_TOOLCHAIN_ARCHIVE_CHECK := $(FROG_TOOLCHAIN_ARCHIVE).verified
FROG_TOOLCHAIN_SETUP_STAMP := $(BUILD_DIR)/toolchain/.stamp-frog-toolchain

# Fetch the pinned host-appropriate frog-toolchain only when the default
# prefix is being used.  An explicitly supplied TOOLCHAIN_DIR is treated as
# an already managed installation and is verified below without overwriting
# it.  The release digest comes from the GitHub asset metadata.
ifeq ($(abspath $(TOOLCHAIN_DIR)),$(abspath $(FROG_TOOLCHAIN_PREFIX)))
$(FROG_TOOLCHAIN_ARCHIVE):
	mkdir -p '$(dir $@)'
	set -eu; \
	tmp='$@.tmp'; \
	rm -f "$$tmp"; \
	$(CURL_DOWNLOAD) -o "$$tmp" '$(FROG_TOOLCHAIN_URL)'; \
	test -s "$$tmp"; \
	mv "$$tmp" '$@'

$(FROG_TOOLCHAIN_ARCHIVE_CHECK): $(FROG_TOOLCHAIN_ARCHIVE) Makefile
	test "$$(sha256sum '$<' | awk '{print $$1}')" = '$(FROG_TOOLCHAIN_SHA256)'
	printf 'sha256=%s\narchive=%s\n' '$(FROG_TOOLCHAIN_SHA256)' '$(abspath $(FROG_TOOLCHAIN_ARCHIVE))' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

$(FROG_TOOLCHAIN_SETUP_STAMP): FORCE $(FROG_TOOLCHAIN_ARCHIVE_CHECK) Makefile
	mkdir -p '$(FROG_TOOLCHAIN_WORK)' '$(dir $@)'
	if test ! -x '$(FROG_TOOLCHAIN_PREFIX)/bin/$(TOOLCHAIN_TUPLE)-gcc'; then \
		tar -xJf '$(FROG_TOOLCHAIN_ARCHIVE)' -C '$(FROG_TOOLCHAIN_WORK)'; \
	fi
	test -x '$(FROG_TOOLCHAIN_PREFIX)/bin/$(TOOLCHAIN_TUPLE)-gcc'
	printf 'prefix=%s\narchive=%s\nsha256=%s\n' \
		'$(abspath $(FROG_TOOLCHAIN_PREFIX))' \
		'$(abspath $(FROG_TOOLCHAIN_ARCHIVE))' \
		'$(FROG_TOOLCHAIN_SHA256)' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi
else
$(FROG_TOOLCHAIN_SETUP_STAMP): Makefile
	mkdir -p '$(dir $@)'
	test -x '$(TARGET_CC)'
	test "$$( $(TARGET_CC) -dumpmachine )" = '$(TOOLCHAIN_TUPLE)'
	printf 'prefix=%s\ncompiler=%s\ntuple=%s\n' \
		'$(abspath $(TOOLCHAIN_DIR))' '$(abspath $(TARGET_CC))' \
		'$(TOOLCHAIN_TUPLE)' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi
endif

# Regenerate the specs override from the toolchain's own -dumpspecs output so
# it stays in sync with that toolchain's startfile spec.
$(TOOLCHAIN_SPECS): $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(TARGET_CC)' -dumpspecs | \
		awk '/^\*startfile:/{p=1; print; next} p && /^\*[a-zA-Z_]+:/{exit} p{print}' | \
		sed 's/static:crt1\.o%s;[[:space:]]*static-pie:rcrt1\.o%s;/static-pie:rcrt1.o%s; static:crt1.o%s;/' > '$@'

USERSPACE_PIE_CFLAGS := -Os -Wall -Wextra -march=mips32 -mabi=32 -msoft-float \
	-fPIC -mabicalls -ffunction-sections -fdata-sections
USERSPACE_PIE_LDFLAGS := -nostartfiles -static -Wl,-pie \
	-Wl,--no-dynamic-linker -Wl,-z,text \
	-Wl,--gc-sections
USERSPACE_SUPERVISOR_CFLAGS := $(USERSPACE_PIE_CFLAGS) -ffreestanding -fno-builtin
USERSPACE_INIT_SOURCE ?= $(INIT_BIN)
INIT_CFLAGS ?=
ifeq ($(ENABLE_EXPERIMENTAL_DEVTESTS),1)
USERSPACE_EXPERIMENTAL_DEVTESTS := $(USERSPACE_DEVTEST) \
	$(USERSPACE_EFUSE_DEVICE) $(USERSPACE_VDEC_DEVICE) \
	$(USERSPACE_DSC_DEVICE)
USERSPACE_POWERD_CPPFLAGS := -DSF2000_EXPERIMENTAL_DEVTESTS
endif
# Frontend Makefiles nest further into libretro core trees.  Inheriting the
# outer jobserver deadlocks nested makes and each child waits for tokens the
# parent still holds. Drop the jobserver and give each frontend invocation its
# own -j budget.
FRONTEND_MAKE = env -u MAKEFLAGS -u MFLAGS $(MAKE) -j'$(JOBS)' \
	-C '$(FRONTEND_PROJECT)' \
	SF2000_LINUX_DIR='$(abspath .)' \
	TOOLCHAIN_DIR='$(abspath $(TOOLCHAIN_DIR))' CCACHE='$(CCACHE)'
# Kernel, frontend, and userspace recursive makes get private job budgets so
# nested jobserver tokens cannot deadlock one another.
ISOLATED_MAKE = env -u MAKEFLAGS -u MFLAGS $(MAKE) -j'$(JOBS)'
ifeq ($(ROOTFS),tiny)
ROOTFS_SUFFIX :=
ROOTFS_CPIO := $(INITRAMFS)
LINUX_OUT := $(BUILD_DIR)/linux-sf2000
else ifeq ($(ROOTFS),full)
ROOTFS_SUFFIX := -full
ROOTFS_CPIO := $(ROOTFS_FULL_CPIO)
LINUX_OUT := $(BUILD_DIR)/linux-sf2000-full
else
$(error unsupported ROOTFS '$(ROOTFS)', expected tiny or full)
endif
LINUX_VMLINUX := $(LINUX_OUT)/vmlinux
LINUX_CONFIG_STAMP := $(LINUX_OUT)/.stamp-config
LINUX_CMDLINE_STAMP := $(LINUX_OUT)/.stamp-cmdline
LINUX_MODE_STAMP := $(LINUX_OUT)/.stamp-mode
LINUX_DEFCONFIG ?= 32r1el_defconfig
LINUX_PATCHES := $(wildcard patches/linux-$(LINUX_VERSION)/*.patch)
SF2000_DTB := $(BUILD_DIR)/sf2000.dtb
SF2000_BOOT_VISUAL ?= browser
SF2000_BOOT_COLOR ?= 0x0000
SF2000_BOOT_HOLD_MS ?= 750

LINUX_DEFAULT_CMDLINE := console=ttyS0,115200 earlycon init=/init initramfs_async=0 \
	SF2000_BOOT_VISUAL=$(SF2000_BOOT_VISUAL) \
	SF2000_BOOT_COLOR=$(SF2000_BOOT_COLOR) \
	SF2000_BOOT_HOLD_MS=$(SF2000_BOOT_HOLD_MS)
LINUX_CMDLINE ?= $(LINUX_DEFAULT_CMDLINE)

# A diagnostic command line produces a private QEMU/test ASD.  Do not let it
# overwrite the normal SD-card image that is copied to a physical device.
ifeq ($(strip $(LINUX_CMDLINE)),$(strip $(LINUX_DEFAULT_CMDLINE)))
SDCARD_ASD_SYNC_DEFAULT := 1
else
SDCARD_ASD_SYNC_DEFAULT := 0
endif
SDCARD_ASD_SYNC ?= $(SDCARD_ASD_SYNC_DEFAULT)
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
SDCARD_RELEASE_ASD := $(BUILD_DIR)/sf2000-linux-full.asd
SDCARD_BOOT_OPTIONS := $(BUILD_DIR)/sdcard/BOOT-OPTIONS.txt
SDCARD_LOG_TXT := $(BUILD_DIR)/sdcard/log.txt
SDCARD_USER_CONFIG := $(BUILD_DIR)/sdcard/sf2000.conf
SDCARD_UI_FONT := $(BUILD_DIR)/sdcard/sf2000/ui.ttf
SDCARD_UI_LATIN_FONT := $(BUILD_DIR)/sdcard/sf2000/ui-latin.ttf
SDCARD_UI_FONT_LICENSE := $(BUILD_DIR)/sdcard/sf2000/OFL.txt
SDCARD_CORE_STAMP := $(BUILD_DIR)/sdcard/sf2000/cores/.stamp-built
# Core staging atomically replaces its output directory, so keep the input
# signature beside that directory rather than inside it.
SDCARD_CORE_PROFILE := $(BUILD_DIR)/sdcard/.core-build-profile
# The frontend has one shared build directory. Build its two userspace
# programs in one recursive make before the rootfs and core-packaging branches
# can use that checkout. Without this barrier, core-packages may run the
# frontend's JS2300 clean rule while the standalone JS2300 binary is linking.
FRONTEND_UI_STAMP := $(BUILD_DIR)/.frontend-ui.stamp
FRONTEND_UI_SOURCES := $(shell find '$(FRONTEND_PROJECT)'/src \
	'$(FRONTEND_PROJECT)'/include -type f 2>/dev/null) \
	ge/hcge_linux.c ge/hcge_node.c ge/ge_api.h ge/hcge_node.h \
	$(FRONTEND_PROJECT)/Makefile $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
# The sdcard core copies are re-staged from the frontend's freshly built
# executables.  Depending the stamp on those executables - not just frontend
# sources - makes a forced core rebuild in the frontend repo (e.g. `make
# build/sf2000-qpsx`) propagate into the sdcard staging and the ASD, instead
# of silently re-shipping the previously packaged core.  $(wildcard) keeps
# fresh-checkout builds working: absent executables contribute no
# prerequisite, so the stamp rule still runs via its source find deps.
FRONTEND_CORE_OUTPUTS := $(wildcard $(addprefix $(FRONTEND_PROJECT)/build/sf2000-, \
	quicknes prosystem snes9x2005 snes9x2002 stella2014 gearboy pce-fast \
	gambatte gpsp fceumm js2300-core \
	gpsp-multicore picodrive qpsx mame2000 fbalpha2012 a5200 atari800lib \
	handy race beetle-cygne gearcoleco frodo fake08 bluemsx \
	snes9x2005-prosty snes9x2002-prosty gambatte-prosty quicknes-prosty \
	fceumm-prosty))
SDCARD_QUICKNES := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-quicknes
SDCARD_QUICKNES_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/quicknes-LICENSE
SDCARD_PROSYSTEM := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-prosystem
SDCARD_PROSYSTEM_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/prosystem-LICENSE
SDCARD_SNES9X2005 := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-snes9x2005
SDCARD_SNES9X2005_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/snes9x2005-copyright
SDCARD_SNES9X2002 := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-snes9x2002
SDCARD_SNES9X2002_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/snes9x2002-copyright.h
SDCARD_STELLA2014 := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-stella2014
SDCARD_STELLA2014_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/stella2014-license.txt
SDCARD_GEARBOY := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-gearboy
SDCARD_GEARBOY_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/gearboy-LICENSE
SDCARD_PCE_FAST := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-pce-fast
SDCARD_PCE_FAST_LICENSE := $(BUILD_DIR)/sdcard/sf2000/cores/licenses/pce-fast-COPYING
SDCARD_GAMBATTE := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-gambatte
SDCARD_GPSP := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-gpsp
SDCARD_FCEUMM := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-fceumm
SDCARD_JS2300_CORE := $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-js2300-core
SDCARD_JS2300_SCRIPT := $(BUILD_DIR)/sdcard/sf2000/js2300-cores/chip8.js
SDCARD_MUFROG_CORES := \
	sf2000-gpsp-multicore \
	sf2000-picodrive \
	sf2000-qpsx \
	sf2000-mame2000 \
	sf2000-fbalpha2012 \
	sf2000-a5200 \
	sf2000-atari800lib \
	sf2000-handy \
	sf2000-race \
	sf2000-beetle-cygne \
	sf2000-gearcoleco \
	sf2000-frodo \
	sf2000-fake08 \
	sf2000-bluemsx \
	sf2000-snes9x2005-prosty \
	sf2000-snes9x2002-prosty \
	sf2000-gambatte-prosty \
	sf2000-quicknes-prosty \
	sf2000-fceumm-prosty
SDCARD_MUFROG_LICENSES := \
	gpsp-multicore-COPYING \
	picodrive-COPYING \
	qpsx-LICENSE \
	fbalpha2012-COPYING \
	a5200-License.txt \
	atari800lib-COPYING \
	handy-license.txt \
	race-license.txt \
	beetle-cygne-COPYING \
	gearcoleco-LICENSE \
	frodo-COPYING \
	fake08-LICENSE.MD \
	bluemsx-license.txt \
	snes9x2005-prosty-copyright \
	snes9x2002-prosty-copyright.h \
	gambatte-prosty-COPYING \
	quicknes-prosty-LICENSE \
	fceumm-prosty-Copying
SDCARD_FRONTEND_CORES := sf2000-gambatte sf2000-gpsp sf2000-fceumm
SDCARD_FRONTEND_LICENSES := gambatte-COPYING gpsp-COPYING fceumm-Copying
SDCARD_CHECKSUMS := $(BUILD_DIR)/sdcard/SHA256SUMS
SDCARD_RELEASE_ID ?= $(shell git describe --tags --exact-match HEAD 2>/dev/null || git rev-parse --short HEAD)
SDCARD_RELEASE_LABEL := $(subst /,-,$(SDCARD_RELEASE_ID))
SDCARD_PACKAGE_ROOT := $(BUILD_DIR)/sdcard-packages
BUILD_PROVENANCE := $(BUILD_DIR)/sf2000-linux-build-info.txt
SDCARD_BOOTLOADER_STAGE := $(SDCARD_PACKAGE_ROOT)/bootloader
SDCARD_STANDALONE_ZIP ?= $(BUILD_DIR)/sf2000-linux-standalone-$(SDCARD_RELEASE_LABEL).zip
SDCARD_BOOTLOADER_ZIP ?= $(BUILD_DIR)/sf2000-linux-bootloader-$(SDCARD_RELEASE_LABEL).zip
LINUX_ROM_SD_IMAGE := $(BUILD_DIR)/sf2000-linux$(ROOTFS_SUFFIX)-rom.sd.img
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
QPSX_REAL_IMAGE ?=
QPSX_REAL_MENU_AT_START ?= 1
QPSX_REAL_TEST_MIN_MIB ?= 128
QPSX_REAL_TEST_SD := $(BUILD_DIR)/qpsx-real-test.sd.img
QPSX_REAL_TEST_BASE_PROFILE := $(BUILD_DIR)/qpsx-real-test.base-profile
QPSX_REAL_TEST_CORE_PROFILE := $(BUILD_DIR)/qpsx-real-test.core-profile
QPSX_REAL_TEST_CORE_STAMP := $(BUILD_DIR)/qpsx-real-test.core-installed
QPSX_OPTIMIZE ?= -O2
QPSX_TEST_CORE ?= $(BUILD_DIR)/sdcard/sf2000/cores/sf2000-qpsx
QPSX_AUDIT_STAMP := $(BUILD_DIR)/sdcard/sf2000/cores/.qpsx-mips32r1-audited
QPSX_REAL_CORE_DEP ?= qpsx-mips32r1-audit
QPSX_BENCHMARK_SD_TARGET ?= qpsx-no-menu-test-sd
QPSX_BENCHMARK_ASD_TARGET ?= linux-full-asd
QPSX_BENCHMARK_SECONDS ?= 25
SDCARD_QPSX_STARTUP_CONFIG := $(BUILD_DIR)/sdcard/cores/config/psx_startup.cfg
SDCARD_QPSX_STARTUP_CHECKSUM := $(BUILD_DIR)/sdcard/cores/config/psx_startup.cfg.sha256
FRONTEND_LIFECYCLE_TEST_SD := $(BUILD_DIR)/frontend-lifecycle-test.sd.img
JS2300_TEST_SD := $(BUILD_DIR)/js2300-test.sd.img
JS2300_UI_SMOKE_SCRIPT := $(FRONTEND_PROJECT)/tests/js2300-ui-smoke.js
BOOTROM_BUGFIX ?= /root/host-frogdev/universal/orig_firmware/UpdateFirmware/SF2000_XMC_XM25QH40B_4mbit_bugfix.bin
STOCK_ASD ?= /root/host-frogdev/universal/orig_firmware/bisrv_08_03.asd
QEMU_ORACLE_ARGS = QEMU_JOBS='$(JOBS)' FIRMWARE_BUGFIX='$(BOOTROM_BUGFIX)' ASD='$(STOCK_ASD)'
UNIFROG_DIR ?= $(abspath ../UniFrog)
HCRTOS_SDK_URL ?= https://github.com/axgdev/unifrog-hcrtos-sdk.git
HCRTOS_SDK_BRANCH ?= 260819_1
HCRTOS_SDK_COMMIT ?= d319f166f5910752d11bea14bb49d0a81b98ab66
HCRTOS_SDK_CACHE ?= $(abspath .deps/unifrog-hcrtos-sdk)
HCRTOS_SDK_DEFAULT_DIR := $(if $(wildcard ../unifrog-hcrtos-sdk/lib/vendor/libge.a),$(abspath ../unifrog-hcrtos-sdk),$(HCRTOS_SDK_CACHE))
HCRTOS_SDK_DIR ?= $(HCRTOS_SDK_DEFAULT_DIR)
HCRTOS_SDK_STAMP := $(BUILD_DIR)/.hcrtos-sdk-$(HCRTOS_SDK_BRANCH)-$(HCRTOS_SDK_COMMIT)
UNIFROG_ASD ?= $(UNIFROG_DIR)/bisrv.asd
UNIFROG_SD_ROOT ?= $(UNIFROG_DIR)/output/sdcard
UNIFROG_QEMU_SD := $(BUILD_DIR)/unifrog-qemu.sd.img
QEMU_BOOT_TIMEOUT ?= 90s
QEMU_PANEL_PROBE_TIMEOUT ?= 10s
METRICS_LOG ?= /root/host-frogdev/universal/latest_log/sf2000_linux/loglinux0027.txt
PHYSICAL_CONTRACT_LOG ?= /root/host-frogdev/universal/latest_log/sf2000_linux/log92.txt
QEMU_CONTRACT_LOG ?= $(BUILD_DIR)/logs/linux-full-display.log
QEMU_BENCH_SECONDS ?= 15
QEMU_DISPLAY_ARGS ?=
QEMU_FIDELITY_ARGS ?= -icount shift=1,sleep=on,align=on
GE_VENDOR_ARCHIVE ?= $(HCRTOS_SDK_DIR)/lib/vendor/libge.a
GE_REVERSE_DIR := $(BUILD_DIR)/reverse-ge
GE_NODE_TEST := $(BUILD_DIR)/hcge-node-test
GE_VENDOR_NODE_TEST := $(BUILD_DIR)/hcge-vendor-node-test
GE_LINUX_OBJ := $(BUILD_DIR)/hcge-linux.o
GE_VENDOR_CAPTURE := $(BUILD_DIR)/hcge-vendor-capture
GE_SOURCE_CAPTURE := $(BUILD_DIR)/hcge-source-capture
GE_VENDOR_CAPTURE_GOLDEN := ge/hcge_vendor_capture.golden
GE_SOURCE_CAPTURE_GOLDEN := ge/hcge_source_capture.golden
GE_ELF_CC := $(TARGET_CC_RUN)

FFMPEG_VERSION ?= 8.1.2
FFMPEG_URL := https://ffmpeg.org/releases/ffmpeg-$(FFMPEG_VERSION).tar.xz
FFMPEG_SRC ?= /tmp/sf2000-ffmpeg/ffmpeg-$(FFMPEG_VERSION)
FFMPEG_OUT := $(BUILD_DIR)/ffmpeg
FFMPEG_INSTALL := $(abspath $(FFMPEG_OUT)/install)
FFMPEG_STAMP := $(FFMPEG_OUT)/.stamp-built

USERSPACE_DEVTEST_SRC := userspace/sf2000-devtest.c
USERSPACE_DEVTEST := $(ROOTFS_GENERATED_OVERLAY)/usr/sbin/sf2000-devtest
USERSPACE_EFUSE_DEVICE := $(ROOTFS_GENERATED_OVERLAY)/usr/bin/test_efuse_device
USERSPACE_VDEC_DEVICE := $(ROOTFS_GENERATED_OVERLAY)/usr/bin/test_vdec_device
USERSPACE_DSC_DEVICE := $(ROOTFS_GENERATED_OVERLAY)/usr/bin/test_dsc_device
USERSPACE_PLAYER := $(ROOTFS_GENERATED_OVERLAY)/usr/bin/sf2000-player

PLAYER_TEST_SD := $(BUILD_DIR)/player-test.sd.img
PLAYER_TEST_WAV := $(BUILD_DIR)/test-tone.wav
MKTESTWAV := $(BUILD_DIR)/mktestwav

SMOKE_INIT_PATTERN ?= sf2000_linux: init alive
LOADER_CFLAGS := -Os -ffreestanding -fno-builtin -nostdlib \
	-march=mips32 -mabi=32 -msoft-float -mno-abicalls -fno-pic -mno-gpopt -G 0 \
	-Wall -Wextra

.PHONY: all help check check-linux-early-handoff check-linux-cacheflush memory-layout-audit check-vendor hcrtos-sdk elf-audit status build-info ci-fresh ci-fresh-js2300 qemu rootfs full full-reconfigure toolchain audio-test linux linux-reextract linux-reconfigure linux-asd linux-full physical-linux-asd \
	linux-full-asd linux-full-test-asd sdcard-linux sdcard-full linux-rom-sd \
	linux-full-rom-sd sdcard-zips run-linux smoke-linux run-linux-asd \
	smoke-linux-asd run-linux-rom smoke-linux-rom run-linux-full-asd \
	smoke-linux-full-asd smoke-linux-full-ge-no-irq smoke-linux-full-handoff-quiet smoke-linux-full-stale-ram run-linux-full-storage \
	smoke-linux-full-storage run-linux-full-storage-fast \
	smoke-linux-full-storage-fast run-linux-full-storage-writeback \
smoke-linux-full-storage-writeback run-linux-full-storage-probe-writeback \
smoke-linux-full-storage-probe-writeback run-linux-full-storage-enumeration \
smoke-linux-full-storage-enumeration smoke-linux-full-persistent-storage \
smoke-linux-full-partitioned-storage \
smoke-linux-full-fat16-storage \
smoke-linux-full-exfat-storage \
smoke-linux-full-mixed-fs-storage \
smoke-linux-full-superfloppy-storage \
run-linux-full-rom \
run-linux-full-storage-launch smoke-linux-full-storage-launch \
run-qemu-stock-fatfs-writeback smoke-qemu-stock-fatfs-writeback \
	smoke-linux-full-rom run-linux-full-display \
	smoke-linux-full-display \
	smoke-linux-full-fb-test run-linux-full-panel \
	smoke-linux-full-panel run-linux-full-panel-fast \
	smoke-linux-full-panel-fast full-panel-probe-link run-linux-input smoke-linux-input \
	run-linux-power smoke-linux-power \
	run-linux-frontend smoke-linux-frontend \
	run-linux-js2300 smoke-linux-js2300 \
	run-linux-gpsp smoke-linux-gpsp run-linux-fceumm smoke-linux-fceumm \
	run-linux-snes9x2005 smoke-linux-snes9x2005 \
	run-linux-snes9x2002 smoke-linux-snes9x2002 \
	gpsp-real-test-sd run-linux-gpsp-real smoke-linux-gpsp-real \
	qpsx-mips32r1-audit qpsx-real-test-sd run-linux-qpsx-real smoke-linux-qpsx-real \
	qpsx-no-menu-test-sd run-linux-qpsx-no-menu smoke-linux-qpsx-no-menu \
	qpsx-dev-real-test-sd qpsx-dev-no-menu-test-sd \
	run-linux-qpsx-attract-benchmark benchmark-linux-qpsx-attract \
	benchmark-linux-qpsx-attract-dev \
	qpsx-no-menu-physical \
	gpsp-smc-test-roms run-linux-gpsp-smc smoke-linux-gpsp-smc \
	run-linux-frontend-lifecycle smoke-linux-frontend-lifecycle \
	run-linux-full-input smoke-linux-full-input \
	metrics-linux metrics-frontend metrics-qemu-fidelity benchmark-qemu-linux \
	run-linux-full-fidelity smoke-linux-full-fidelity \
	smoke-linux-physical-contract metrics-qemu-timing \
	run-linux-reboot smoke-linux-reboot run-linux-full-reboot \
	smoke-linux-full-reboot run-linux-full-reset-snapshot \
	run-linux-full-audio smoke-linux-full-audio \
	run-linux-full-audio-gb300 smoke-linux-full-audio-44100 \
	smoke-linux-full-audio-gb300 \
	run-qemu-unifrog smoke-qemu-unifrog \
	run-qemu-unifrog-display smoke-qemu-unifrog-display \
	smoke-linux-full-reset-snapshot run-linux-full-reset-restore \
	smoke-linux-full-reset-restore reverse-ge test-ge-node \
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

help:
	@printf '%s\n' \
		'make check                 fast host-side regression suite' \
		'make check-vendor          source/vendor GE parity tests' \
		'make hcrtos-sdk            verify or fetch the pinned public HCRTOS SDK' \
		'make toolchain              download, verify, and unpack the pinned frog-toolchain' \
		'make TOOLCHAIN_DIR=<prefix> linux-full-asd  use an already installed frog-toolchain' \
		'make linux-full-asd        build the physical-device artifact' \
		'make physical-linux-asd    explicit alias for the physical full-rootfs ASD' \
		'make sdcard-zips           create standalone and bootloader SD-card ZIPs' \
		'make ci-fresh              test a fresh Linux/frontend checkout' \
		'make CI_FRESH_TARGET=sdcard-zips ci-fresh  test a fresh full package graph' \
		'make elf-audit             reject bFLT/dynamic ELF in the rootfs' \
		'make METRICS_LOG=loglinux.txt metrics-frontend  summarize emulator sessions' \
		'make smoke-linux-full-asd  boot the full-rootfs artifact in QEMU' \
		'make smoke-linux-full-ge-no-irq  boot with the GE completion IRQ suppressed' \
		'make smoke-linux-full-stale-ram  boot with stale garbage prefill in RAM' \
		'make ROOTFS=full qpsx-no-menu-physical  stage a temporary direct-game QPSX SD diagnostic'

ci-fresh:
	@set -eu; \
	work="$$(mktemp -d "$${TMPDIR:-/tmp}/sf2000-linux-ci.XXXXXX")"; \
	trap 'status=$$?; rm -rf "$$work"; exit $$status' EXIT HUP INT TERM; \
	linux="$$work/linux"; frontend="$$work/frontend"; \
	git clone --no-local --quiet '$(abspath .)' "$$linux"; \
	git clone --no-local --quiet '$(abspath $(FRONTEND_PROJECT))' "$$frontend"; \
	$(MAKE) -C "$$linux" '$(CI_FRESH_TARGET)' \
		BUILD_DIR="$$work/build" \
		FRONTEND_PROJECT="$$frontend" \
		FROG_TOOLCHAIN_ARCHIVE='$(abspath $(FROG_TOOLCHAIN_ARCHIVE))' \
		FROG_TOOLCHAIN_WORK="$$work/toolchain" \
		LINUX_SRC="$$work/kernel" \
		LINUX_SLIM_ARCHIVE='$(abspath $(LINUX_SLIM_ARCHIVE))' \
		BUSYBOX_WORK="$$work/busybox" \
		BUSYBOX_ARCHIVE='$(abspath $(BUSYBOX_ARCHIVE))' \
		FB_TEST_APP_WORK="$$work/fb-test-app" \
		FB_TEST_APP_ARCHIVE='$(abspath $(FB_TEST_APP_ARCHIVE))' \
		CCACHE='$(CCACHE)' \
		CCACHE_DIR='$(abspath $(CCACHE_DIR))' \
		JOBS='$(JOBS)' SDCARD_RELEASE_ID=ci-fresh; \
	printf 'fresh CI target passed: %s\n' '$(CI_FRESH_TARGET)'

ci-fresh-js2300: $(USERSPACE_JS2300)
	@printf 'fresh JS2300 userspace target passed: %s\n' '$(USERSPACE_JS2300)'

check: audio-test efuse-test vdec-test vdec-codec-test dsc-test test-ge-node \
	memory-layout-audit check-linux-early-handoff check-linux-cacheflush

check-linux-early-handoff:
	grep -Fq 'sf2000_watchdog_arm("early-watchdog-armed")' $(LINUX_PATCHES)
	grep -Fq 'if (!IS_ENABLED(CONFIG_MIPS_SF2000))' $(LINUX_PATCHES)
	grep -Fq 'sf2000_init_mark("start-after-setup-arch")' $(LINUX_PATCHES)
	grep -Fq 'sf2000_before_irq_enable();' $(LINUX_PATCHES)
	grep -Fq 'clear_c0_cause(CAUSEF_IV);' $(LINUX_PATCHES)
	grep -Fq 'change_c0_intctl(0x3e0, 0);' $(LINUX_PATCHES)

# A QEMU display smoke cannot model the physical HC15xx's non-coherent L1.
# Keep the physical-device prerequisite explicit: the generated NOMMU kernel
# must implement the D-cache part of userspace cacheflush(BCACHE), not merely
# the historical instruction-cache-only path.
check-linux-cacheflush: $(LINUX_SRC)/.patched $(LINUX_CONFIG_STAMP)
	grep -Fq '#include <asm/cachectl.h>' '$(LINUX_SRC)/arch/mips/mm/cache.c'
	grep -Fq '#include <asm/io.h>' '$(LINUX_SRC)/arch/mips/mm/cache.c'
	grep -q '^CONFIG_DMA_NONCOHERENT=y$$' '$(LINUX_OUT)/.config'
	grep -Fq 'if (cache & DCACHE)' '$(LINUX_SRC)/arch/mips/mm/cache.c'
	grep -Fq 'dma_cache_wback_inv(addr, bytes);' '$(LINUX_SRC)/arch/mips/mm/cache.c'
	grep -Fq 'if (cache & ICACHE)' '$(LINUX_SRC)/arch/mips/mm/cache.c'

check-vendor: check $(GE_VENDOR_ARCHIVE) test-ge-utils test-ge-matrix test-ge-queue

hcrtos-sdk: $(HCRTOS_SDK_STAMP)

$(HCRTOS_SDK_STAMP):
	@set -eu; \
		sdk='$(HCRTOS_SDK_DIR)'; \
		cache='$(HCRTOS_SDK_CACHE)'; \
		if test -f "$$sdk/lib/vendor/libge.a"; then \
			test -e "$$sdk/.git" || { \
				printf 'HCRTOS_SDK_DIR must be a git checkout: %s\n' "$$sdk" >&2; \
				exit 1; \
			}; \
			actual="$$(git -C "$$sdk" rev-parse HEAD)"; \
			test "$$actual" = '$(HCRTOS_SDK_COMMIT)' || { \
				printf 'HCRTOS SDK at %s is %s; expected %s\n' \
					"$$sdk" "$$actual" '$(HCRTOS_SDK_COMMIT)' >&2; \
				printf 'use HCRTOS_SDK_DIR for a different SDK only when its ABI is intentional\n' >&2; \
				exit 1; \
			}; \
		else \
			test "$$sdk" = "$$cache" || { \
				printf 'HCRTOS SDK is missing from %s; refusing to clone into an explicit HCRTOS_SDK_DIR\n' "$$sdk" >&2; \
				exit 1; \
			}; \
			if test -e "$$sdk/.git"; then \
				git -C "$$sdk" remote set-url origin '$(HCRTOS_SDK_URL)'; \
			else \
				test ! -e "$$sdk" || { \
					printf 'HCRTOS SDK cache exists but is not a git checkout: %s\n' "$$sdk" >&2; \
					exit 1; \
				}; \
				mkdir -p "$$(dirname "$$sdk")"; \
				git clone --filter=blob:none --no-checkout '$(HCRTOS_SDK_URL)' "$$sdk"; \
			fi; \
			git -C "$$sdk" remote set-url origin '$(HCRTOS_SDK_URL)'; \
			git -C "$$sdk" fetch --depth=1 origin 'refs/heads/$(HCRTOS_SDK_BRANCH)'; \
			git -C "$$sdk" checkout --detach '$(HCRTOS_SDK_COMMIT)'; \
			test "$$(git -C "$$sdk" rev-parse HEAD)" = '$(HCRTOS_SDK_COMMIT)'; \
			test "$$(git -C "$$sdk" rev-parse FETCH_HEAD)" = '$(HCRTOS_SDK_COMMIT)'; \
		fi; \
		test -f "$$sdk/lib/vendor/libge.a"; \
		mkdir -p '$(dir $@)'; \
		{ \
			printf 'url=%s\n' '$(HCRTOS_SDK_URL)'; \
			printf 'branch=%s\n' '$(HCRTOS_SDK_BRANCH)'; \
			printf 'commit=%s\n' '$(HCRTOS_SDK_COMMIT)'; \
			printf 'path=%s\n' "$$sdk"; \
		} > '$@.tmp'; \
		mv '$@.tmp' '$@'

status:
	@printf 'sf2000_linux workspace\n'
	@printf '  qemu:    %s\n' '$(QEMU_DIR)'
	@printf '  hclinux: %s\n' '$(HCLINUX_DIR)'
	@printf '  host cc: %s\n' '$(HOSTCC)'
	@printf '  userspace: direct BusyBox + fb-test-app\n'
	@printf '  toolchain: %s\n' '$(CROSS_COMPILE)'
	@printf '  ccache:  %s\n' '$(if $(strip $(CCACHE)),$(CCACHE),disabled)'
	@printf '  ccache dir: %s\n' '$(abspath $(CCACHE_DIR))'
	@printf '  hcrtos sdk: %s\n' '$(HCRTOS_SDK_DIR)'
	@printf '  hcrtos sdk exists: '
	@test -f '$(GE_VENDOR_ARCHIVE)' && printf 'yes\n' || printf 'no\n'
	@printf '  rootfs:  %s\n' '$(ROOTFS)'
	@printf '  rootfs build: %s\n' '$(ROOTFS_FULL_CPIO)'
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
	@printf '  full rootfs cpio exists: '
	@test -f '$(ROOTFS_FULL_CPIO)' && printf 'yes\n' || printf 'no\n'
	@printf '  bugfix ROM exists: '
	@test -f '$(BOOTROM_BUGFIX)' && printf 'yes\n' || printf 'no\n'

$(GE_VENDOR_ARCHIVE): $(HCRTOS_SDK_STAMP)
	@test -f '$@'

reverse-ge: $(GE_VENDOR_ARCHIVE)
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
		$(TOOLCHAIN_STAMP) $(GE_VENDOR_ARCHIVE)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_utils_test.c '$(GE_VENDOR_ARCHIVE)'

test-ge-utils: $(GE_UTILS_TEST) $(GE_VENDOR_UTILS_TEST)
	qemu-mipsel '$(GE_VENDOR_UTILS_TEST)' > '$(BUILD_DIR)/hcge-utils.vendor'
	'$(GE_UTILS_TEST)' | cmp - '$(BUILD_DIR)/hcge-utils.vendor'

$(GE_MATRIX_TEST): ge/hcge_matrix.c ge/hcge_matrix_test.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_matrix.c ge/hcge_matrix_test.c -lm

$(GE_VENDOR_MATRIX_TEST): ge/hcge_matrix_test.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP) $(GE_VENDOR_ARCHIVE)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_matrix_test.c '$(GE_VENDOR_ARCHIVE)' -lm

test-ge-matrix: $(GE_MATRIX_TEST) $(GE_VENDOR_MATRIX_TEST)
	qemu-mipsel '$(GE_VENDOR_MATRIX_TEST)' > '$(BUILD_DIR)/hcge-matrix.vendor'
	qemu-mipsel '$(GE_MATRIX_TEST)' | cmp - '$(BUILD_DIR)/hcge-matrix.vendor'

$(GE_QUEUE_TEST): ge/hcge_linux.c ge/hcge_node.c ge/hcge_queue_test.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -ffunction-sections \
		-Wl,--gc-sections -o '$@' ge/hcge_linux.c ge/hcge_node.c \
		ge/hcge_queue_test.c

$(GE_VENDOR_QUEUE_TEST): ge/hcge_queue_test.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP) $(GE_VENDOR_ARCHIVE)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_queue_test.c '$(GE_VENDOR_ARCHIVE)'

test-ge-queue: $(GE_QUEUE_TEST) $(GE_VENDOR_QUEUE_TEST)
	qemu-mipsel '$(GE_VENDOR_QUEUE_TEST)' > '$(BUILD_DIR)/hcge-queue.vendor'
	qemu-mipsel '$(GE_QUEUE_TEST)' | cmp - '$(BUILD_DIR)/hcge-queue.vendor'

$(GE_BATCH_TEST): ge/hcge_linux.c ge/hcge_node.c ge/hcge_batch_test.c \
		ge/ge_api.h $(TOOLCHAIN_STAMP)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -ffunction-sections \
		-Wl,--gc-sections -Wl,--wrap=ioctl -o '$@' ge/hcge_linux.c \
		ge/hcge_node.c ge/hcge_batch_test.c

test-ge-batch: $(GE_BATCH_TEST)
	qemu-mipsel '$(GE_BATCH_TEST)' | grep -q \
		'^ret=0 calls=2 words=7 data=1,2,3,4,5,6,7$$'

$(GE_FILTER_TEST): ge/hcge_filter.c ge/hcge_filter_test.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' \
		ge/hcge_filter.c ge/hcge_filter_test.c -lm

$(GE_VENDOR_FILTER_TEST): ge/hcge_filter_test.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP) $(GE_VENDOR_ARCHIVE)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' \
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
		ge/hcge_vendor_compare.c $(TOOLCHAIN_STAMP) reverse-ge
	$(GE_ELF_CC) -std=c99 -O2 -ffunction-sections -fdata-sections \
		-Wl,--gc-sections -static -Ige -o '$@' ge/hcge_node.c \
		ge/hcge_vendor_compare.c '$(GE_REVERSE_DIR)'/hcge_node_ctx.c.o

test-ge-node-vendor: $(GE_VENDOR_NODE_TEST)
	qemu-mipsel '$(GE_VENDOR_NODE_TEST)'

$(GE_LINUX_OBJ): ge/hcge_linux.c ge/ge_api.h $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) -std=c99 -Os -Wall -Wextra -Werror -Ige -c -o '$@' '$<'

$(GE_VENDOR_CAPTURE): ge/hcge_vendor_capture.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP) $(GE_VENDOR_ARCHIVE)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -o '$@' '$<' \
		'$(GE_VENDOR_ARCHIVE)' -lm -Wl,--wrap=open -Wl,--wrap=close \
		-Wl,--wrap=ioctl -Wl,--wrap=mmap -Wl,--wrap=munmap \
		-Wl,--wrap=usleep

capture-ge-vendor: $(GE_VENDOR_CAPTURE)
	qemu-mipsel '$(GE_VENDOR_CAPTURE)'

test-ge-vendor-capture: $(GE_VENDOR_CAPTURE) $(GE_VENDOR_CAPTURE_GOLDEN)
	qemu-mipsel '$(GE_VENDOR_CAPTURE)' | cmp - '$(GE_VENDOR_CAPTURE_GOLDEN)'

$(GE_SOURCE_CAPTURE): ge/hcge_vendor_capture.c ge/hcge_linux.c ge/hcge_node.c ge/ge_api.h \
		$(TOOLCHAIN_STAMP)
	$(GE_ELF_CC) -std=c99 -O2 -static -Ige -DHCGE_SOURCE_CAPTURE \
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
toolchain: $(TOOLCHAIN_STAMP)
direct-rootfs: $(ROOTFS_FULL_CPIO)
full: $(ROOTFS_FULL_CPIO)

rootfs-reconfigure:
	$(MAKE) ROOTFS=full direct-rootfs

full-reconfigure:
	$(MAKE) ROOTFS=full direct-rootfs

$(BUSYBOX_ARCHIVE):
	mkdir -p '$(dir $@)'
	set -eu; \
	tmp='$@.tmp'; \
	rm -f "$$tmp"; \
	$(CURL_DOWNLOAD) -o "$$tmp" '$(BUSYBOX_URL)'; \
	test -s "$$tmp"; \
	mv "$$tmp" '$@'

$(BUSYBOX_ARCHIVE_CHECK): $(BUSYBOX_ARCHIVE) Makefile
	test "$$(sha256sum '$<' | awk '{print $$1}')" = '$(BUSYBOX_SHA256)'
	printf 'sha256=%s\narchive=%s\n' '$(BUSYBOX_SHA256)' '$(abspath $(BUSYBOX_ARCHIVE))' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

$(BUSYBOX_SOURCE_STAMP): $(BUSYBOX_ARCHIVE_CHECK)
	mkdir -p '$(BUSYBOX_WORK)' '$(dir $@)'
	rm -rf '$(BUSYBOX_SRC)'
	mkdir -p '$(BUSYBOX_SRC)'
	tar -xjf '$(BUSYBOX_ARCHIVE)' -C '$(BUSYBOX_SRC)' --strip-components=1
	touch '$@'

$(BUSYBOX_PATCH_STAMP): $(BUSYBOX_SOURCE_STAMP) $(BUSYBOX_PATCHES)
	@for patch_file in $(BUSYBOX_PATCHES); do \
		patch -d '$(BUSYBOX_SRC)' --dry-run -p1 < "$$patch_file" >/dev/null 2>&1 || \
			{ printf 'cannot apply %s to the clean BusyBox tree\n' "$$patch_file" >&2; exit 1; }; \
		patch -d '$(BUSYBOX_SRC)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(BUSYBOX_OUT)/.config: $(BUSYBOX_PATCH_STAMP) $(BUSYBOX_CONFIG) Makefile
	mkdir -p '$(BUSYBOX_OUT)'
	cp '$(BUSYBOX_CONFIG)' '$@.tmp'
	mv '$@.tmp' '$@'
	yes '' | KCONFIG_NOTIMESTAMP=1 $(MAKE) -C '$(BUSYBOX_SRC)' \
		O='$(abspath $(BUSYBOX_OUT))' oldconfig

BUSYBOX_CFLAGS := -Os -g0 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE \
	-D_FILE_OFFSET_BITS=64 -G0 -fPIC -mabicalls -march=mips32 -mabi=32 \
	-msoft-float -ffunction-sections -fdata-sections -fno-guess-branch-probability \
	-funsigned-char -fomit-frame-pointer -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -fno-builtin-printf -Oz
BUSYBOX_LDFLAGS := -static-pie -specs=$(abspath $(TOOLCHAIN_SPECS))
BUSYBOX_CC := $(CCACHE_COMPILE)$(TARGET_CC)

$(BUSYBOX_STAMP): $(BUSYBOX_OUT)/.config $(TOOLCHAIN_SPECS) Makefile
	mkdir -p '$(BUSYBOX_ROOT)'
	rm -rf '$(BUSYBOX_ROOT)'
	mkdir -p '$(BUSYBOX_ROOT)'
	$(MAKE) -C '$(BUSYBOX_SRC)' O='$(abspath $(BUSYBOX_OUT))' -j'$(JOBS)' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' \
		CC='$(BUSYBOX_CC)' AR='$(CROSS_COMPILE)ar' NM='$(CROSS_COMPILE)nm' \
		RANLIB='$(CROSS_COMPILE)ranlib' CFLAGS='$(BUSYBOX_CFLAGS)' \
		EXTRA_LDFLAGS='$(BUSYBOX_LDFLAGS)' CONFIG_PREFIX='$(abspath $(BUSYBOX_ROOT))' \
		PREFIX='$(abspath $(BUSYBOX_ROOT))' SKIP_STRIP=y install-noclobber
	'$(TARGET_STRIP)' --strip-all '$(BUSYBOX_ROOT)'/bin/busybox
	touch '$@'

$(FB_TEST_APP_ARCHIVE):
	mkdir -p '$(dir $@)'
	set -eu; \
	tmp='$@.tmp'; \
	rm -f "$$tmp"; \
	$(CURL_DOWNLOAD) -o "$$tmp" '$(FB_TEST_APP_URL)'; \
	test -s "$$tmp"; \
	mv "$$tmp" '$@'

$(FB_TEST_APP_ARCHIVE_CHECK): $(FB_TEST_APP_ARCHIVE) Makefile
	test "$$(sha256sum '$<' | awk '{print $$1}')" = '$(FB_TEST_APP_SHA256)'
	printf 'sha256=%s\narchive=%s\n' '$(FB_TEST_APP_SHA256)' '$(abspath $(FB_TEST_APP_ARCHIVE))' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

$(FB_TEST_APP_SOURCE_STAMP): $(FB_TEST_APP_ARCHIVE_CHECK)
	mkdir -p '$(FB_TEST_APP_WORK)' '$(dir $@)'
	rm -rf '$(FB_TEST_APP_SRC)'
	mkdir -p '$(FB_TEST_APP_SRC)'
	tar -xzf '$(FB_TEST_APP_ARCHIVE)' -C '$(FB_TEST_APP_SRC)' --strip-components=1
	touch '$@'

FB_TEST_APP_CFLAGS := -Os -G0 -fPIC -mabicalls -march=mips32 -mabi=32 -msoft-float
FB_TEST_APP_LDFLAGS := -static-pie -specs=$(abspath $(TOOLCHAIN_SPECS))

$(FB_TEST_APP_STAMP): $(FB_TEST_APP_SOURCE_STAMP) $(TOOLCHAIN_SPECS) Makefile
	mkdir -p '$(FB_TEST_APP_OUT)/install'
	$(MAKE) -C '$(FB_TEST_APP_SRC)' -j'$(JOBS)' \
		CROSS_COMPILE='$(CCACHE_COMPILE)$(CROSS_COMPILE)' \
		CFLAGS='$(FB_TEST_APP_CFLAGS)' LDFLAGS='$(FB_TEST_APP_LDFLAGS)'
	for program in perf rect fb-test offset fb-string; do \
		case "$$program" in \
			perf) installed=fb-test-perf ;; \
			rect) installed=fb-test-rect ;; \
			fb-test) installed=fb-test ;; \
			offset) installed=fb-test-offset ;; \
			fb-string) installed=fb-test-string ;; \
			*) printf 'unexpected fb-test-app program: %s\n' "$$program" >&2; exit 1 ;; \
		esac; \
		cp '$(FB_TEST_APP_SRC)'/$$program '$(FB_TEST_APP_OUT)/install/'"$$installed"; \
		'$(TARGET_STRIP)' --strip-all '$(FB_TEST_APP_OUT)/install/'"$$installed"; \
	done
	touch '$@'

$(INIT_BIN): init/sf2000-init.S $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(CC_MIPS_RUN) $(INIT_CFLAGS) -Os -nostdlib -ffreestanding \
		-march=mips32 -mabi=32 -msoft-float -mabicalls \
		-fPIE -pie -mno-gpopt -G 0 \
		-Wl,--no-dynamic-linker -Wl,-e,_start \
		-Wl,--gc-sections -Wl,-z,noexecstack \
		-o '$@' '$<'

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
	'$(GEN_INIT_CPIO)' -t '$(INITRAMFS_EPOCH)' '$(INITRAMFS_LIST)' > '$@'

$(ROOTFS_GENERATED_OVERLAY_STAMP):
	mkdir -p '$(dir $@)' '$(ROOTFS_GENERATED_OVERLAY)'
	touch '$@'

$(TOOLCHAIN_STAMP): $(FROG_TOOLCHAIN_SETUP_STAMP) Makefile
	mkdir -p '$(dir $@)'
	test -x '$(TARGET_CC)'
	test -x '$(TARGET_CXX)'
	test -x '$(TARGET_STRIP)'
	test "$$( $(TARGET_CC) -dumpmachine )" = '$(TOOLCHAIN_TUPLE)'
	test -f '$(PIE_SYSROOT)/rcrt1.o'
	test -f '$(PIE_SYSROOT)/crti.o'
	test -f '$(PIE_SYSROOT)/crtn.o'
	test -f '$(PIE_SYSROOT)/libc.a'
	test -f "$$( '$(TARGET_CC)' -print-file-name=crtbeginS.o)"
	printf 'compiler=%s\ntuple=%s\nsysroot=%s\n' \
		'$(abspath $(TARGET_CC))' '$(TOOLCHAIN_TUPLE)' \
		'$(abspath $(PIE_SYSROOT))' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

$(PIE_STAMP): $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	test -f '$(PIE_SYSROOT)/rcrt1.o'
	test -f "$$('$(TARGET_CC)' -print-file-name=crtbeginS.o)"
	touch '$@'


# The panel-probe targets deliberately override USERSPACE_INIT_SOURCE to run
# sf2000-screen as /init.  A later normal build must restore the real init
# binary; without a content check, make sees the same destination and silently
# leaves the probe executable in the rootfs (and therefore in the ASD image).
$(USERSPACE_INIT): FORCE $(USERSPACE_INIT_SOURCE) Makefile
	mkdir -p '$(dir $@)'
	if ! cmp -s '$(USERSPACE_INIT_SOURCE)' '$@' 2>/dev/null; then \
		cp '$(USERSPACE_INIT_SOURCE)' '$@'; \
		chmod 0755 '$@'; \
	fi

$(USERSPACE_SUPERVISOR): $(USERSPACE_INIT_SRC) $(USERSPACE_INIT_CLONE) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_SUPERVISOR_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_INIT_CLONE)' '$(USERSPACE_INIT_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_PAD): $(USERSPACE_PAD_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_PAD_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_POWERD): $(USERSPACE_POWERD_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		$(USERSPACE_POWERD_CPPFLAGS) '$<' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o


$(FRONTEND_UI_STAMP): $(FRONTEND_UI_SOURCES)
	$(FRONTEND_MAKE) browser js2300-ui \
		CROSS_COMPILE='$(patsubst %gcc,%,$(TARGET_CC))'
	test -s '$(FRONTEND_PROJECT)/build/sf2000-browser'
	test -s '$(FRONTEND_PROJECT)/build/sf2000-js2300-ui'
	mkdir -p '$(dir $@)'
	touch '$@'

$(USERSPACE_FRONTEND): $(FRONTEND_UI_STAMP)
	mkdir -p '$(dir $@)'
	cp '$(FRONTEND_PROJECT)'/build/sf2000-browser '$@'

$(USERSPACE_JS2300): $(FRONTEND_UI_STAMP)
	mkdir -p '$(dir $@)'
	cp '$(FRONTEND_PROJECT)'/build/sf2000-js2300-ui '$@'

$(USERSPACE_AUDIO): $(USERSPACE_AUDIO_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_AUDIO_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_HEARTBEAT): $(USERSPACE_HEARTBEAT_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$<' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_LOGD): $(USERSPACE_LOGD_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$<' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_MOUNT): $(USERSPACE_MOUNT_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$<' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_SCREEN_SOURCE_STAMP): FORCE $(USERSPACE_SCREEN_SRC) \
		ge/hcge_linux.c ge/hcge_node.c \
		ge/hcge_node.h ge/ge_api.h Makefile
	mkdir -p '$(dir $@)'
	{ sha256sum '$(USERSPACE_SCREEN_SRC)' \
		ge/hcge_linux.c ge/hcge_node.c ge/hcge_node.h ge/ge_api.h; \
		printf '%s\n' '$(USERSPACE_PIE_CFLAGS)' '$(USERSPACE_SCREEN_CFLAGS)' \
			'$(USERSPACE_PIE_LDFLAGS)'; \
	} > '$@.tmp'
	if cmp -s '$@.tmp' '$@'; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

$(USERSPACE_SCREEN): $(USERSPACE_SCREEN_SOURCE_STAMP) $(PIE_STAMP) $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	SOURCE_DATE_EPOCH=0 $(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_SCREEN_CFLAGS) \
		-Ige $(USERSPACE_PIE_LDFLAGS) -o '$@' \
		'$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_SCREEN_SRC)' ge/hcge_linux.c ge/hcge_node.c \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

full-panel-probe-link: $(USERSPACE_PANEL_PROBE_LINK)

$(USERSPACE_PANEL_PROBE_LINK): $(USERSPACE_PANEL_FASTPROBE) Makefile
	mkdir -p '$(dir $(USERSPACE_PANEL_PROBE_LINK))'
	ln -sf '$(USERSPACE_PANEL_PROBE_TARGET)' '$(USERSPACE_PANEL_PROBE_LINK)'

$(USERSPACE_PANEL_INIT): $(USERSPACE_PANEL_INIT_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_PANEL_INIT_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_PANEL_FASTPROBE): $(USERSPACE_PANEL_FASTPROBE_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_PANEL_FASTPROBE_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_STORAGE_PROBE): $(USERSPACE_STORAGE_PROBE_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) -ffreestanding -fno-builtin \
		$(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_STORAGE_PROBE_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_STORAGE_FASTPROBE): $(USERSPACE_STORAGE_FASTPROBE_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_STORAGE_FASTPROBE_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_RESET_FASTPROBE): $(USERSPACE_RESET_FASTPROBE_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$(USERSPACE_RESET_FASTPROBE_SRC)' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(ROOTFS_FULL_CPIO): $(BUSYBOX_STAMP) $(FB_TEST_APP_STAMP) \
	$(USERSPACE_INIT) $(USERSPACE_SUPERVISOR) $(USERSPACE_PAD) \
	$(USERSPACE_POWERD) $(USERSPACE_FRONTEND) $(USERSPACE_JS2300) \
	$(USERSPACE_AUDIO) $(USERSPACE_HEARTBEAT) $(USERSPACE_LOGD) \
	$(USERSPACE_MOUNT) $(USERSPACE_SCREEN) $(USERSPACE_PANEL_INIT) \
	$(USERSPACE_PANEL_FASTPROBE) $(USERSPACE_PANEL_PROBE_LINK) \
	$(USERSPACE_STORAGE_PROBE) $(USERSPACE_STORAGE_FASTPROBE) \
	$(USERSPACE_RESET_FASTPROBE) $(USERSPACE_EXPERIMENTAL_DEVTESTS) \
	$(USERSPACE_PLAYER) $(ROOTFS_BASE_FILES) $(ROOTFS_OVERLAY_FILES) \
	$(ROOTFS_GENERATED_OVERLAY_STAMP) $(GEN_INIT_CPIO) Makefile
	mkdir -p '$(dir $@)'
	rm -rf '$(ROOTFS_DIR)'
	mkdir -p '$(ROOTFS_DIR)'
	cp -a '$(ROOTFS_BASE)'/. '$(ROOTFS_DIR)'/
	cp -a '$(BUSYBOX_ROOT)'/. '$(ROOTFS_DIR)'/
	cp -a '$(ROOTFS_OVERLAY)'/. '$(ROOTFS_DIR)'/
	cp -a '$(ROOTFS_GENERATED_OVERLAY)'/. '$(ROOTFS_DIR)'/
	mkdir -p '$(ROOTFS_DIR)'/usr/bin '$(ROOTFS_DIR)'/usr/sbin \
		'$(ROOTFS_DIR)'/bin '$(ROOTFS_DIR)'/dev '$(ROOTFS_DIR)'/etc \
		'$(ROOTFS_DIR)'/lib '$(ROOTFS_DIR)'/run '$(ROOTFS_DIR)'/tmp \
		'$(ROOTFS_DIR)'/var '$(ROOTFS_DIR)'/var/lib \
		'$(ROOTFS_DIR)'/media '$(ROOTFS_DIR)'/mnt '$(ROOTFS_DIR)'/opt \
		'$(ROOTFS_DIR)'/proc '$(ROOTFS_DIR)'/root '$(ROOTFS_DIR)'/sys \
		'$(ROOTFS_DIR)'/run/lock \
		'$(ROOTFS_DIR)'/etc/network/if-down.d \
		'$(ROOTFS_DIR)'/etc/network/if-post-down.d \
		'$(ROOTFS_DIR)'/etc/network/if-up.d
	cp '$(FB_TEST_APP_OUT)'/install/fb-test-perf \
		'$(ROOTFS_DIR)'/usr/bin/fb-test-perf
	cp '$(FB_TEST_APP_OUT)'/install/fb-test-rect \
		'$(ROOTFS_DIR)'/usr/bin/fb-test-rect
	cp '$(FB_TEST_APP_OUT)'/install/fb-test \
		'$(ROOTFS_DIR)'/usr/bin/fb-test
	cp '$(FB_TEST_APP_OUT)'/install/fb-test-offset \
		'$(ROOTFS_DIR)'/usr/bin/fb-test-offset
	cp '$(FB_TEST_APP_OUT)'/install/fb-test-string \
		'$(ROOTFS_DIR)'/usr/bin/fb-test-string
	ln -sfn ../proc/self/mounts '$(ROOTFS_DIR)'/etc/mtab
	ln -sfn ../run/resolv.conf '$(ROOTFS_DIR)'/etc/resolv.conf
	ln -sfn ../usr/lib/os-release '$(ROOTFS_DIR)'/etc/os-release
	ln -sfn ../proc/self/fd '$(ROOTFS_DIR)'/dev/fd
	ln -sfn ../tmp/log '$(ROOTFS_DIR)'/dev/log
	ln -sfn ../proc/self/fd/2 '$(ROOTFS_DIR)'/dev/stderr
	ln -sfn ../proc/self/fd/0 '$(ROOTFS_DIR)'/dev/stdin
	ln -sfn ../proc/self/fd/1 '$(ROOTFS_DIR)'/dev/stdout
	ln -sfn lib '$(ROOTFS_DIR)'/lib32
	ln -sfn lib '$(ROOTFS_DIR)'/usr/lib32
	ln -sfn bin/busybox '$(ROOTFS_DIR)'/linuxrc
	ln -sfn ../tmp '$(ROOTFS_DIR)'/var/cache
	ln -sfn ../../tmp '$(ROOTFS_DIR)'/var/lib/misc
	ln -sfn ../run/lock '$(ROOTFS_DIR)'/var/lock
	ln -sfn ../tmp '$(ROOTFS_DIR)'/var/log
	ln -sfn ../run '$(ROOTFS_DIR)'/var/run
	ln -sfn ../tmp '$(ROOTFS_DIR)'/var/spool
	ln -sfn ../tmp '$(ROOTFS_DIR)'/var/tmp
	# These cores are SD extensions, not part of the boot ASD.  Remove stale
	# copies left by an older build before generating the new initramfs.
	rm -f '$(ROOTFS_DIR)'/usr/bin/sf2000-gambatte \
		'$(ROOTFS_DIR)'/usr/bin/sf2000-gpsp \
		'$(ROOTFS_DIR)'/usr/bin/sf2000-fceumm
	@set -e; \
	find '$(ROOTFS_DIR)' -type f -perm /111 -print | \
	while IFS= read -r executable; do \
		magic=$$(od -An -N4 -tx1 "$$executable" | tr -d ' \n'); \
		if test "$$magic" = 62464c54; then \
			printf 'bFLT executable is forbidden: %s\n' "$$executable" >&2; \
			exit 1; \
		fi; \
		if test "$$magic" = 7f454c46; then \
			type=$$('$(READELF_MIPS)' -h "$$executable" | \
				awk '/Type:/ { print $$2; exit }'); \
			test "$$type" = DYN || \
			{ test '$(FIXED_ET_EXEC)' = 1 && test "$$type" = EXEC; } || { \
				printf 'unsupported ELF type %s: %s\n' \
					"$$type" "$$executable" >&2; \
				exit 1; \
			}; \
			if '$(READELF_MIPS)' -l "$$executable" | grep -q INTERP; then \
				printf 'dynamic ELF interpreter is forbidden: %s\n' \
					"$$executable" >&2; \
				exit 1; \
			fi; \
			if test "$$type" = EXEC; then \
				'$(READELF_MIPS)' -lW "$$executable" | \
				awk '$$1 == "LOAD" { print $$3, $$6 }' | \
				while read -r address size; do \
					start=$$((address)); end=$$((address + size)); \
					if test $$start -lt $$((0x85000000)) || \
					   test $$end -gt $$((0x853e0000)); then \
						printf 'ET_EXEC segment outside fixed window: %s\n' \
							"$$executable" >&2; \
						exit 1; \
					fi; \
				done; \
			else \
				if ! '$(READELF_MIPS)' -rW "$$executable" | \
					awk '/R_MIPS_/ { \
						if ($$2 !~ /^0000000[03]$$/ || \
						    ($$3 != "R_MIPS_NONE" && \
						     $$3 != "R_MIPS_REL32")) \
							bad = 1 \
					} END { exit bad }'; then \
					printf 'unsupported static-PIE relocation: %s\n' \
						"$$executable" >&2; \
					exit 1; \
				fi; \
			fi; \
		fi; \
	done
	rm -rf '$(ROOTFS_DIR)'/run/* '$(ROOTFS_DIR)'/tmp/*
	mkdir -p '$(ROOTFS_DIR)'/run/lock
	mkdir -p '$(ROOTFS_DIR)'/dev/input '$(ROOTFS_DIR)'/dev/snd \
		'$(ROOTFS_DIR)'/dev/pts '$(ROOTFS_DIR)'/dev/shm
	{ \
		printf 'nod /dev/console 0622 0 0 c 5 1\n'; \
		printf 'nod /dev/null 0666 0 0 c 1 3\n'; \
		printf 'nod /dev/mem 0640 0 0 c 1 1\n'; \
		printf 'nod /dev/tty 0666 0 0 c 5 0\n'; \
		printf 'nod /dev/ttyS0 0660 0 5 c 4 64\n'; \
		printf 'nod /dev/kmsg 0600 0 0 c 1 11\n'; \
		printf 'nod /dev/mmcblk0 0600 0 0 b 179 0\n'; \
		printf 'nod /dev/mmcblk0p1 0600 0 0 b 179 1\n'; \
		printf 'nod /dev/mmcblk0p2 0600 0 0 b 179 2\n'; \
		printf 'nod /dev/uinput 0660 0 0 c 10 223\n'; \
		printf 'nod /dev/ge 0660 0 0 c 10 243\n'; \
		printf 'nod /dev/fb0 0660 0 0 c 29 0\n'; \
		printf 'nod /dev/snd/pcmC0D0p 0660 0 0 c 116 16\n'; \
		printf 'nod /dev/input/event0 0660 0 0 c 13 64\n'; \
		printf 'nod /dev/input/event1 0660 0 0 c 13 65\n'; \
		printf 'nod /dev/input/event2 0660 0 0 c 13 66\n'; \
		printf 'nod /dev/input/event3 0660 0 0 c 13 67\n'; \
	} > '$(ROOTFS_DEVICE_CPIO_LIST)'
	mkdir -p '$(BUILD_DIR)'/usr
	ln -sf ../gen_init_cpio '$(BUILD_DIR)'/usr/gen_init_cpio
	cd '$(BUILD_DIR)' && '$(abspath $(LINUX_SRC))'/usr/gen_initramfs.sh \
		-o '$(abspath $@)' -d '$(INITRAMFS_DATE)' -u squash -g squash \
		'$(abspath $(ROOTFS_DIR))' \
		'$(abspath $(ROOTFS_DEVICE_CPIO_LIST))'

elf-audit: $(ROOTFS_FULL_CPIO)
	@printf 'ELF-only rootfs audit passed: %s\n' '$(ROOTFS_DIR)'

$(LINUX_ARCHIVE):
	mkdir -p '$(dir $@)'
	set -eu; \
	tmp='$@.tmp'; \
	rm -f "$$tmp"; \
	$(CURL_DOWNLOAD) -o "$$tmp" '$(LINUX_URL)'; \
	test -s "$$tmp"; \
	mv "$$tmp" '$@'

$(LINUX_ARCHIVE_CHECK): $(LINUX_ARCHIVE) Makefile
	test "$$(sha256sum '$<' | awk '{print $$1}')" = '$(LINUX_SHA256)'
	printf 'sha256=%s\narchive=%s\n' '$(LINUX_SHA256)' '$(abspath $(LINUX_ARCHIVE))' > '$@.tmp'
	if cmp -s '$@.tmp' '$@' 2>/dev/null; then rm -f '$@.tmp'; else mv '$@.tmp' '$@'; fi

# Build a reusable source cache from a filtered shallow Git checkout by
# default. The sparse patterns come from the same keep-lists as the pruning
# script, so blobs that would be deleted are never downloaded. The archive
# method remains available as an explicit fallback for offline or mirrored
# environments.
$(LINUX_SLIM_ARCHIVE): scripts/kernel-slim.sh
	set -eu; \
	work='$(LINUX_SLIM_WORK)'; \
	tmp='$(abspath $@.tmp)'; \
	rm -rf "$$work"; \
	rm -f "$$tmp"; \
	mkdir -p '$(dir $@)'; \
	mkdir -p "$$work"; \
	case '$(LINUX_SOURCE_METHOD)' in \
	git) \
		git clone --filter=blob:none --no-tags --no-checkout --depth=1 \
			'$(LINUX_GIT_URL)' "$$work"; \
		git -C "$$work" fetch --depth=1 origin \
			'refs/tags/$(LINUX_GIT_TAG):refs/tags/$(LINUX_GIT_TAG)'; \
		'$(abspath scripts/kernel-slim.sh)' --sparse-patterns | \
			git -C "$$work" sparse-checkout set --no-cone --stdin; \
		git -C "$$work" checkout --detach --force \
			'$(LINUX_GIT_TAG)^{commit}';; \
	archive) \
		$(MAKE) '$(LINUX_ARCHIVE_CHECK)'; \
		tar -xf '$(LINUX_ARCHIVE)' -C "$$work" --strip-components=1;; \
	*) \
		printf 'unsupported LINUX_SOURCE_METHOD: %s\n' '$(LINUX_SOURCE_METHOD)' >&2; \
		exit 2;; \
	esac; \
	'$(abspath scripts/kernel-slim.sh)' "$$work"; \
	rm -rf "$$work/.git"; \
	XZ_OPT='-T0 -6' tar --create --xz --file "$$tmp" \
		--sort=name --mtime='1970-01-01 00:00:00 UTC' \
		--owner=0 --group=0 --numeric-owner --directory "$$work" .; \
	test -s "$$tmp"; \
	mv "$$tmp" '$@'; \
	rm -rf "$$work"

# The archive preserves upstream timestamps, so using its extracted Makefile
# as the source target makes the verified archive marker look newer on every
# invocation. Use a local stamp instead: it is touched after extraction and
# pruning, and it is removed along with the managed source tree when a clean
# setup is requested. Re-extract before every patch application so patches
# never meet a partially-pruned or already-patched tree.
$(LINUX_SRC)/.slimmed: $(LINUX_SLIM_ARCHIVE) scripts/kernel-slim.sh
	rm -rf '$(LINUX_SRC)'
	mkdir -p '$(LINUX_SRC)'
	test -s '$(LINUX_SLIM_ARCHIVE)'
	XZ_OPT='-T0' tar -xJf '$(LINUX_SLIM_ARCHIVE)' -C '$(LINUX_SRC)'
	touch '$@'

$(LINUX_SRC)/.patched: $(LINUX_SRC)/.slimmed $(LINUX_PATCHES)
	@if test -e '$@'; then \
		printf 'linux patch series changed; applying incrementally in %s\n' '$(LINUX_SRC)'; \
	fi
	@for patch in $(LINUX_PATCHES); do \
		if test -e '$@' && ! test "$$patch" -nt '$@'; then \
			continue; \
		fi; \
		if patch --batch --forward -d '$(LINUX_SRC)' --dry-run -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'applying %s\n' "$$patch"; \
			patch --batch --forward -d '$(LINUX_SRC)' -p1 < "$$patch" || exit 1; \
		elif patch --batch -d '$(LINUX_SRC)' --dry-run -R -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'already applied %s\n' "$$patch"; \
		else \
			printf 'cannot apply %s; run make linux-reextract for a clean kernel tree\n' "$$patch" >&2; \
			exit 1; \
		fi; \
	done
	touch '$@'

$(SF2000_DTB): linux/sf2000.dts Makefile FORCE
	mkdir -p '$(dir $@)'
	dtc -I dts -O dtb -o '$@' '$<'
	if test '$(FIXED_ET_EXEC)' = 1; then \
		fdtput -t s '$@' /reserved-memory/identity-exec@5000000 status okay; \
	fi

memory-layout-audit: $(SF2000_DTB)
	test "$$(fdtget -t x '$<' /reserved-memory/recovery@7a00000 reg)" = \
		"7a00000 600000"
	test "$$(fdtget -t s '$<' /reserved-memory/identity-exec@5000000 status)" = \
		"$(if $(filter 1,$(FIXED_ET_EXEC)),okay,disabled)"
	! fdtget '$<' /reserved-memory/bootrom-scratch@1000000 reg >/dev/null 2>&1

FORCE:

$(AUDIO_TEST): audio/hc15xx_audio.c audio/hc15xx_audio.h audio/test_hc15xx_audio.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Iaudio \
		audio/hc15xx_audio.c audio/test_hc15xx_audio.c -o '$@'

$(AUDIO_RESAMPLER_TEST): audio/hc15xx_resampler.c \
		audio/hc15xx_resampler.h audio/test_hc15xx_resampler.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Iaudio \
		audio/hc15xx_resampler.c audio/test_hc15xx_resampler.c -o '$@'

$(AUDIO_AVSYNC_TEST): audio/hc15xx_audio.c audio/hc15xx_audio.h \
		audio/hc15xx_avsync.c audio/hc15xx_avsync.h \
		audio/test_hc15xx_avsync.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Iaudio \
		audio/hc15xx_audio.c audio/hc15xx_avsync.c \
		audio/test_hc15xx_avsync.c -o '$@'

$(RETAINED_TEST): platform/hc15xx_retained.c include/hc15xx_retained.h \
		tests/hc15xx-retained-test.c
	mkdir -p '$(dir $@)'
	'$(HOSTCC)' -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		platform/hc15xx_retained.c tests/hc15xx-retained-test.c -o '$@'

audio-test: $(AUDIO_TEST) $(AUDIO_AVSYNC_TEST) $(AUDIO_RESAMPLER_TEST) \
		$(RETAINED_TEST)
	'$(AUDIO_TEST)'
	'$(AUDIO_AVSYNC_TEST)'
	'$(AUDIO_RESAMPLER_TEST)'
	'$(RETAINED_TEST)'

$(EFUSE_TEST): efuse/hc15xx_efuse.c efuse/hc15xx_efuse.h efuse/test_hc15xx_efuse.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Iefuse \
		efuse/hc15xx_efuse.c efuse/test_hc15xx_efuse.c -o '$@'

efuse-test: $(EFUSE_TEST)
	'$(EFUSE_TEST)'

$(VDEC_TEST): vdec/hc15xx_vdec.c vdec/hc15xx_vdec.h vdec/test_hc15xx_vdec.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Ivdec \
		vdec/hc15xx_vdec.c vdec/test_hc15xx_vdec.c -o '$@'

vdec-test: $(VDEC_TEST)
	'$(VDEC_TEST)'

$(VDEC_CODEC_TEST): vdec/hc15xx_vdec.c vdec/hc15xx_vdec.h \
		vdec/hc15xx_vdec_codec.c vdec/hc15xx_vdec_codec.h \
		vdec/test_hc15xx_vdec_codec.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Ivdec \
		vdec/hc15xx_vdec.c vdec/hc15xx_vdec_codec.c \
		vdec/test_hc15xx_vdec_codec.c -o '$@'

vdec-codec-test: $(VDEC_CODEC_TEST)
	'$(VDEC_CODEC_TEST)'

$(DSC_TEST): dsc/hc15xx_dsc.c dsc/hc15xx_dsc.h dsc/test_hc15xx_dsc.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -Wextra -Werror -Idsc \
		dsc/hc15xx_dsc.c dsc/test_hc15xx_dsc.c -o '$@'

dsc-test: $(DSC_TEST)
	'$(DSC_TEST)'

DEVICE_TESTS := $(BUILD_DIR)/test_efuse_device $(BUILD_DIR)/test_vdec_device \
		$(BUILD_DIR)/test_dsc_device

$(BUILD_DIR)/test_efuse_device: tests/test_efuse_device.c $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) -Os -Wall -Wextra \
		-march=mips32 -mabi=32 -msoft-float \
		$(USERSPACE_ELF_LDFLAGS) -o '$@' '$<'

$(BUILD_DIR)/test_vdec_device: tests/test_vdec_device.c $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) -Os -Wall -Wextra \
		-march=mips32 -mabi=32 -msoft-float \
		$(USERSPACE_ELF_LDFLAGS) -o '$@' '$<'

$(BUILD_DIR)/test_dsc_device: tests/test_dsc_device.c $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) -Os -Wall -Wextra \
		-march=mips32 -mabi=32 -msoft-float \
		$(USERSPACE_ELF_LDFLAGS) -o '$@' '$<'

device-tests: $(DEVICE_TESTS)

# --- FFmpeg static libraries ---

$(FFMPEG_SRC)/configure:
	mkdir -p '$(dir $@)'
	curl -fSL '$(FFMPEG_URL)' | tar -xJ -C '$(dir $@)' --strip-components=1

FFMPEG_CONFIGURE_FLAGS := \
	--prefix='$(FFMPEG_INSTALL)' \
	--cross-prefix='$(CROSS_COMPILE)' \
	--arch=mipsel --target-os=linux --cpu=mips32 \
	--extra-cflags='-Os -march=mips32 -mabi=32 -msoft-float -G0 -mno-abicalls -fno-pic -fno-pie -static' \
	--extra-ldflags='-static' \
	--enable-static --disable-shared \
	--disable-pthreads \
	--disable-asm --disable-mipsfpu --disable-mipsdsp --disable-mipsdspr2 \
	--disable-doc --disable-programs --disable-network \
	--disable-avdevice --disable-avfilter \
	--disable-encoders --disable-muxers \
	--disable-protocols --enable-protocol=file \
	--disable-indevs --disable-outdevs \
	--disable-decoders \
	--enable-decoder=mp3,aac,flac,vorbis,pcm_s16le,pcm_s16be \
	--disable-demuxers \
	--enable-demuxer=wav,mp3,flac,ogg \
	--disable-parsers \
	--enable-parser=mpegaudio,aac,flac,vorbis \
	--disable-bsfs \
	--disable-swscale --enable-swresample \
	--disable-iconv --disable-zlib --disable-bzlib --disable-lzma \
	--disable-securetransport --disable-schannel --disable-gnutls --disable-openssl \
	--disable-vdpau --disable-vaapi --disable-dxva2 --disable-d3d11va \
	--disable-runtime-cpudetect --disable-autodetect \
	--pkg-config=/bin/false

$(FFMPEG_STAMP): $(FFMPEG_SRC)/configure $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(FFMPEG_OUT)'
	cd '$(FFMPEG_OUT)' && '$(FFMPEG_SRC)/configure' $(FFMPEG_CONFIGURE_FLAGS)
	sed -i 's/#define HAVE_POSIX_MEMALIGN 1/#define HAVE_POSIX_MEMALIGN 0/' \
		'$(FFMPEG_OUT)'/config.h
	sed -i 's/#define HAVE_MEMALIGN 1/#define HAVE_MEMALIGN 0/' \
		'$(FFMPEG_OUT)'/config.h
	sed -i 's/atomic_load_explicit(&max_alloc_size, memory_order_relaxed)/INT_MAX/g' \
		'$(FFMPEG_SRC)'/libavutil/mem.c
	$(MAKE) -C '$(FFMPEG_OUT)' -j'$(JOBS)'
	$(MAKE) -C '$(FFMPEG_OUT)' install
	touch '$@'

ffmpeg: $(FFMPEG_STAMP)

# --- Device test runner + overlay installs ---

$(USERSPACE_DEVTEST): $(USERSPACE_DEVTEST_SRC) $(PIE_STAMP) $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		'$<' \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

$(USERSPACE_EFUSE_DEVICE): $(BUILD_DIR)/test_efuse_device
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(USERSPACE_VDEC_DEVICE): $(BUILD_DIR)/test_vdec_device
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(USERSPACE_DSC_DEVICE): $(BUILD_DIR)/test_dsc_device
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(USERSPACE_PLAYER): player/sf2000-player.c $(PIE_STAMP) \
		$(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(TARGET_CC_RUN) $(USERSPACE_PIE_CFLAGS) $(USERSPACE_PIE_LDFLAGS) \
		-o '$@' '$(PIE_SYSROOT)'/rcrt1.o '$(PIE_SYSROOT)'/crti.o \
		$$('$(TARGET_CC)' -print-file-name=crtbeginS.o) \
		player/sf2000-player.c \
		-L'$(PIE_SYSROOT)' -lc -lgcc \
		$$('$(TARGET_CC)' -print-file-name=crtendS.o) '$(PIE_SYSROOT)'/crtn.o

player: $(USERSPACE_PLAYER)

# --- Test WAV generator ---

$(MKTESTWAV): tools/mktestwav.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) -O2 -std=c11 -Wall -o '$@' '$<' -lm

$(PLAYER_TEST_WAV): $(MKTESTWAV)
	'$<' '$@'

$(PLAYER_TEST_SD): $(PLAYER_TEST_WAV) Makefile
	mkdir -p '$(dir $@)'
	rm -f '$@'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/MEDIA
	mcopy -i '$@' '$(PLAYER_TEST_WAV)' ::/MEDIA/TEST.WAV

# --- QEMU device test smoke ---

run-linux-devtest: $(ROOTFS_FULL_CPIO) qemu linux-full-asd
	mkdir -p '$(BUILD_DIR)'/logs
	{ sleep 8; printf 'sendkey ret-x 500\n'; sleep 10; printf 'quit\n'; } | \
		timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append '$(LINUX_CMDLINE)' \
		-display none -serial none -monitor stdio \
		-d '$(QEMU_DEBUG)' -D '$(BUILD_DIR)'/logs/linux-devtest.log \
		> '$(BUILD_DIR)'/logs/linux-devtest.console 2>&1 || true

smoke-linux-devtest: run-linux-devtest
	grep -q 'sf2000-devtest: device test suite begin' '$(BUILD_DIR)'/logs/linux-devtest.log
	grep -q 'sf2000-devtest: suite complete' '$(BUILD_DIR)'/logs/linux-devtest.log
	! grep -Eq 'reloc outside program|Kernel panic' '$(BUILD_DIR)'/logs/linux-devtest.log
	@printf 'PASS smoke-linux-devtest\n'

# --- QEMU player smoke ---

run-linux-player: $(ROOTFS_FULL_CPIO) qemu linux-full-asd $(PLAYER_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	{ sleep 8; printf 'sendkey x 100\n'; sleep 2; \
		printf 'sendkey x 100\n'; sleep 2; \
		printf 'sendkey x 100\n'; sleep 10; \
		printf 'sendkey z 100\n'; sleep 3; \
		printf 'sendkey ret-q 500\n'; sleep 3; printf 'quit\n'; } | \
		timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000,audiodev=sf2000wav $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append '$(LINUX_CMDLINE)' \
		-drive if=none,id=sd0,file='$(PLAYER_TEST_SD)',format=raw \
		-audiodev wav,id=sf2000wav,path='$(BUILD_DIR)'/sf2000-player.wav \
		-display none -serial none -monitor stdio \
		-d '$(QEMU_DEBUG)' -D '$(BUILD_DIR)'/logs/linux-player.log \
		> '$(BUILD_DIR)'/logs/linux-player.console 2>&1 || true

smoke-linux-player: run-linux-player
	grep -q 'sf2000-player: audio playback ready' '$(BUILD_DIR)'/logs/linux-player.log
	grep -q 'sf2000-player: playback complete' '$(BUILD_DIR)'/logs/linux-player.log
	! grep -Eq 'reloc outside program|Kernel panic' '$(BUILD_DIR)'/logs/linux-player.log
	test -s '$(BUILD_DIR)'/sf2000-player.wav
	@printf 'PASS smoke-linux-player\n'

$(LINUX_CMDLINE_STAMP): Makefile FORCE | $(LINUX_SRC)/.patched
	mkdir -p '$(dir $@)'
	printf '%s\n' '$(LINUX_CMDLINE)' > '$@.tmp'
	if ! cmp -s '$@.tmp' '$@' 2>/dev/null; then mv '$@.tmp' '$@'; else rm -f '$@.tmp'; fi

$(LINUX_MODE_STAMP): Makefile FORCE | $(LINUX_SRC)/.patched
	mkdir -p '$(dir $@)'
	printf 'FIXED_ET_EXEC=%s\n' '$(FIXED_ET_EXEC)' > '$@.tmp'
	if ! cmp -s '$@.tmp' '$@' 2>/dev/null; then mv '$@.tmp' '$@'; else rm -f '$@.tmp'; fi

$(LINUX_CONFIG_STAMP): $(TOOLCHAIN_STAMP) Makefile \
		$(LINUX_CMDLINE_STAMP) $(LINUX_MODE_STAMP) | $(LINUX_SRC)/.patched
	mkdir -p '$(LINUX_OUT)'
	$(ISOLATED_MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' CC='$(KERNEL_CC)' '$(LINUX_DEFCONFIG)'
	# NOMMU static PIE executables occupy one contiguous allocation.  The
	# largest packaged core has a roughly 20 MiB load image, so order 13 is the
	# smallest general ceiling that can load it with its stack. This changes the
	# buddy limit, not a 32 MiB reservation.
	set -e; '$(LINUX_SRC)'/scripts/config --file '$(LINUX_OUT)/.config' \
		--enable BLK_DEV_INITRD \
		--set-str INITRAMFS_SOURCE '$(abspath $(ROOTFS_CPIO))' \
		--enable CMDLINE_BOOL \
		--set-str CMDLINE '$(LINUX_CMDLINE)' \
		--disable MMU \
		--disable BINFMT_ELF \
		--disable COMPAT_BINFMT_ELF \
		--disable BINFMT_FLAT \
		--enable BINFMT_ELF_NOMMU \
		$(if $(filter 1,$(FIXED_ET_EXEC)),--enable,--disable) BINFMT_ELF_NOMMU_FIXED \
		--set-val ARCH_FORCE_MAX_ORDER 13 \
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
		--disable RD_BZIP2 \
		--disable RD_LZMA \
		--disable RD_XZ \
		--disable RD_LZO \
		--disable RD_LZ4 \
		--disable RD_ZSTD \
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
		--enable EFI_PARTITION \
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
		--disable TIME_NS \
		--disable TIME_NS_VDSO \
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
		--enable EXFAT_FS \
		--enable NLS \
		--enable NLS_CODEPAGE_437 \
		--enable NLS_ISO8859_1 \
		--enable NLS_UTF8 \
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
	$(ISOLATED_MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' CC='$(KERNEL_CC)' olddefconfig
	# The HC15xx core has 128-byte L1 lines and a board cache-ops table.
	# MIPS_SF2000 selects both in the kernel Kconfig; assert the regenerated
	# config kept them so future defconfig/scripts/config drift cannot
	# silently produce a kernel that faults on device with stale text lines
	# (Reserved Instruction at a valid text address).
	grep -q '^CONFIG_MIPS_L1_CACHE_SHIFT=7$$' '$(LINUX_OUT)/.config'
	grep -q '^CONFIG_BOARD_SCACHE=y$$' '$(LINUX_OUT)/.config'
	grep -q '^CONFIG_MIPS_SF2000=y$$' '$(LINUX_OUT)/.config'
	! grep -q '^CONFIG_MMU=y$$' '$(LINUX_OUT)/.config'
	grep -q '^CONFIG_BINFMT_ELF_NOMMU=y$$' '$(LINUX_OUT)/.config'
	touch '$@'

$(LINUX_VMLINUX): $(LINUX_SRC)/.patched $(LINUX_CONFIG_STAMP) $(ROOTFS_CPIO)
	env -u MAKEFLAGS -u MFLAGS $(MAKE) -j'$(KERNEL_JOBS)' \
		-C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' CC='$(KERNEL_CC)' vmlinux
	# The loader consumes only allocated ELF sections.  Keeping DWARF in the
	# embedded image wastes most of RAM and makes relocation overlap needlessly
	# large; the kernel build's vmlinux.unstripped remains available for symbols.
	'$(STRIP_MIPS)' --strip-debug '$(LINUX_VMLINUX)'

linux: $(LINUX_VMLINUX) $(SF2000_DTB)

# Re-extracting is intentionally rootfs-independent: switching between tiny
# and the full userspace build must not retain a kernel config from the other
# output tree.
linux-reextract:
	rm -rf '$(LINUX_SRC)' '$(BUILD_DIR)/linux-sf2000' \
		'$(BUILD_DIR)/linux-sf2000-full'

linux-reconfigure:
	rm -f '$(LINUX_OUT)/.config' '$(LINUX_CONFIG_STAMP)'
	$(MAKE) ROOTFS='$(ROOTFS)' linux

$(ASDPACK): tools/asdpack.c Makefile
	mkdir -p '$(dir $@)'
	'$(HOSTCC)' -O2 -Wall -Wextra -o '$@' '$<'

$(LINUX_LOADER_ENTRY_OBJ): boot/linux-loader-entry.S $(TOOLCHAIN_STAMP) \
		Makefile
	mkdir -p '$(dir $@)'
	$(CC_MIPS_RUN) $(LOADER_CFLAGS) -c -o '$@' '$<'

$(LINUX_LOADER_OBJ): boot/linux-loader.c $(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	$(CC_MIPS_RUN) $(LOADER_CFLAGS) -c -o '$@' '$<'

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

$(LINUX_LOADER_BLOBS_OBJ): $(LINUX_LOADER_BLOBS_S) \
		$(TOOLCHAIN_STAMP)
	$(CC_MIPS_RUN) $(LOADER_CFLAGS) -c -o '$@' '$<'

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
		printf '  Early Linux records named progress entries at uncached 0xa7a00000.\n'; \
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
		printf '  START+SELECT opens the pause/options menu; START+RIGHT saves logs.\n'; \
		printf '  Use Reset or Safe Shutdown from the home menu for ordered storage handling.\n'; \
	} > '$@'

$(SDCARD_LOG_TXT): Makefile
	mkdir -p '$(dir $@)'
	dd if=/dev/zero of='$@' bs=262144 count=1 >/dev/null 2>&1

$(SDCARD_USER_CONFIG): $(ROOTFS_OVERLAY)/etc/sf2000.conf
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(SDCARD_UI_FONT): fonts/unifrog-ui.ttf
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

# The Latin primary font is a checked-in subset of Noto Sans Regular (OFL,
# same license family as OFL.txt) so the build never needs network access to
# produce the UI.  Both this and ui.ttf are loaded by the frontend at boot;
# keeping them small is what bounds the boot-time font read.
$(SDCARD_UI_LATIN_FONT): fonts/unifrog-ui-latin.ttf
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

$(SDCARD_UI_FONT_LICENSE): fonts/OFL.txt
	mkdir -p '$(dir $@)'
	cp '$<' '$@'

# Only inputs that affect frontend compilation or the staged core set belong
# in this signature.  Depending directly on this top-level Makefile made an
# unrelated QEMU or packaging edit rebuild every emulator.  The phony profile
# recipe still runs each time, but compare-and-replace preserves its timestamp
# until a relevant value or cross compiler actually changes.
$(SDCARD_CORE_PROFILE): FORCE $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	@set -eu; \
	tmp='$@.tmp'; \
	{ \
		printf '%s\n' \
			'FRONTEND_PROJECT=$(abspath $(FRONTEND_PROJECT))' \
			'TARGET_CC=$(TARGET_CC)' \
			'FRONTEND_CORES=$(SDCARD_FRONTEND_CORES)' \
			'MUFROG_CORES=$(SDCARD_MUFROG_CORES)'; \
		stat -c 'compiler=%n|size=%s|mtime=%y' '$(TARGET_CC)'; \
	} > "$$tmp"; \
	if cmp -s "$$tmp" '$@' 2>/dev/null; then \
		rm -f "$$tmp"; \
	else \
		mv "$$tmp" '$@'; \
	fi

$(SDCARD_CORE_STAMP): $(shell find '$(FRONTEND_PROJECT)'/src \
		'$(FRONTEND_PROJECT)'/include '$(FRONTEND_PROJECT)'/patches/quicknes \
		'$(FRONTEND_PROJECT)'/patches/gambatte \
		'$(FRONTEND_PROJECT)'/patches/gpsp \
		'$(FRONTEND_PROJECT)'/patches/fceumm \
		'$(FRONTEND_PROJECT)'/patches/prosystem \
		'$(FRONTEND_PROJECT)'/patches/mufrog \
		-type f 2>/dev/null) $(FRONTEND_PROJECT)/Makefile $(SDCARD_CORE_PROFILE) \
		$(FRONTEND_UI_STAMP) \
		$(FRONTEND_CORE_OUTPUTS)
	$(FRONTEND_MAKE) core-packages \
		CROSS_COMPILE='$(patsubst %gcc,%,$(TARGET_CC))'
	rm -rf '$(dir $@)'
	mkdir -p '$(dir $@)/licenses'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-quicknes \
		'$(SDCARD_QUICKNES)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-prosystem \
		'$(SDCARD_PROSYSTEM)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-snes9x2005 \
		'$(SDCARD_SNES9X2005)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-snes9x2002 \
		'$(SDCARD_SNES9X2002)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-stella2014 \
		'$(SDCARD_STELLA2014)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-gearboy \
		'$(SDCARD_GEARBOY)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-pce-fast \
		'$(SDCARD_PCE_FAST)'
	set -eu; \
	for core in $(SDCARD_FRONTEND_CORES); do \
		cp '$(FRONTEND_PROJECT)'/build/core-packages/$$core \
			'$(BUILD_DIR)/sdcard/sf2000/cores/'$$core; \
	done
	set -eu; \
	for core in $(SDCARD_MUFROG_CORES); do \
		cp '$(FRONTEND_PROJECT)'/build/core-packages/$$core \
			'$(BUILD_DIR)/sdcard/sf2000/cores/'$$core; \
	done
	mkdir -p '$(dir $(SDCARD_JS2300_SCRIPT))'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/sf2000-js2300-core \
		'$(SDCARD_JS2300_CORE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/js2300-cores/chip8.js \
		'$(SDCARD_JS2300_SCRIPT)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/quicknes-LICENSE \
		'$(SDCARD_QUICKNES_LICENSE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/prosystem-LICENSE \
		'$(SDCARD_PROSYSTEM_LICENSE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/snes9x2005-copyright \
		'$(SDCARD_SNES9X2005_LICENSE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/snes9x2002-copyright.h \
		'$(SDCARD_SNES9X2002_LICENSE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/stella2014-license.txt \
		'$(SDCARD_STELLA2014_LICENSE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/gearboy-LICENSE \
		'$(SDCARD_GEARBOY_LICENSE)'
	cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/pce-fast-COPYING \
		'$(SDCARD_PCE_FAST_LICENSE)'
	set -eu; \
	for license in $(SDCARD_FRONTEND_LICENSES); do \
		cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/$$license \
			'$(BUILD_DIR)/sdcard/sf2000/cores/licenses/'$$license; \
	done
	set -eu; \
	for license in $(SDCARD_MUFROG_LICENSES); do \
		cp '$(FRONTEND_PROJECT)'/build/core-packages/licenses/$$license \
			'$(BUILD_DIR)/sdcard/sf2000/cores/licenses/'$$license; \
	done
	touch '$@'

$(SDCARD_CHECKSUMS): $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_USER_CONFIG) $(SDCARD_UI_FONT) \
		$(SDCARD_UI_LATIN_FONT) $(SDCARD_UI_FONT_LICENSE) $(SDCARD_CORE_STAMP)
	cd '$(BUILD_DIR)/sdcard' && sha256sum bios/bisrv.asd \
		firmware/linux.asd firmware/unifrog.bin sf2000.conf \
		sf2000/ui.ttf sf2000/ui-latin.ttf sf2000/OFL.txt \
		sf2000/js2300-cores/chip8.js \
		sf2000/cores/sf2000-js2300-core \
		sf2000/cores/sf2000-quicknes \
		sf2000/cores/sf2000-prosystem \
		sf2000/cores/sf2000-snes9x2005 \
		sf2000/cores/sf2000-snes9x2002 \
		sf2000/cores/sf2000-stella2014 \
		sf2000/cores/sf2000-gearboy \
		sf2000/cores/sf2000-pce-fast \
		sf2000/cores/licenses/quicknes-LICENSE \
		sf2000/cores/licenses/prosystem-LICENSE \
		sf2000/cores/licenses/snes9x2005-copyright \
		sf2000/cores/licenses/snes9x2002-copyright.h \
		sf2000/cores/licenses/stella2014-license.txt \
		sf2000/cores/licenses/gearboy-LICENSE \
		sf2000/cores/licenses/pce-fast-COPYING \
		$(foreach core,$(SDCARD_MUFROG_CORES),sf2000/cores/$(core)) \
		$(foreach core,$(SDCARD_FRONTEND_CORES),sf2000/cores/$(core)) \
		$(foreach license,$(SDCARD_MUFROG_LICENSES),sf2000/cores/licenses/$(license)) \
		$(foreach license,$(SDCARD_FRONTEND_LICENSES),sf2000/cores/licenses/$(license)) > SHA256SUMS

$(LINUX_ROM_SD_IMAGE): $(LINUX_ASD) $(QEMU_MKSD)
	'$(QEMU_MKSD)' '$(LINUX_ASD)' '$@' fat32

ifeq ($(ROOTFS),tiny)
# The tiny image is intentionally diagnostic-only.  Never let a bare
# `make linux-asd` overwrite the physical SD-card artifact with a menu-less
# initramfs, even when SDCARD_ASD_SYNC is inherited from the normal command
# line configuration.
linux-asd: $(LINUX_ASD)
	@echo "diagnostic ASD: tiny rootfs; refusing to update build/sdcard artifacts"
else ifeq ($(SDCARD_ASD_SYNC),1)
linux-asd: $(LINUX_ASD) $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_CHECKSUMS)
	cmp '$(LINUX_ASD)' '$(SDCARD_LINUX_ASD)'
	cmp '$(LINUX_ASD)' '$(SDCARD_BIOS_ASD)'
	cd '$(BUILD_DIR)/sdcard' && sha256sum -c SHA256SUMS
else
linux-asd: $(LINUX_ASD)
	@echo "diagnostic ASD: leaving build/sdcard normal artifacts unchanged"
endif

linux-full:
	$(ISOLATED_MAKE) ROOTFS=full linux

linux-full-asd:
	$(ISOLATED_MAKE) ROOTFS=full linux-asd

# QEMU tests need the kernel/rootfs artifact, not a synchronized physical SD
# layout.  Keeping that distinction explicit prevents an emulator iteration
# from rebuilding and checksumming every libretro core.
linux-full-test-asd:
	$(ISOLATED_MAKE) ROOTFS=full SDCARD_ASD_SYNC=0 linux-asd

# The SD-card artifact must contain the full userspace/menu.  Keep this
# explicit alias next to the historical target so a bare `make linux-asd`
# (which intentionally defaults to the tiny diagnostic rootfs) cannot be
# mistaken for a physical-device build.
physical-linux-asd:
	# Remove a prior diagnostic before syncing the normal physical artifact.
	rm -f '$(SDCARD_QPSX_STARTUP_CONFIG)' '$(SDCARD_QPSX_STARTUP_CHECKSUM)'
	$(ISOLATED_MAKE) ROOTFS=full SDCARD_ASD_SYNC=1 linux-asd

ifeq ($(SDCARD_ASD_SYNC),1)
sdcard-linux: $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_BOOT_OPTIONS) $(SDCARD_LOG_TXT) \
		$(SDCARD_USER_CONFIG) $(SDCARD_UI_FONT) $(SDCARD_CHECKSUMS)
else
sdcard-linux:
	@echo "refusing to assemble an SD card from a diagnostic command line" >&2
	@exit 2
endif

sdcard-full:
	$(ISOLATED_MAKE) ROOTFS=full sdcard-linux

.PHONY: build-info
build-info:
	@set -eu; \
	frontend='$(abspath $(FRONTEND_PROJECT))'; \
	mkdir -p '$(dir $(BUILD_PROVENANCE))'; \
	{ \
		printf 'sf2000_linux build provenance\n'; \
		printf 'release_id=%s\n' '$(SDCARD_RELEASE_ID)'; \
		printf 'rootfs=%s\n' '$(ROOTFS)'; \
		repo() { \
			label="$$1"; path="$$2"; \
			if git -C "$$path" rev-parse --git-dir >/dev/null 2>&1; then \
				commit="$$(git -C "$$path" rev-parse HEAD)"; \
				ref="$$(git -C "$$path" symbolic-ref --short -q HEAD 2>/dev/null || \
					git -C "$$path" describe --tags --exact-match HEAD 2>/dev/null || \
					printf 'detached')"; \
				printf '%s: path=%s ref=%s commit=%s\n' \
					"$$label" "$$path" "$$ref" "$$commit"; \
			else \
				printf '%s: path=%s unavailable\n' "$$label" "$$path"; \
			fi; \
		}; \
		repo linux '$(abspath .)'; \
		repo frontend "$$frontend"; \
		repo js2300 "$$frontend/.deps/frog2k-javascript"; \
		repo qpsx "$$frontend/.deps/core-sources/.deps/cores/sf2000-qpsx-playstation-emulator"; \
		printf 'kernel: method=%s url=%s tag=%s slim_script_sha256=%s\n' \
			'$(LINUX_SOURCE_METHOD)' '$(LINUX_GIT_URL)' '$(LINUX_GIT_TAG)' \
			'$(LINUX_SLIM_SCRIPT_SHA256)'; \
		printf 'toolchain: version=%s tuple=%s arch=%s sha256=%s path=%s\n' \
			'$(FROG_TOOLCHAIN_VERSION)' '$(TOOLCHAIN_TUPLE)' \
			'$(FROG_TOOLCHAIN_ARCH)' '$(FROG_TOOLCHAIN_SHA256)' \
			'$(TOOLCHAIN_DIR)'; \
	} > '$(BUILD_PROVENANCE).tmp'; \
	mv '$(BUILD_PROVENANCE).tmp' '$(BUILD_PROVENANCE)'

sdcard-zips: $(SDCARD_STANDALONE_ZIP) $(SDCARD_BOOTLOADER_ZIP)
	@$(MAKE) --no-print-directory build-info
	@printf 'SD-card packages:\n  %s\n  %s\n' \
		'$(SDCARD_STANDALONE_ZIP)' '$(SDCARD_BOOTLOADER_ZIP)'
	@printf 'Build provenance:\n'
	@cat '$(BUILD_PROVENANCE)'

$(SDCARD_STANDALONE_ZIP): sdcard-full Makefile
	@set -eu; \
	test -s '$(SDCARD_BIOS_ASD)'; \
	(cd '$(BUILD_DIR)/sdcard' && sha256sum -c SHA256SUMS >/dev/null); \
	tmp='$(abspath $@.tmp)'; \
	zip_file='$(abspath $@.tmp)'; \
	rm -f "$$tmp"; \
	(cd '$(BUILD_DIR)/sdcard' && \
		find . -type f ! -name '.*' -print | sed 's#^\\./##' | \
		LC_ALL=C sort | zip -X -q "$$zip_file" -@); \
	zip -T "$$tmp" >/dev/null; \
	mv "$$tmp" '$@'

$(SDCARD_BOOTLOADER_ZIP): sdcard-full Makefile
	@set -eu; \
	stage='$(SDCARD_BOOTLOADER_STAGE)'; \
	rm -rf "$$stage"; \
	mkdir -p "$$stage"; \
	cp -a '$(BUILD_DIR)/sdcard/.' "$$stage/"; \
	rm -f "$$stage/bios/bisrv.asd" "$$stage/firmware/linux.asd" \
		"$$stage/SHA256SUMS"; \
	test ! -e "$$stage/bios/bisrv.asd"; \
	mkdir -p "$$stage/system/firmware"; \
	cp '$(SDCARD_RELEASE_ASD)' \
		"$$stage/system/firmware/frog2k-linux-$(SDCARD_RELEASE_LABEL).asd"; \
	( cd "$$stage"; \
		find . -type f ! -name '.*' ! -name SHA256SUMS -print | \
			sed 's#^\\./##' | LC_ALL=C sort | \
			while IFS= read -r file; do sha256sum "$$file"; done > SHA256SUMS; \
		sha256sum -c SHA256SUMS >/dev/null ); \
	tmp='$(abspath $@.tmp)'; \
	zip_file='$(abspath $@.tmp)'; \
	rm -f "$$tmp"; \
	(cd "$$stage" && \
		find . -type f ! -name '.*' -print | sed 's#^\\./##' | \
		LC_ALL=C sort | zip -X -q "$$zip_file" -@); \
	zip -T "$$tmp" >/dev/null; \
	mv "$$tmp" '$@'

linux-rom-sd: $(LINUX_ROM_SD_IMAGE)

linux-full-rom-sd:
	$(ISOLATED_MAKE) ROOTFS=full linux-rom-sd

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
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000$(QEMU_MACHINE_ARGS) $(QEMU_CPU_ARGS) -kernel '$(LINUX_ASD)' \
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

run-linux-power: qemu linux-full-asd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey ret-a 500\n'; sleep 4; \
		printf 'sendkey x 2500\n'; sleep 4; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-power.log \
		> '$(BUILD_DIR)'/logs/linux-power.console 2>&1

smoke-linux-power: run-linux-power
	grep -q 'screen-ready-done' '$(BUILD_DIR)'/logs/linux-power.log
	grep -q 'sf2000-powerd: display standby entering' '$(BUILD_DIR)'/logs/linux-power.log
	grep -q 'sf2000-powerd: display standby resumed' '$(BUILD_DIR)'/logs/linux-power.log
	! grep -Eq 'reloc outside program|Kernel panic' '$(BUILD_DIR)'/logs/linux-power.log

$(BROWSER_TEST_ROM_TOOL): tools/mkgbtest.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) $(HOSTCFLAGS) -o '$@' '$<'

$(BROWSER_TEST_ROM): $(BROWSER_TEST_ROM_TOOL)
	'$<' '$@'

$(GPSP_TEST_ROM_TOOL): tools/mkgabatest.c
	mkdir -p '$(dir $@)'
	$(HOSTCC) $(HOSTCFLAGS) -o '$@' '$<'

$(GPSP_TEST_ROM): $(GPSP_TEST_ROM_TOOL)
	'$<' '$@'

gpsp-smc-test-roms: $(GPSP_SMC_TEST_ROMS)

$(BUILD_DIR)/gpsp-smc-%.gba: $(GPSP_TEST_ROM_TOOL)
	'$<' '$@' 'smc-$*'

$(GPSP_TEST_SD): Makefile $(GPSP_TEST_ROM) $(SDCARD_CORE_STAMP) \
		$(SDCARD_USER_CONFIG) $(SDCARD_UI_FONT)
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/GBA ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(GPSP_TEST_ROM)' ::/GBA/TEST.GBA
	mcopy -i '$@' '$(SDCARD_GPSP)' ::/sf2000/cores/sf2000-gpsp
	mcopy -i '$@' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$@' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf

gpsp-real-test-sd: Makefile $(SDCARD_CORE_STAMP) $(SDCARD_USER_CONFIG) \
		$(SDCARD_UI_FONT)
	@test -n '$(GPSP_REAL_ROM)' || { \
		echo 'set GPSP_REAL_ROM=/path/to/a/legal GBA ROM' >&2; exit 2; }
	@test -f '$(GPSP_REAL_ROM)' || { \
		echo 'GPSP_REAL_ROM does not name a regular file' >&2; exit 2; }
	mkdir -p '$(dir $(GPSP_REAL_TEST_SD))'
	truncate -s 128M '$(GPSP_REAL_TEST_SD)'
	mkfs.vfat -F 32 -n SFTEST '$(GPSP_REAL_TEST_SD)' >/dev/null
	mmd -i '$(GPSP_REAL_TEST_SD)' ::/GBA ::/sf2000 ::/sf2000/cores
	mcopy -i '$(GPSP_REAL_TEST_SD)' '$(GPSP_REAL_ROM)' ::/GBA/TEST.GBA
	mcopy -i '$(GPSP_REAL_TEST_SD)' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$(GPSP_REAL_TEST_SD)' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf
	mcopy -i '$(GPSP_REAL_TEST_SD)' \
		'$(BUILD_DIR)/sdcard/sf2000/cores/sf2000-gpsp-multicore' \
		::/sf2000/cores/sf2000-gpsp-multicore

$(QPSX_AUDIT_STAMP): $(SDCARD_CORE_STAMP)
	$(FRONTEND_MAKE) qpsx-mips32r1-audit \
		CROSS_COMPILE='$(patsubst %gcc,%,$(TARGET_CC))'
	touch '$@'

qpsx-mips32r1-audit: $(QPSX_AUDIT_STAMP)

# A PSX disc can be hundreds of MiB, so hashing or recopying it in every test
# loop is itself a substantial benchmark tax.  Record high-resolution metadata
# for the disc and its CUE sidecars, while hashing the small configuration and
# font inputs.  Compare-and-replace gives Make an honest content-change edge.
$(QPSX_REAL_TEST_BASE_PROFILE): FORCE $(SDCARD_USER_CONFIG) \
		$(SDCARD_UI_FONT) $(SDCARD_UI_LATIN_FONT)
	@test -n '$(QPSX_REAL_IMAGE)' || { \
		echo 'set QPSX_REAL_IMAGE=/path/to/a/legal PSX .cue or raw image' >&2; exit 2; }
	@test -f '$(QPSX_REAL_IMAGE)' || { \
		echo 'QPSX_REAL_IMAGE does not name a regular file' >&2; exit 2; }
	mkdir -p '$(dir $@)'
	@set -eu; \
	tmp='$@.tmp'; \
	{ \
		printf 'menu_at_start=%s\nmin_mib=%s\nimage=%s\n' \
			'$(QPSX_REAL_MENU_AT_START)' '$(QPSX_REAL_TEST_MIN_MIB)' \
			'$(QPSX_REAL_IMAGE)'; \
		sha256sum '$(SDCARD_USER_CONFIG)' '$(SDCARD_UI_FONT)' \
			'$(SDCARD_UI_LATIN_FONT)'; \
		stat -c 'disc=%n|size=%s|mtime=%y' '$(QPSX_REAL_IMAGE)'; \
		case '$(QPSX_REAL_IMAGE)' in \
			*.[cC][uU][eE]) \
				cue_dir=$$(dirname '$(QPSX_REAL_IMAGE)'); \
				awk -F'"' '/^[[:space:]]*FILE[[:space:]]+"/ {print $$2}' \
					'$(QPSX_REAL_IMAGE)' | while IFS= read -r relpath; do \
					test -n "$$relpath" || continue; \
					sidecar="$$cue_dir/$$relpath"; \
					test -f "$$sidecar" || { \
						echo "missing cue FILE: $$sidecar" >&2; exit 2; }; \
					stat -c 'sidecar=%n|size=%s|mtime=%y' "$$sidecar"; \
				done;; \
		esac; \
	} > "$$tmp"; \
	if cmp -s "$$tmp" '$@' 2>/dev/null; then \
		rm -f "$$tmp"; \
	else \
		mv "$$tmp" '$@'; \
	fi

$(QPSX_REAL_TEST_SD): $(QPSX_REAL_TEST_BASE_PROFILE)
	mkdir -p '$(dir $(QPSX_REAL_TEST_SD))'
	@set -eu; \
	disc_bytes=$$(stat -c %s '$(QPSX_REAL_IMAGE)'); \
	case '$(QPSX_REAL_IMAGE)' in \
		*.[cC][uU][eE]) \
			cue_dir=$$(dirname '$(QPSX_REAL_IMAGE)'); \
			list=$$(mktemp '$(BUILD_DIR)/.qpsx-real-sidecars.XXXXXX'); \
			trap 'rm -f "$$list"' EXIT HUP INT TERM; \
			awk -F'"' '/^[[:space:]]*FILE[[:space:]]+"/ {print $$2}' \
				'$(QPSX_REAL_IMAGE)' > "$$list"; \
			while IFS= read -r relpath; do \
				test -n "$$relpath" || continue; \
				sidecar="$$cue_dir/$$relpath"; \
				disc_bytes=$$((disc_bytes + $$(stat -c %s "$$sidecar"))); \
			done < "$$list"; \
			rm -f "$$list"; \
			trap - EXIT HUP INT TERM;; \
	esac; \
	image_mib=$$(((disc_bytes + 33554432 + 1048575) / 1048576)); \
	if test "$$image_mib" -lt '$(QPSX_REAL_TEST_MIN_MIB)'; then \
		image_mib='$(QPSX_REAL_TEST_MIN_MIB)'; \
	fi; \
	truncate -s "$${image_mib}M" '$(QPSX_REAL_TEST_SD)'
	mkfs.vfat -F 32 -n SFTEST '$(QPSX_REAL_TEST_SD)' >/dev/null
	mmd -i '$(QPSX_REAL_TEST_SD)' ::/PSX ::/saves ::/sf2000 ::/sf2000/cores ::/cores
	mmd -i '$(QPSX_REAL_TEST_SD)' ::/cores/config
	mattrib +h -i '$(QPSX_REAL_TEST_SD)' ::/cores
	@case '$(QPSX_REAL_IMAGE)' in \
		*.[cC][uU][eE]) \
			mcopy -i '$(QPSX_REAL_TEST_SD)' '$(QPSX_REAL_IMAGE)' \
				::/PSX/00-TEST.cue; \
				cue_dir=$$(dirname '$(QPSX_REAL_IMAGE)'); \
				awk -F'"' '/^[[:space:]]*FILE[[:space:]]+"/ {print $$2}' \
					'$(QPSX_REAL_IMAGE)' | while IFS= read -r relpath; do \
					test -n "$$relpath" || continue; \
					sidecar="$$cue_dir/$$relpath"; \
					test -f "$$sidecar" || { echo "missing cue FILE: $$sidecar" >&2; exit 2; }; \
					destdir=$$(dirname "$$relpath"); \
					if test "$$destdir" != .; then \
						mmd -p -i '$(QPSX_REAL_TEST_SD)' "::/PSX/$$destdir"; \
					fi; \
					mcopy -i '$(QPSX_REAL_TEST_SD)' "$$sidecar" \
						"::/PSX/$$relpath"; \
				done;; \
		*) mcopy -i '$(QPSX_REAL_TEST_SD)' '$(QPSX_REAL_IMAGE)' \
				::/PSX/TEST.BIN;; \
	esac
	mcopy -i '$(QPSX_REAL_TEST_SD)' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	printf 'debug_log=1\nmenu_at_start=$(QPSX_REAL_MENU_AT_START)\n' > '$(BUILD_DIR)'/.qpsx-real-test.cfg
	config_name='TEST.BIN.cfg'; \
	case '$(QPSX_REAL_IMAGE)' in \
		*.[cC][uU][eE]) config_name='00-TEST.cue.cfg';; \
	esac; \
	mcopy -i '$(QPSX_REAL_TEST_SD)' '$(BUILD_DIR)'/.qpsx-real-test.cfg \
		"::/cores/config/$$config_name"
	if test '$(QPSX_REAL_MENU_AT_START)' = 0; then \
		printf 'menu_at_start=0\nauto_menu=0\n' > '$(BUILD_DIR)'/.qpsx-startup.cfg; \
		mcopy -i '$(QPSX_REAL_TEST_SD)' '$(BUILD_DIR)'/.qpsx-startup.cfg \
			::/cores/config/psx_startup.cfg; \
		rm -f '$(BUILD_DIR)'/.qpsx-startup.cfg; \
	fi
	rm -f '$(BUILD_DIR)'/.qpsx-real-test.cfg
	mcopy -i '$(QPSX_REAL_TEST_SD)' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf
	mcopy -i '$(QPSX_REAL_TEST_SD)' '$(SDCARD_UI_LATIN_FONT)' \
		::/sf2000/ui-latin.ttf

# Core edits are the common iteration case.  Install only the changed core
# into the already-populated image instead of formatting and recopying the
# disc.  Hashing this small executable avoids timestamp ambiguity when it is
# produced by a deterministic or compare-and-replace build.
$(QPSX_REAL_TEST_CORE_PROFILE): FORCE $(QPSX_REAL_CORE_DEP)
	@test -f '$(QPSX_TEST_CORE)' || { \
		echo 'QPSX_TEST_CORE does not name a regular file' >&2; exit 2; }
	mkdir -p '$(dir $@)'
	@set -eu; \
	tmp='$@.tmp'; \
	{ \
		printf 'core=%s\n' '$(QPSX_TEST_CORE)'; \
		sha256sum '$(QPSX_TEST_CORE)'; \
	} > "$$tmp"; \
	if cmp -s "$$tmp" '$@' 2>/dev/null; then \
		rm -f "$$tmp"; \
	else \
		mv "$$tmp" '$@'; \
	fi

$(QPSX_REAL_TEST_CORE_STAMP): $(QPSX_REAL_TEST_SD) \
		$(QPSX_REAL_TEST_CORE_PROFILE)
	mcopy -o -i '$(QPSX_REAL_TEST_SD)' '$(QPSX_TEST_CORE)' \
		::/sf2000/cores/sf2000-qpsx
	touch '$@'

qpsx-real-test-sd: $(QPSX_REAL_TEST_CORE_STAMP)

# Rebuild and stage only QPSX from its development checkout.  This avoids the
# all-core SDCARD_CORE_STAMP and is the intended edit/build/QEMU loop.
qpsx-dev-real-test-sd: FORCE
	$(FRONTEND_MAKE) qpsx-dev-mips32r1-audit \
		CROSS_COMPILE='$(patsubst %gcc,%,$(TARGET_CC))' \
		QPSX_OPTIMIZE='$(QPSX_OPTIMIZE)'
	$(MAKE) qpsx-real-test-sd QPSX_REAL_CORE_DEP= \
		QPSX_TEST_CORE='$(FRONTEND_PROJECT)/build/sf2000-qpsx-dev'

qpsx-dev-no-menu-test-sd: FORCE
	$(MAKE) QPSX_REAL_MENU_AT_START=0 qpsx-dev-real-test-sd

# These are deliberately separate from the normal physical image.  The
# no-menu SD target reuses the real-image builder with menu_at_start=0 for
# QEMU.  The physical target stages only the startup option after producing a
# normal full-rootfs ASD; the option is removed by the next physical-linux-asd
# invocation, so it cannot silently become the default user image.
qpsx-no-menu-test-sd: FORCE
	$(MAKE) QPSX_REAL_MENU_AT_START=0 qpsx-real-test-sd

qpsx-no-menu-physical: physical-linux-asd
	mkdir -p '$(dir $(SDCARD_QPSX_STARTUP_CONFIG))'
	printf 'menu_at_start=0\nauto_menu=0\n' > '$(SDCARD_QPSX_STARTUP_CONFIG)'
	( cd '$(BUILD_DIR)/sdcard' && sha256sum 'cores/config/psx_startup.cfg' ) > '$(SDCARD_QPSX_STARTUP_CHECKSUM)'
	@printf 'QPSX no-menu diagnostic staged at %s\n' '$(SDCARD_QPSX_STARTUP_CONFIG)'
	@printf '%s\n' 'Copy this file together with the normal build/sdcard tree to the test SD card, then launch QPSX without closing an internal menu.'

$(BROWSER_TEST_SD): Makefile $(BROWSER_TEST_ROM) $(SDCARD_CORE_STAMP) \
		$(SDCARD_USER_CONFIG) $(SDCARD_UI_FONT) $(SDCARD_UI_LATIN_FONT)
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/GB ::/GBC ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(BROWSER_TEST_ROM)' '::/GB/TEST GAME.GB'
	mcopy -i '$@' '$(SDCARD_GAMBATTE)' ::/sf2000/cores/sf2000-gambatte
	mcopy -i '$@' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$@' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf
	mcopy -i '$@' '$(SDCARD_UI_LATIN_FONT)' ::/sf2000/ui-latin.ttf
	mcopy -i '$@' Makefile ::/README.TXT

$(FRONTEND_LIFECYCLE_TEST_SD): Makefile $(BROWSER_TEST_ROM) $(GPSP_TEST_ROM) \
		$(SDCARD_CORE_STAMP) $(SDCARD_USER_CONFIG) $(SDCARD_UI_FONT)
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/GB ::/GBA ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(BROWSER_TEST_ROM)' '::/GB/TEST GAME.GB'
	mcopy -i '$@' '$(GPSP_TEST_ROM)' ::/GBA/TEST.GBA
	mcopy -i '$@' '$(SDCARD_GAMBATTE)' ::/sf2000/cores/sf2000-gambatte
	mcopy -i '$@' '$(SDCARD_GPSP)' ::/sf2000/cores/sf2000-gpsp
	mcopy -i '$@' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$@' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf

$(JS2300_TEST_SD): Makefile $(SDCARD_CORE_STAMP) $(SDCARD_USER_CONFIG) \
		$(SDCARD_UI_FONT) $(JS2300_UI_SMOKE_SCRIPT)
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/CHIP8 ::/SCRIPTS ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(SDCARD_JS2300_SCRIPT)' ::/CHIP8/TEST.JS
	mcopy -i '$@' '$(JS2300_UI_SMOKE_SCRIPT)' ::/SCRIPTS/TEST.JS
	mcopy -i '$@' '$(SDCARD_JS2300_CORE)' \
		::/sf2000/cores/sf2000-js2300-core
	mcopy -i '$@' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$@' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf

run-linux-frontend: qemu linux-full-asd $(BROWSER_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey ret-right 500\n'; sleep 1; \
	printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 5; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(BROWSER_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-frontend.log \
		> '$(BUILD_DIR)'/logs/linux-frontend.console 2>&1
	mcopy -o -i '$(BROWSER_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-frontend-loglinux.txt

smoke-linux-frontend: run-linux-frontend
	grep -q 'screen-ready-done' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-powerd: frontend launch' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-browser: font loaded path=/mnt/sd/sf2000/ui.ttf' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-browser: framebuffer write complete bytes=153600 stride=640 presenter=GE' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Eq 'sf2000-browser: directory path=/mnt/sd entries=[1-9][0-9]*' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-browser: ready: home menu A select B back' '$(BUILD_DIR)'/logs/linux-frontend.log
	strings -a '$(BUILD_DIR)'/userspace-generated-overlay/usr/bin/sf2000-frontend | \
		grep -Fq 'UI diagnostic path=%s source='
	! grep -q 'sf2000-browser: cannot open directory' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Fq 'sf2000-browser: launch Gambatte /mnt/sd/GB/TEST GAME.GB' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-logd: RAM journal begin: FAT writes deferred' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Eq 'sf2000-logd: RAM journal end: bytes=[1-9][0-9]* peak=[1-9][0-9]* dropped=0 metrics=[1-9][0-9]* metric_bytes=[1-9][0-9]*' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-logd: RAM journal drained after frontend exit' '$(BUILD_DIR)'/logs/linux-frontend.log
	awk '/sf2000-frontend: ROM load complete/{active=1} /sf2000-frontend: returned cleanly/{active=0} active && /name=hc15-write-op/{bad=1} END{exit bad}' '$(BUILD_DIR)'/logs/linux-frontend.log
	! grep -q 'Kernel panic' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: frontend running START+SELECT pauses; START+RIGHT saves logs' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'source=logd log flush checkpoint reason=pre-core-launch' \
		'$(BUILD_DIR)'/logs/linux-frontend-loglinux.txt
	grep -q 'source=logd log flush checkpoint reason=START+RIGHT' \
		'$(BUILD_DIR)'/logs/linux-frontend-loglinux.txt
	grep -q 'sf2000-frontend: pause menu opened' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: pause UI prepared font=1 fb=320x240 stride=640' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Eq 'sf2000-frontend: pause GE fence complete pending=[1-9][0-9]*' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: pause framebuffer wrote bytes=153600 stride=640' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: pause framebuffer wrote bytes=153600 stride=640 presenter=GE' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: pause menu exit selected' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Eq 'sf2000-frontend: GE RGB565 stretch presenter ready .* buffers=2 fenced_depth=2' '$(BUILD_DIR)'/logs/linux-frontend.log
	! grep -Eq 'GE (unavailable|present failed|framebuffer source allocation failed)|CPU presenter active|using CPU write' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-frontend: first frame ' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -Eq 'source=frontend-metric audio metric generated=[0-9]+ submitted=[0-9]+ dropped=0 eagain=0 xrun=0 interval_xrun=0' '$(BUILD_DIR)'/logs/linux-frontend-loglinux.txt
	grep -q 'sf2000-frontend: returned cleanly' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-powerd: frontend first frame visible' '$(BUILD_DIR)'/logs/linux-frontend.log
	grep -q 'sf2000-powerd: relaunch browser after application exit' '$(BUILD_DIR)'/logs/linux-frontend.log
	! grep -q 'storage-test=' '$(BUILD_DIR)'/logs/linux-frontend.log
	! grep -Eq 'screen (stop|resume) failed|reloc outside program|Kernel panic|Data bus error|Oops\[#' \
		'$(BUILD_DIR)'/logs/linux-frontend.log

run-linux-js2300: qemu linux-full-asd $(JS2300_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 12; printf 'sendkey backspace-ret 500\n'; sleep 2; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(JS2300_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-js2300-core.log \
		> '$(BUILD_DIR)'/logs/linux-js2300-core.console 2>&1
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 1; printf 'sendkey x 100\n'; sleep 8; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(JS2300_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-js2300-ui.log \
		> '$(BUILD_DIR)'/logs/linux-js2300-ui.console 2>&1
	mcopy -o -i '$(JS2300_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-js2300-loglinux.txt

smoke-linux-js2300: run-linux-js2300
	grep -q 'sf2000-browser: launch JS2300 /mnt/sd/CHIP8/TEST.JS' \
		'$(BUILD_DIR)'/logs/linux-js2300-core.log
	grep -q 'sf2000-frontend: core init complete' \
		'$(BUILD_DIR)'/logs/linux-js2300-core.log
	grep -q 'sf2000-frontend: ROM load complete' \
		'$(BUILD_DIR)'/logs/linux-js2300-core.log
	grep -q 'sf2000-frontend: first frame 320x240' \
		'$(BUILD_DIR)'/logs/linux-js2300-core.log
	grep -q 'sf2000-browser: launch JS2300 UI /mnt/sd/SCRIPTS/TEST.JS' \
		'$(BUILD_DIR)'/logs/linux-js2300-ui.log
	grep -q 'ui smoke complete mode=extension' \
		'$(BUILD_DIR)'/logs/linux-js2300-ui.log
	grep -q 'js2300 runtime phase=entry_eval .*exception=0' \
		'$(BUILD_DIR)'/logs/linux-js2300-ui.log
	! grep -Eq 'page allocation failure|Kernel panic|Data bus error|fatal signal|core (init|load|run) timeout' \
		'$(BUILD_DIR)'/logs/linux-js2300-core.log \
		'$(BUILD_DIR)'/logs/linux-js2300-ui.log
	@printf 'PASS smoke-linux-js2300\n'

run-linux-gpsp: qemu linux-full-asd $(GPSP_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 5; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey left 100\n'; \
		sleep 1; printf 'sendkey z 100\n'; sleep 6; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey right 100\n'; \
		sleep 1; printf 'sendkey z 100\n'; sleep 7; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(GPSP_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-gpsp.log \
		> '$(BUILD_DIR)'/logs/linux-gpsp.console 2>&1
	mcopy -o -i '$(GPSP_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt

smoke-linux-gpsp: run-linux-gpsp
	grep -q 'sf2000-browser: launch gpSP /mnt/sd/GBA/TEST.GBA' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q 'sf2000-logd: RAM journal begin: FAT writes deferred' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -Eq 'sf2000-logd: RAM journal end: bytes=[1-9][0-9]* peak=[1-9][0-9]* dropped=0 metrics=[1-9][0-9]* metric_bytes=[1-9][0-9]*' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q 'sf2000-logd: RAM journal drained after frontend exit' '$(BUILD_DIR)'/logs/linux-gpsp.log
	awk '/sf2000-frontend: ROM load begin/{active=1} /sf2000-frontend: returned cleanly/{active=0} active && /name=hc15-write-op/{bad=1} END{exit bad}' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q 'sf2000-frontend: ROM load begin' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q 'source=logd log flush checkpoint reason=pre-core-launch' \
		'$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -q 'sf2000-frontend: ROM load complete' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q '\[gpSP ROM\] size=4194304 buffer_mib=4 swapped=0 direct=0' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -Eq 'sf2000-frontend: GE RGB565 stretch presenter ready .* buffers=2 fenced_depth=2' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -Eq 'sf2000-frontend: first frame 240x160 .*source_hash=[0-9a-f]{8} scanout_hash=[0-9a-f]{8}' '$(BUILD_DIR)'/logs/linux-gpsp.log
	awk '/sf2000-browser: launch gpSP/{launched=1} launched && /scanout-oracle/ && /distinct=([3-9]|1[0-7])/{visible=1} END{exit !visible}' \
		'$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q 'sf2000: ge-queue start seq=' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -q 'sf2000: ge-queue complete seq=' '$(BUILD_DIR)'/logs/linux-gpsp.log
	! grep -q 'GE doorbell while command queue busy' '$(BUILD_DIR)'/logs/linux-gpsp.log
	grep -Eq 'source=frontend-metric audio metric generated=[0-9]+ submitted=[0-9]+ dropped=0 eagain=0 xrun=0 interval_xrun=0' '$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -q 'source=frontend-metric mode event mode=uncapped audio=suppressed pacing=disabled full_frame=1' '$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -Eq 'source=frontend-metric audio metric .*suppressed=[1-9][0-9]* .*mode=uncapped presenter=GE' '$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -Eq 'source=frontend-metric audio metric .*ge_stage_frames=[1-9][0-9]*.*buffered_frames=0.*mode=uncapped presenter=GE' '$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -q 'source=frontend-metric mode event mode=normal audio=enabled pacing=core full_frame=1' '$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -Eq 'source=frontend-metric audio metric .*xrun=0 .*delay=([1-9][0-9]*|0) resample_hz=(32000|0) .*mode=normal presenter=GE' \
		'$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -Eq 'source=frontend-metric audio metric .*ge_stage_frames=[1-9][0-9]*.*buffered_frames=0.*mode=normal presenter=GE' \
		'$(BUILD_DIR)'/logs/linux-gpsp-loglinux.txt
	grep -q 'sf2000-frontend: returned cleanly' '$(BUILD_DIR)'/logs/linux-gpsp.log
	! grep -Eq 'reloc outside program|Kernel panic|frontend: fault' '$(BUILD_DIR)'/logs/linux-gpsp.log

run-linux-gpsp-real: qemu linux-full-asd gpsp-real-test-sd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 15; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(GPSP_REAL_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-gpsp-real.log \
		> '$(BUILD_DIR)'/logs/linux-gpsp-real.console 2>&1
	mcopy -o -i '$(GPSP_REAL_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-gpsp-real-loglinux.txt

smoke-linux-gpsp-real: run-linux-gpsp-real
	grep -q 'sf2000-browser: launch gpSP multicore /mnt/sd/GBA/TEST.GBA' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: ROM load begin' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: ROM load complete' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: load ROM 16/16' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: load GPSP_LOAD_OK 7/7' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	! grep -Eq 'Instruction bus error|Data bus error|fatal signal|signal 11|Kernel panic|frontend: fault' \
		'$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: ALSA mono DMA presenter ready' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: timing .*audio_core_hz=22050 audio_output_hz=22050' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -Eq 'sf2000-frontend: first frame 240x160 .*source_hash=[0-9a-f]{8} scanout_hash=[0-9a-f]{8}' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	awk '/sf2000-browser: launch gpSP/{launched=1} launched && /scanout-oracle/ && /distinct=([3-9]|1[0-7])/{visible=1} END{exit !visible}' \
		'$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000: ge-queue start seq=' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000: ge-queue complete seq=' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	! grep -q 'GE doorbell while command queue busy' '$(BUILD_DIR)'/logs/linux-gpsp-real.log
	! grep -Eq 'reloc outside program|Kernel panic|frontend: fault|core (load|run) timeout' \
		'$(BUILD_DIR)'/logs/linux-gpsp-real.log

run-linux-qpsx-real: qemu linux-full-asd qpsx-real-test-sd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 2; printf 'sendkey ret 100\n'; sleep 25; printf 'q\n') | \
		SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(QPSX_REAL_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-qpsx-real.log \
		> '$(BUILD_DIR)'/logs/linux-qpsx-real.console 2>&1

smoke-linux-qpsx-real: run-linux-qpsx-real
	@case '$(QPSX_REAL_IMAGE)' in \
		*.[cC][uU][eE]) expected='00-TEST.cue';; \
		*) expected='TEST.BIN';; \
	esac; \
	grep -q "sf2000-browser: launch QPSX /mnt/sd/PSX/$$expected" \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	grep -q 'sf2000-frontend: core init complete' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	grep -q 'sf2000-frontend: ROM load complete' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	grep -Eq 'QPSX_232_TEST: psxM=0x[0-9A-Fa-f]+ \(fixed=0\)' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	! grep -q 'Using FIXED address 0x85000000 for PSX RAM' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	grep -Eq 'sf2000-frontend: first frame [0-9]+x[0-9]+ .*source_hash=[0-9a-f]{8} scanout_hash=[0-9a-f]{8}' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	grep -q 'QPSX: Menu closed - resyncing SPU' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	awk '/QPSX: Menu closed - resyncing SPU/{closed=1} closed && /scanout-oracle/ { \
		distinct=0; nonblack=0; \
		for (i=1; i<=NF; i++) { \
			if ($$i ~ /^distinct=/) { split($$i, a, "="); distinct=a[2]+0; } \
			if ($$i ~ /^nonblack=/) { split($$i, b, "="); nonblack=b[2]+0; } \
		} \
		if (distinct >= 3 && nonblack > 0) visible=1; \
	} END{exit !visible}' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	awk '/QPSX: Menu closed - resyncing SPU/{closed=1} closed && /QPSX: retro_run progress: frame [0-9]+/ { \
		if (match($$0, /frame [0-9]+/)) { \
			value=substr($$0, RSTART+6, RLENGTH-6)+0; \
			if (value >= 120) progress=1; \
		} \
	} END{exit !progress}' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log
	! grep -Eq 'Instruction bus error|Data bus error|fatal signal|signal 11|Kernel panic|frontend: fault|core (init|load|run) timeout' \
		'$(BUILD_DIR)'/logs/linux-qpsx-real.log

run-linux-qpsx-no-menu: qemu linux-full-asd qpsx-no-menu-test-sd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 20; printf 'q\n') | \
		SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(QPSX_REAL_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log \
		> '$(BUILD_DIR)'/logs/linux-qpsx-no-menu.console 2>&1

smoke-linux-qpsx-no-menu: run-linux-qpsx-no-menu
	@case '$(QPSX_REAL_IMAGE)' in \
		*.[cC][uU][eE]) expected='00-TEST.cue';; \
		*) expected='TEST.BIN';; \
	esac; \
	grep -q "sf2000-browser: launch QPSX /mnt/sd/PSX/$$expected" \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	grep -q 'sf2000-frontend: ROM load complete' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	grep -q 'QPSX: .*Skipping menu at startup (menu_at_start=OFF)' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	@case '$(QPSX_REAL_IMAGE)' in \
		*.[cC][uU][eE]) expected='00-TEST';; \
		*) expected='TEST';; \
	esac; \
	grep -q "QPSX: Per-game memcard will be created on first save: /mnt/sd/saves/PSX/$$expected.mcd" \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	grep -q 'SIO: LoadMcd: file not found, deferring creation until first save' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	grep -Eq 'sf2000-frontend: first frame [0-9]+x[0-9]+ .*source_hash=[0-9a-f]{8} scanout_hash=[0-9a-f]{8}' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	awk '/sf2000-browser: launch QPSX/{launched=1} launched && /scanout-oracle/ { \
		distinct=0; nonblack=0; \
		for (i=1; i<=NF; i++) { \
			if ($$i ~ /^distinct=/) { split($$i, a, "="); distinct=a[2]+0; } \
			if ($$i ~ /^nonblack=/) { split($$i, b, "="); nonblack=b[2]+0; } \
		} \
		if (distinct >= 3 && nonblack > 0) visible=1; \
	} END{exit !visible}' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	awk '/QPSX: retro_run progress: frame [0-9]+/ { \
		if (match($$0, /frame [0-9]+/)) { \
			value=substr($$0, RSTART+6, RLENGTH-6)+0; \
			if (value >= 120) progress=1; \
		} \
	} END{exit !progress}' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	! grep -Eq 'QPSX: QPSX_084: AUTO-(OPENING|CLOSING) INVISIBLE MENU' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	! grep -Eq 'Instruction bus error|Data bus error|fatal signal|signal 11|Kernel panic|frontend: fault|core (init|load|run) timeout' \
		'$(BUILD_DIR)'/logs/linux-qpsx-no-menu.log
	@printf 'PASS smoke-linux-qpsx-no-menu\\n'

# End-to-end save-state round trip: launch QPSX, open the pause menu with the
# SELECT+START chord, jump to SAVE STATE (item 4), confirm the save, jump to
# LOAD STATE (item 5), confirm the load, then resume.  The frontend kmsg
# markers are the oracle: "save state serialize size ... bytes=" proves
# retro_serialize_size() is no longer 0, and the written/loaded markers prove
# retro_serialize()/retro_unserialize() round-trip through the core's native
# freeze machinery.
run-linux-qpsx-savestate: qemu linux-full-asd qpsx-real-test-sd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 15; printf 'sendkey ret-backspace 2000\n'; sleep 3; \
		printf 'sendkey down 200\n'; sleep 0.5; \
		printf 'sendkey down 200\n'; sleep 0.5; \
		printf 'sendkey down 200\n'; sleep 0.5; \
		printf 'sendkey down 200\n'; sleep 0.5; \
		printf 'sendkey x 200\n'; sleep 12; \
		printf 'sendkey down 200\n'; sleep 0.5; \
		printf 'sendkey x 200\n'; sleep 12; \
		printf 'sendkey z 200\n'; sleep 6; printf 'quit\n') | \
		SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(QPSX_REAL_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log \
		> '$(BUILD_DIR)'/logs/linux-qpsx-savestate.console 2>&1

smoke-linux-qpsx-savestate: run-linux-qpsx-savestate
	grep -q 'sf2000-frontend: pause menu opened' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	grep -Eq 'sf2000-frontend: save state serialize size slot=0 bytes=[1-9][0-9]*' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	grep -q 'sf2000-frontend: save state serialize complete' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	grep -q 'sf2000-frontend: save state written slot=0' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	grep -q 'sf2000-frontend: save state loaded slot=0' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	grep -q 'sf2000-frontend: save state resume frame=1 complete' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	grep -q 'sf2000-frontend: save state resume frame=2 complete' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	! grep -Eq 'save state (unavailable|allocation failed|serialization failed|write failed|rejected|read failed|unserialization failed)' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log
	! grep -Eq 'Instruction bus error|Data bus error|fatal signal|signal 11|Kernel panic|frontend: fault|core (init|load|run) timeout' '$(BUILD_DIR)'/logs/linux-qpsx-savestate.log

# Launch Ridge Racer without game input, switch the frontend to uncapped mode,
# and measure an identical section of its recorded race.  QEMU wall time is a
# comparative emulator-engineering metric; physical logs remain authoritative
# for absolute FPS because TCG does not model the HC15xx pipeline or caches.
run-linux-qpsx-attract-benchmark: qemu $(QPSX_BENCHMARK_ASD_TARGET) $(QPSX_BENCHMARK_SD_TARGET)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 12; printf 'sendkey ret-backspace 2000\n'; sleep 2; \
		printf 'sendkey down 200\n'; sleep 1; \
		printf 'sendkey right 200\n'; sleep 0.3; \
		printf 'sendkey right 200\n'; sleep 0.3; \
		printf 'sendkey right 200\n'; sleep 0.3; \
		printf 'sendkey right 200\n'; sleep 1; \
		printf 'sendkey z 200\n'; sleep '$(QPSX_BENCHMARK_SECONDS)'; \
		printf 'quit\n') | \
		SF2000_SCANOUT_ORACLE=0 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(QPSX_REAL_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp \
		-D '$(BUILD_DIR)'/logs/linux-qpsx-attract-benchmark.log \
		> '$(BUILD_DIR)'/logs/linux-qpsx-attract-benchmark.console 2>&1

benchmark-linux-qpsx-attract: run-linux-qpsx-attract-benchmark
	@awk '/QPSX: retro_run progress: frame (1200|1800)$$/ { \
		line=$$0; sub(/^.*\[/, "", line); sub(/\].*$$/, "", line); \
		time=line+0; frame=$$NF+0; if (frame == 1200) start=time; \
		if (frame == 1800) stop=time; \
	} END { \
		if (!start || !stop || stop <= start) exit 1; \
		printf "QPSX Ridge Racer attract benchmark: 600 frames in %.6f s (%.2f fps)\n", \
			stop-start, 600/(stop-start); \
	}' '$(BUILD_DIR)'/logs/linux-qpsx-attract-benchmark.log
	! grep -Eq 'Instruction bus error|Data bus error|fatal signal|signal 11|Kernel panic|frontend: fault|core (init|load|run) timeout' \
		'$(BUILD_DIR)'/logs/linux-qpsx-attract-benchmark.log

benchmark-linux-qpsx-attract-dev:
	$(MAKE) benchmark-linux-qpsx-attract \
		QPSX_BENCHMARK_SD_TARGET=qpsx-dev-no-menu-test-sd \
		QPSX_BENCHMARK_ASD_TARGET=linux-full-test-asd

run-linux-gpsp-smc: gpsp-smc-test-roms
	@case ' $(GPSP_SMC_TEST_MODES) ' in \
		*' $(GPSP_SMC_MODE) '*) ;; \
		*) echo 'invalid GPSP_SMC_MODE=$(GPSP_SMC_MODE)' >&2; exit 2 ;; \
	esac
	$(MAKE) run-linux-gpsp-real \
		GPSP_REAL_ROM='$(BUILD_DIR)/gpsp-$(GPSP_SMC_MODE).gba'

smoke-linux-gpsp-smc: run-linux-gpsp-smc
	grep -Eq 'scanout-oracle .*hash=6ddf4dc5 distinct=1 nonblack=76800 ' \
		'$(BUILD_DIR)'/logs/linux-gpsp-real.log
	grep -q 'sf2000-frontend: returned cleanly' \
		'$(BUILD_DIR)'/logs/linux-gpsp-real.log
	! grep -Eq 'bad jump|Instruction bus error|Data bus error|signal 11|Kernel panic|reloc outside program|frontend: fault' \
		'$(BUILD_DIR)'/logs/linux-gpsp-real.log

run-linux-frontend-lifecycle: qemu linux-full-asd \
		$(FRONTEND_LIFECYCLE_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 10; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 5; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 3; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey z 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 8; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(FRONTEND_LIFECYCLE_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp \
		-D '$(BUILD_DIR)'/logs/linux-frontend-lifecycle.log \
		> '$(BUILD_DIR)'/logs/linux-frontend-lifecycle.console 2>&1
	mcopy -o -i '$(FRONTEND_LIFECYCLE_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle-loglinux.txt

smoke-linux-frontend-lifecycle: run-linux-frontend-lifecycle
	grep -q 'sf2000-browser: launch gpSP /mnt/sd/GBA/TEST.GBA' \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle.log
	grep -Fq 'sf2000-browser: launch Gambatte /mnt/sd/GB/TEST GAME.GB' \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle.log
	grep -Eq '\[gpSP JIT cache\] calls=[1-9][0-9]* failures=0' \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle.log
	test "$$(grep -c 'sf2000-frontend: returned cleanly' \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle.log)" -eq 2
	grep -Eq 'source=frontend-metric audio metric .*sampled_present_us=[0-9]+ .*presenter=GE' \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle-loglinux.txt
	! grep -Eq 'GE present failed|screen (stop|resume) failed|GE invalid unmarked ring rewind|unaligned instruction access|frontend signal=|fatal signal=|reloc outside program|Kernel panic' \
		'$(BUILD_DIR)'/logs/linux-frontend-lifecycle.log

run-linux-reboot: qemu linux-rom-sd
	test -f '$(BOOTROM_BUGFIX)'
	mkdir -p '$(BUILD_DIR)'/logs
	# Home menu: Library, Settings, Reset, Safe Shutdown.  Select Reset (A).
	(sleep 12; printf 'sendkey down 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 12; \
		printf 'quit\n') | \
			'$(QEMU_BIN)' -M sf2000 $(QEMU_ROM_CPU_ARGS) -bios '$(BOOTROM_BUGFIX)' \
		-drive if=none,id=sd0,file='$(LINUX_ROM_SD_IMAGE)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-reboot.log \
		> '$(BUILD_DIR)'/logs/linux-reboot.console 2>&1

smoke-linux-reboot: run-linux-reboot
	grep -q 'sf2000_linux: init alive' '$(BUILD_DIR)'/logs/linux-reboot.log
	grep -Eq 'sf2000-browser: system action reset|sf2000_userspace: clean restart requested|sf2000_userspace: storage synchronized, restarting|sf2000: watchdog restart' \
		'$(BUILD_DIR)'/logs/linux-reboot.log
	test "$$(grep -c 'sf2000: uart:  Hichip Bootloader' '$(BUILD_DIR)'/logs/linux-reboot.log)" -ge 1

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

run-linux-full-asd:
	$(MAKE) ROOTFS=full \
		SDCARD_ASD_SYNC=0 \
		SMOKE_INIT_PATTERN='sf2000_linux: init alive' run-linux-asd

smoke-linux-full-asd:
	$(MAKE) ROOTFS=full \
		SDCARD_ASD_SYNC=0 \
		SMOKE_INIT_PATTERN='sf2000_linux: init alive' smoke-linux-asd
	grep -q 'sf2000_userspace: userspace alive' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000-ge .*HC15xx GE queue at' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000_userspace: graphics engine ready /dev/ge' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'new USB bus registered' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q '18818600.serial: ttyS0 .*irq = 17' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'unexpected IRQ' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-asd.log

# Boot with the graphics-engine completion IRQ suppressed (the physical device
# can deliver 0 GE IRQs through the sysint cascade).  The engine still completes
# queues and latches STATUS.DONE, so the kernel must reach screen-ready through
# the HCGE poll path; a regression to IRQ-only completion waits would stall here.
smoke-linux-full-ge-no-irq:
	$(MAKE) ROOTFS=full QEMU_MACHINE_ARGS=',ge-no-irq=on' \
		SMOKE_INIT_PATTERN='sf2000_linux: init alive' smoke-linux-asd
	grep -q 'sf2000_userspace: userspace alive' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000_userspace: graphics engine ready /dev/ge' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-sync-ok' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -qE 'sync timeout|ETIMEDOUT|completion.*timeout|timed out' '$(BUILD_DIR)'/logs/linux-asd.log

# Complete one optimized RGB565 GE blit without writing its destination.  This
# reproduces the physical symptom: HC15xx reports sync completion, but the
# scanout sample is wrong.  A completed verification failure must repair the
# scanout on the CPU without resetting the already-running VOU/GMA owner.
smoke-linux-full-ge-verify-cpu-fallback:
	$(MAKE) ROOTFS=full QEMU_MACHINE_ARGS=',ge-fault-dest=on' \
		SMOKE_INIT_PATTERN='sf2000_linux: init alive' smoke-linux-asd
	grep -q 'sf2000: ge fault-dest: skipped RGB565 blit 4 destination write' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000_userspace: userspace alive' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-verify-fail' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-present-fail' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-band-mask' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'value=0x000000ff name=screen-ge-band-mask' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-band-count' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-node-words' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-node-hash' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'GE present failed .*sync=0 verify=0' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'copied on CPU' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ge-cpu-only' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-asd.log
	cpu_only_line="$$(grep -n 'name=screen-ge-cpu-only' '$(BUILD_DIR)'/logs/linux-asd.log | head -n 1 | cut -d: -f1)"; \
		test -n "$$cpu_only_line"; \
		! tail -n +"$$cpu_only_line" '$(BUILD_DIR)'/logs/linux-asd.log | grep -q 'name=screen-ge-submit'
	! grep -q 'name=screen-ge-verify-reset' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'name=screen-ge-sync-reset' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'name=screen-ge-reset-gma-invalid' '$(BUILD_DIR)'/logs/linux-asd.log

# Keep the old name as a compatibility alias; completed verification failures
# intentionally use CPU repair and do not reset the running VOU/GMA owner.
smoke-linux-full-ge-verify-reset: smoke-linux-full-ge-verify-cpu-fallback

# The screen service must not write /dev/kmsg while the GMA/VOU display
# handoff is in flight: the ttyS0 console drains every record synchronously
# in the writer's context and the service's own /dev/kmsg reader renders
# records back onto the live scanout.  Runs 135..142 (per-message kmsg
# logging through the handoff, b3bfda4) scrambled the physical panel on
# every boot and both boot paths; runs 143/144 (kmsg-quiet handoff, c325f8b)
# were clean with an identical GE sequence and byte-identical handoff
# registers.  Handoff diagnostics are deferred and flushed only after the
# scanout is stable.
# A non-default LINUX_CMDLINE makes SDCARD_ASD_SYNC_DEFAULT=0, so the
# diagnostic build is never copied to the physical SD image; rebuild the
# default ASD afterwards (smoke-linux-full-asd) before flashing.
smoke-linux-full-handoff-quiet:
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='$(LINUX_DEFAULT_CMDLINE) SF2000_DISPLAY_DIAG=1' \
		SMOKE_INIT_PATTERN='sf2000_linux: init alive' smoke-linux-asd
	grep -q 'sf2000_userspace: userspace alive' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000-screen: handoff diagnostics flushed' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000-screen: disp-state handoff-done' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-handoff-kmsg-calls' '$(BUILD_DIR)'/logs/linux-asd.log
	# No screen-service kmsg record may appear between the last pre-handoff
	# record (backlight ownership) and the deferred-flush marker.  The
	# "backlight ownership" anchor is the last log_line emitted before
	# run_direct_console() sets handoff_critical; keep it that way (the
	# comment above the handoff_critical assignment documents the guard).
	last_pre="$$(grep -n 'sf2000-screen: taking backlight ownership' '$(BUILD_DIR)'/logs/linux-asd.log | tail -n 1 | cut -d: -f1)"; \
		flush="$$(grep -n 'sf2000-screen: handoff diagnostics flushed' '$(BUILD_DIR)'/logs/linux-asd.log | tail -n 1 | cut -d: -f1)"; \
		test -n "$$last_pre" -a -n "$$flush"; \
		awk -v a="$$last_pre" -v b="$$flush" \
			'NR > a && NR < b && (/sf2000-screen:/ || /sf2000-perf:/) { bad = 1 } END { exit bad }' \
			'$(BUILD_DIR)'/logs/linux-asd.log

# Boot with the kernel load window (KSEG0 0x80600000-0x80c00000) prefilled
# with the stale word 0x61006441 (a COP1 instruction) -- the physical-device
# warm-boot condition behind runs 108/110/112/113/114, where RAM in the load
# window still holds garbage from a previous image.  A loader that leaves any
# gap in its copy+flush leaves that word executable and the kernel faults;
# the uncached-KSEG1 loader must overwrite the whole window and reach
# screen-ready with no Reserved Instruction.
smoke-linux-full-stale-ram:
	$(MAKE) ROOTFS=full QEMU_MACHINE_ARGS=',stale-ram=on' \
		SMOKE_INIT_PATTERN='sf2000_linux: init alive' smoke-linux-asd
	grep -q 'sf2000: stale-ram=on: prefilled' '$(BUILD_DIR)'/logs/linux-asd.console
	grep -q 'sf2000_userspace: userspace alive' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000_userspace: graphics engine ready /dev/ge' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -qE 'ri-insn-data|ri-epc' '$(BUILD_DIR)'/logs/linux-asd.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-full-storage:
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-full-storage-fast

smoke-linux-full-storage:
	$(MAKE) ROOTFS=full smoke-linux-full-storage-writeback

run-linux-full-storage-fast:
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-asd

smoke-linux-full-storage-fast:
	$(MAKE) ROOTFS=full \
		SF2000_TRACE_SDIO='1' \
		QEMU_DEBUG='guest_errors,unimp' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-full-storage-fast
	grep -q 'Run /usr/sbin/sf2000-storage-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'hc15-probe' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'HC15 SD/MMC host registered' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sdio-access write addr=0x1884c004' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sdio-access write addr=0x1884c002' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-full-storage-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback $(QEMU_ORACLE_ARGS)

smoke-linux-full-storage-writeback:
	$(MAKE) -C '$(QEMU_ORACLE_DIR)' smoke-stock-fatfs-writeback $(QEMU_ORACLE_ARGS)

run-linux-full-storage-probe-writeback:
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-asd

smoke-linux-full-storage-probe-writeback:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-storage-probe-writeback.XXXXXX.img); \
	trap 'rm -f $$tmp_sd' EXIT; \
	truncate -s 16M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	$(MAKE) ROOTFS=full \
		SF2000_TRACE_SDIO='1' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-full-storage-probe-writeback; \
	grep -q 'Run /usr/sbin/sf2000-storage-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'mmcblk0: mmc0:' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-access write addr=0x1884c024' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-access write addr=0x1884c02c' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-dma-write lba=16 .*len=4096 copied=4096' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'value=0x00000000 name=exit-code' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -a -q 'sf2000 linux sd write test 0239' "$$tmp_sd"

run-linux-full-storage-enumeration:
	$(MAKE) ROOTFS=full \
		SF2000_TRACE_SDIO='1' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-asd

smoke-linux-full-storage-enumeration:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-storage-enumeration.XXXXXX.img); \
	trap 'rm -f $$tmp_sd' EXIT; \
	truncate -s 16M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	$(MAKE) ROOTFS=full \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-full-storage-enumeration; \
	grep -q 'sdio-dma-scr .*len=8 result=0' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'mmc0: new SDXC card' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'mmcblk0: mmc0:' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sdio-dma-read .*len=4096 copied=4096' '$(BUILD_DIR)'/logs/linux-asd.log

smoke-linux-full-persistent-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-persistent-storage.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-loglinux.XXXXXX.txt); \
	trap 'rm -f $$tmp_sd $$tmp_log' EXIT; \
	truncate -s 64M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	mcopy -i "$$tmp_sd" ::loglinux.txt "$$tmp_log"; \
	grep -q 'source=kmsg .*sf2000-mount: mount ok' "$$tmp_log"; \
	grep -q 'source=kmsg ' "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux pre-mount profile begin ---' "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux storage mounted ---' "$$tmp_log"; \
	grep -q 'source=proc-stat ' "$$tmp_log"; \
	grep -q 'source=proc-meminfo ' "$$tmp_log"; \
	grep -q 'source=proc-interrupts ' "$$tmp_log"; \
	grep -q 'source=heartbeat alive' "$$tmp_log"; \
	grep -q 'sf2000-mount: mount service start (hotplug)' '$(BUILD_DIR)'/logs/linux-asd.log; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log

# Most consumer SD cards are MBR/GPT + a VFAT partition (/dev/sda1 on a host,
# /dev/mmcblk0p1 on the device).  Superfloppy images used by other smokes do not
# cover that layout.
smoke-linux-full-partitioned-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-partitioned-storage.XXXXXX.img); \
	tmp_p1=$$(mktemp '$(BUILD_DIR)'/sf2000-partitioned-p1.XXXXXX.img); \
	tmp_p2=$$(mktemp '$(BUILD_DIR)'/sf2000-partitioned-p2.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-part-loglinux.XXXXXX.txt); \
	trap 'rm -f $$tmp_sd $$tmp_p1 $$tmp_p2 $$tmp_log' EXIT; \
	p1_lba=2048; \
	p1_sectors=$$((32 * 1024 * 1024 / 512)); \
	p2_lba=$$((p1_lba + p1_sectors)); \
	p2_sectors=$$((32 * 1024 * 1024 / 512)); \
	truncate -s $$(((p2_lba + p2_sectors) * 512)) "$$tmp_sd"; \
	truncate -s $$((p1_sectors * 512)) "$$tmp_p1"; \
	truncate -s $$((p2_sectors * 512)) "$$tmp_p2"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_p1" >/dev/null 2>&1; \
	mkfs.vfat -F 32 -n ROMS "$$tmp_p2" >/dev/null 2>&1; \
	mmd -i "$$tmp_p1" ::bios ::firmware ::saves ::sf2000; \
	mcopy -i "$$tmp_p1" Makefile ::sf2000/ui.ttf; \
	mmd -i "$$tmp_p2" ::GB; \
	python3 -c "import struct; \
p1_lba=$$p1_lba; p1_sec=$$p1_sectors; p2_lba=$$p2_lba; p2_sec=$$p2_sectors; \
mbr=bytearray(512); \
mbr[0x1be:0x1be+16]=struct.pack('<BBBBBBBBII',0x80,1,1,0,0x0c,0xfe,0xff,0xff,p1_lba,p1_sec); \
mbr[0x1ce:0x1ce+16]=struct.pack('<BBBBBBBBII',0x00,1,1,0,0x0c,0xfe,0xff,0xff,p2_lba,p2_sec); \
mbr[510:512]=b'\\x55\\xaa'; open('$$tmp_sd','r+b').write(mbr)"; \
	dd if="$$tmp_p1" of="$$tmp_sd" bs=512 seek="$$p1_lba" conv=notrunc status=none; \
	dd if="$$tmp_p2" of="$$tmp_sd" bs=512 seek="$$p2_lba" conv=notrunc status=none; \
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	grep -Eq 'mmcblk0: *p1 p2' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: volume /dev/mmcblk0p1 type=vfat score=457' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: mount ok primary=/dev/mmcblk0p1' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: volume /dev/mmcblk0p2' '$(BUILD_DIR)'/logs/linux-asd.log; \
	mcopy -i "$$tmp_sd@@$$((p1_lba * 512))" ::loglinux.txt "$$tmp_log"; \
	grep -q 'source=kmsg .*sf2000-mount: mount ok primary=/dev/mmcblk0p1' "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux storage mounted ---' "$$tmp_log"; \
	grep -q 'source=heartbeat alive' "$$tmp_log"; \
	grep -q 'sf2000-mount: mount service start (hotplug)' '$(BUILD_DIR)'/logs/linux-asd.log; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log; \
	! grep -q 'sf2000-mount: mount failed' '$(BUILD_DIR)'/logs/linux-asd.log

# Bootloader-proven FAT16 primary (QEMU stock full-chain + ROM FAT strings).
smoke-linux-full-fat16-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-fat16-storage.XXXXXX.img); \
	tmp_part=$$(mktemp '$(BUILD_DIR)'/sf2000-fat16-part.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-fat16-loglinux.XXXXXX.txt); \
	trap 'rm -f $$tmp_sd $$tmp_part $$tmp_log' EXIT; \
	part_lba=2048; \
	part_sectors=$$((48 * 1024 * 1024 / 512)); \
	truncate -s $$(((part_lba + part_sectors) * 512)) "$$tmp_sd"; \
	truncate -s $$((part_sectors * 512)) "$$tmp_part"; \
	mkfs.vfat -F 16 -n SF2000 "$$tmp_part" >/dev/null 2>&1; \
	mmd -i "$$tmp_part" ::bios ::firmware ::saves; \
	python3 -c "import struct; lba=$$part_lba; sec=$$part_sectors; mbr=bytearray(512); mbr[0x1be:0x1be+16]=struct.pack('<BBBBBBBBII',0x80,1,1,0,0x06,0xfe,0xff,0xff,lba,sec); mbr[510:512]=b'\\x55\\xaa'; open('$$tmp_sd','r+b').write(mbr)"; \
	dd if="$$tmp_part" of="$$tmp_sd" bs=512 seek="$$part_lba" conv=notrunc status=none; \
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	grep -q 'sf2000-mount: mount ok primary=/dev/mmcblk0p1' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -Eq 'sf2000-mount: volume /dev/mmcblk0p1 type=(vfat|msdos)' '$(BUILD_DIR)'/logs/linux-asd.log; \
	mcopy -i "$$tmp_sd@@$$((part_lba * 512))" ::loglinux.txt "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux storage mounted ---' "$$tmp_log"; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log

# Bootloader ROM contains EXFAT OEM/type strings; Linux mounts exFAT volumes.
smoke-linux-full-exfat-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-exfat-storage.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-exfat-loglinux.XXXXXX.txt); \
	trap 'rm -f $$tmp_sd $$tmp_log' EXIT; \
	part_lba=2048; \
	part_bytes=$$((64 * 1024 * 1024)); \
	part_sectors=$$((part_bytes / 512)); \
	truncate -s $$((part_lba * 512 + part_bytes)) "$$tmp_sd"; \
	python3 -c "import struct; lba=$$part_lba; sec=$$part_sectors; mbr=bytearray(512); mbr[0x1be:0x1be+16]=struct.pack('<BBBBBBBBII',0x80,1,1,0,0x07,0xfe,0xff,0xff,lba,sec); mbr[510:512]=b'\\x55\\xaa'; open('$$tmp_sd','r+b').write(mbr)"; \
	loop=$$(losetup -f --show -o $$((part_lba * 512)) "$$tmp_sd"); \
	mkfs.exfat -n SF2000 "$$loop" >/dev/null; \
	losetup -d "$$loop"; \
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	grep -q 'sf2000-mount: volume /dev/mmcblk0p1 type=exfat' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: mount ok primary=/dev/mmcblk0p1' '$(BUILD_DIR)'/logs/linux-asd.log; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log

# Primary FAT32 system partition + secondary exFAT ROM partition.
smoke-linux-full-mixed-fs-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-mixed-fs.XXXXXX.img); \
	tmp_p1=$$(mktemp '$(BUILD_DIR)'/sf2000-mixed-p1.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-mixed-loglinux.XXXXXX.txt); \
	trap 'rm -f $$tmp_sd $$tmp_p1 $$tmp_log' EXIT; \
	p1_lba=2048; \
	p1_sectors=$$((32 * 1024 * 1024 / 512)); \
	p2_lba=$$((p1_lba + p1_sectors)); \
	p2_sectors=$$((64 * 1024 * 1024 / 512)); \
	truncate -s $$(((p2_lba + p2_sectors) * 512)) "$$tmp_sd"; \
	truncate -s $$((p1_sectors * 512)) "$$tmp_p1"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_p1" >/dev/null 2>&1; \
	mmd -i "$$tmp_p1" ::bios ::firmware ::saves; \
	python3 -c "import struct; \
p1_lba=$$p1_lba; p1_sec=$$p1_sectors; p2_lba=$$p2_lba; p2_sec=$$p2_sectors; \
mbr=bytearray(512); \
mbr[0x1be:0x1be+16]=struct.pack('<BBBBBBBBII',0x80,1,1,0,0x0c,0xfe,0xff,0xff,p1_lba,p1_sec); \
mbr[0x1ce:0x1ce+16]=struct.pack('<BBBBBBBBII',0x00,1,1,0,0x07,0xfe,0xff,0xff,p2_lba,p2_sec); \
mbr[510:512]=b'\\x55\\xaa'; open('$$tmp_sd','r+b').write(mbr)"; \
	dd if="$$tmp_p1" of="$$tmp_sd" bs=512 seek="$$p1_lba" conv=notrunc status=none; \
	loop=$$(losetup -f --show -o $$((p2_lba * 512)) "$$tmp_sd"); \
	mkfs.exfat -n ROMS "$$loop" >/dev/null; \
	losetup -d "$$loop"; \
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	grep -Eq 'mmcblk0: *p1 p2' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: volume /dev/mmcblk0p1 type=vfat' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: volume /dev/mmcblk0p2 type=exfat' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: mount ok primary=/dev/mmcblk0p1 extras=1' '$(BUILD_DIR)'/logs/linux-asd.log; \
	mcopy -i "$$tmp_sd@@$$((p1_lba * 512))" ::loglinux.txt "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux storage mounted ---' "$$tmp_log"; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log

# Bootloader-proven stock superfloppy layout: FAT32 at sector 0 with no usable
# MBR.  The mount service must try /dev/mmcblk0 first instead of probing the
# ghost partition nodes (~300 ms each on the physical device).
smoke-linux-full-superfloppy-storage:
	set -e; \
	tmp_sd=$$(mktemp '$(BUILD_DIR)'/sf2000-superfloppy.XXXXXX.img); \
	tmp_log=$$(mktemp '$(BUILD_DIR)'/sf2000-superfloppy-loglinux.XXXXXX.txt); \
	trap 'rm -f $$tmp_sd $$tmp_log' EXIT; \
	truncate -s 64M "$$tmp_sd"; \
	mkfs.vfat -F 32 -n SF2000 "$$tmp_sd" >/dev/null 2>&1; \
	mmd -i "$$tmp_sd" ::sf2000; \
	mcopy -i "$$tmp_sd" Makefile ::sf2000/ui.ttf; \
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		QEMU_SD_ARGS="-drive if=none,id=sd0,file=$$tmp_sd,format=raw" \
		run-linux-asd; \
	grep -q 'sf2000-mount: superfloppy: whole disk first' '$(BUILD_DIR)'/logs/linux-asd.log; \
	grep -q 'sf2000-mount: mount ok primary=/dev/mmcblk0' '$(BUILD_DIR)'/logs/linux-asd.log; \
	mcopy -i "$$tmp_sd" ::loglinux.txt "$$tmp_log"; \
	grep -q 'source=logd --- SF2000 Linux storage mounted ---' "$$tmp_log"; \
	grep -q 'source=heartbeat alive' "$$tmp_log"; \
	! grep -q 'Kernel bug detected' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-full-storage-launch:
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-storage-fastprobe' \
		run-linux-full-storage-fast

smoke-linux-full-storage-launch: run-linux-full-storage-launch
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

run-linux-full-rom:
	$(MAKE) ROOTFS=full \
		SMOKE_INIT_PATTERN='sf2000_userspace: userspace alive' run-linux-rom

smoke-linux-full-rom:
	$(MAKE) ROOTFS=full \
		SMOKE_INIT_PATTERN='sf2000_userspace: userspace alive' smoke-linux-rom
	grep -q 'sf2000: uart: .*sf2000: early watchdog armed' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'sf2000_userspace: early watchdog disabled' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'name=screen-after-gma-desc' '$(BUILD_DIR)'/logs/linux-rom.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-rom.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-rom.log

run-linux-full-display: qemu
	$(MAKE) ROOTFS=full linux-asd
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/screenshots/linux-full-gma
	rm -f '$(BUILD_DIR)'/screenshots/linux-full-gma/sf2000-gma-*.ppm \
		'$(BUILD_DIR)'/screenshots/linux-full-gma/sf2000-gma-latest.ppm
	SF2000_TRACE_GMA=1 \
	SF2000_GMA_DUMP_DIR='$(BUILD_DIR)'/screenshots/linux-full-gma \
	SF2000_GMA_DUMP_LIMIT=8 \
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) $(QEMU_DISPLAY_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-full-display.log \
		> '$(BUILD_DIR)'/logs/linux-full-display.console 2>&1 || test $$? -eq 124

smoke-linux-full-display: run-linux-full-display
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-full-display.console
	grep -q 'sf2000: VOU RGB compositor latch complete' '$(BUILD_DIR)'/logs/linux-full-display.console
	grep -q 'simple-framebuffer .*fb0: simplefb registered' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'sf2000_userspace: framebuffer ready /dev/fb0' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'sf2000-screen: /dev/fb0 RGB565 write ready' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-ge-console-fill-ok' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'sf2000: reserved diag memory gma=0xf00000+0x100000' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-after-backlight' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-after-gma-desc' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-ready-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-loop-present-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-cached-render-ready' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00f00000 name=screen-gma-present-desc' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00f00280 name=screen-gma-present-desc' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-ge-scanout-init-begin' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-ge-scanout-clear-ok' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-ge-scanout-init-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-ge-scanout-init-fail' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-vou-latch-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-rgb-prime-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-rgb-prime2-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-rgb-engine-ready' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-ge-mcu-visible' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-gma-probe-begin' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00137002 name=screen-rgb-vou-connect-ctrl' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000015 name=screen-rgb-vou-connect-mode' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'VOU raster disconnected from PRGB' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00040001 name=screen-raster-ctl-hw' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00f00280 name=screen-raster-expected' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00f00000 name=screen-raster-expected' '$(BUILD_DIR)'/logs/linux-full-display.log
	test "$$(grep -c 'name=screen-raster-wait-ok' '$(BUILD_DIR)'/logs/linux-full-display.log)" -ge 2
	! grep -q 'name=screen-raster-wait-fail' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-rgb-handoff-abort' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-post-gma-dmba-hw' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000004 name=screen-native-hold-begin' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000004 name=screen-native-hold-count' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000004 name=screen-native-hold-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-native-present' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-native-hold-ms' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x01300378 name=screen-native-hold-vou' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00040001 name=screen-native-hold-gma' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-native-present-fail' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-te-conditioning-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-te-stream-start' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-te-rearm-edge' '$(BUILD_DIR)'/logs/linux-full-display.log
	gate1="$$(sed -n 's/.*value=\(0x[0-9a-fA-F]*\) name=screen-post-gate1.*/\1/p' '$(BUILD_DIR)'/logs/linux-full-display.log | tail -n 1)"; \
		test $$((gate1 & 0x600)) -eq 1536
	grep -q 'name=screen-panel-id' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=screen-panel-aux' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-probe-restore-present' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'name=screen-native-hold-fail' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000000 name=screen-rgb-source' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x01300378 name=screen-vou-total' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x028e000a name=screen-vou-hactive' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x011e002e name=screen-vou-vactive' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000001 name=screen-vou-geometry-contract' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00060600 name=screen-rgb-vsync' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0xb6060606 name=screen-rgb-pad-clock' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000029 name=screen-panel-command-final' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'panel-cmd cmd=0x2c' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'panel-data cmd=0x36 index=0 value=0x0070' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'GMA doorbell before VOU RGB latch' '$(BUILD_DIR)'/logs/linux-full-display.log
	# QEMU always boots linux directly (no bootloader UI), so the display
	# cold re-init for an inherited bootloader state must be a no-op here.
	! grep -q 'name=screen-cold-takeover-done' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'GMA scanout with panel VSYNC disconnected' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'GMA scanout with panel pixel clock disconnected' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'GMA scanout while panel remains in MCU RAMWR state' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'GMA scanout while panel RAMCTRL remains MCU-owned' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'panel entered RGB mode before VOU/GMA raster was active' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'Data bus error' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'assert(common.c' '$(BUILD_DIR)'/logs/linux-full-display.log
	! grep -q 'Invalid argument\|No such device' '$(BUILD_DIR)'/logs/linux-full-display.log
	test -s '$(BUILD_DIR)'/screenshots/linux-full-gma/sf2000-gma-latest.ppm

metrics-linux:
	@awk '\
	function ktime(   p,n,a) { \
		p = index($$0, "source=kmsg "); \
		if (!p) return -1; \
		n = split(substr($$0, p + 12), a, ","); \
		return n >= 3 ? a[3] + 0 : -1; \
	} \
	/source=kmsg .*sf2000_userspace: starting screen/ { screen_start = ktime() } \
	/source=kmsg .*sf2000-screen: main entry/ && !screen_main { screen_main = ktime() } \
	/source=kmsg .*guarded panel init begin/ { panel_begin = ktime() } \
	/source=kmsg .*guarded panel init done/ { panel_done = ktime() } \
	/source=kmsg .*sf2000_userspace: screen ready/ { screen_ready = ktime() } \
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

metrics-frontend:
	@awk '\
	function field(name,   i,p) { \
		for (i = 1; i <= NF; i++) { \
			p = index($$i, "="); \
			if (p && substr($$i, 1, p - 1) == name) \
				return substr($$i, p + 1); \
		} \
		return ""; \
	} \
	function emit(   fps) { \
		if (!have) return; \
		fps = last_fps / 1000; \
		printf "frontend.session.%u.mode=%s\n", session, mode; \
		printf "frontend.session.%u.frames=%u\n", session, last_frames; \
		printf "frontend.session.%u.fps=%.3f\n", session, fps; \
		printf "frontend.session.%u.xruns=%u\n", session, xruns; \
		printf "frontend.session.%u.eagain=%u\n", session, eagain; \
		printf "frontend.session.%u.dropped=%u\n", session, dropped; \
		printf "frontend.session.%u.pacing_resets=%u\n", session, resets; \
		printf "frontend.session.%u.sampled_max_run_us=%u\n", session, max_run; \
		printf "frontend.session.%u.sampled_present_us=%u\n", session, max_present; \
		printf "frontend.session.%u.presenter=%s\n", session, presenter; \
	} \
	/source=frontend-metric audio metric/ { \
		frames = field("frames") + 0; current_mode = field("mode"); \
		if (have && (frames < last_frames || current_mode != mode)) { \
			emit(); session++; max_run = max_present = 0; \
		} \
		have = 1; mode = current_mode; last_frames = frames; \
		last_fps = field("fps_milli") + 0; xruns = field("xrun") + 0; \
		eagain = field("eagain") + 0; dropped = field("dropped") + 0; \
		resets = field("pacing_resets") + 0; presenter = field("presenter"); \
		run = field("sampled_max_run_us") + 0; \
		present = field("sampled_present_us") + 0; \
		if (run > max_run) max_run = run; \
		if (present > max_present) max_present = present; \
	} \
	/\[gpSP JIT reason\]/ { \
		printf "frontend.gpsp.%u.jit_capacity_flushes=%u\n", \
			++gpsp, field("capacity") + 0; \
		printf "frontend.gpsp.%u.jit_store_flushes=%u\n", \
			gpsp, field("store") + 0; \
		printf "frontend.gpsp.%u.jit_dma_flushes=%u\n", \
			gpsp, field("dma") + 0; \
	} \
	/\[gpSP JIT cache\]/ { \
		printf "frontend.gpsp.%u.cache_sync_calls=%u\n", \
			gpsp, field("calls") + 0; \
		printf "frontend.gpsp.%u.cache_sync_failures=%u\n", \
			gpsp, field("failures") + 0; \
	} \
	END { emit() } \
	' '$(METRICS_LOG)'

benchmark-qemu-linux: qemu linux-full-asd
	mkdir -p '$(BUILD_DIR)'/metrics
	/usr/bin/time -f 'qemu.wall_s=%e\nqemu.user_s=%U\nqemu.sys_s=%S\nqemu.host_cpu_pct=%P\nqemu.max_rss_kb=%M' \
		-o '$(BUILD_DIR)'/metrics/qemu-linux.txt \
		timeout '$(QEMU_BENCH_SECONDS)s' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
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

run-linux-full-fidelity:
	$(MAKE) QEMU_DISPLAY_ARGS='$(QEMU_FIDELITY_ARGS)' \
		QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' run-linux-full-display

smoke-linux-physical-contract: linux-full-asd
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
	/sf2000_userspace: starting screen/ { start[source] = source == 1 ? physical_time() : qemu_time() } \
	/sf2000-screen: main entry/ && !main[source] { main[source] = source == 1 ? physical_time() : qemu_time() } \
	/guarded panel init begin/ { panel0[source] = source == 1 ? physical_time() : qemu_time() } \
	/guarded panel init done/ { panel1[source] = source == 1 ? physical_time() : qemu_time() } \
	/sf2000_userspace: screen ready/ { ready[source] = source == 1 ? physical_time() : qemu_time() } \
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

smoke-linux-full-fidelity:
	$(MAKE) QEMU_DISPLAY_ARGS='$(QEMU_FIDELITY_ARGS)' \
		QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' smoke-linux-full-display
	$(MAKE) smoke-linux-physical-contract
	$(MAKE) metrics-qemu-fidelity
	$(MAKE) metrics-qemu-timing

smoke-linux-full-fb-test:
	$(MAKE) ROOTFS=full QEMU_BOOT_TIMEOUT='$(QEMU_BOOT_TIMEOUT)' \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon init=/init SF2000_FB_TEST=1' \
		run-linux-full-display
	grep -q 'sf2000_userspace: stopping screen for framebuffer test' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'name=init-screen-stop-wait' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'sf2000_userspace: exec /usr/bin/fb-test -p 0' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'fb-test 1.1.1' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'fb res 320x240 virtual 320x240, line_len 640, bpp 16' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'sf2000_userspace: framebuffer test complete' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'value=0x00000000 name=init-fb-test-exit' '$(BUILD_DIR)'/logs/linux-full-display.log
	grep -q 'gma-present .*mode=6' '$(BUILD_DIR)'/logs/linux-full-display.log
	test -s '$(BUILD_DIR)'/screenshots/linux-full-gma/sf2000-gma-latest.ppm
	pixel() { \
		dd if='$(BUILD_DIR)'/screenshots/linux-full-gma/sf2000-gma-latest.ppm \
			bs=1 skip=$$((15 + 3 * ($$2 * 320 + $$1))) count=3 2>/dev/null | \
			od -An -tx1 | tr -d ' \n'; \
	}; \
	test "$$(pixel 0 0)" = ffffff; \
	test "$$(pixel 100 0)" = 00ff00; \
	test "$$(pixel 0 100)" = 0000ff; \
	test "$$(pixel 319 100)" = ff0000; \
	test "$$(pixel 100 239)" = ffff00
run-linux-full-audio: qemu
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1' linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	rm -f '$(BUILD_DIR)'/sf2000-audio.wav
	(sleep 6; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000,audiodev=sf2000wav $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append 'console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1' \
		-audiodev wav,id=sf2000wav,path='$(BUILD_DIR)'/sf2000-audio.wav \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-full-audio.log \
		> '$(BUILD_DIR)'/logs/linux-full-audio.console 2>&1

smoke-linux-full-audio: run-linux-full-audio
	grep -q 'sf2000-pcm .*PCM playback ready' '$(BUILD_DIR)'/logs/linux-full-audio.log
	grep -q 'sf2000-pcm .*PCM playback ready (IRQ)' '$(BUILD_DIR)'/logs/linux-full-audio.log
	grep -q 'sf2000-audio: ALSA PCM DMA tone active' '$(BUILD_DIR)'/logs/linux-full-audio.log
	grep -q 'sf2000: audio guest DMA active' '$(BUILD_DIR)'/logs/linux-full-audio.console
	! grep -q 'sf2000-audio: ALSA PCM write failed' '$(BUILD_DIR)'/logs/linux-full-audio.log
	test -s '$(BUILD_DIR)'/sf2000-audio.wav
	dd if='$(BUILD_DIR)'/sf2000-audio.wav bs=1 skip=44 2>/dev/null | \
		od -An -v -td2 | \
		awk '{ for (i = 1; i <= NF; i++) if ($$i != 0) found = 1 } \
			END { exit !found }'

run-linux-full-audio-44100: qemu
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1 SF2000_AUDIO_RATE=44100' linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	rm -f '$(BUILD_DIR)'/sf2000-audio-44100.wav
	(sleep 6; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000,audiodev=sf2000wav44100 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append 'console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1 SF2000_AUDIO_RATE=44100' \
		-audiodev wav,id=sf2000wav44100,path='$(BUILD_DIR)'/sf2000-audio-44100.wav \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-full-audio-44100.log \
		> '$(BUILD_DIR)'/logs/linux-full-audio-44100.console 2>&1

smoke-linux-full-audio-44100: run-linux-full-audio-44100
	grep -q 'sf2000-pcm .*PCM playback ready' '$(BUILD_DIR)'/logs/linux-full-audio-44100.log
	grep -Eq 'sf2000-pcm .*DMA .*rate=44100 config=[0-9a-f]+ pll=080000c0 mn=0119000e ctl470=[0-9a-f]+ ctl474=[0-9a-f]+ vol=1e' \
		'$(BUILD_DIR)'/logs/linux-full-audio-44100.log
	grep -q 'sf2000-audio: ALSA PCM DMA tone active rate=44100' \
		'$(BUILD_DIR)'/logs/linux-full-audio-44100.log
	grep -q 'sf2000: audio guest DMA active' \
		'$(BUILD_DIR)'/logs/linux-full-audio-44100.console
	! grep -q 'sf2000-audio: ALSA PCM write failed' \
		'$(BUILD_DIR)'/logs/linux-full-audio-44100.log
	test -s '$(BUILD_DIR)'/sf2000-audio-44100.wav
	dd if='$(BUILD_DIR)'/sf2000-audio-44100.wav bs=1 skip=44 2>/dev/null | \
		od -An -v -td2 | \
		awk '{ for (i = 1; i <= NF; i++) if ($$i != 0) found = 1 } \
			END { exit !found }'

run-linux-full-audio-gb300: qemu
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1' linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	rm -f '$(BUILD_DIR)'/gb300-audio.wav
	(sleep 6; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000,board-profile=gb300,audiodev=gb300wav \
		$(QEMU_CPU_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append 'console=ttyS0,115200 earlycon init=/init SF2000_AUDIO_TEST=1' \
		-audiodev wav,id=gb300wav,path='$(BUILD_DIR)'/gb300-audio.wav \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-full-audio-gb300.log \
		> '$(BUILD_DIR)'/logs/linux-full-audio-gb300.console 2>&1

smoke-linux-full-audio-gb300: run-linux-full-audio-gb300
	grep -Eq 'sf2000-pcm .*channels=2 .*route=gb300_l15' \
		'$(BUILD_DIR)'/logs/linux-full-audio-gb300.log
	grep -Eq 'sf2000-pcm .*DMA .*channels=2 rate=[0-9]+ config=0b11[0-9a-f]{4} pll=[0-9a-f]+ mn=01580007 ctl470=[0-9a-f]+ ctl474=00106000 vol=8f' \
		'$(BUILD_DIR)'/logs/linux-full-audio-gb300.log
	grep -q 'sf2000-audio: ALSA PCM DMA tone active' \
		'$(BUILD_DIR)'/logs/linux-full-audio-gb300.log
	grep -q 'sf2000: audio guest DMA active' \
		'$(BUILD_DIR)'/logs/linux-full-audio-gb300.console
	! grep -q 'sf2000-audio: ALSA PCM write failed' \
		'$(BUILD_DIR)'/logs/linux-full-audio-gb300.log
	test -s '$(BUILD_DIR)'/gb300-audio.wav
	dd if='$(BUILD_DIR)'/gb300-audio.wav bs=1 skip=44 2>/dev/null | \
		od -An -v -td2 | \
		awk '{ for (i = 1; i <= NF; i++) if ($$i != 0) found = 1 } \
			END { exit !found }'

$(UNIFROG_QEMU_SD): $(UNIFROG_ASD) $(UNIFROG_SD_ROOT) Makefile
	test -d '$(UNIFROG_SD_ROOT)'
	mkdir -p '$(dir $@)'
	truncate -s 128M '$@'
	mkfs.vfat -F 32 -n UNIFROG '$@' >/dev/null
	mcopy -i '$@' -s '$(UNIFROG_SD_ROOT)'/* ::/

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

run-linux-full-panel: qemu
	$(MAKE) ROOTFS=full full-panel-probe-link \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon SF2000_PANEL_PROBE=1' linux-asd
	mkdir -p '$(BUILD_DIR)'/logs; \
	SF2000_TRACE_PC='1' timeout '$(QEMU_PANEL_PROBE_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append 'console=ttyS0,115200 earlycon SF2000_PANEL_PROBE=1' \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-full-panel.log \
		> '$(BUILD_DIR)'/logs/linux-full-panel.console 2>&1 || test $$? -eq 124

smoke-linux-full-panel: run-linux-full-panel
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-full-panel.console
	grep -q 'Run /init as init process' '$(BUILD_DIR)'/logs/linux-full-panel.log
	grep -q 'sf2000-screen: main entry' '$(BUILD_DIR)'/logs/linux-full-panel.log
	grep -q 'sf2000-screen: panel probe begin' '$(BUILD_DIR)'/logs/linux-full-panel.log
	grep -q 'sf2000: panel-read cmd=0x04 bytes=4 data=e4:85:85:52:00' '$(BUILD_DIR)'/logs/linux-full-panel.log
	! grep -Eq 'service exec failed|Kernel panic|Attempted to kill init' '$(BUILD_DIR)'/logs/linux-full-panel.log

run-linux-full-panel-fast: qemu
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-panel-fastprobe' \
		SF2000_TRACE_PC='1' run-linux-asd
	mv '$(BUILD_DIR)'/logs/linux-asd.log '$(BUILD_DIR)'/logs/linux-full-panel-fast.log
	mv '$(BUILD_DIR)'/logs/linux-asd.console '$(BUILD_DIR)'/logs/linux-full-panel-fast.console

smoke-linux-full-panel-fast: run-linux-full-panel-fast
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-full-panel-fast.console
	grep -q 'Run /usr/sbin/sf2000-panel-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-full-panel-fast.log
	grep -q 'ret-syscall-exit' '$(BUILD_DIR)'/logs/linux-full-panel-fast.log

run-linux-full-input:
	$(MAKE) ROOTFS=full run-linux-input

smoke-linux-full-input:
	$(MAKE) ROOTFS=full smoke-linux-input

run-linux-full-reboot:
	$(MAKE) ROOTFS=full run-linux-reboot

smoke-linux-full-reboot:
	$(MAKE) ROOTFS=full smoke-linux-reboot
	grep -q 'sf2000-pad: SELECT pressed, rebooting' '$(BUILD_DIR)'/logs/linux-reboot.log
	grep -q 'sf2000_userspace: restarting' '$(BUILD_DIR)'/logs/linux-reboot.log
	grep -q 'Hichip bootloader' '$(BUILD_DIR)'/logs/linux-reboot.log

run-linux-full-reset-snapshot:
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		linux-asd
	mkdir -p '$(BUILD_DIR)'/logs; \
	SF2000_TRACE_PC='1' timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) -kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-append 'console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-full-reset-snapshot.log \
		> '$(BUILD_DIR)'/logs/linux-full-reset-snapshot.console 2>&1 || test $$? -eq 124

smoke-linux-full-reset-snapshot: run-linux-full-reset-snapshot
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-full-reset-snapshot.console
	grep -q 'Run /usr/sbin/sf2000-reset-fastprobe as init process' '$(BUILD_DIR)'/logs/linux-full-reset-snapshot.log
	grep -q 'ret-syscall-exit' '$(BUILD_DIR)'/logs/linux-full-reset-snapshot.log

run-linux-full-reset-restore: qemu
	$(MAKE) ROOTFS=full \
		LINUX_CMDLINE='console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		linux-asd
	mkdir -p '$(BUILD_DIR)'/logs '$(BUILD_DIR)'/qmp '$(BUILD_DIR)'/state
	python3 '$(USERSPACE_RESET_RESTORE_SCRIPT)' \
		--qemu '$(QEMU_BIN)' \
		--cpu '$(QEMU_CPU)' \
		--kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		--append 'console=ttyS0,115200 earlycon rdinit=/usr/sbin/sf2000-reset-fastprobe' \
		--state '$(USERSPACE_RESET_RESTORE_STATE)' \
		--source-console '$(USERSPACE_RESET_RESTORE_PREFIX).source.console' \
		--source-log '$(USERSPACE_RESET_RESTORE_PREFIX).source.log' \
		--restore-console '$(USERSPACE_RESET_RESTORE_PREFIX).restore.console' \
		--restore-log '$(USERSPACE_RESET_RESTORE_PREFIX).restore.log' \
		--socket '$(USERSPACE_RESET_RESTORE_SOCKET)' \
		--restore-socket '$(USERSPACE_RESET_RESTORE_SOCKET_DEST)' \
		--timeout '$(QEMU_BOOT_TIMEOUT)'

smoke-linux-full-reset-restore: run-linux-full-reset-restore
	grep -q 'sf2000: loaded ASD' '$(USERSPACE_RESET_RESTORE_PREFIX).source.console'
	grep -q 'sf2000: entry-bytes storage_probe_entry pc=0x047c0050' '$(USERSPACE_RESET_RESTORE_PREFIX).restore.log'

clean:
	rm -rf '$(BUILD_DIR)' '$(LINUX_SRC)'

FCEUMM_TEST_SD := $(BUILD_DIR)/fceumm-test.sd.img
FCEUMM_TEST_ROM ?= /root/host-frogdev/roms/Super Mario Bros. 3 (USA) (Rev 1).nes

$(FCEUMM_TEST_SD): FORCE Makefile $(SDCARD_CORE_STAMP) \
		$(SDCARD_USER_CONFIG) $(SDCARD_UI_FONT)
	@test -f '$(FCEUMM_TEST_ROM)' || { \
		echo 'set FCEUMM_TEST_ROM=/path/to/a.nes' >&2; exit 2; }
	mkdir -p '$(dir $@)'
	truncate -s 64M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/NES ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(FCEUMM_TEST_ROM)' ::/NES/TEST.NES
	mcopy -i '$@' '$(SDCARD_FCEUMM)' ::/sf2000/cores/sf2000-fceumm
	mcopy -i '$@' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$@' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf

run-linux-fceumm: qemu linux-full-asd $(FCEUMM_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 8; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(FCEUMM_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-fceumm.log \
		> '$(BUILD_DIR)'/logs/linux-fceumm.console 2>&1
	mcopy -o -i '$(FCEUMM_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-fceumm-loglinux.txt 2>/dev/null || true

smoke-linux-fceumm: run-linux-fceumm
	grep -q 'sf2000-browser: launch FCEUmm /mnt/sd/NES/TEST.NES' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -q 'sf2000-frontend: first frame 256x224' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -q 'sf2000-frontend: GE RGB565 stretch presenter ready .* buffers=2 fenced_depth=2' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -q 'sf2000-frontend: pause framebuffer wrote bytes=153600 stride=640 presenter=GE' '$(BUILD_DIR)'/logs/linux-fceumm.log
	! grep -Eq 'GE (unavailable|present failed|framebuffer source allocation failed)|CPU presenter active|using CPU write' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -q 'sf2000-logd: RAM journal begin: FAT writes deferred' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -Eq 'sf2000-logd: RAM journal end: bytes=[1-9][0-9]* peak=[1-9][0-9]* dropped=0 metrics=[1-9][0-9]* metric_bytes=[1-9][0-9]*' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -q 'sf2000-logd: RAM journal drained after frontend exit' '$(BUILD_DIR)'/logs/linux-fceumm.log
	grep -Eq 'source=frontend-metric .*xrun=0 .*input_polls=[1-9][0-9]* .*input_max_latency_us=[0-9]+' \
		'$(BUILD_DIR)'/logs/linux-fceumm-loglinux.txt
	grep -q 'sf2000-frontend: returned cleanly' '$(BUILD_DIR)'/logs/linux-fceumm.log
	! grep -Eq 'reloc outside program|Kernel panic|frontend: fault' '$(BUILD_DIR)'/logs/linux-fceumm.log

SNES_TEST_SD := $(BUILD_DIR)/snes9x-test.sd.img
SNES_TEST_ROM ?= /root/host-frogdev/roms/SNES/Mega Man X (USA) (Rev 1).sfc

$(SNES_TEST_SD): FORCE Makefile $(SDCARD_CORE_STAMP) $(SDCARD_USER_CONFIG) \
		$(SDCARD_UI_FONT)
	@test -f '$(SNES_TEST_ROM)' || { \
		echo 'set SNES_TEST_ROM=/path/to/an.sfc' >&2; exit 2; }
	mkdir -p '$(dir $@)'
	truncate -s 64M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/SNES ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(SNES_TEST_ROM)' '::/SNES/MEGA MAN X.SFC'
	mcopy -i '$@' '$(SDCARD_USER_CONFIG)' ::/sf2000.conf
	mcopy -i '$@' '$(SDCARD_UI_FONT)' ::/sf2000/ui.ttf
	mcopy -i '$@' '$(SDCARD_SNES9X2005)' '::/sf2000/cores/sf2000-snes9x2005'
	mcopy -i '$@' '$(SDCARD_SNES9X2002)' '::/sf2000/cores/sf2000-snes9x2002'

run-linux-snes9x2005: qemu linux-full-asd $(SNES_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 1; printf 'sendkey x 100\n'; sleep 2; \
		printf 'sendkey x 100\n'; sleep 25; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(SNES_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-snes9x2005.log \
		> '$(BUILD_DIR)'/logs/linux-snes9x2005.console 2>&1

smoke-linux-snes9x2005: run-linux-snes9x2005
	grep -q 'sf2000-browser: launch Snes9x 2005 /mnt/sd/SNES/MEGA MAN X.SFC' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: first frame 256x224 pitch=1024 .*scanout_hash=485b4dc5' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: GE RGB565 stretch presenter ready .* buffers=2 fenced_depth=2' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: pause menu opened' '$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: pause UI prepared font=1 fb=320x240 stride=640' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -Eq 'sf2000-frontend: pause GE fence complete pending=[1-9][0-9]*' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: pause framebuffer wrote bytes=153600 stride=640' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: pause framebuffer wrote bytes=153600 stride=640 presenter=GE' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	! grep -Eq 'GE (unavailable|present failed|framebuffer source allocation failed)|CPU presenter active|using CPU write' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log
	grep -q 'sf2000-frontend: returned cleanly' '$(BUILD_DIR)'/logs/linux-snes9x2005.log
	! grep -Eq 'malloc-failed|Data bus error|reloc outside program|Kernel panic|frontend: fault' \
		'$(BUILD_DIR)'/logs/linux-snes9x2005.log

run-linux-snes9x2002: qemu linux-full-asd $(SNES_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 1; printf 'sendkey x 100\n'; sleep 2; \
		printf 'sendkey down 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 25; printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(SNES_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-snes9x2002.log \
		> '$(BUILD_DIR)'/logs/linux-snes9x2002.console 2>&1

smoke-linux-snes9x2002: run-linux-snes9x2002
	grep -q 'sf2000-browser: launch Snes9x 2002 /mnt/sd/SNES/MEGA MAN X.SFC' \
		'$(BUILD_DIR)'/logs/linux-snes9x2002.log
	grep -q 'sf2000-frontend: first frame 256x224 pitch=640 .*scanout_hash=485b4dc5' \
		'$(BUILD_DIR)'/logs/linux-snes9x2002.log
	grep -q 'sf2000-frontend: returned cleanly' '$(BUILD_DIR)'/logs/linux-snes9x2002.log
	! grep -Eq 'Data bus error|reloc outside program|Kernel panic|frontend: fault' \
		'$(BUILD_DIR)'/logs/linux-snes9x2002.log

QUICKNES_TEST_SD := $(BUILD_DIR)/quicknes-test.sd.img

$(QUICKNES_TEST_SD): FORCE Makefile $(SDCARD_CORE_STAMP)
	@test -f '$(FCEUMM_TEST_ROM)' || { \
		echo 'set FCEUMM_TEST_ROM=/path/to/a.nes' >&2; exit 2; }
	mkdir -p '$(dir $@)'
	truncate -s 64M '$@'
	mkfs.vfat -F 32 -n SFTEST '$@' >/dev/null
	mmd -i '$@' ::/QUICKNES ::/sf2000 ::/sf2000/cores
	mcopy -i '$@' '$(FCEUMM_TEST_ROM)' '::/QUICKNES/SUPER MARIO BROS 3.NES'
	mcopy -i '$@' '$(SDCARD_QUICKNES)' ::/sf2000/cores/sf2000-quicknes

run-linux-quicknes: qemu linux-full-asd $(QUICKNES_TEST_SD)
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 5; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 1; printf 'sendkey x 100\n'; sleep 1; \
		printf 'sendkey x 100\n'; sleep 8; \
		printf 'sendkey backspace-ret 500\n'; sleep 1; \
		printf 'sendkey up 100\n'; sleep 1; printf 'sendkey x 100\n'; \
		sleep 3; printf 'quit\n') | \
			SF2000_SCANOUT_ORACLE=1 '$(QEMU_BIN)' -M sf2000 $(QEMU_CPU_ARGS) \
		-kernel '$(BUILD_DIR)'/sf2000-linux-full.asd \
		-drive if=none,id=sd0,file='$(QUICKNES_TEST_SD)',format=raw \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-quicknes.log \
		> '$(BUILD_DIR)'/logs/linux-quicknes.console 2>&1
	mcopy -o -i '$(QUICKNES_TEST_SD)' ::/loglinux.txt \
		'$(BUILD_DIR)'/logs/linux-quicknes-loglinux.txt 2>/dev/null || true

smoke-linux-quicknes: run-linux-quicknes
	grep -Fq 'sf2000-browser: launch QuickNES /mnt/sd/QUICKNES/SUPER MARIO BROS 3.NES' \
		'$(BUILD_DIR)'/logs/linux-quicknes.log
	grep -q 'sf2000-frontend: first frame 256x224' \
		'$(BUILD_DIR)'/logs/linux-quicknes.log
	grep -q 'sf2000-frontend: returned cleanly' \
		'$(BUILD_DIR)'/logs/linux-quicknes.log
	! grep -Eq 'reloc outside program|Kernel panic|frontend: fault|Data bus error' \
		'$(BUILD_DIR)'/logs/linux-quicknes.log
.NOTPARALLEL:
