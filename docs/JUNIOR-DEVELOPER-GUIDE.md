# SF2000 Linux development and hardware-port guide

This is the working map for extending `sf2000_linux`. It covers the target,
repository, diagnosis, performance work, vendor-driver reverse engineering,
media acceleration, and the prioritized roadmap. Read `AGENTS.md` and
`CONTRIBUTING-HARDWARE-PORTS.md` before editing.

## 1. Target contract

The SF2000 is an HC15xx-family SoC with one little-endian MIPS32r1 CPU,
128 MiB RAM, no FPU, and no MMU. The shipping boot ROM establishes some DDR,
clock, pinmux, and panel state before Linux. An HC16xx sequence, generic
ST7789 sequence, or vendor SDK default is reference material, not proof of the
SF2000 contract.

| Property | Engineering consequence |
|---|---|
| MIPS32r1 | Use `-march=mips32`; never emit MIPS32r2 `synci`, DSP, or FPU instructions. |
| Little endian | Tool tuple is `mipsel-buildroot-linux-uclibc`; peripheral bit fields still follow the device specification. |
| No FPU | Compile every target object with `-msoft-float`; hard/soft-float ABI mixing corrupts calls. |
| No MMU | No `fork`, copy-on-write, demand paging, ASLR, or overcommit. Use `vfork()+execve()` carefully. Anonymous mappings consume physically contiguous pages. |
| Static PIE | Normal programs are static `ET_DYN`, relocated by the NOMMU loader, with no ELF interpreter. MIPS `$gp`/`$t9` rules apply to PIC C and dynarecs. |
| Non-coherent DMA | CPU, GE, MMC, audio, and video do not automatically agree on cached memory. Every buffer needs an owner and cache transition. |
| One weak CPU | Serial printk, polling, filesystem sync, conversion, and excessive syscalls cost frame time. |
| Retained RAM | Warm reset may preserve RAM. `0x07a00000`–`0x08000000` is reserved for recovery and must not become an application arena. |

Address vocabulary:

- **physical address**: address used by DMA engines;
- **KSEG0**: cached alias, normally `0x80000000 + physical`;
- **KSEG1**: uncached alias, normally `0xa0000000 + physical`;
- **VMA**: address range owned by a Linux process mapping;
- **IRQ**: device-to-CPU interrupt notification;
- **DMA descriptor**: memory structure describing an accelerator transfer;
- **fence**: proof that earlier accelerator commands completed;
- **pitch/stride**: bytes between adjacent image rows, not visible width;
- **XRUN**: audio DMA underrun/overrun;
- **SMC**: self-modifying guest code requiring dynarec invalidation;
- **MMIO**: register access through a memory address.

Do not convert arbitrary pointers to physical addresses by masking. Use the
managed-buffer ABI, validate the owning VMA, document alignment/extent, and
reject addresses outside RAM. Device completion and compiler ordering are
different requirements. Every wait needs a timeout and recovery path.

## 2. Repository map

| Path | Purpose |
|---|---|
| `Makefile` | Build graph, fetches, audits, host tests, QEMU runs, packaging. Prefer a target over a new script. |
| `linux/` | DTS and board configuration. |
| `patches/linux-7.1.4/` | Ordered Linux board, GE, MMC, audio, ELF, and cache patches. |
| `buildroot/` | Defconfig, BusyBox, rootfs, PID 1/services, Buildroot/uClibc patches. |
| `boot/`, `init/` | Firmware handoff, loader, and earliest init. |
| `ge/` | OS-neutral GE implementation, Linux adapter, vendor differential tests. |
| `audio/` | OS-neutral SND0, A/V sync, fixed-point resampling. Kernel ALSA is in the Linux patches. |
| `vdec/`, `dsc/`, `efuse/` | Portable register/state cores and fake-MMIO tests; VDEC/DSC are not complete Linux drivers. |
| `platform/`, `include/` | Shared retained-log contract. |
| `player/` | Minimal media test, not a media framework. |
| `tests/`, `tools/` | Small C fixtures and host tools. |
| `external/hclinux/` | Read-only HC Linux reference, not SF2000 truth. |
| `../sf2000_qemu` | QEMU machine/peripherals. Commit model changes separately and cross-reference them. |
| `../sf2000_linux_frontend` | Application/frontend and libretro cores. |

