# Hardware acceleration map

<!-- SPDX-License-Identifier: MIT -->

This file separates hardware blocks from vendor middleware. Replacing every
archive is not the same as accelerating the SF2000.

| Block | Linux/source status | Useful for games |
|---|---|---|
| GE 2D engine | Source command encoder, managed buffers, queue/fence ABI, QEMU model and vendor node comparisons are complete for the supported public API | Yes: fill, copy, format effects and scaling |
| SND0/PWM DAC | Source MMIO core plus ALSA DMA driver, fixed-point resampler and QEMU audio model | Yes: removes PCM service and conversion pressure |
| SND0 STC0/STC1 | Source tick/divisor/pause routines and vendor-compatible A/V update/drop/hold policy, with a virtual-time QEMU model | Indirectly: media A/V sync; emulators use monotonic pacing |
| HC15 MMC | Linux command path, aligned DMA, PIO fallback and safe writeback | Yes: ROM and save-state I/O |
| Video decoders | Not yet exposed by Linux | No for emulator rasterization; valuable for a media player |
| Image decoder | Not yet exposed by Linux | Menus, artwork and thumbnails only |
| CVBS display output | Panel output works; composite adapter remains | External display, not frame throughput |

## Vendor archive routing

- `libge.a` maps to `ge/` and the Linux GE driver.
- `libauddrv.a` maps to `audio/` and the Linux ALSA driver. Capture,
  SPDIF/HDMI, EQ and DRC are not SF2000 playback dependencies.
- `libmmchosthc15.a` maps to the Linux HC15 MMC host. `libmmc.a` is a storage
  protocol layer, not another accelerator.
- `libviddrv.a` and its H.264, MPEG-2, MPEG-4, VC-1, VP8 and image-decoder
  archives are one HC video subsystem. Their supplied `built-in.o` symbols are
  mostly obfuscated; the stable contract is the `hcuapi/viddec.h` ioctl ABI.
- `libffplayer.a` is player/demux policy over audio/video devices, not a
  hardware driver.
- `libdsc.a`, `libefuse.a`, `libntfs.a`, `liblnx.a`, and USB archives do not
  accelerate the emulator loop. USB is also not routed to the external SF2000
  connector as a normal data port.

## Measured priorities

Physical log140 validates RGB565 hardware staging in both modes. Against
log139, long-running Gambatte uncapped throughput rises from 175.6 to
188.8 fps (+7.5%). The demanding gpSP scene rises from 87.2 to 101.5 fps
(+16.3%), while its sampled presentation cost falls from roughly 1.6 ms to
0.6 ms. Paced gpSP remains 59.3 fps with no xrun, drop, or EAGAIN event.

The remaining normal-mode anomaly is scheduler policy: log140 contains 252
gpSP pacing resets, mostly for only 2--5 ms of lateness. The frontend now
preserves the absolute timeline for misses shorter than one complete frame and
rebases only after an actually lost frame. This is an OS-neutral pacer module
with boundary tests, not core-specific timing.

After these changes the largest general-purpose costs are core execution and
cache ownership, not scanout. The next hardware work should therefore be:

1. validate the corrected pacing-reset count on the physical device;
2. expose the video decoder behind a Linux media API using the documented
   ioctl packet/status contract, with a QEMU register/queue model before
   enabling hardware;
3. add image-decode conformance fixtures for menu assets;
4. add a Linux CVBS adapter independently of the panel/GE path.

Codec work must not enter the libretro frame callback or duplicate GE, audio,
MMC, clock, or cache primitives. Each block gets an OS-neutral register/packet
core, a Linux adapter, a QEMU model, and host conformance tests.
