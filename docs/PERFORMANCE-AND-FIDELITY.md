# Performance and fidelity measurements

The current physically verified reference is the log92/loglinux0027 run.  The
older log90/loglinux0025 run remains the pre-optimization comparison point.
Performance changes must preserve the display and storage contract; a faster
boot with a damaged raster or filesystem is a regression.

## Reproducible commands

`make metrics-linux` reads `METRICS_LOG` (loglinux0025 by default) and reports
screen startup, panel initialization, the 256 KiB write/verify test, sampled
guest CPU use, and the new retained display milestones when present.

`make benchmark-qemu-linux` runs the same Buildroot ASD for 15 seconds and
records wall time, host user/system CPU, CPU percentage, and maximum RSS in
`build/metrics/qemu-linux.txt`.  Set `QEMU_BENCH_SECONDS` to change the window.

`make metrics-qemu-fidelity` compares 15 hardware-derived display values in a
physical retained log against the latest QEMU display run.  Override
`PHYSICAL_CONTRACT_LOG` and `QEMU_CONTRACT_LOG` to compare other runs.

`make smoke-linux-buildroot-fidelity` runs the complete display smoke suite in
a cycle-throttled mode, verifies that the physical HC15xx WAIT prohibition is
still present, compares all 15 register contracts, and checks CPU/panel timing
against the physical reference.  Normal smoke targets remain the fast mode.

## 2026-07-18 baseline and checkpoint

The log90 physical baseline measured:

- screen process spawn to main: 1512.382 ms
- guarded panel initialization: 4146.055 ms
- screen spawn to ready: 6722.799 ms
- 256 KiB storage write and verification: 80.128 ms
- sampled guest CPU busy time: 1.21%

The log92 checkpoint measured:

- screen process spawn to main: 1519.435 ms
- guarded panel initialization: 2059.253 ms
- screen spawn to ready: 4419.975 ms
- 256 KiB storage write and verification: 78.236 ms
- sampled guest CPU busy time: 2.67%

Screen readiness therefore improved by 2302.824 ms (34.3%), and guarded panel
initialization improved by 2086.802 ms (50.3%).  Retained milestones put panel
initialization, the first panel push, RGB readiness, and service readiness at
2.142, 2.261, 2.361, and 2.362 seconds from screen entry respectively.

The fidelity mode uses QEMU icount shift 1.  It reports 497.66 BogoMIPS versus
611.32 on hardware (ratio 0.814) and panel initialization of 1933.494 ms versus
2059.253 ms (ratio 0.939). The historical process-spawn baseline was measured
with the retired bFLT userspace; new comparisons must record fixed-address ELF
and static-PIE ELF separately. Cache/MMIO/memory latency remains the next
emulator fidelity gap rather than panel timing.

QEMU Linux uses an entire host core when the guest is idle: 15.01 seconds user
CPU in a 15.00 second window (100%), with roughly 62.6 MiB RSS.  Live GDB
inspection showed `cpu_wait == NULL`, because the conservative HC15xx CPU
profile deliberately does not select the generic 4Kc WAIT path.

An experiment enabling generic 4Kc WAIT reduced QEMU to 18% host CPU, but the
physical log91 run stopped immediately after `screen-after-gma-desc`: the CPU
did not wake to continue panel setup, no display appeared, and loglinux stayed
empty.  Therefore HC15xx WAIT is not a valid Linux idle mechanism.  It remains
disabled on both targets; emulator acceleration must not change this physical
CPU contract.

QEMU originally reproduced 13 of the 15 retained display values (86.67%).  Its
panel model shifted one serial bit on every GPIO read, while the SF2000 uses an
8080-style parallel bus split across GPIO L and T and advances data on RD.  The
parallel readback implementation plus the measured B3/F2 responses now matches
15 of 15 values (100%).

The screen logger formerly opened `/dev/kmsg`, issued two writes, and closed it
for every line.  Log90 shows roughly 49--52 ms between each GPIO diagnostic;
43 such messages consumed about 2.1 seconds.  The screen now keeps one kmsg
descriptor, emits one write per record, and summarizes GPIO directions by
bank.  Retained progress records remain the detailed source of truth.  The
next physical log will quantify the resulting startup reduction.

QEMU's continuous 30 Hz scanout now tracks dirty pages in the reserved GMA
arena and skips composition and hashing when no framebuffer data changed.  It
does not skip panel timers or alter guest-visible raster state.

## GBA ROM residency and generated-code cost

