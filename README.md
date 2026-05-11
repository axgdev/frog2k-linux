# SF2000 Linux Bring-Up

<!-- SPDX-License-Identifier: MIT -->

This project is the Linux bring-up workspace for the SF2000, GB300, DY12, and
related HC15xx/SF2000-family handhelds. The first hardware target is SF2000.

The goal is a small, fast iteration loop:

1. build the minimum Linux kernel and initramfs needed for a serial console;
2. boot it in the local `external/sf2000_qemu` submodule;
3. add storage, framebuffer, input, and audio only after the kernel is alive.

Generated files stay under `build/`. Large vendor trees are referenced through
local links instead of copied into this repository.

## Local References

- `external/sf2000_qemu`: local submodule used for emulator bring-up.
- `external/hclinux/2024.02.y.2`: symlink to the Hichip Linux/Buildroot SDK.
- `/root/host-frogdev/universal/sf2000_hcrtos`: HC15xx HCRTOS/AVP board data.
- `/root/host-frogdev/universal/unifrog`: working FreeRTOS-derived open stack.

## Current Strategy

The HCLinux tree has useful Linux support for HC16xx, but not a ready HC15xx
Linux board port. We use it as driver and platform reference while keeping the
initial SF2000 target small:

- MIPS little-endian, using the existing SF2000 toolchain under `/opt/`.
- UART console first.
- Initramfs first; SD rootfs later.
- No desktop stack, no package manager, no X11/Wayland during bring-up.

Buildroot is the preferred userspace generator for phase 1 because it can make
tiny reproducible soft-float root filesystems. Alpine/postmarketOS can be
revisited after the kernel ABI and core devices are proven.

## Commands

```sh
make qemu
make rootfs
make status
```

`make linux` and `make run` will become the main loop once the kernel source and
SF2000 Linux loader path are in place.
