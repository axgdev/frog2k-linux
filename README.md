# SF2000 Linux Bring-Up

<!-- SPDX-License-Identifier: MIT -->

This project is the Linux bring-up workspace for the SF2000, GB300, DY12, and
related HC15xx/SF2000-family handhelds. The first hardware target is SF2000.

The goal is a small, fast iteration loop:

1. build the SF2000 NOMMU kernel and soft-float Buildroot userspace;
2. boot the same ASD in the local `external/sf2000_qemu` board model;
3. verify the physical display, SD, input, audio, and retained-log contracts.

Generated files stay under `build/`. Large vendor trees are referenced through
local links instead of copied into this repository.
`sdcard-linux` also writes `build/sdcard/SHA256SUMS`; verify the files again
after copying them to removable media when a staged and direct ASD appear to
behave differently.

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

The detailed hardware contract, bring-up history, failed assumptions, current
support matrix, and application-porting constraints are documented in
[`docs/SF2000-LINUX-PORT.md`](docs/SF2000-LINUX-PORT.md).

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
make ROOTFS=buildroot smoke-linux-buildroot-persistent-storage
make ROOTFS=buildroot smoke-linux-buildroot-display
make ROOTFS=buildroot smoke-linux-buildroot-boot-logo
make ROOTFS=buildroot smoke-linux-buildroot-fb-test
make smoke-linux-buildroot-audio
make smoke-qemu-unifrog
make smoke-qemu-mufrog
make smoke-qemu-unifrog-display
make smoke-qemu-mufrog-display
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

Normal Buildroot boots keep the detected VFAT card mounted at `/mnt/sd` and
run a 256 KiB write, `fsync`, reopen, read, and checksum test. The
`sf2000-logd` service appends `/loglinux.txt` on that card. Every record carries
the 100 Hz monotonic tick, monotonic microseconds, process elapsed ticks, and
logger user/system CPU ticks. It drains `/dev/kmsg`, records input events and
USB/input topology changes, and samples CPU, memory, interrupt, uptime, load,
and VM counters from `/proc` every two seconds. A 512 KiB RAM buffer captures
the pre-mount boot and is committed once VFAT is ready; later data is committed
after 8 KiB or two seconds, whichever occurs first. The integrity payload is retained as
`/sf2000-storage-test.bin` so persistence can also be checked after shutdown.

`smoke-linux-buildroot-asd` now covers the normal multi-exec init path through
the screen service's ready marker and rejects data-bus faults.
`smoke-linux-buildroot-display` additionally requires a mode-6 RGB565 GMA
scanout and a captured 320x240 framebuffer. The kernel reserves that scanout
as a 320x240 RGB565 simple framebuffer and exposes it as `/dev/fb0`; the smoke
requires device registration and a successful userspace write through the
standard fbdev interface. The screen service still programs the vendor GMA
descriptors and panel registers directly. The MIPS NOMMU mapping path preserves
the uncached KSEG1 alias while validating it against the framebuffer physical
address, so ordinary applications can use fbdev `mmap` as well as `read`,
`write`, and ioctls.

The Buildroot image includes the upstream `fb-test-app` 1.1.1 utilities. Once
the physical display contract has been validated, normal boots retain the
working console instead of stopping it for diagnostic test screens. Append
`SF2000_FB_TEST=1` to the kernel command line to run `fb-test` automatically.
The broad MCU/GE and G1-G8 cards are available with `SF2000_GE_DIAG=1`, but
normal boot does not display or depend on them.

The missing production operation was found by tracing the closed firmware:
before its first GMA doorbell it clears the exact future scanout bitmap through
GE. The visible E2 diagnostic had accidentally done that in every sharp test
run. Production now performs that GE clear and render-to-scanout copy while the
ST7789 remains MCU-owned, starts VOU, alternates the two immutable 0x280-byte
descriptor blocks used by the vendor framebuffer driver, verifies both hardware
latches, transfers the panel to RGB mode, and completes four bounded userspace
TE/RAMWR boundaries. Steady RGB scanout then needs no panel interrupt.

The level-triggered kernel TE experiment was removed after physical log77
showed roughly 113 interrupts per second and recreated moving static by
repeatedly reclaiming the shared LCD bus. The display service sleeps through
all ordinary delays and redraw polling; it no longer consumes a core in a
diagnostic busy loop. GE performs full-screen clears and every presentation,
while idle console ticks redraw only the small changing regions.

