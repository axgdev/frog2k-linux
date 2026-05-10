# SPDX-License-Identifier: MIT

QEMU_DIR := external/sf2000_qemu
HCLINUX_DIR := external/hclinux/2024.02.y.2
BUILD_DIR := build
ROOTFS_DIR := $(BUILD_DIR)/rootfs
INITRAMFS := $(BUILD_DIR)/initramfs.cpio
QEMU_BIN := $(QEMU_DIR)/.cache/qemu-10.2.2/build/qemu-system-mipsel

.PHONY: all status qemu rootfs clean

all: status

status:
	@printf 'sf2000_linux workspace\n'
	@printf '  qemu:    %s\n' '$(QEMU_DIR)'
	@printf '  hclinux: %s\n' '$(HCLINUX_DIR)'
	@printf '  qemu bin exists: '
	@test -x '$(QEMU_BIN)' && printf 'yes\n' || printf 'no\n'
	@printf '  initramfs exists: '
	@test -f '$(INITRAMFS)' && printf 'yes\n' || printf 'no\n'

qemu:
	$(MAKE) -C '$(QEMU_DIR)' build

rootfs: $(INITRAMFS)

$(INITRAMFS): scripts/mkinitramfs.sh
	sh scripts/mkinitramfs.sh '$(ROOTFS_DIR)' '$(INITRAMFS)'

clean:
	rm -rf '$(BUILD_DIR)'
