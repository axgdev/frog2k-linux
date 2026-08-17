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

## Toolchains

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

### Prebuilt external toolchain

Building the Buildroot toolchain is the dominant cost of a first build (many
GB of `host/` output). A prebuilt crosstool-ng uClibc toolchain can replace it:

```sh
make EXTERNAL_TOOLCHAIN=1 \
	TOOLCHAIN_DIR=/path/to/extracted/toolchain-prefix \
	BUILDROOT_TARGET_TUPLE=mipsel-unknown-linux-uclibc \
	linux-asd
```

The toolchain must be a `mipsel` uClibc toolchain that provides the static-PIE
startup (`rcrt1.o`, `crti.o`, `crtn.o` in the sysroot) and the PIC crt objects
(`crtbeginS.o`/`crtendS.o`), exactly as the Buildroot toolchain does. In this
mode the toolchain stamp only verifies those pieces instead of building them,
so the kernel, the boot loader, and every Makefile-built static-PIE binary
compile against the prebuilt toolchain and no Buildroot toolchain build
happens. `LINUX_SRC`/`BUILDROOT_WORK` remain overridable if you also want to
keep the kernel and Buildroot work trees out of `/tmp`.

A crosstool-ng gcc startfile spec lists `-static` before `-static-pie`, so
`-static -static-pie` (what Buildroot packages such as BusyBox emit when
`CONFIG_STATIC=y`) selects the non-PIC `crt1.o` and the MIPS static-PIE link
fails with `R_MIPS_HI16` against `_gp`. In external-toolchain mode the build
generates a gcc specs override from the toolchain's own `-dumpspecs` (with the
`static-pie:rcrt1.o` clause moved ahead of `static:crt1.o`) into
`build/external-toolchain/static-pie.specs` and injects it into every package
link through `BR2_TARGET_LDFLAGS`, so the rootfs packages link as static-PIE
`ET_DYN` exactly like the Makefile-built binaries.

Buildroot is otherwise retained for the root filesystem and applications. Its
output is kept outside this repository and reused across builds; it is not
rebuilt unless its configuration or toolchain inputs change.

## Patch policy

The old repository retained every experimental Linux patch and a second copy
of selected patched source files. This repository instead carries one final
Linux patch against the declared upstream version. New work should update that
patch or add a small, durable follow-up patch. Temporary probes and abandoned
assumptions should not become permanent layers.
