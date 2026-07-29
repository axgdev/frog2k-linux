# Build layout and toolchains

This repository contains only maintained source, configuration, final patches,
and tests. Generated files never belong in the source tree:

- `build/` contains final images, test output, and short-lived packaging files.
- `.cache/` contains downloaded release archives.
- `.work/` is available for local scratch trees.
- Buildroot and Linux work trees default to `/tmp/sf2000_linux-*`.
- QEMU is a separate sibling checkout, selected with `QEMU_DIR`.
- The browser/libretro application is the sibling `sf2000_linux_frontend`;
  this repository packages its binaries but does not duplicate its sources.

All four locations are disposable. Removing them cannot remove maintained
source. The physical-device image is rebuilt with:

```sh
make linux-buildroot-asd
```

The QEMU checkout defaults to `../sf2000_qemu`. Override it when necessary:

```sh
make QEMU_DIR=/path/to/sf2000_qemu smoke-linux-buildroot-display
```

## Why Buildroot still builds a toolchain

The SF2000 has no MMU, so normal dynamically interpreted Linux ELF executables
cannot be used. Its default userspace is entirely static-PIE ELF. Optional
fixed ELF compatibility is disabled unless requested. Producing those requires
all of the following as one compatible ABI set:

- a `mipsel-*-linux-uclibc` compiler which emits MIPS32r1 soft-float PIC;
- the patched no-MMU uClibc static-PIE startup and syscall ABI;
- the SF2000 kernel's fixed/static-PIE ELF loader;
- linker and rootfs audits which reject interpreters and unsupported
  relocations.

The frog toolchain release is a bare-metal `mipsel-*-elf` toolchain using
newlib. It remains suitable for the freestanding loader and for compiling the
Linux kernel, but newlib is not the Linux/uClibc userspace ABI and the release
does not provide the Linux/uClibc runtime. Substituting it for the Buildroot
toolchain would create binaries that the no-MMU kernel cannot use as normal
Linux processes.

Buildroot is therefore retained for the root filesystem and applications. Its
output is kept outside this repository and reused across builds; it is not
rebuilt unless its configuration or toolchain inputs change.

## Patch policy

The old repository retained every experimental Linux patch and a second copy
of selected patched source files. This repository instead carries one final
Linux patch against the declared upstream version. New work should update that
patch or add a small, durable follow-up patch. Temporary probes and abandoned
assumptions should not become permanent layers.
