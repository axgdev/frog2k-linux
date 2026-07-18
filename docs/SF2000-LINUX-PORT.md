# SF2000 Linux port: hardware contract and bring-up notes

<!-- SPDX-License-Identifier: MIT -->

This document records the current SF2000 contract, the conclusions supported
by physical testing, and the main false leads encountered during bring-up. It
is intended to prevent a future cleanup, QEMU change, or frontend port from
reintroducing assumptions that already failed on hardware.

## Target and constraints

The first target is the Data Frog SF2000, an HC15xx-family system:

- one little-endian MIPS32r1 CPU;
- 128 MiB RAM;
- no MMU and no FPU;
- ST7789-compatible 320x240 panel, physically rotated in the handheld;
- HC15 graphics engine (GE), graphics memory adapter (GMA), and video output
  unit (VOU);
- HC15 SD/MMC controller with a byte-register command interface and DMA;
- HC15 SND0 audio/DAC block;
- keypad GPIOs and two MUSB-compatible controller instances.

HC16xx HCLinux is useful as a structural reference, but its clock gates,
register layouts, and display assumptions are not authoritative for SF2000.
The original firmware and MuFrog/Unifrog are the physical HC15xx oracles.

Linux is built without an MMU and executes static MIPS bFLT binaries. “Normal
Linux programs” therefore means source-portable programs that do not require:

- `fork()` with copy-on-write semantics;
- ELF dynamic linking, `dlopen()`, or shared objects;
- an FPU;
- virtual-memory features such as arbitrary file-backed mappings;
- more RAM or CPU than the handheld can provide.

Programs using `vfork()`/`exec()`, threads supported by the selected uClibc
configuration, ordinary files, evdev, ALSA, and fbdev can be ported. Keep
executables small: on NOMMU each program is allocated and relocated as a whole,
so a tiny dedicated helper starts much faster than a large multi-call binary.

One late QEMU failure exposed a particularly subtle bFLT rule. Taking the
address of a large static BSS object in a non-PIC MIPS FLAT executable can
leave the link-time low address in generated code even though the process
itself entered through its KSEG0 alias. The GE context was therefore reported
as `0x00027768` instead of an address in the process mapping and faulted on its
first initialization store. The small GE context is therefore kept in
`main()`'s persistent stack frame; the minimal NOMMU process deliberately has
no usable general-purpose heap. Retained address markers make this failure
visible. The valid QEMU address is in the process KSEG0 range. This was not a
GE register or `memset()` problem.

## Boot artifacts and loader handoff

`make ROOTFS=buildroot sdcard-linux` stages:

- `build/sf2000-linux-buildroot.asd`, the direct build artifact;
- `build/sdcard/bios/bisrv.asd`, the direct ROM boot copy;
- `build/sdcard/firmware/linux.asd`, the Unifrog menu copy;
- `build/sdcard/firmware/unifrog.bin`, the raw fastboot loader.

The Makefile makes the ASD copies from the same prerequisite and writes
`build/sdcard/SHA256SUMS`. Comparing those hashes catches a stale staged image,
which caused several apparently contradictory physical tests during bring-up.

The vendor loader does not provide a clean architectural reset. Linux must
survive warm state left in caches, CP0, interrupt registers, display RAM, and
peripheral clocks. The loader therefore:

1. records progress into a retained RAM journal;
2. performs the cache/ROM handoff required by this CPU;
3. arms a watchdog for early failures;
4. emits one physical health blink;
5. leaves the backlight dark until the display service takes ownership.

On the next quick power cycle, the retained journal is recovered into
`log.txt`. It is deliberately outside ordinary kernel and GMA working memory.
Kernel milestones are nonblocking: retained RAM and UART carry the detail
without adding backlight delays.

## NOMMU MIPS bring-up

The CPU identifies closely enough to the MIPS 24K family for the generic core
code to be useful, but the SF2000 execution environment is not a conventional
24K MMU system. Bring-up required:

