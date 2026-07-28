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
first initialization store. Long-lived caller-owned objects that must be
passed by address are now kept in the persistent `main()` stack frame (or
allocated by a proven allocator), and retained address markers make this
failure visible. The valid QEMU address is in the process KSEG0 range. This was
not a GE register or `memset()` problem.

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
5. leaves the backlight dark until userspace owns a complete frame.

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

1. keep the backlight off;
2. configure and reset the SF2000 ST7789 panel in the original command order;
3. render the selected console, solid color, or built-in logo;
4. push one complete 320x240 MCU GRAM transaction;
5. turn on the backlight, so inherited panel RAM is never visible;
6. use GE to clear the exact future GMA scanout surface and copy the prepared
   render surface into it;
7. start and latch VOU while the panel pins remain MCU-owned;
8. build the inactive native 320x240 RGB565 GMA descriptor, ring its DMBA
   doorbell, and verify the hardware mirror;
9. repeat using the alternate 0x280-byte descriptor block, proving that the
   live fetcher—not a stale mirror—is responding;
10. program ST7789 `RAMCTRL`, `RGBCTRL`, `COLMOD`, address window, inversion,
    and display-on state, then switch the shared pads to RGB;
11. submit one native descriptor after the ownership switch and service four
    bounded L08 TE/RAMWR boundaries in userspace;
12. stop touching the MCU bus and leave VOU/GMA in continuous RGB mode.

Subsequent frames match the recovered vendor framebuffer behavior: GE copies
the completed render surface, then software alternates two immutable
0x280-byte descriptor blocks. The active block is never rewritten in place.
The kernel exposes three ownership-tracked managed GE surfaces: one remains
with the console and two are available to a foreground emulator. The libretro
host alternates those two source surfaces and waits only before reusing an
in-flight surface, allowing CPU emulation to overlap GE scaling while retaining
an explicit cache and lifetime boundary.

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
- A fixed descriptor is not the vendor framebuffer update contract. The
  original driver alternates descriptor blocks after completed frames.
- Continuous kernel TE service is unnecessary for steady RGB scanout. The
  level-triggered experiment fired at about 113 Hz and repeatedly reclaimed the
  shared MCU/RGB bus, reproducing static.
- Descriptor readback alone is insufficient. The hardware `CTL_HW` and
  `DMBA_HW` mirrors must be observed after VOU is live.
- The brief pre-Linux noise frame was old ST7789 GRAM exposed by the backlight,
  not a need for another GMA diagnostic. Keeping the backlight dark through the
  first complete MCU push removes it.

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
periodic flushes while the system is idle.

Before launching the browser, `sf2000-powerd` creates a performance request
and waits for `sf2000-logd` to acknowledge it. The logger first synchronizes
all earlier records, switches to a bounded 512 KiB RAM journal, and only then
acknowledges the request. It continues draining kernel and input messages and
records a low-rate heartbeat, but performs no FAT write or `fsync` until the
browser and selected emulator have exited. Frontend metrics are appended to a
tmpfs spool with a single write and imported into the journal at that
transition; they never enter printk or its synchronous 115200-baud console
during gameplay. The logger then records journal byte, peak, dropped-byte,
metric-record, and metric-byte counters and drains the journal. The supervisor
owns the request lifetime, so it is released even when a child exits with an
error.
This is necessary because the logger and ROM loader share one MMC channel:
the former two-second snapshot-plus-fsync cycle reproducibly intercepted
gpSP's fourth 1 MiB ROM cache read, and the same storage stalls produced
periodic ALSA underruns. Detailed two-second profiling and durable flushes
resume automatically afterward.

An abrupt watchdog reset can leave the FAT dirty even after all payload data
was flushed. This is distinct from the fixed DMA corruption: the card remains
readable and a filesystem check clears the dirty state. START+SELECT now asks
init to stop and flush the logger, `sync()`, unmount `/mnt/sd`, and invoke the
kernel restart path. The hardware watchdog remains a bounded fallback if that
orderly path cannot complete.

## Input, audio, and USB

