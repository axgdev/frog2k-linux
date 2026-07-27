# Vendor driver replacement status

The Linux port does not link vendor objects. “Working on SF2000” and
“drop-in ABI-equivalent to every archive symbol” are different milestones.
The current status is:

| Vendor archive | Current source replacement | Status |
|---|---|---|
| `libauddrv.a` | `audio/hc15xx_audio.c` plus the ALSA platform driver | SF2000 playback path complete: APLL, PWM DAC, mono I2S, volume, DMA ring, IRQ, underrun and amplifier routing. Not archive-wide parity: capture I2S, PDM/TDM/PCM, SPDIF/HDMI, decoder/sink, AV sync, EQ and DRC remain outside the handheld playback requirement. |
| `libge.a` | `ge/` plus the Linux GE driver | Supported vendor HC15xx compositor path is source-compatible and node-tested against the archive. Flags that the vendor public header itself marks unsupported remain rejected. It is not a binary ABI clone of every private helper. |
| `libmmchosthc15.a` | Linux `hc15_mmc` host driver | Functional SD command, PIO fallback and aligned DMA read/write support, including safe FAT writeback. It is a Linux MMC host implementation, not an HCRTOS archive ABI clone or a replacement for the separate filesystem/protocol `libmmc.a`. |
| `lslibauddrv.a` | — | No archive with this name exists in the supplied SDK. The available related archive is `libauddrv.a`. |

For game performance, the next useful vendor hardware blocks are:

1. `libauddsp.a` functionality, if the SF2000 mask actually exposes its audio
   DSP, for resampling/mixing without spending CPU cycles.
2. `libviddrv.a` and the codec archives for hardware-assisted video playback.
   These do not accelerate normal emulator rasterization.
3. Image decode from `libviddrv_imagedec.a` for fast menu artwork and
   thumbnails.
4. A shared OS-neutral HC15xx MMC controller core. Linux already uses DMA, so
   this is mainly valuable for one maintained implementation across Linux and
   MuFrog rather than a prerequisite for correct storage.
5. Clock/reset/DMA frameworks with measured governors, allowing CPU, GE,
   storage and media clocks to scale independently without duplicated magic
   register writes.

USB and networking vendor libraries are intentionally low priority on SF2000:
the external connector is not wired as a normal USB data port.
