# Run 369 — SF2000 audio: correct per-chip 44.1 kHz I2S divisors

## What the user reported

Run 369 (testing the run-368 "44.1 kHz divisor fix", commit `4fb9229`):
audio had **high pitch** — "for many emulators, for example qpsx, the audio
has now high pitch, for example there are voices in ridge racer from a male
narrator that sound like a little kid".

## Root cause

The run-368 fix (`4fb9229`) applied the **GB300** 44.1 kHz I2S divisors to
the **SF2000**.  The vendor `libauddrv` branches on the chip ID (high 16 bits
of SYS+0x00) for the M/N divisors, and the chip mapping was inverted:

| Chip | 48 kHz family (32k/48k) | 44.1 kHz family (44.1k/22.05k) |
|---|---|---|
| **SF2000 (HC1512)** | M=284/N=13, 0x470 bits 16,21 only | **M=281/N=14, 0x470 bits 16,21 only** |
| **GB300 (non-1512)** | M=344/N=7 + 0x470 bit3 clear + 0x474=0x00106000 | M=316/N=7 + 0x470 bit3 clear + 0x474=0x001b6000 |

M=316/N=7 on the SF2000 clocks the I2S bit stream ~2.25x fast, which shifts
every 44.1/22.05 kHz sample up by more than an octave — the "kid voice".
M=281/N=14 is the correct SF2000 value (and was the original multirate value).

### How this was verified

Disassembled the **linked** vendor `libauddrv` inside the UniFrog firmware
(`/root/UniFrog/output/unifrog.bin`, which links the real driver with
resolved relocations — the earlier SDK disassembly had relocations stripped):

- `0x803cd330` 44.1 kHz family: chip ID == 0x1512 → `li 281` → `sh` to
  SYS+0x47a, `li 14` → `sh` to SYS+0x478, then only 0x470 bits 16+21 set
  (`0x803cd35c`/`0x803cd3c0`).  Non-1512 → `0x803cd4e8`: `li 316`, `li 7`,
  0x470 bit 3 cleared, 0x474 |= 0x001b6000 (`lui 0x1b` + addiu 24576).
- `0x803cd388` 48 kHz family: 0x1512 → M=284/N=13 bits 16+21 only;
  non-1512 → M=344/N=7, bit3 clear, 0x474 |= 0x00106000.

## Changes

`patches/linux-7.1.4/0029-sf2000-multirate-audio.patch`:

- `sf2000_i2s_sf2000_44k`: `0x013c0007` (M=316/N=7) + extras → `0x0119000e`
  (M=281/N=14), no extras — restores the correct SF2000 clock.
- `sf2000_i2s_gb300_44k`: `0x0119000e` (M=281/N=14) → `0x013c0007`
  (M=316/N=7) + `clear_div_ctl_bit3` + `div_ctl2 = 0x001b6000` — the value
  the firmware actually uses on the GB300.
- SF2000/GB300 48 kHz rows unchanged (already correct).
- Comment block rewritten to document the verified per-chip/per-family table.
- Volume byte `0xff` and the extended DMA diag log from `4fb9229` are kept.

`Makefile`: the 44.1 kHz smoke assertion now expects the SF2000 values
`pll=080000c0 mn=0119000e ctl470=<..> ctl474=00000000 vol=ff` (the old
assertion expected `ctl474=001b6000`, which was the *GB300* value and is no
longer programmed on the SF2000).

## Verification (all passed)

- `smoke-linux-buildroot-audio` (32 kHz): `mn=011c000d` unchanged.
- `smoke-linux-buildroot-audio-44100`: `rate=44100 pll=080000c0
  mn=0119000e ctl470=0021001a ctl474=00000000 vol=ff`, non-silent WAV.
- `smoke-linux-buildroot-audio-gb300` (32 kHz GB300 route): `mn=01580007
  ctl474=00106000`.
- `smoke-linux-asd`, `smoke-linux-frontend`, `smoke-linux-gpsp`,
  `make check` — all pass.

## Testing on the device

- **ASD:** `/root/host-frogdev/testing/linux-asd-44k1-chip-divisor-fix.asd`
  — kernel-only change, replaces `firmware/linux.asd`; the 32 kHz path is
  untouched, so any regression would show only at 44.1/22.05 kHz.
- Expected: qpsx (Ridge Racer) narrator back to a normal male voice; 44.1 kHz
  cores pitch-correct.

## Notes / risk

- Only the 32 kHz row was ever proven on real hardware before this session.
  The 44.1 kHz family values are now grounded in the linked firmware
  disassembly (both chips), which is the strongest available evidence, but a
  hardware listen is the final confirmation.
- The "too much treble / little bass" complaint from run 368 was NOT a clock
  problem — it is likely speaker response plus the flat (non-tapered) volume
  curve; this fix only addresses pitch.  If the tone is still thin after this
  run, the next lever is the SND0 FADE/volume curve (vendor writes 0xff at
  max) and possibly a gentle low-shelf in the frontend resampler.
