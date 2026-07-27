# Hardware acceleration map

<!-- SPDX-License-Identifier: MIT -->

This file separates hardware blocks from vendor middleware. Replacing every
archive is not the same as accelerating the SF2000.

| Block | Linux/source status | Useful for games |
|---|---|---|
| GE 2D engine | Source command encoder, managed buffers, queue/fence ABI, QEMU model and vendor node comparisons are complete for the supported public API | Yes: fill, copy, format effects and scaling |
| SND0/PWM DAC | Source MMIO core plus ALSA DMA driver, fixed-point resampler and QEMU audio model | Yes: removes PCM service and conversion pressure |
| SND0 STC0/STC1 | Source tick/divisor/pause routines recovered from `libauddrv.a` | Indirectly: media A/V sync; emulators use monotonic pacing |
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

Physical log139 validates paced gpSP presentation at about 0.56--0.72 ms,
down from roughly 1.6--2.1 ms in log138, and eliminates the two observed audio
xruns. Normal gpSP remains at 59.2 fps. Its demanding uncapped scene falls to
about 87 fps while the callback path still reports 1.6--2.4 ms CPU-buffered
presentation, so RGB565 hardware staging is now used in uncapped mode too.

After that change the largest general-purpose costs are core execution and
cache ownership, not scanout. The next hardware work should therefore be:

1. measure uncapped GE staging on the physical device;
2. expose the video decoder behind a Linux media API using the documented
   ioctl packet/status contract, with a QEMU register/queue model before
   enabling hardware;
3. add image-decode conformance fixtures for menu assets;
4. add a Linux CVBS adapter independently of the panel/GE path.

Codec work must not enter the libretro frame callback or duplicate GE, audio,
MMC, clock, or cache primitives. Each block gets an OS-neutral register/packet
core, a Linux adapter, a QEMU model, and host conformance tests.
