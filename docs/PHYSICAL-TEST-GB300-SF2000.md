# Physical GB300/SF2000 test

The current family-compatible Linux ASD is:

```text
build/sdcard/bios/bisrv.asd
```

Copy that file to the normal firmware location on the SD card. Copy the
contents of `build/sdcard/sf2000/cores/` and its `licenses/` directory as a
matching set; the ASD and all cores were built together. The authoritative
hashes are generated with each build in `build/sdcard/SHA256SUMS`; run
`(cd build/sdcard && sha256sum -c SHA256SUMS)` before copying. Do not copy a
hash from this document, because the artifact necessarily changes with the
kernel or embedded root filesystem.

## GB300

1. Boot with a charged battery.
2. Confirm that the main menu is upright, not mirrored, and that white text
   and primary-color menu elements have the expected colors.
3. Start a small Stella game and confirm that audio is audible and continuous.
4. Open and close the pause menu with `START+SELECT`.
5. Run a C64 `T64` or `D64` title for at least 20 seconds. The screen should
   remain stable without a horizontal scan/frequency line.

## SF2000 regression

Repeat the menu, Stella, pause-menu, and C64 checks on an SF2000. Audio and
the existing landscape display behavior must remain unchanged.

## QPSX

Use a cue/bin pair in the same directory. For the supplied Ridge Racer test,
copy these two files together into `/psx`:

```text
Ridge Racer (Track 01).cue
Ridge Racer (Track 01).bin
```

The cue must reference exactly `Ridge Racer (Track 01).bin`. Select the `.cue`
file, wait at least 30 seconds, and do not power-cycle during startup. A
successful launch produces these markers in `log.txt`:

```text
core init begin
load QPSX_CDR_OPEN_OK 13/16
load QPSX_LOAD_CDROM_OK 15/16
load QPSX_DONE 16/16
ROM load complete
```

Selecting a raw `.bin` is supported only when its matching same-basename
`.cue` is also present, because QPSX uses that cue to discover track layout.
For multi-track images, every file named by the cue must be present.

## Collecting diagnostics

If a core appears stuck, press `START+RIGHT` once and wait several seconds
before power-cycling. This forces a log checkpoint. After reboot, collect
`/log.txt`, `/loglinux.txt`, and any `ui-diagnostic*.bin` file. `START+UP`
captures the UI diagnostic artifact. In a QPSX failure, the most useful point
is the last `load QPSX_*` marker; if only `pre-core-launch` is present, the
core process did not reach its initialization path.