- The built-in controls are exposed through evdev and drive the diagnostic
  console. Applications should consume evdev rather than private GPIO state.
- UART1 is SYSINT source 17. It must not be assigned directly to MIPS CPU IRQ
  3, because IRQ 3 is one of the parent cascade lines for the SoC interrupt
  controller; sharing those incompatible handlers caused intermittent
  `unexpected IRQ # 3` reports under console traffic.
- ALSA PCM playback is implemented for the SND0 coherent circular-DMA path at
  32 kHz, signed 16-bit mono. The vendor transfer routine establishes that
  SND0+0x5c takes the PCM period size in frames and SND0+0x04 bit 16 must be asserted only
  after the ring is primed; the I2S/DMA enable bits alone leave the amplifier
  emitting idle hiss without advancing the consumer. QEMU enforces this
  ordering and captures the same guest DMA stream as WAV.
- Core stereo is mixed and linearly resampled to that hardware format by
  `audio/hc15xx_resampler.c`, a fixed-point, allocation-free module shared with
  RTOS adapters. The frontend uses a circular staging queue and retains queued
  current audio across ALSA recovery. Playback now primes four 1024-frame
  periods, providing 128 ms of scheduling lead while retaining the HC15xx
  equal-cursor ring guard. Performance records use an append-only tmpfs spool
  and do not query ALSA or call printk synchronously: log130 proved that the
  old split observer stopped the sole emulation thread for 125-150 ms every
  300 frames, while log132 showed that even one combined printk record still
  incremented the ALSA xrun counter at almost every five-second report.
  The spool is imported after the supervised frontend exits, so these metrics
  survive core failures and remain in `loglinux.txt` without consuming UART
  or FAT latency during gameplay. A shared, OS-independent retained-ring
  writer also stores one compact `frontend-audio` record per interval directly
  in uncached RAM. Its upper 16 bits are the cumulative xrun count and its
  lower 16 bits are cumulative pacing resets; the next quick boot recovers
  these records even when the filesystem spool could not be drained.
  Log133 separated a remaining gpSP failure from the earlier logging stalls:
  Gambatte held 59.74 FPS with no xrun, while a demanding gpSP scene declined
  to 57.86 FPS and generated about 31.1k output frames per wall-clock second
  for a fixed 32 kHz sink. The 128 ms lead therefore drained predictably.
  Shared-path work now batches PCM writes by complete periods, samples core
  runtime once per second rather than issuing an extra clock syscall every
  frame, gives the foreground frontend highest nice priority, and blocks the
  supervisor after first-frame handoff instead of polling it every 10 ms.
  SELECT+R provides a reversible uncapped, audio-suppressed, no-frameskip
  measurement window. Its `mode=uncapped`, `fps_milli`, and `suppressed`
  records measure real device headroom without changing an emulator core.
  Log135 measured gpSP at 76.7--127.6 FPS across the two uncapped runs, with
  about 85--93 FPS in the sustained demanding section. The remaining normal
  mode budget is therefore dominated by shared host work rather than an
  absolute dynarec ceiling. Tightly packed RGB565 frames now use one
  whole-frame copy instead of 160 row copies, stereo resampling crosses the
  portable module boundary once per batch instead of once per sample, and a
  sampled `sampled_present_us` field separates GE presentation cost from core
  execution without placing timing syscalls on every frame.
  The same log exposed repeated Gambatte unaligned-instruction exceptions
  after switching cores. The frontend's 256 KiB NOMMU bump allocator never
  reclaimed a core's C++ allocations. It has been replaced by reclaimable
  anonymous mappings, covered by a host allocation stress test and a combined
  gpSP-exit-Gambatte-exit QEMU lifecycle gate.
  Log136 then showed that this lifecycle fix held, and isolated the remaining
  sound defect: gpSP accumulated four new xruns and hundreds of deadline
  rebases after returning from uncapped mode, while Gambatte accumulated no
  xruns in the same test. gpSP still sustained roughly 87--131 FPS uncapped,
  so this was depleted audio lead during intermittent heavy frames rather
  than an absolute compute ceiling. Playback now primes seven periods
  (224 ms), samples ALSA delay at a low rate, and applies a bounded 0.8%
  resampling correction only while rebuilding or draining the target lead.
  Metrics expose `delay` and `resample_hz`; the QEMU transition gate requires
  a positive normal-mode delay, nominal 32 kHz resampling, and zero xruns.
  Audio staging now scans and copies whole converted batches rather than
  performing circular-buffer arithmetic for every sample. The later
  three-source GE experiment was invalidated by physical log 137 and is
  documented under GE presentation depth below.
  Log138 validates the restored two-source contract across normal and uncapped
  Gambatte and gpSP runs. It also locates the next shared bottleneck: physical
  gpSP spends roughly 8--16 ms in sampled core work and 1.6--2.1 ms presenting
  a 240x160 frame. Paced RGB565 cores now expose their physically contiguous
  NOMMU KSEG0 framebuffer to GE for a hardware staging copy into a managed
  source. The frontend fences that copy before returning from the libretro
  callback, then submits asynchronous scaling from the snapshot. This removes
  4.4 MiB/s of CPU frame copies at 60 Hz without extending the callback
  buffer's lifetime. Log139 then validated this path at roughly 0.56--0.72 ms
  for gpSP versus the earlier 1.6--2.1 ms CPU-buffered path, with zero xruns.
  Hardware staging is consequently used in both paced and uncapped modes: the
  staging copy is fenced to honor the callback lifetime, while the following
  stretch remains asynchronous and overlaps the core. `ge_stage_frames` and
  `buffered_frames` metrics gate that contract in QEMU.
  That run also completed its first 300 Gambatte frames in 4.478 seconds but
  every later interval in about 5.02 seconds. The first expensive core frame
  had left the absolute deadline in the past, causing a startup catch-up burst
  and the initial 10 KiB discard. The host now rebases a missed deadline and
  reports `pacing_resets` rather than replaying stale work.
