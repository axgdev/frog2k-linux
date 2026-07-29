# Display incident: log78 through log89

> Historical note: this incident occurred with the retired bFLT userspace.
> The current tree ships only fixed-address and static-PIE ELF executables.
> Its stale-artifact and display-ownership lessons still apply.

## What failed

The build identified as commit `78cb6a0` was reported working on the physical
SF2000 in log78, but a clean rebuild of that commit produced a blank display in
log88.  Later display changes were therefore compared with a false baseline and
alternated between a blank panel and moving, scrambled pixels.

The panel was not relying on the G1--G8 pictures as a magic initialization
sequence.  Those pictures merely kept an older, working `sf2000-screen` binary
in the generated Buildroot overlay.

## Root cause

At the time of log78, the `sf2000-screen` target was not forced to rebuild when
switching commits.  The artifact tested as log78 combined:

- the kernel, root filesystem, init and services from `78cb6a0`; and
- `sf2000-screen` from its parent `2f6195c4`.

The retained physical trace proves this from its string pointers.  For example,
`screen-ge-submit`, `screen-ge-console-fill-ok`, `screen-ready-done`,
`screen-panel-aux`, and `screen-panel-push-begin` resolve to the exact offsets
in the parent executable (including the 32-byte bFLT header), not to the clean
`78cb6a0` executable.  A clean `78cb6a0` build also logged
`screen-ge-clock-fast=0` and stopped after panel identification; the historical
executable contains no such clock change.

The reconstructed hybrid was tested as log89 and works correctly on the
physical device.  Its ASD SHA-256 is
`f51b526109f2255ca8aad91b85985e0ed69f3fac26e4a3fb5591689d95f61722`.

## Permanent correction

The source tree now explicitly contains the physically verified parent display
service together with the log78 Linux base.  The Makefile hashes every display
source, its build flags, and the Makefile into a content stamp.  An old binary
therefore cannot silently survive a checkout, while an unchanged build does not
needlessly relink it or regenerate the initramfs and kernel.  Its bFLT build
timestamp is fixed through `SOURCE_DATE_EPOCH`, making clean builds reproducible.

Future display work must satisfy all three checks before replacing this base:

1. build from a clean output directory;
2. pass the QEMU display and Buildroot boot smoke tests; and
3. compare the generated `sf2000-screen` hash and retained marker layout with
   the executable intended by the source revision.

Physical success remains the final authority for panel timing and GE behavior;
QEMU validates control flow and memory safety but cannot yet prove the analog
RGB timing seen by the ST7789 panel.