- avoiding unsafe EBase write-gate and watch-register probes;
- adopting the ROM-locked exception-vector state;
- constraining or removing early R4K TLB operations that trap on this target;
- entering the first bFLT process through the physical/KSEG handoff used by
  the boot environment;
- completing MIPS bFLT HI16/LO16 and R32 relocations correctly;
- restoring interrupts in the NOMMU syscall path;
- rearming a CP0 Compare value that was already pending at the first interrupt
  enable;
- flushing warm-boot kernel aliases before transferring control.

These changes are represented as ordered patches under
`patches/linux-5.12.4/`. Many retained markers are intentionally still present:
they are low-cost crash evidence on hardware without a dependable serial
console. Dead userspace storage and display experiments, however, should not be
kept in production control flow.

## Memory layout

The device tree reserves the areas that cannot be handed to the page allocator:

| Physical range | Purpose |
| --- | --- |
| `0x00400000..0x007fffff` | first-process identity/NOMMU execution window |
| `0x00f00000..0x00ffffff` | GMA descriptors and graphics DMA arena |
| `0x00f10000..0x00f357ff` | 320x240 RGB565 GMA scanout / `/dev/fb0` |
| `0x00fa8000..0x00fcffff` | RGB565 render surface used by the console |
| `0x01000000..0x03ffffff` | ROM/loader/retained diagnostic scratch |

The graphics arena is accessed through an uncached alias for DMA ownership.
Cache maintenance is still required whenever ownership changes between the CPU
and GE. The simple framebuffer validates mappings against the physical scanout
range while preserving the usable uncached NOMMU alias.

## Display pipeline

The display is not just an SPI-style ST7789 framebuffer. Four blocks cooperate:

1. the ST7789 MCU/8080 bus initializes registers and can write panel GRAM;
2. VOU produces the panel timing;
3. GMA scans an RGB565 bitmap through a descriptor;
4. GE fills, converts, composites, and copies graphics buffers.

The shared panel pins change ownership from the MCU GPIO bus to parallel RGB.
Register reads alone cannot prove that pixels are ready or that GMA has latched
a descriptor.

### Proven production sequence

The physically stable sequence is:

1. keep the backlight dark after the loader's single health blink;
2. have the display service take exclusive ownership and enable the backlight;
3. configure and reset the SF2000 ST7789 panel in the original command order;
4. render the selected console, solid color, or built-in logo;
5. push one complete 320x240 MCU GRAM transaction;
6. replace inherited panel GRAM with the controlled frame;
7. use GE to clear the exact future GMA scanout surface and copy the prepared
   render surface into it;
8. start and latch VOU while the panel pins remain MCU-owned;
9. build the inactive native 320x240 RGB565 GMA descriptor, ring its DMBA
   doorbell, and verify the hardware mirror;
10. repeat using the alternate 0x280-byte descriptor block, proving that the
   live fetcher—not a stale mirror—is responding;
11. program ST7789 `RAMCTRL`, `RGBCTRL`, `COLMOD`, address window, inversion,
    and display-on state, then switch the shared pads to RGB;
12. submit one native descriptor after the ownership switch and service four
    bounded L08 TE/RAMWR boundaries in userspace;
13. transfer TE ownership to the kernel synchronizer, which masks L08, performs
    the recovered GPIO `CASET`/`RASET`/`RAMWR` transaction, waits for TE to
    deassert, acknowledges the GPIO latch, and restores RGB once per frame;
14. keep the final native descriptor fixed and update only its RGB565 source
    surface with GE.

The two descriptor blocks are alternated only while proving the live GMA
fetcher during handoff. Subsequent console frames keep one already-latched
descriptor immutable and update its source pixels, avoiding redundant DMBA
transactions and any race with the panel ownership interrupt.

### Why the visible G1-G8 sequence appeared necessary

