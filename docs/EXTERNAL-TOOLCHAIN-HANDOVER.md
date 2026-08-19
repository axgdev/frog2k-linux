# Direct toolchain and userspace setup

**Updated:** 2026-08-19

The supported target compiler is the prebuilt frog-toolchain release:

```text
mipsel-unknown-linux-uclibc
GCC 16.2.0 / binutils 2.47 / uClibc-ng 1.0.59
```

The repository does not build a target compiler. The full rootfs is assembled
directly from pinned upstream BusyBox and fb-test-app sources plus the
maintained SF2000 base and overlay.

## Pinned release assets

| Host | Asset | SHA-256 |
|---|---|---|
| arm64 | `toolchain-uclibc-static-arm64-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz` | `ff7e9742a9b6fbbfcf58394b92c0805c4a0a7bdf21592a546fb665e24ee60fc4` |
| x86_64 | `toolchain-uclibc-static-x86_64-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz` | `8d4599a27ec2493ba56cc3940025973f86947569eeaed7e95836957649f4d88b` |

The release URL is:

```text
https://github.com/axgdev/frog-toolchain/releases/download/v1.3.2/toolchain-uclibc-static-<host>-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz
```

BusyBox 1.38.0 and fb-test-app 1.1.1 are also downloaded with pinned SHA-256
digests in the Makefile. Linux 7.1.4 is pinned to
1c63922a119675d38e3ae0f8f6ee07f15c41a786ab9ed66563749bb8c9a08e2e.
All four source archives are cached separately from the disposable build
output.

## Clean-room workflow

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

For an existing installation, replace the toolchain work variables with:

```sh
make TOOLCHAIN_DIR=/path/to/mipsel-unknown-linux-uclibc \
	BUILD_DIR=/tmp/sf2000-build ROOTFS=full linux-full-test-asd
```

The Makefile verifies the compiler tuple, sysroot startup objects, libc, and
PIC crt objects. A mismatched prefix fails before any target compilation.

## Why the specs file exists

The frog compiler's startfile spec lists `static:crt1.o` before
`static-pie:rcrt1.o`. The direct userspace recipes use `-static-pie`; the
Makefile derives `$(BUILD_DIR)/toolchain/static-pie.specs` from the selected
compiler's `-dumpspecs` and orders the PIC startup first. The BusyBox patch
filters that final option only from internal relocatable `ld -r` links. Final
BusyBox, fb-test-app, service, and frontend binaries remain static-PIE `ET_DYN`
without an interpreter.

## Build-time results

The isolated measurements used separate `/tmp` source, toolchain, output, and
cache paths on an arm64 host:

| Work | Result |
|---|---:|
| clean kernel, one job | 221.5 s |
| clean kernel, `KERNEL_JOBS=8` | 53.8 s |
| separate warm kernel output with shared ccache | 23.5 s |
| direct rootfs rebuild after ccache/config warm-up | 7.4 s |
| unchanged direct rootfs target | 0.17 s |

The kernel remains the dominant cold compilation. Parallel kernel make cuts
the measured wall time by about 76%; ccache makes repeated service, frontend,
and kernel compilations substantially cheaper. Archive extraction, final
linking, stripping, initramfs generation, and ELF auditing are not compiler
cacheable and remain visible in the build log.

## Validation

The direct workflow is validated with:

```sh
make check
make ROOTFS=full elf-audit
make ROOTFS=full linux-full-test-asd
make smoke-linux-full-asd
```

The QEMU smoke uses system emulation because MIPS Linux-user cannot model the
SF2000 NOMMU loader and hardware contracts. The fast `linux-full-test-asd`
target avoids SD core staging; the physical `linux-full-asd` target stages the
complete SD image when the frontend checkout contains all selected core
repositories.

## Rollback point

This checkpoint is self-contained: revert it to restore the previous build
graph. No generated compiler tree or package-generator source is required for
the direct workflow, and the main checkout is not modified by the isolated
worktree experiment.