- Both HC15 MUSB controllers enumerate as host controllers in Linux/QEMU, with
  vendor endpoint/FIFO layout, reset, PHY, ID override, session edge, and SYSINT
  sources represented.
- Physical tests have shown USB power but no connect event for keyboard/mouse.
  The board connector/routing may expose power or a bit-banged path without
  wiring a controller D+/D- pair. Software can prove controller state and lack
  of connect indication, but cannot create missing board traces. USB should not
  be a release blocker for the handheld’s built-in use case.

## QEMU as an oracle

The separate `sf2000_qemu` checkout selected by `QEMU_DIR` must remain able to
boot:

- the closed original firmware;
- the Linux ASD;
- existing Unifrog and MuFrog artifacts.

The useful model is contract-accurate rather than cycle-accurate. Tests cover:

- ASD/ROM boot and the SF2000 `4Km` fixed-mapping CPU model;
- system interrupt routing and the first CP0 timer event;
- HC15 SD enumeration, DMA read/write, raw-image persistence, and stock FAT
  writeback;
- ST7789 commands, VOU latch, alternating GMA descriptors, live scanout, and
  frame capture;
- functional GE command queues and effects, including the physical asynchronous
  BUSY/DONE transition, completion interrupt, and rejection of a second
  doorbell while work is active;
- an opt-in scanout oracle which hashes changed frames and classifies their
  color diversity, active pixels, and GMA owner;
- keypad redraws;
- SND0 guest DMA to WAV;
- MUSB platform registration;
- stock, GB300, Unifrog, and MuFrog display paths.

QEMU should warn on impossible ordering such as a GMA doorbell before VOU/panel
ownership is valid, but it must not silently “repair” guest state. Physical
markers remain authoritative for analog timing, cache effects, panel persistence,
and connector wiring.

`smoke-linux-gpsp` enables the scanout oracle and runs a self-contained GBA
cartridge which selects mode 3 and fills VRAM with an RGB555 ramp.  The test
requires an asynchronous GE start/completion pair, no busy-doorbell violation,
and a diverse post-launch panel frame.  This detects the class of failure seen
in log 137, where emulation and audio continued normally behind a blank physical
scanout; the former all-zero test cartridge could only produce a solid frame
and could not make that distinction.