The loader emits one health blink and then leaves the backlight visible. This
deliberately favors an inherited or dirty panel frame over an apparently dead
device during early bring-up. The display service then takes ownership, blanks
the backlight while it resets the panel and transfers a complete controlled
frame, and enables it again only after that frame is committed:

```sh
# Immediate console (default)
make ROOTFS=buildroot SF2000_BOOT_VISUAL=console sdcard-linux

# Built-in logo, held for 750 ms
make ROOTFS=buildroot SF2000_BOOT_VISUAL=logo sdcard-linux

# RGB565 solid color, held for 1200 ms
make ROOTFS=buildroot SF2000_BOOT_VISUAL=color \
	SF2000_BOOT_COLOR=0x001f SF2000_BOOT_HOLD_MS=1200 sdcard-linux
```

`SF2000_BOOT_HOLD_MS` is limited to five seconds. The logo is generated directly
into the framebuffer and adds no image decoder or filesystem dependency.
Press START+SELECT to flush `loglinux.txt`, synchronize and unmount the card,
then restart through the kernel; the watchdog is retained only as a fallback.

When explicitly enabled, init disarms the display watchdog, terminates the
console by its supervised PID, and waits for kernel-owned GE cleanup without
disturbing the live GMA descriptor. It then executes `fb-test -p 0` directly. No
intermediate shell, `killall`, or timeout process is involved in the handoff.
The expected final screen is a sharp test card with a green top edge, yellow
bottom edge, blue left field, red right field, RGB labels, and diagonals. The
`smoke-linux-buildroot-fb-test` alias runs the full display smoke and checks
representative pixels in QEMU's continuously refreshed scanout. This covers a
real independently maintained Linux bFLT program, signal/child handling, all
eight o32 syscall argument slots, fbdev ioctls, framebuffer `mmap`, and CPU
writes becoming visible through the same GMA scanout used by the device. Add
`SF2000_FB_TEST=1` explicitly because the production default leaves the
accelerated console running.

`smoke-linux-buildroot-audio` opens the in-kernel SF2000 ALSA PCM device from
NOMMU userspace, streams a 32 kHz S16 mono test signal through a coherent SND0
DMA ring, and requires QEMU's WAV backend to receive the guest samples.  The
same driver and device-tree node are used by the physical image.

The two HC15xx MUSB instances use the vendor endpoint layout (seven endpoints,
4 KiB FIFO RAM) and run as PIO host controllers. Their glue reproduces the
SF2000 vendor reset, shared-PHY trim, UTMI power, host-mode, and session-edge
sequence. Reset and PHY power run during platform initialization, while the ID
override and session edge run from the MUSB `set_mode` callback after core has
finished clearing `POWER` and `DEVCTL`. HC15xx uses opposite ID-override
polarity on its two ports: host mode sets USB0 UTMI bit 7 and clears USB1 UTMI
bit 6, matching the vendor `USB_DR_MODE_HOST` branch and the SF2000 USB-A
routing to USB1. The normal Buildroot smoke
requires both USB buses to register. USB1 uses SYSINT sources 51 and 50,
matching the proven SF2000 firmware contract.
`sf2000-logd` dynamically opens `/dev/input/event0` through `event15`, so a
physical USB keyboard or mouse produces tick-stamped `source=input` records in
`/loglinux.txt` without taking exclusive ownership away from applications.
The on-screen console also refreshes all sixteen event nodes after hotplug,
reports each input device name, and displays key presses.

`smoke-qemu-unifrog` and `smoke-qemu-mufrog` consume the existing, read-only
frontend build artifacts, construct disposable FAT images under `build/`, and
follow their persistent boot-trace ABI through module initialization, storage
completion, JavaScript/frontend startup, and boot-logo presentation.  Override
`UNIFROG_DIR` or `MUFROG_DIR` when those sibling checkouts live elsewhere.
The MuFrog fixture adds an empty `ROMS` directory because MuFrog intentionally
rejects package-only update media; this represents a blank user card while
still requiring a successful VFAT mount and storage-readiness result.
The corresponding `*-display` targets capture QEMU's 320x240 panel after
frontend startup and reject an all-black pixel payload, covering continuous
GMA scanout of the framebuffer after its descriptor has been installed. They
then inject a D-pad down event, capture a second frame, and require the pixels
to change, covering the keypad-to-frontend redraw path as well.

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

Normal boots now use one loader health blink. Kernel and userspace milestones
continue to be recorded in the retained RAM journal and UART without delaying
boot or exposing stale panel memory. Warm recovery writes the newest 256
retained entries to `log.txt`; older entries are counted in a
`skipped-oldest` line instead of delaying boot with hundreds of sector writes.