Read-only evidence:

- `../unifrog-hcrtos-sdk/lib/vendor`: vendor archives;
- `../../sf2000_chipset_documentation`: board/panel/general HC15xx documents;
- `../latest_log`: physical logs.

## 3. Build and verification

Record the baseline:

```sh
git status --short
git log -5 --oneline
make status
make help
```

Normal ladder:

```sh
make check
make check-vendor
make ROOTFS=buildroot elf-audit
make smoke-linux-frontend-lifecycle
make smoke-linux-buildroot-asd
make linux-buildroot-asd
sha256sum build/sf2000-linux-buildroot.asd build/sdcard/bios/bisrv.asd
```

Run the smallest relevant test first:

```sh
make test-ge-node
make test-ge-queue
make test-ge-batch
make audio-test
make vdec-test
make vdec-codec-test
make dsc-test
make smoke-linux-buildroot-display
make smoke-linux-buildroot-audio
make smoke-linux-gpsp
make smoke-linux-gpsp-smc
make smoke-linux-frontend-lifecycle
make smoke-linux-player
```

Read logs safely and summarize performance:

```sh
strings -a ../latest_log/sf2000_linux/loglinux0089.txt | less
make METRICS_LOG=../latest_log/sf2000_linux/loglinux0089.txt metrics-linux
make METRICS_LOG=../latest_log/sf2000_linux/loglinux0089.txt metrics-frontend
```

`metrics-frontend` splits capped/uncapped sessions and reports final FPS,
audio errors, pacing resets, sampled core/present time, presenter, JIT
invalidation reasons, and cache-sync failures. Compare the same ROM, scene,
duration, clock, and build. Capped FPS measures behavior; uncapped full-frame
FPS measures capacity.

Outputs outside the repository are deliberate:

- kernel: `/tmp/sf2000-linux-next-kernel-7.1.4`;
- Buildroot: `/tmp/sf2000-linux-next-buildroot`;
- QEMU: `/tmp/sf2000-qemu/qemu-10.2.2/build/qemu-system-mipsel`.

If cleaning is required to make a source edit visible, fix the Make dependency.
Do not make “clean first” part of the workflow.

## 4. Failure classification

Classify before editing:

1. **No backlight/log**: loader, decompression, entry ABI, DDR overwrite,
   watchdog, or fatal early bus access.
2. **Backlight, no framebuffer**: panel/VOU/GMA, stale executable, ownership,
   or launch.
3. **Scrambled/static pixels**: pitch, geometry, descriptor, cache ownership,
   GE ring corruption, panel timing/source. Do not assume color conversion.
4. **Browser remains visible**: child launch/ready marker or display handoff.
5. **White core frame**: no video callback, stale/invalid dynarec code, ROM
   mapping failure, or blank callback data.
6. **Audio hiss**: amplifier/DAC active but PCM clock, format, cursor, cache,
   routing, or samples wrong.
7. **Audio hiccup**: separate transient driver XRUN from sustained sub-real-time
   emulation. Buffering cannot repair 35 FPS work required at 60 FPS.
8. **Second launch fails**: mapping/ownership leak, stale marker, incomplete
   teardown, or invalid retained state.
9. **Physical-only failure**: QEMU lacks cache, IRQ, reset, error, or timing
   behavior. Add the invariant to QEMU; do not weaken the driver.
10. **QEMU-only failure**: decide whether the model rejects valid hardware or
    hardware merely tolerates an invalid sequence.

Search:

```sh
strings -a LOG | rg -n \
 'Kernel panic|Oops|Data bus error|fatal signal|GE present failed|ownership|blank|white|xrun|EAGAIN|cache.*fail|reloc|bad jump'
```