For the complete gpSP startup path, including BIOS-hook translation and a real
game's first frame, run:

```
make smoke-linux-gpsp-real GPSP_REAL_ROM=/path/to/a/legal/game.gba
```

The gpSP gates deliberately observe the platform boundary: browser launch,
frontend ROM-load begin/completion, the first hashed 240x160 frame, GE scanout,
and clean process return. They do not depend on progress callbacks inserted
inside a core. QEMU still rejects instruction/data bus errors, frontend
signals, bFLT relocation failures, GE queue misuse, and kernel panics, so a
core can be replaced or updated without weakening the regression oracle.

The ROM is copied only into an ignored temporary FAT image. It is never added
to an artifact or repository. The gate requires both JIT BIOS-hook markers,
successful ROM loading, a 240x160 first frame, and a clean frontend return; it
also rejects MIPS bus faults and flat-loader relocation failures.

gpSP's MIPS translator has an important NOMMU build invariant. GCC switch jump
tables contain absolute targets which cannot safely follow the translator
through the bFLT layout/relocation path. `cpu_threaded.c` is therefore compiled
with jump-table lowering disabled, and its build checks that
`translate_block_arm` contains no computed switch jump. The translator's
non-reentrant scratch area lives after the executable mmap-backed JIT caches:
this avoids both bFLT BSS relocations and the recursive 8 KiB stack allocation
which previously made startup depend on incidental binary layout.

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

The bootloader normally leaves HC1512 on selector 0, the 594 MHz profile, whose
CP0 counter runs at 297 MHz. Linux originally described that physical boot rate
correctly, but 594 MHz does not leave enough CPU headroom for the heavier
emulators. The ASD loader now applies the vendor-validated selector-7 digital
PLL sequence for 918 MHz before entering the kernel, and the device tree
describes the resulting stable 459 MHz CP0 half-rate. Doing this before Linux
timekeeping starts avoids a discontinuity in `CLOCK_MONOTONIC`, delay
calibration, and clock events. Future runtime frequency switching still needs a
proper cpufreq/clocksource integration; it must not be implemented as an
uncoordinated userspace register write. GE clock selection is independent and
is already set to the measured fast profile.

The SoC also contains video decode and additional audio blocks, but they do not
yet have clean Linux drivers. They are future acceleration work, not part of
the proven framebuffer/ALSA contract.

### GE presentation depth

The physical HC15xx queue has a single running segment and one software-held
continuation segment.  Linux therefore exposes three managed GE allocations:
one long-lived console render surface and two frontend source surfaces.  The
frontend fences before either source surface is reused.  Increasing this to
three frontend surfaces did not increase measured throughput: log 137 proves
that gpSP kept executing thousands of frames while physical scanout remained
blank.  The same build looked healthy in QEMU because its GE doorbell completed
synchronously.  Two source surfaces are the hardware-safe ownership contract,
not a memory-saving fallback.

The first frontend frame now records both the libretro source hash and the
post-GE framebuffer hash in kmsg and retained RAM.  A non-changing or solid
scanout can therefore be separated from a stopped core even when the
performance journal has not yet been copied to FAT.

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

- expand the integrated launcher beyond the working file browser;
- add further emulator or libretro-core ports built static/soft-float and
  audited for NOMMU assumptions (Gambatte and dynarec gpSP are working);
- a user-facing shutdown/poweroff policy in addition to the implemented clean
  START+SELECT restart;
- packaging/save-state policy that limits SD write amplification;
- optional drivers for hardware video decode and any additional audio engines.

A useful next visual application is a tiny launcher rather than a desktop
toolkit: one process, predecoded RGB565 assets, GE-composited selection cards,
evdev navigation, and an ALSA click/tone. It exercises the interfaces a real
retro frontend needs without spending the CPU and memory budget on X11,
Wayland, SDL software scaling, or dynamic linking.