The log146/loglinux0079 Pokemon Unbound run establishes that this workload is
CPU-bound inside gpSP's generated ARM-to-MIPS code. At representative points,
normal mode fell from 36.302 FPS at frame 300 to 27.429 FPS at frame 1200;
uncapped full-frame mode reached 58.229 FPS initially but settled near 30 FPS.
GE presentation remained about 0.55--0.68 ms per frame, so removing presentation
or audio work cannot supply the requested 50% improvement.

Maximum GBA ROMs previously depended on a paged file cache. Linux now maps the
otherwise-unused physical `0x02000000..0x03ffffff` reservation through the
narrow `/dev/sf2000-rombuf` device. gpSP divides the 32 MiB fallback into
32 KiB allocations so NOMMU fragmentation cannot make content loading fail,
but uses the contiguous device mapping on SF2000. For an exactly 32 MiB ROM,
the MIPS emitter masks the cartridge address and adds the aligned ROM base
directly; this removes the read-map index, table load, and dependent pointer
load from every generated ROM data access.

The real-ROM QEMU smoke test requires:

- `buffer_mib=32`, `swapped=0`, and `direct=1`;
- zero runtime ROM page loads and bytes;
- valid frames, GE presentation, audio, clean exit, and no guest fault.

The core also reports ROM/RAM JIT peak usage and flush counts at unload. Those
counters distinguish translation-cache churn from instruction-selection cost
in future physical profiles without adding work to the per-instruction path.

The log147/loglinux0080 comparison isolates the remaining Pokemon Unbound
bottleneck. Two Unbound sessions recorded 87,003 and 76,219 RAM translation
flushes, while FireRed recorded only 43 over more than 4,500 frames and held
59.7 FPS. QEMU attributed every one of 27,211 sampled flushes to an ordinary
generated RAM store: capacity and DMA flush counts were both zero, and peak
RAM JIT use was only 29,240 of 131,072 bytes.

The MIPS store handlers now compare tagged RAM before invalidating translated
code. Recopying an identical byte, halfword, or word cannot change execution,
so that write and its global flush are safely omitted; changed code still uses
the original store-before-flush path. DMA applies the same endian-normalized
rule. In the real-ROM QEMU workload this removed 20.3% of RAM flushes and
improved the 600-frame rate from 43.050 to 44.947 FPS without changing scanout,
audio, or lifecycle behavior. Eliminating the remaining flushes safely would
require per-block coverage plus stable indirect gates or incoming-edge tracking
for every RAM translation; merely retaining the current directly linked code
would execute stale instructions.

## Rejected RAM-version experiment

Phase A measured 121,382 RAM translations over approximately 22.5 seconds.
Of those, 90,954 were exact repeats (74.93%), making content reuse a better
target than simply enlarging the cache. The guest footprint was 21.5 MiB but
the corresponding host allocation reached 126.99 MiB (5.91x). The Unbound
QEMU baseline was 51.572 FPS at frame 300 and 45.034 FPS at frame 600, with
21,666 full-RAM store flushes.

The attempted exact-content cache was rejected after physical log149. Both
Unbound and FireRed remained on the white boot frame while alternating only
between PCs `081dd81a` and `081dd066`. The run reported `ram_peak=0`, zero
version hits, and roughly one million store callbacks: the experimental path
was changing ROM-startup behavior before it had reused a single RAM
translation. A stronger QEMU test reproduced invalid guest jumps when the
generated block-transfer helpers were exercised.

The production build therefore retains the proven 128 KiB RAM JIT and the
safe identical-write optimization above. It does not publish the experimental
ROM recursion, selective block-transfer invalidation, or 1 MiB RAM arena.
After rollback, the real Unbound QEMU run again progressed from the white boot
frame to PCs `080080d4` and `08006b92`, produced nonzero audio, rendered 600
frames, and exited cleanly. Exact-content reuse remains a measured opportunity,
but it must be redesigned without modifying ROM translation publication and
must prove a nonzero RAM hit rate before entering a device build.

The generated-ROM SMC diagnostic suite covers `smc-ab`, `smc-range`, `smc-isa`,
`smc-mirror`, `smc-dma-epoch`, and `smc-block` (ARM/Thumb, aliases, DMA epoch,
and block-transfer paths). Each mode must return cleanly with no bad jump or
bus fault and produce the single-color scanout oracle hash `6ddf4dc5` (the
green 240x160 frame, 76,800 pixels).