Correlate using `mono_us`; persistent logging batches messages. Retained
entries survive crashes but hold less context. `/dev/kmsg` at 115200 baud can
perturb audio, so high-rate telemetry goes to tmpfs and is drained after the
foreground job.

For an exception record EPC/PC, Cause, BadVAddr, SP, RA, load bias, and exact
ELF. Inspect, do not guess:

```sh
${CROSS_COMPILE}readelf -hW EXE
${CROSS_COMPILE}readelf -lW EXE
${CROSS_COMPILE}readelf -dW EXE
${CROSS_COMPILE}readelf -rW EXE
${CROSS_COMPILE}objdump -dr EXE
${CROSS_COMPILE}addr2line -e EXE -f -C ADDRESS
```

Normal userspace is `ET_DYN`, has no `PT_INTERP`, and only loader-supported
relocations. For PIE, subtract runtime load bias when symbolizing. MIPS PIC
callees derive `$gp` from `$t9`; generated/indirect calls must enter with the
callee in `$t9`. A dynarec may use `$gp` as a guest register only if every C
transition restores the ABI. Generated instructions require a validated
`cacheflush` before execution. QEMU TCG can hide stale-cache bugs, so tests
assert nonzero sync calls and zero failures.

## 5. Performance method

Use a frame budget:

```text
frame = core + audio/mix + cache/JIT + present + scheduler/syscalls + I/O
```

At 60 FPS the budget is 16.667 ms. If GE presentation is 0.6–1.7 ms and core
work is 40–95 ms, scanout cannot recover real time. If uncapped improves but
capped XRUNs do not, investigate burst latency/pacing.

For each optimization:

1. state the measured counter/bottleneck;
2. preserve baseline log and commit;
3. change one algorithm/ownership contract;
4. add a test that fails before the fix;
5. compare capped and uncapped metrics;
6. check binary size and memory;
7. run repeated launch/exit;
8. revert below-noise changes unless they simplify code.

The log156/loglinux0089 baseline:

- Gambatte: ~59.7 FPS capped, zero audio errors; ~166 FPS uncapped.
- FireRed gpSP: ~59.7 FPS capped, zero audio errors; safely above real time
  uncapped, varying with scene.
- Unbound: ~32–44 FPS capped, ~38–46 FPS uncapped, 32 capped XRUNs.
- GE presentation: ~0.6–1.7 ms.
- Unbound: ~47,700 store-triggered RAM JIT flushes and ~68,000 instruction
  cache synchronizations; FireRed has only three store flushes.

The next Unbound optimization is therefore precise SMC invalidation, not a GE
fast path. Correctness means invalidating every translation whose guest bytes
changed and no unrelated translations. First add ARM/Thumb, byte/half/word,
DMA, mirror, overlapping-block, branch-link, and cache-wrap fixtures. Then
evaluate per-page generations/block dependencies against full-cache flush.
Never skip invalidation because one ROM happens to run.

NOMMU memory rules:

- use page-sized mappings unless physically contiguous DMA memory is required;
- managed DMA buffers must be contiguous and explicitly freed;
- copied ROM plus page cache can temporarily consume twice the ROM size;
- `MemFree` alone is not a leak proof; inspect `Cached`, `Buffers`, `Slab`,
  and repeat-launch deltas;
- allocate process memory on demand and return it at exit;
- reserve a fixed arena only after repeatable allocation failures prove need.

## 6. Reverse engineering a vendor archive

### Inventory

Never modify vendor/reference inputs. Capture:

```sh
V=../unifrog-hcrtos-sdk/lib/vendor/libviddrv.a
sha256sum "$V"
file "$V"
${CROSS_COMPILE}ar t "$V"
${CROSS_COMPILE}nm -A --defined-only "$V" | sort > build/viddrv.defined
${CROSS_COMPILE}nm -A -u "$V" | sort -u > build/viddrv.undefined
${CROSS_COMPILE}objdump -dr -M no-aliases "$V" > build/viddrv.dis
```