The diagnostic path performed three operations that were initially conflated:
panel GRAM initialization, GE operations, and descriptor variation. Physical
logs 52, 55, and 56 were sharp, while shortened paths produced moving static.
Tracing the closed firmware in QEMU showed the missing invariant: it performs a
direct GE clear over its eventual GMA scanout bitmap before the first GMA
doorbell. The E2 diagnostic had accidentally supplied that clear.

Production now performs this operation explicitly and invisibly. G1 is the
recovered native descriptor. G2-G8 remain only as an opt-in differential test;
production does not traverse them.

### Failed display assumptions

- A solid pink/purple screen was VPO/background output, not proof that the
  framebuffer pixels were correct.
- Moving “CRT static” was not uninitialized text memory. It indicated that the
  panel was sampling an invalid RGB stream or that scanout ownership had not
  been established.
- HC16 active-low clock-gate rules do not apply to the HC15 display gates.
- The vendor framebuffer alternates descriptors when layer state changes, but
  a fixed descriptor is correct for the console's fixed geometry and source.
  Re-ringing identical DMBA blocks wastes CPU and can race panel ownership.
- Continuous TE service is part of the vendor panel contract. The failed
  log77 experiment acknowledged GPIO status before the electrical pulse ended;
  its roughly 113 Hz rate was a retrigger bug, not evidence that TE service was
  unnecessary. The corrected threaded handler masks the child and acknowledges
  it only after L08 returns low.
- Descriptor readback alone is insufficient. The hardware `CTL_HW` and
  `DMBA_HW` mirrors must be observed after VOU is live.
- The recovered MuFrog panel tables carry an explicit zero command-count
  terminator. Physical log78 proved the original tight table walker, panel
  transaction, GE clear, MCU frame, and RGB handoff as one complete path.
  Its retained string addresses prove that the generated display binary came
  from `2f6195c`, not from the newer source recorded by the `78cb6a0` commit;
  the generated overlay had been stale.
- Logs 79 through 81 progressively moved the watchdog boundary as table
  validation, per-command retained writes, and post-command markers changed
  the display executable. Log81 completed the entire panel sequence,
  `CASET`/`RASET`/`RAMWR`, `DISPON`, formatting, and file logging, then reset
  before the first GE-render marker. That disproves the earlier conclusion
  that the command table itself was malformed.
- Log82 runs the newer generated binary and stops after `screen-panel-aux` in
  the same boot interval as an MMC delayed rescan. The only new shared-clock
  write is `screen-ge-clock-fast`: its userspace ioctl changes SYSIO `0x7c`
  bits 19:18 from selector 3 to selector 0. That register also contains the
  live SDIO selector in bits 21:20. The retained log proves the new clock write
  is the regression boundary; whether the physical failure is an adjacent-field
  read/modify/write collision or an unstable selector transition still requires
  a successful hardware run with that write absent.
- Log83 disproves a boot or GE-command failure: Linux remains alive beyond 53
  seconds, storage verifies 256 KiB, the console loop presents repeatedly, GE
  interrupts advance, and every VOU/GMA/pinmux value matches visible log78.
  The remaining difference was outside those readbacks. Disassembly of the
  closed HCRToS `vsync_irq()` shows it repeats the panel frame restart on every
  TE interrupt, whereas Linux stopped after four startup edges. This led to the
  corrected lifetime synchronizer above.
- Production leaves GE clock ownership with the kernel driver. It establishes
  selector 3 (238 MHz) before exposing `/dev/ge`; the display service no longer
  performs a slower, racy runtime retime. The long-lived HCGE context remains
  in `main()`'s persistent stack frame: QEMU records an unrelocated low address
  when the current MIPS bFLT toolchain places that object in static BSS.
- The screen executable is an unconditional packaging prerequisite. This keeps
  generated-overlay timestamps from reusing a display binary from another git
  revision, which is how the log78 artifact diverged from its recorded source.
- The loader and kernel stay dark after the single health blink. The display
  service turns the backlight on only after it owns R05 and the panel bus, so a
  failure is visible without allowing kernel progress helpers to interfere
  with display ownership.

### Configurable first frame

