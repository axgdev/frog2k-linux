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

The default compiler is the prebuilt `frog-toolchain` v1.3.2
`mipsel-unknown-linux-uclibc` release. It contains GCC 16.2.0, binutils 2.47,
uClibc-ng 1.0.59, the Linux/uClibc sysroot, and the static-PIE startup files
needed by this no-MMU port. The Makefile also uses it for the freestanding
loader and kernel, so one verified compiler prefix covers the complete build.

### Prebuilt external toolchain

Building the Buildroot toolchain is the dominant cost of a first build (many
GB of `host/` output). The prebuilt frog-toolchain is therefore the default.
`make toolchain` selects the host-appropriate v1.3.2 arm64 or x86_64 asset,
checks its pinned SHA-256 digest, and extracts it under
`FROG_TOOLCHAIN_WORK`:

```sh
make toolchain
make ROOTFS=buildroot linux-buildroot-test-asd
```

For a clean-room build with all large paths separated from the checkout:

```sh
make \
	BUILD_DIR=/tmp/sf2000-build \
	LINUX_SRC=/tmp/sf2000-linux-7.1.4 \
	BUILDROOT_WORK=/tmp/sf2000-buildroot \
	FROG_TOOLCHAIN_WORK=/tmp/sf2000-frog-toolchain \
	FROG_TOOLCHAIN_ARCHIVE=/tmp/sf2000-cache/frog-toolchain.tar.xz \
	ROOTFS=buildroot linux-buildroot-test-asd
```

An already extracted prefix can be supplied with `TOOLCHAIN_DIR`:

```sh
make EXTERNAL_TOOLCHAIN=1 \
	TOOLCHAIN_DIR=/path/to/mipsel-unknown-linux-uclibc \
	BUILDROOT_TARGET_TUPLE=mipsel-unknown-linux-uclibc \
	ROOTFS=buildroot linux-buildroot-test-asd
```

The toolchain must be a `mipsel` uClibc toolchain that provides the static-PIE
startup (`rcrt1.o`, `crti.o`, `crtn.o` in the sysroot) and the PIC crt objects
(`crtbeginS.o`/`crtendS.o`), exactly as the Buildroot toolchain does. In this
mode the toolchain stamp only verifies those pieces instead of building them,
so the kernel, the boot loader, and every Makefile-built static-PIE binary
compile against the prebuilt toolchain and no Buildroot toolchain build
happens. `BUILD_DIR`, `LINUX_SRC`, `BUILDROOT_WORK`, `BUILDROOT_OUT`,
`FROG_TOOLCHAIN_WORK`, `FROG_TOOLCHAIN_ARCHIVE`, and `FRONTEND_PROJECT` are all
overridable, which keeps generated state independent of the source checkout.
If an existing `BUILDROOT_OUT` contains a wrapper made for another external
prefix, the Makefile rejects it before compiling; use a fresh output or
`make buildroot-reconfigure` to clean and regenerate that disposable tree.

A crosstool-ng gcc startfile spec lists `-static` before `-static-pie`, so
`-static -static-pie` (what Buildroot packages such as BusyBox emit when
`CONFIG_STATIC=y`) selects the non-PIC `crt1.o` and the MIPS static-PIE link
fails with `R_MIPS_HI16` against `_gp`. In external-toolchain mode the build
generates a gcc specs override from the toolchain's own `-dumpspecs` (with the
`static-pie:rcrt1.o` clause moved ahead of `static:crt1.o`) into
`build/external-toolchain/static-pie.specs` and injects it into every package
link through `BR2_TARGET_LDFLAGS`, so the rootfs packages link as static-PIE
`ET_DYN` exactly like the Makefile-built binaries.

Buildroot is otherwise retained for the root filesystem and applications. The
external-toolchain configuration disables Buildroot's unused CPIO image and
its internal ccache package; the top-level Makefile repacks `target/` into the
kernel initramfs and wraps Buildroot's generated target compiler with the host
ccache. This avoids compiling a second ccache and its host dependency chain
while preserving Buildroot's target compiler flags. Set
`BUILDROOT_INTERNAL_CCACHE=1` only when that separate Buildroot package is
specifically required.

The default ccache directory is `.cache/ccache`, outside disposable
`BUILD_DIR` trees. Set `CCACHE_DIR` to share it between worktrees or
`USE_CCACHE=0` to disable the wrapper. `JOBS` and `KERNEL_JOBS` default to the
host CPU count; `KERNEL_JOBS=1` remains available for constrained machines.

The old Buildroot-toolchain path is intentionally still available for
comparison or recovery, but it must be requested explicitly:

```sh
make EXTERNAL_TOOLCHAIN=0 toolchain
```

Its multi-GB host build is not part of the normal workflow.

## Patch policy

The old repository retained every experimental Linux patch and a second copy
of selected patched source files. This repository instead carries one final
Linux patch against the declared upstream version. New work should update that
patch or add a small, durable follow-up patch. Temporary probes and abandoned
assumptions should not become permanent layers.