Track public symbols, parameter/return ABI, state/error behavior, MMIO/DMA
order, timeouts/recovery, locking, vendor differential results, QEMU/physical
evidence, and explicit unsupported behavior. “Links successfully” is not
parity.

### Static analysis

```sh
rizin -A "$V"
# aaa; afl; ii; iz; axt @ SYMBOL; pdf @ SYMBOL; pdg @ SYMBOL
radare2 -AA "$V"
# afl; axt @ SYMBOL; pdf @ SYMBOL; pdg @ SYMBOL

mkdir -p build/reverse-viddrv
cd build/reverse-viddrv
${CROSS_COMPILE}ar x "$V"
${CROSS_COMPILE}readelf -aW OBJECT.o
${CROSS_COMPILE}objdump -dr -M no-aliases OBJECT.o
```

MIPS traps for reviewers:

- branch/jump delay slots execute before transfer;
- O32 uses `$a0`–`$a3` for four arguments, then stack; `$v0`/`$v1` return;
- PIC commonly derives `$gp` from `$t9`; a `$gp` load may be GOT, not MMIO;
- `lui`+signed `addiu` differs from `lui`+`ori`;
- stripped functions with similar shapes need not share semantics;
- compiler access order does not prove peripheral completion.

Trace public entries toward MMIO, DMA allocation, cache functions, IRQ setup,
clock/reset, queues, and delays. Maintain an evidence table: archive hash,
object/function offset, instructions/decompiler excerpt, inferred fact,
confidence, confirming trace.

### Dynamic oracle

Use the known-working HCRTOS application:

1. locate its vendor API call;
2. build the smallest non-destructive caller;
3. intercept MMIO/cache/delay/queue imports where possible;
4. run in QEMU and record ordered reads/writes;
5. run identical calls through the source fake-MMIO adapter;
6. compare masks, descriptors, status, returns, and timeout behavior;
7. confirm on hardware.

Trace reads and writes. Read-modify-write reveals inherited bits. For DMA
capture physical addresses, sizes, alignment, cache direction, doorbell,
busy/completion/error, IRQ, ack, and reset recovery. A readable register or
issued command is not completion.

Avoid register sweeps. Physical evidence shows the `0x18860000` clock-domain
window can wedge SF2000 without a recoverable bus error. Unknown clocks,
resets, eFuse writes, and unconstrained DMA belong in explicit opt-in probes,
never default images or `make check`.

### Source architecture

```text
portable state/protocol core
  -> read32/write32, cache, delay, lock, DMA callbacks
Linux adapter
  -> platform driver, IRQ, DMA API, subsystem API
QEMU model
  -> registers, timing, IRQ, reset, errors
host/vendor harness
  -> fake MMIO, injected faults, golden traces
```

The portable core contains no `/dev/mem`, file descriptors, FreeRTOS queues,
fixed KSEG aliases, or QEMU types. One driver owns each register block.
Device tree describes resources; it is not a driver. Validate overflow,
alignment, pitch, format, and extent before MMIO. Bound waits. Timeout recovery
quiesces DMA, acknowledges status, resets only the owned block, restores state,
and returns an error.

## 7. Video/audio accelerators and FFmpeg

Relevant vendor archives:

```text
libviddrv.a
libviddrv_h264dec.a
libviddrv_mpeg2dec.a
libviddrv_mpeg4dec.a
libviddrv_vc1dec.a
libviddrv_vp8dec.a
libviddrv_imagedec.a
libffplayer.a
libdsc.a
```

`libffplayer.a` is player/demux policy, not hardware. Codec archives plus
`libviddrv.a` form the video subsystem. Start from
`hcrtos/hcuapi/viddec.h`, then map its packet/ioctl contract to registers using
disassembly and traces. `libdsc.a` covers display/CVBS. SF2000 composite output
does not require HDMI; USB/HDMI are out of scope.

Port order:

