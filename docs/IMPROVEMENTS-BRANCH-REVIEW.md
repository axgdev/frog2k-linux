# Improvements Branch Review

Review baseline: `master` at `90b9405`. Reviewed branch started at `0d1ebb5`.

## Accepted Direction

- Portable callback-based eFuse, VDEC, and DSC source modules are useful
  scaffolding and their fake-MMIO unit tests cover basic sequencing.
- Static ELF is the right long-term userspace format. Fixed `ET_EXEC` and
  static-PIE `ET_DYN` serve different no-MMU ownership needs.
- Device-tree descriptions and focused device probes are useful once their
  register contracts and ownership are established.

## Blocking Findings

1. Commit `801c074` removed roughly 1,600 lines of working build, QEMU, smoke,
   artifact, and synchronization targets. `.PHONY` still advertised deleted
   targets, so `make efuse-test` could print success while doing nothing.
2. The PIE loader allocated memory before `begin_new_exec()`. The exec
   transition discards that old mapping.
3. Several malformed uClibc patches were applied under `|| true`; they never
   changed startup code.
4. `--noinhibit-exec` converted fatal MIPS relocation diagnostics into a
   nominally successful build. The result still contained `PT_INTERP` and was
   not a valid static executable.
5. The rootfs still contained bFLT BusyBox and frontend binaries, so the
   claimed global migration was incomplete.
6. Early binaries and kernel configuration lacked toolchain prerequisites.
   Incremental builds hid the race; clean parallel builds exposed it.
7. The device probes label command submission as PASS without checking a
   decoded frame or displayed result. VDEC and DSC register assignments are
   not yet supported by a committed vendor trace.
8. The new DTS nodes are marked `okay`, but no corresponding Linux platform
   drivers bind them. The current probes bypass kernel ownership.

## Integration Decisions

- Restore the complete build/QEMU verification graph from the last
  pre-deletion ELF work.
- Keep ELF-only output and add a rootfs format audit.
- Make the MIPS kernel loader own static-PIE relocation before entry.
- Build uClibc, BusyBox, services, and the frontend as PIC/static PIE from one
  prebuilt frog-toolchain; retain fixed ELF only for constrained early programs.
- Keep portable vendor-library modules, but describe them as provisional until
  symbol coverage and vendor differential traces exist.
- Keep destructive physical probes out of routine smoke tests.

The detailed process and acceptance gates are in
`docs/CONTRIBUTING-HARDWARE-PORTS.md`.

## Resolution

The integration now has no bFLT build path. The old uClinux target patches,
`elf2flt`, `bfltpack`, and flat startup code were removed. A clean direct
toolchain supports two deliberately separate static ELF forms:

- position-independent `ET_DYN` for init, BusyBox, services, and frontends,
  with no interpreter or shared runtime;
- opt-in fixed-address `ET_EXEC` compatibility, disabled and unreserved in the
  default build.

QEMU exposed the remaining architecture-specific failure: MIPS local GOT
entries are not represented by individual ELF relocation records. The kernel
loader now applies symbol-free `R_MIPS_REL32` records and biases the local GOT
using the `DT_MIPS_*` metadata. It rejects global GOT entries, interpreters,
unknown relocations, and invalid segment layouts. The rootfs audit enforces
the same contract before packaging.

The complete QEMU ASD smoke now reaches the fixed first stage and the static
PIE supervisor, initializes the GE/display path, and reports no address or bus
exception. Physical-device validation remains the final acceptance gate for
the synchronized `bisrv.asd` artifact.

gpSP exposed a second architecture contract after the general loader was
working. Its dynarec uses host `$gp` for emulated GBA r13, while MIPS static-PIE
C code uses `$gp` for the GOT and requires the callee address in `$t9`.
Both assembly-stub calls and runtime-emitted helper calls now select their PIC
sequence from `PIC` or GCC's `__PIC__`. A build audit rejects the known-bad
`$gp`-relative call, and the real-ROM QEMU smoke executes generated I/O helper
calls before accepting the image.
