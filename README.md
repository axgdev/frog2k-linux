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
make ROOTFS=buildroot sdcard-linux
make ROOTFS=buildroot smoke-linux-buildroot-asd
make ROOTFS=buildroot smoke-linux-buildroot-rom
make smoke-qemu-board-contract
make smoke-qemu-display
make ROOTFS=buildroot smoke-linux-buildroot-storage
make ROOTFS=buildroot smoke-linux-buildroot-storage-enumeration
make ROOTFS=buildroot smoke-linux-buildroot-storage-probe-writeback
make ROOTFS=buildroot smoke-linux-buildroot-display
make smoke-linux-buildroot-audio
make smoke-qemu-unifrog
make smoke-qemu-mufrog
make status
```

`smoke-linux-buildroot-storage` now delegates to the sibling
`external/sf2000_qemu` raw-image DMA writeback smoke, so the default storage
regression path uses the stronger emulator-side oracle instead of the brittle
direct guest probe.

`smoke-linux-buildroot-storage-enumeration` exercises the in-tree QEMU model
and Linux HC15 host together. It checks SCR DMA, SD card registration,
`mmcblk0`, and the first 4 KiB block read.

`smoke-linux-buildroot-storage-probe-writeback` boots the minimal NOMMU
storage helper, writes through `mmcblk0`, flushes and reads the data back, and
then verifies the same signature in QEMU's SD image. Linux QEMU runs default
to the MIPS32r1 `4Km` fixed-mapping CPU model; `4Kc` models an R4000-style TLB
and is not an appropriate stand-in for the SF2000's MMU-less CPU.

`smoke-linux-buildroot-asd` now covers the normal multi-exec init path through
the screen service's ready marker and rejects data-bus faults.
`smoke-linux-buildroot-display` additionally requires a mode-6 RGB565 GMA
scanout and a captured 320x240 framebuffer.

`smoke-linux-buildroot-audio` opens the in-kernel SF2000 ALSA PCM device from
NOMMU userspace, streams a 32 kHz S16 mono test signal through a coherent SND0
DMA ring, and requires QEMU's WAV backend to receive the guest samples.  The
same driver and device-tree node are used by the physical image.

`smoke-qemu-unifrog` and `smoke-qemu-mufrog` consume the existing, read-only
frontend build artifacts, construct disposable FAT images under `build/`, and
follow their persistent boot-trace ABI through module initialization, storage
completion, JavaScript/frontend startup, and boot-logo presentation.  Override
`UNIFROG_DIR` or `MUFROG_DIR` when those sibling checkouts live elsewhere.
The MuFrog fixture adds an empty `ROMS` directory because MuFrog intentionally
rejects package-only update media; this represents a blank user card while
still requiring a successful VFAT mount and storage-readiness result.

`smoke-qemu-board-contract` runs the local `external/sf2000_qemu`
board-contract smoke, which keeps the board-profile, display, audio, USB, and
storage snapshots queryable from the Linux workspace as well.

`smoke-qemu-display` runs the local `external/sf2000_qemu` stock-display and
GB300-display smokes, giving the Linux workspace a direct
way to exercise the stronger display oracle without replaying the guest-side
panel boot chain.

`sdcard-linux` writes three intentionally different boot artifacts under
`build/sdcard`:

- `bios/bisrv.asd`: direct ROM boot. Copy this over the SD card
  `/bios/bisrv.asd` when bypassing fastboot/Unifrog.
- `firmware/unifrog.bin`: existing fastboot auto-load path. This is the raw
  loader binary, not an ASD image.
- `firmware/linux.asd`: Unifrog menu handoff path. Boot Unifrog first and
  select `linux.asd`.
- `log.txt`: fixed-size early boot log. Keep this file in the SD root if you
  want kernel stage names written back to the card.

Hardware diagnostics use counted backlight-off pulse groups, mirrored on the
L25 status LED when that LED is present:

- 1 pulse: Linux loader entered.
- 2 pulses: loader is jumping to the kernel.
- 3 pulses: kernel entered MIPS `setup_arch`.
- 4 pulses: kernel finished MIPS `setup_arch`.
- 5 pulses: `start_kernel` resumed after `setup_arch`.
- 6 pulses: `mm_init` completed.
- 7 pulses: `time_init` completed.
- 8 pulses: the kernel is about to enable IRQs.
- 9 pulses: the kernel is about to exec `/init`.
- 10 pulses: initramfs `/init` reached userspace.

If the pulse groups stop, the next stage is where the boot is hanging.
