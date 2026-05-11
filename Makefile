# SPDX-License-Identifier: MIT

QEMU_DIR := external/sf2000_qemu
HCLINUX_DIR := external/hclinux/2024.02.y.2
BUILD_DIR := build
INITRAMFS := $(BUILD_DIR)/initramfs.cpio
INITRAMFS_LIST := $(BUILD_DIR)/initramfs.list
INIT_BIN := $(BUILD_DIR)/initramfs-init
GEN_INIT_CPIO := $(BUILD_DIR)/gen_init_cpio
ASDPACK := $(BUILD_DIR)/asdpack
QEMU_BIN := $(QEMU_DIR)/.cache/qemu-10.2.2/build/qemu-system-mipsel
QEMU_MKSD := $(QEMU_DIR)/build/mksf2000sd
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
LINUX_SRC := $(BUILD_DIR)/linux-$(LINUX_VERSION)
BUILDROOT_VERSION := 2024.02.12
BUILDROOT_TARBALL := buildroot-$(BUILDROOT_VERSION).tar.xz
BUILDROOT_URL := https://buildroot.org/downloads/$(BUILDROOT_TARBALL)
BUILDROOT_SRC := $(BUILD_DIR)/buildroot-$(BUILDROOT_VERSION)
BUILDROOT_WORK ?= /tmp/sf2000_linux-buildroot
BUILDROOT_OUT ?= $(BUILDROOT_WORK)/buildroot-sf2000
BUILDROOT_DEFCONFIG := buildroot/sf2000_defconfig
BUILDROOT_OVERLAY := buildroot/sf2000-rootfs-overlay
BUILDROOT_GENERATED_OVERLAY := $(BUILD_DIR)/buildroot-generated-overlay
BUILDROOT_GENERATED_OVERLAY_STAMP := $(BUILD_DIR)/.stamp-buildroot-generated-overlay
BUILDROOT_INIT_SRC := buildroot/sf2000-init.c
BUILDROOT_INIT := $(BUILDROOT_GENERATED_OVERLAY)/init
BUILDROOT_PAD_SRC := buildroot/sf2000-pad.c
BUILDROOT_PAD := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-pad
BUILDROOT_HEARTBEAT_SRC := buildroot/sf2000-heartbeat.c
BUILDROOT_HEARTBEAT := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-heartbeat
BUILDROOT_SCREEN_SRC := buildroot/sf2000-screen.c
BUILDROOT_SCREEN := $(BUILDROOT_GENERATED_OVERLAY)/usr/sbin/sf2000-screen
BUILDROOT_DEVICE_TABLE := buildroot/sf2000_device_table.txt
BUILDROOT_PATCHES := buildroot/patches
BUILDROOT_OVERLAY_FILES := $(shell find '$(BUILDROOT_OVERLAY)' -type f 2>/dev/null)
BUILDROOT_PATCH_FILES := $(shell find '$(BUILDROOT_PATCHES)' -type f 2>/dev/null)
BUILDROOT_CPIO := $(BUILD_DIR)/rootfs-buildroot.cpio
BUILDROOT_TOOLCHAIN_STAMP := $(BUILDROOT_OUT)/.stamp-toolchain
BUILDROOT_TARGET_STAMP := $(BUILDROOT_OUT)/.stamp-target
BUILDROOT_REPACK_DIR := $(BUILD_DIR)/buildroot-repack-root
BUILDROOT_DEVICE_CPIO_LIST := $(BUILD_DIR)/buildroot-device-nodes.list
BUILDROOT_CC := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-linux-musl-gcc
BUILDROOT_STRIP := $(BUILDROOT_OUT)/host/bin/mipsel-buildroot-linux-musl-strip
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
LINUX_ROM_SD_IMAGE := $(BUILD_DIR)/sf2000-linux$(ROOTFS_SUFFIX)-rom.sd.img
BOOTROM_BUGFIX ?= /root/host-frogdev/universal/orig_firmware/UpdateFirmware/SF2000_XMC_XM25QH40B_4mbit_bugfix.bin
QEMU_BOOT_TIMEOUT ?= 90s
SMOKE_INIT_PATTERN ?= sf2000_linux: initramfs alive
LOADER_CFLAGS := -Os -ffreestanding -fno-builtin -nostdlib \
	-march=mips32 -mabi=32 -msoft-float -mno-abicalls -fno-pic -G 0 \
	-Wall -Wextra

