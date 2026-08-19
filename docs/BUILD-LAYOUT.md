# Build layout and direct userspace

This repository contains maintained source, configuration, final patches, and
tests. Generated files stay outside the source tree when a disposable layout
is requested:

- `BUILD_DIR` contains kernel objects, direct userspace objects, images, and
  test output.
- `.cache` contains verified download archives and the shared ccache by
  default.
- `LINUX_SRC`, `BUSYBOX_WORK`, and `FB_TEST_APP_WORK` contain extracted source
  trees and are disposable.
- `FROG_TOOLCHAIN_WORK` contains the extracted prebuilt target toolchain.
- `QEMU_DIR` and `FRONTEND_PROJECT` point to sibling checkouts.

The normal full image is assembled directly from upstream BusyBox, the
upstream `fb-test-app` utilities, the curated rootfs base, the repository
overlay, and the small in-tree SF2000 services. No package generator or target
compiler build is downloaded, built, or used.

## Clean setup

The host needs a C compiler, GNU make, normal Unix utilities, `curl`, `tar`,
the decompressor for the pinned archives, `dtc`, and optionally `ccache`.
The target compiler is downloaded as the prebuilt frog-toolchain artifact:

```sh
make toolchain
```

To keep every generated location separate from this checkout:

```sh
make \
	BUILD_DIR=/tmp/sf2000-build \
	LINUX_SRC=/tmp/sf2000-linux-7.1.4 \
	LINUX_ARCHIVE=/tmp/sf2000-cache/linux-7.1.4.tar.xz \
	BUSYBOX_WORK=/tmp/sf2000-busybox-1.38.0 \
	BUSYBOX_ARCHIVE=/tmp/sf2000-cache/busybox-1.38.0.tar.bz2 \
	FB_TEST_APP_WORK=/tmp/sf2000-fb-test-app-1.1.1 \
	FB_TEST_APP_ARCHIVE=/tmp/sf2000-cache/fb-test-app-1.1.1.tar.gz \
	FROG_TOOLCHAIN_WORK=/tmp/sf2000-frog-toolchain \
	FROG_TOOLCHAIN_ARCHIVE=/tmp/sf2000-cache/frog-toolchain.tar.xz \
	QEMU_DIR=/tmp/sf2000-qemu-source \
	QEMU_WORK=/tmp/sf2000-qemu-build \
	FRONTEND_PROJECT=/tmp/sf2000-linux-frontend \
	CCACHE_DIR=/tmp/sf2000-cache/ccache \
	toolchain

make -j"$(nproc)" \
	BUILD_DIR=/tmp/sf2000-build \
	LINUX_SRC=/tmp/sf2000-linux-7.1.4 \
	LINUX_ARCHIVE=/tmp/sf2000-cache/linux-7.1.4.tar.xz \
	BUSYBOX_WORK=/tmp/sf2000-busybox-1.38.0 \
	BUSYBOX_ARCHIVE=/tmp/sf2000-cache/busybox-1.38.0.tar.bz2 \
	FB_TEST_APP_WORK=/tmp/sf2000-fb-test-app-1.1.1 \
	FB_TEST_APP_ARCHIVE=/tmp/sf2000-cache/fb-test-app-1.1.1.tar.gz \
	FROG_TOOLCHAIN_WORK=/tmp/sf2000-frog-toolchain \
	FROG_TOOLCHAIN_ARCHIVE=/tmp/sf2000-cache/frog-toolchain.tar.xz \
	QEMU_DIR=/tmp/sf2000-qemu-source \
	QEMU_WORK=/tmp/sf2000-qemu-build \
	FRONTEND_PROJECT=/tmp/sf2000-linux-frontend \
	CCACHE_DIR=/tmp/sf2000-cache/ccache \
	ROOTFS=full linux-full-test-asd
```

`make toolchain` selects the host-appropriate frog-toolchain v1.3.2 release,
verifies its pinned SHA-256 digest, and extracts it under
`FROG_TOOLCHAIN_WORK`. An already extracted prefix can be supplied with
`TOOLCHAIN_DIR=/path/to/mipsel-unknown-linux-uclibc`; it is verified and never
overwritten.

The full rootfs can be built independently with `make ROOTFS=full
direct-rootfs` or `make full`. `make rootfs` remains the small early-init
ramfs target. `make elf-audit` checks every executable in the full image for
static-PIE `ET_DYN`, no interpreter, and the supported MIPS NOMMU relocation
set.

## Toolchain and link contract

The SF2000 uses MIPS32 little-endian soft-float without an MMU. The pinned
`mipsel-unknown-linux-uclibc` frog-toolchain contains GCC 16.2.0, binutils
2.47, uClibc-ng 1.0.59, and the static-PIE startup objects. The Makefile
verifies the tuple, `rcrt1.o`, `crti.o`, `crtn.o`, libc, and the PIC GCC crt
objects before compiling.

The toolchain's startfile specification lists the non-PIC static startup before
the static-PIE startup. The Makefile derives a small specs override from
`-dumpspecs` so direct links consistently select `rcrt1.o`. The BusyBox patch
keeps the final `-static-pie` option out of internal `ld -r` aggregation links;
the final executable still receives it and is audited as `ET_DYN`.

## Build-time behavior

`JOBS` controls direct userspace and nested frontend builds. `KERNEL_JOBS`
controls the isolated kernel make and defaults to `JOBS`. ccache is enabled by
default when installed and lives in `CCACHE_DIR`, outside `BUILD_DIR`, so
separate output directories can share compiled objects. Set `USE_CCACHE=0`
when comparing uncached timings.

The direct rootfs uses persistent extraction and patch stamps. A warm build
does not unpack BusyBox or fb-test-app again, and an unchanged full image is
up to date without relinking. Source changes rebuild only their dependent
service; ccache covers repeated compiler invocations across output trees. The
initramfs generator uses a fixed epoch by default, so checkout mtimes do not
silently change the packaged rootfs; override `INITRAMFS_DATE` and
`INITRAMFS_EPOCH` together when a different image timestamp is required.

The full artifact targets are named `linux-full-*` and `smoke-linux-full-*`.
They replace the old package-generator-specific target names while keeping the
rootfs choice explicit through `ROOTFS=full`.

## Patch policy

The repository carries one final Linux patch series against the declared
upstream version and the small BusyBox compatibility patch series required by
the pinned release. Temporary probes and extracted source trees stay outside
the repository. Changes to the kernel loader, binary format, or hardware
contract must follow `docs/CONTRIBUTING-HARDWARE-PORTS.md`.
