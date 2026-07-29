# Agent Notes

Keep this repository small and direct.
Use lightweight build tools.
Compiling should be fast.
Don't hack things to work, do it properly.

## Editing Rules

- Prefer Makefile changes over shell scripts.
- Avoid Python, CMake, Autotools, and generated configure layers.
- Keep dependencies to a host C compiler, the MIPS cross toolchain, `make`,
  `dtc`, the SDK, fetched dependencies, and normal Unix utilities.
- Do not hide compiler, linker, patch, QEMU, or test failures. In particular,
  never use `--noinhibit-exec` or `|| true` on correctness-critical steps.

## Checkpoints

- Commit each coherent checkpoint with a descriptive title and a detailed
  body that explains the evidence, risk, verification, and rollback point.
- Run `make check` before committing source-only changes.
- Run `make ROOTFS=buildroot elf-audit` for userspace changes.
- Run the relevant QEMU smoke target before producing an ASD.
- Treat physical-device tests as confirmation, not the first test environment.
- Read `docs/CONTRIBUTING-HARDWARE-PORTS.md` before changing a binary format,
  kernel loader, hardware register contract, or vendor-library replacement.