1. characterize codecs, profiles, dimensions, reference memory, packet layout,
   output format, stride/alignment, and actual SF2000 block presence;
2. finish portable VDEC clock/reset/configure, input DMA, output/reference
   buffers, start, status, IRQ ack, timeout/error/reset;
3. model descriptors, lifecycle, IRQ, reset, invalid requests, and deterministic
   output in QEMU;
4. add a Linux V4L2 mem2mem/stateless adapter if the hardware contract fits,
   otherwise use a narrow internal device until understood;
5. pass decoded surfaces to GE/GMA without CPU copies, with explicit ownership
   and cache transitions;
6. reuse infrastructure for image decode while keeping image commands separate;
7. implement DSC/VPO/CVBS independently of the panel path;
8. expose decoder frames through FFmpeg `AVHWDeviceContext`/
   `AVHWFramesContext` or a small hwaccel wrapper.

Current `make ffmpeg` builds minimal static software audio libraries. Before
production use:

- replace downloaded-source `sed` edits with versioned patches/stamps;
- build final programs with the static-PIE ABI; current `-mno-abicalls
  -fno-pic -fno-pie` must not leak into PIE executables;
- keep MIPS asm/FPU/DSP disabled until audited for MIPS32r1 soft-float;
- enable only needed demuxers/parsers/codecs/protocols/formats;
- avoid threads unless they improve blocking ownership on the single CPU;
- use fixed-point paths where profiles justify it;
- handle large contiguous reference-frame allocation failure explicitly;
- test frame hashes, timestamps, sample counts, drain/EOS, seek/flush,
  malformed packets, timeout, and repeated open/close.

First useful milestone:

```text
file demux -> parser -> hardware packet -> IRQ
-> zero-copy surface -> GE scale/colorspace -> panel
-> timestamped PCM -> ALSA DMA
```

Measure CPU/wall/decoder busy time, queue depth, copies/bytes, cache bytes, IRQs,
late/dropped frames, XRUNs, and peak memory against software decode.

## 8. Top ten expansions

1. **Precise gpSP SMC invalidation**: dependency-aware invalidation proven by
   adversarial tests; the measured route to a large Unbound improvement.
2. **Complete VDEC Linux/QEMU stack**: real codec packets, IRQ/error/reset,
   zero-copy surfaces.
3. **Hardware image decode**: artwork/thumbnails cached as RGB565 and sent to
   GE without CPU conversion.
4. **CVBS output**: source DSC/VPO/CVBS PAL/NTSC driver with independent panel
   ownership and audio routing.
5. **Clock/reset/DMA framework**: replace magic writes with measured domains,
   safe sequences, profiles, battery/thermal guardrails, and counters.
6. **Calibrated suspend/resume**: time-retaining and deepest standby, button
   wake, state save/restore, hours-long discharge measurements.
7. **Application SDK/ABI**: sysroot, headers, static-PIE rules, sample app,
   packaging, ELF audit, compatibility version.
8. **General libretro services**: core discovery/options, atomic saves/states,
   controller/aspect/filter policy, fault recovery, bounded background work.
9. **QEMU physical contract**: cache faults, IRQ timing, reset values, watchdog,
   DMA bounds, GE/audio timing, retained RAM, repeatable frontend traces.
10. **Release/recovery**: reproducible signed artifacts, known-good boot,
    atomic state, retained crash decoder, artifact/hash/log manifests.

Networking and USB host are lower priority: the board has no network and the
external USB connector is not wired as a normal data interface.

## 9. Checkpoint discipline

Before committing:

```sh
git diff --check
git diff --stat
git diff
make check
make ROOTFS=buildroot elf-audit       # userspace/loader changes
make RELEVANT-QEMU-SMOKE
git status --short
```

Each commit body records baseline evidence, changed contract, tests/results,
remaining physical risk, artifact hash, and related QEMU/frontend commit.
Never hide compiler, linker, patch, QEMU, or test failures. Keep format
migrations, drivers, and unrelated optimization in separate checkpoints.