.PHONY: all status qemu rootfs buildroot linux linux-asd linux-buildroot \
	linux-buildroot-asd sdcard-linux sdcard-buildroot linux-rom-sd \
	linux-buildroot-rom-sd run-linux smoke-linux run-linux-asd \
	smoke-linux-asd run-linux-rom smoke-linux-rom run-linux-buildroot-asd \
	smoke-linux-buildroot-asd run-linux-buildroot-rom \
	smoke-linux-buildroot-rom run-linux-buildroot-display \
	smoke-linux-buildroot-display run-linux-input smoke-linux-input \
	run-linux-buildroot-input smoke-linux-buildroot-input clean

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

$(INIT_BIN): init/sf2000-init.c Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' -Os -static -nostdlib -ffreestanding \
		-march=mips32 -mabi=32 -msoft-float -G 0 \
		-Wl,-e,_start -Wl,--gc-sections -Wl,-z,noexecstack -o '$@' '$<'

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

$(BUILDROOT_OUT)/.config: $(BUILDROOT_SRC)/Makefile $(BUILDROOT_DEFCONFIG) $(BUILDROOT_DEVICE_TABLE) Makefile $(BUILDROOT_PATCH_FILES) | $(BUILDROOT_GENERATED_OVERLAY_STAMP)
	mkdir -p '$(BUILDROOT_OUT)'
	$(BUILDROOT_MAKE) BR2_DEFCONFIG='$(abspath $(BUILDROOT_DEFCONFIG))' defconfig
	'$(BUILDROOT_SRC)'/utils/config --file '$@' \
		--set-str GLOBAL_PATCH_DIR '$(abspath $(BUILDROOT_PATCHES))' \
		--set-str ROOTFS_OVERLAY '$(abspath $(BUILDROOT_OVERLAY)) $(abspath $(BUILDROOT_GENERATED_OVERLAY))' \
		--set-str ROOTFS_DEVICE_TABLE 'system/device_table.txt $(abspath $(BUILDROOT_DEVICE_TABLE))'
	$(BUILDROOT_MAKE) olddefconfig

$(BUILDROOT_TOOLCHAIN_STAMP): $(BUILDROOT_OUT)/.config
	FORCE_UNSAFE_CONFIGURE=1 $(BUILDROOT_MAKE) -j'$(JOBS)' toolchain \
		HOST_CFLAGS='$(BUILDROOT_HOST_CFLAGS)' \
		HOST_CXXFLAGS='$(BUILDROOT_HOST_CXXFLAGS)'
	touch '$@'

$(BUILDROOT_INIT): $(BUILDROOT_INIT_SRC) Makefile
	mkdir -p '$(dir $@)'
	'$(CC_MIPS)' -Os -static -nostdlib -ffreestanding -fno-builtin \
		-march=mips32 -mabi=32 -msoft-float -mno-abicalls \
		-fno-pic -G 0 -Wall -Wextra \
		-Wl,-e,_start -Wl,--gc-sections -Wl,-z,noexecstack -o '$@' '$<'
	'$(STRIP_MIPS)' '$@'