The Makefile adds these kernel command-line values:

- `SF2000_BOOT_VISUAL=console` (default);
- `SF2000_BOOT_VISUAL=color`;
- `SF2000_BOOT_VISUAL=logo`;
- `SF2000_BOOT_COLOR=<RGB565>`;
- `SF2000_BOOT_HOLD_MS=0..5000`.

The logo is compiled from drawing primitives and costs no filesystem access or
image decoder. A future frontend may replace it with an application-owned
asset after the display service has completed the same safe handoff.

## Graphics engine reconstruction

The clean implementation under `ge/` is designed to be shared with MuFrog
without linking the closed archive. Recovered and tested functionality includes:

- fill, blit, cropped stretch blit;
- RGB565, ARGB1555, XRGB8888, ARGB8888, and ARGB4444;
- horizontal/vertical flip and 90/180/270-degree rotation;
- alpha blending, color alpha, colorize, A8 masks;
- source/destination keys and all recovered custom key operators;
- premultiply, demultiply, XOR, and the complete command-node serializer;
- caller-owned contexts suitable for small NOMMU programs;
- reset, queue, IRQ, completion, and clock-selection ioctls.

Vendor-node byte comparisons run under qemu-user, and system QEMU executes the
supported operations functionally. Flags that the surviving vendor header marks
unsupported are rejected rather than serialized approximately. See
`ge/README.md` and `make reverse-ge`.

The console selects the measured 198 MHz GE profile, uses GE for full-surface
clears and every render-to-scanout copy, and redraws only the title/counter
region on idle ticks. CPU text rasterization is currently adequate for the
diagnostic console; a game/menu frontend should keep static UI in surfaces and
use GE blits/composition rather than repeatedly drawing glyph pixels.

## SD/MMC and storage safety

SF2000 does not use the HC16 DesignWare host that first appeared plausible. Its
HC15 controller uses byte-sized command/response registers plus separate DMA
address and length registers. The Linux driver follows the vendor sequence and:

- configures SF2000 SD pinmux and a one-bit bus;
- supports command response types used by SD initialization;
- uses 32-byte-aligned coherent/bounce DMA buffers;
- preserves an armed write DMA operation across command launch;
- waits for the real data-complete IRQ before releasing mappings;
- handles multi-block STOP completion through the vendor latch;
- flushes, reopens, reads, and checksums a 256 KiB persistence payload.

Releasing a write mapping on command completion rather than data completion was
the catastrophic corruption bug: the controller could still DMA after Linux
had reused the buffer, damaging unrelated FAT sectors and the superblock.

The production mount service is a small static bFLT at
`/usr/sbin/sf2000-mount`. It replaces a large BusyBox shell exec in the boot
hot path, retries the first two partitions and raw card, mounts VFAT with
`noatime`, and publishes `/run/sf2000-storage-mounted`. `sf2000-logd` buffers
early records in RAM, appends `loglinux.txt` after mount, and uses bounded
periodic flushes.

An abrupt watchdog reset can leave the FAT dirty even after all payload data
was flushed. This is distinct from the fixed DMA corruption: the card remains
readable and a filesystem check clears the dirty state. START+SELECT now asks
init to stop and flush the logger, `sync()`, unmount `/mnt/sd`, and invoke the
kernel restart path. The hardware watchdog remains a bounded fallback if that
orderly path cannot complete.

## Input, audio, and USB

- The built-in controls are exposed through evdev and drive the diagnostic
  console. Applications should consume evdev rather than private GPIO state.
- ALSA PCM playback is implemented for the SND0 coherent circular-DMA path at
  32 kHz, signed 16-bit mono. QEMU captures the same guest DMA stream as WAV.
- Both HC15 MUSB controllers enumerate as host controllers in Linux/QEMU, with
  vendor endpoint/FIFO layout, reset, PHY, ID override, session edge, and SYSINT
  sources represented.