$(BUILDROOT_PAD): $(BUILDROOT_PAD_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' -Os -static -Wall -Wextra -o '$@' '$<'
	'$(BUILDROOT_STRIP)' '$@'

$(BUILDROOT_HEARTBEAT): $(BUILDROOT_HEARTBEAT_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' -Os -static -Wall -Wextra -o '$@' '$<'
	'$(BUILDROOT_STRIP)' '$@'

$(BUILDROOT_SCREEN): $(BUILDROOT_SCREEN_SRC) $(BUILDROOT_TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(dir $@)'
	'$(BUILDROOT_CC)' -Os -static -Wall -Wextra -o '$@' '$<'
	'$(BUILDROOT_STRIP)' '$@'

$(BUILDROOT_TARGET_STAMP): $(BUILDROOT_OUT)/.config $(BUILDROOT_TOOLCHAIN_STAMP) | $(BUILDROOT_GENERATED_OVERLAY_STAMP)
	FORCE_UNSAFE_CONFIGURE=1 $(BUILDROOT_MAKE) -j'$(JOBS)' \
		HOST_CFLAGS='$(BUILDROOT_HOST_CFLAGS)' \
		HOST_CXXFLAGS='$(BUILDROOT_HOST_CXXFLAGS)'
	touch '$@'

$(BUILDROOT_CPIO): $(BUILDROOT_TARGET_STAMP) $(BUILDROOT_INIT) $(BUILDROOT_PAD) $(BUILDROOT_HEARTBEAT) $(BUILDROOT_SCREEN) $(BUILDROOT_OVERLAY_FILES) $(BUILDROOT_DEVICE_TABLE) $(GEN_INIT_CPIO)
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
	@for patch in $(LINUX_PATCHES); do \
		if patch -d '$(LINUX_SRC)' --dry-run -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'applying %s\n' "$$patch"; \
			patch -d '$(LINUX_SRC)' -p1 < "$$patch"; \
		elif patch -d '$(LINUX_SRC)' --dry-run -R -p1 < "$$patch" >/dev/null 2>&1; then \
			printf 'already applied %s\n' "$$patch"; \
		else \
			printf 'cannot apply %s\n' "$$patch" >&2; \
			exit 1; \
		fi; \
	done
	touch '$@'

$(SF2000_DTB): linux/sf2000.dts
	mkdir -p '$(dir $@)'
	dtc -I dts -O dtb -o '$@' '$<'

$(LINUX_OUT)/.config: $(LINUX_SRC)/.patched Makefile
	mkdir -p '$(LINUX_OUT)'
	$(MAKE) -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' '$(LINUX_DEFCONFIG)'
	'$(LINUX_SRC)'/scripts/config --file '$@' \
		--enable BLK_DEV_INITRD \
		--set-str INITRAMFS_SOURCE '$(abspath $(ROOTFS_CPIO))' \
		--enable CMDLINE_BOOL \
		--set-str CMDLINE '$(LINUX_CMDLINE)' \
		--disable MODULES \
		--disable DEBUG_INFO \
		--disable DEBUG_INFO_REDUCED \
		--disable IKCONFIG \
		--disable IKCONFIG_PROC \
		--disable KALLSYMS \
		--disable KALLSYMS_ALL \
		--disable KALLSYMS_BASE_RELATIVE \
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
		--disable MIPS_CPS \
		--disable MIPS_CPS_PM \
		--disable MIPS_CM \
		--disable MIPS_CPC \
		--disable MIPS_CMP \
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
		--disable USB_SUPPORT \
		--disable USB \
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
		--disable MMC \
		--disable MMC_BLOCK \
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

$(LINUX_VMLINUX): $(LINUX_OUT)/.config $(ROOTFS_CPIO)
	$(MAKE) -j'$(JOBS)' -C '$(LINUX_SRC)' O='$(abspath $(LINUX_OUT))' \
		ARCH=mips CROSS_COMPILE='$(CROSS_COMPILE)' vmlinux

linux: $(LINUX_VMLINUX) $(SF2000_DTB)

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
		printf 'Visible stages use counted backlight-off pulses:\n'; \
		printf '  1 pulse: Linux loader entered\n'; \
		printf '  2 pulses: loader is jumping to the kernel\n'; \
		printf '  3 pulses: kernel entered MIPS setup_arch\n'; \
		printf '  4 pulses: kernel finished MIPS setup_arch\n'; \
		printf '  5 pulses: initramfs /init reached userspace\n'; \
	} > '$@'

$(LINUX_ROM_SD_IMAGE): $(LINUX_ASD) $(QEMU_MKSD)
	'$(QEMU_MKSD)' '$(LINUX_ASD)' '$@' fat32

linux-asd: $(LINUX_ASD)

linux-buildroot:
	$(MAKE) ROOTFS=buildroot linux

linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot linux-asd

sdcard-linux: $(SDCARD_LINUX_ASD) $(SDCARD_BIOS_ASD) \
		$(SDCARD_FASTBOOT_BIN) $(SDCARD_BOOT_OPTIONS)

sdcard-buildroot:
	$(MAKE) ROOTFS=buildroot sdcard-linux

linux-rom-sd: $(LINUX_ROM_SD_IMAGE)

linux-buildroot-rom-sd:
	$(MAKE) ROOTFS=buildroot linux-rom-sd

run-linux: qemu linux
	mkdir -p '$(BUILD_DIR)'/logs
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 -kernel '$(LINUX_VMLINUX)' \
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
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 -kernel '$(LINUX_ASD)' \
		-display none -serial none -monitor none \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-asd.log \
		> '$(BUILD_DIR)'/logs/linux-asd.console 2>&1 || test $$? -eq 124

smoke-linux-asd: run-linux-asd
	grep -q 'sf2000: loaded ASD' '$(BUILD_DIR)'/logs/linux-asd.console
	grep -q 'sf2000: uart: .*linux-loader: jump entry=' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q '$(SMOKE_INIT_PATTERN)' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-input: qemu linux-asd
	mkdir -p '$(BUILD_DIR)'/logs
	(sleep 45; printf 'sendkey right 1000\n'; sleep 2; \
		printf 'sendkey x 1000\n'; sleep 2; printf 'quit\n') | \
		'$(QEMU_BIN)' -M sf2000 -kernel '$(LINUX_ASD)' \
		-display none -serial none -monitor stdio \
		-d guest_errors,unimp -D '$(BUILD_DIR)'/logs/linux-input.log \
		> '$(BUILD_DIR)'/logs/linux-input.console 2>&1

smoke-linux-input: run-linux-input
	grep -q 'sf2000-pad: userspace input bridge ready' '$(BUILD_DIR)'/logs/linux-input.log
	grep -q 'sf2000-pad: state=.*RIGHT' '$(BUILD_DIR)'/logs/linux-input.log
	grep -q 'sf2000-pad: state=.*A' '$(BUILD_DIR)'/logs/linux-input.log

run-linux-rom: qemu linux-rom-sd
	test -f '$(BOOTROM_BUGFIX)'
	mkdir -p '$(BUILD_DIR)'/logs
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 -bios '$(BOOTROM_BUGFIX)' \
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
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' run-linux-asd

smoke-linux-buildroot-asd:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' smoke-linux-asd
	grep -q 'sf2000-heartbeat: backlight heartbeat ready' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000-screen: panel init done' '$(BUILD_DIR)'/logs/linux-asd.log
	grep -q 'sf2000-screen: gma console ready' '$(BUILD_DIR)'/logs/linux-asd.log

run-linux-buildroot-rom:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' run-linux-rom

smoke-linux-buildroot-rom:
	$(MAKE) ROOTFS=buildroot \
		SMOKE_INIT_PATTERN='sf2000_buildroot: userspace alive' smoke-linux-rom
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
	timeout '$(QEMU_BOOT_TIMEOUT)' '$(QEMU_BIN)' -M sf2000 -kernel '$(BUILD_DIR)'/sf2000-linux-buildroot.asd \
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

run-linux-buildroot-input:
	$(MAKE) ROOTFS=buildroot run-linux-input

smoke-linux-buildroot-input:
	$(MAKE) ROOTFS=buildroot smoke-linux-input

clean:
	rm -rf '$(BUILD_DIR)'