- Physical tests have shown USB power but no connect event for keyboard/mouse.
  The board connector/routing may expose power or a bit-banged path without
  wiring a controller D+/D- pair. Software can prove controller state and lack
  of connect indication, but cannot create missing board traces. USB should not
  be a release blocker for the handheld’s built-in use case.

## QEMU as an oracle

`external/sf2000_qemu` must remain able to boot:

- the closed original firmware;
- the Linux ASD;
- existing Unifrog and MuFrog artifacts.

The useful model is contract-accurate rather than cycle-accurate. Tests cover:

- ASD/ROM boot and the SF2000 `4Km` fixed-mapping CPU model;
- system interrupt routing and the first CP0 timer event;
- HC15 SD enumeration, DMA read/write, raw-image persistence, and stock FAT
  writeback;
- ST7789 commands, TE aggregate cadence and frame-restart service, VOU latch,
  GMA descriptors, live scanout, and frame capture;
- functional GE command queues and effects;
- keypad redraws;
- SND0 guest DMA to WAV;
- MUSB platform registration;
- stock, GB300, Unifrog, and MuFrog display paths.

QEMU should warn on impossible ordering such as a GMA doorbell before VOU/panel
ownership is valid, but it must not silently “repair” guest state. Physical
markers remain authoritative for analog timing, cache effects, panel persistence,
and connector wiring.

## Performance findings

The latest physical profile is mostly idle once boot has completed. The large
system-time total is accumulated during early kernel/userspace bring-up rather
than by the steady console. The main practical rules are:

- avoid large binaries and unnecessary execs on NOMMU;
- never busy-spin for display timing;
- use GE for bulk pixels, scaling, conversion, and composition;
- keep CPU/GE cache ownership explicit;
- use SD DMA and batch/fsync writes rather than tiny synchronous records;
- disable detailed two-second profiling for a shipping game frontend;
- reuse long-lived processes and preallocated surfaces.

The log-78 timings also show where optimization is worthwhile: panel bring-up
and the GE/VOU handoff complete early, while starting a large shell-based
storage path dominated later boot time. Replacing it with the small
`sf2000-mount` bFLT removes that avoidable exec/relocation cost. Further boot
work should be measured from retained ticks or `loglinux.txt`, not inferred
from the visual counter, because QEMU wall time and the CP0 guest clock are not
the same quantity.

The device tree records the observed 918 MHz CPU clock and the CP0 counter runs
at the expected half-rate. Runtime frequency switching should not be added as a
raw register write: a proper cpufreq/clocksource integration must update delay
calibration and timer accounting across transitions. GE clock selection is
independent and is already set to the measured fast profile.

The SoC also contains video decode and additional audio blocks, but they do not
yet have clean Linux drivers. They are future acceleration work, not part of
the proven framebuffer/ALSA contract.

## Current support and next application target

Proven now:

- stable NOMMU kernel and multi-process bFLT userspace;
- accelerated 320x240 RGB565 display and standard `/dev/fb0`;
- retained crash logs and persistent `loglinux.txt`;
- built-in evdev input;
- ALSA SND0 DMA playback;
- SD read/write with persistence and corruption regression tests;
- QEMU coverage for Linux, original firmware, Unifrog, and MuFrog.

Still needed for a complete retro-gaming distribution:

- a small launcher/menu using fbdev + the source HCGE API + evdev + ALSA;
- emulator or libretro-core ports built static/soft-float and audited for
  NOMMU assumptions;
- audio mixing/resampling appropriate to core output;
- a user-facing shutdown/poweroff policy in addition to the implemented clean
  START+SELECT restart;
- packaging/save-state policy that limits SD write amplification;
- optional drivers for hardware video decode and any additional audio engines.

A useful next visual application is a tiny launcher rather than a desktop
toolkit: one process, predecoded RGB565 assets, GE-composited selection cards,
evdev navigation, and an ALSA click/tone. It exercises the interfaces a real
retro frontend needs without spending the CPU and memory budget on X11,
Wayland, SDL software scaling, or dynamic linking.
