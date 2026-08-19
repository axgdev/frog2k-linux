# External Toolchain (frog-toolchain) Handover

**Updated:** 2026-08-19

The normal compiler for this repository is the prebuilt v1.3.2
`axgdev/frog-toolchain` release:

```
mipsel-unknown-linux-uclibc
GCC 16.2.0 / binutils 2.47 / uClibc-ng 1.0.59
```

It replaces the multi-GB Buildroot toolchain build. Buildroot remains the
userspace generator, but uses this verified prefix instead of building GCC,
binutils, and uClibc itself.

## 1. Normal setup

`make toolchain` selects the host-appropriate asset, downloads it to
`.cache/`, verifies the pinned digest, and extracts it to the disposable
`FROG_TOOLCHAIN_WORK` prefix. The supported release assets are:

| Host | Asset | SHA-256 |
|---|---|---|
| arm64 | `toolchain-uclibc-static-arm64-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz` | `ff7e9742a9b6fbbfcf58394b92c0805c4a0a7bdf21592a546fb665e24ee60fc4` |
| x86_64 | `toolchain-uclibc-static-x86_64-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz` | `8d4599a27ec2493ba56cc3940025973f86947569eeaed7e95836957649f4d88b` |

The corresponding release URL is:

```
https://github.com/axgdev/frog-toolchain/releases/download/v1.3.2/toolchain-uclibc-static-<host>-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz
```

A fresh build can keep every generated tree outside the checkout:

```sh
make \
    BUILD_DIR=/tmp/sf2000-build \
    LINUX_SRC=/tmp/sf2000-linux-7.1.4 \
    BUILDROOT_WORK=/tmp/sf2000-buildroot \
    FROG_TOOLCHAIN_WORK=/tmp/sf2000-frog-toolchain \
    FROG_TOOLCHAIN_ARCHIVE=/tmp/sf2000-cache/frog-toolchain.tar.xz \
    ROOTFS=buildroot linux-buildroot-test-asd
```

An already extracted prefix is accepted with `TOOLCHAIN_DIR`; the Makefile
verifies it and never overwrites it:

```sh
make EXTERNAL_TOOLCHAIN=1 \
    TOOLCHAIN_DIR=/path/to/mipsel-unknown-linux-uclibc \
    ROOTFS=buildroot linux-buildroot-test-asd
```

Use `linux-buildroot-test-asd` for QEMU/kernel validation. The physical
`linux-buildroot-asd` target additionally stages the SD-card core packages.
The latter requires complete frontend dependency checkouts, including the
nested repositories used by the selected cores.

The old toolchain build is an explicit compatibility path:

```sh
make EXTERNAL_TOOLCHAIN=0 toolchain
```

## 2. Build-system changes

The Makefile now:

- defaults `EXTERNAL_TOOLCHAIN=1` and the `mipsel-unknown-linux-uclibc` tuple;
- pins the frog release name, URL, host architecture, and SHA-256 digest;
- treats `make toolchain` as a download/verification/extraction step in the
  default mode, while retaining the Buildroot toolchain under
  `EXTERNAL_TOOLCHAIN=0`;
- verifies the compiler tuple, sysroot startup files, libc archive, and PIC crt
  objects before any target is compiled;
- regenerates the static-PIE gcc specs override from the selected compiler;
- uses Buildroot's generated target compiler wrapper, wrapped by host ccache,
  so Buildroot's `-G0 -fPIC -mabicalls` flags are retained;
- disables Buildroot's unused CPIO image and its internal ccache package by
  default. The top-level Makefile repacks Buildroot `target/` itself, and the
  host ccache avoids rebuilding ccache plus its host dependency chain;
- keeps ccache in `.cache/ccache` by default, allowing separate `BUILD_DIR`
  trees to reuse compile results; and
- defaults the isolated kernel make to `KERNEL_JOBS=$(JOBS)`, with an explicit
  `KERNEL_JOBS=1` escape hatch for constrained hosts.
- rejects an existing Buildroot output whose generated wrapper embeds a
  different external prefix, preventing silent cross-environment reuse; use a
  fresh `BUILDROOT_OUT` or `make buildroot-reconfigure` to regenerate it.

The generated Buildroot configuration still selects the external custom
toolchain and uses the repository's GCC 16 compatibility patch. Buildroot is
therefore retained for packages, target filesystem metadata, and the external
toolchain probe, not as a second compiler distribution.

## 3. Why the specs override is required

The frog compiler's startfile spec lists `static:crt1.o` before
`static-pie:rcrt1.o`. Buildroot packages such as BusyBox combine
`-static -static-pie`; that ordering selects the non-PIC `crt1.o`, and the MIPS
static-PIE link fails with `R_MIPS_HI16` against `_gp`.

The Makefile extracts the selected compiler's `-dumpspecs`, moves
`static-pie:rcrt1.o` ahead of `static:crt1.o`, and writes
`$(BUILD_DIR)/external-toolchain/static-pie.specs`. Buildroot receives that
file through `BR2_TARGET_LDFLAGS`. The resulting rootfs executables are
static-PIE `ET_DYN` binaries with no interpreter, matching the repository's
own userspace binaries.

## 4. Measured results in the isolated worktree

The measurements below used private `/tmp` source, toolchain, Buildroot, and
output paths on an arm64 host:

| Work | Result |
|---|---:|
| Buildroot toolchain cold start | 458.8 s elapsed before it was stopped, with ~5.7 GiB of partial output |
| frog-toolchain setup (download/check/extract path) | 4.45 s with the tested release asset |
| clean kernel, one job | 221.5 s |
| clean kernel, `KERNEL_JOBS=8` | 53.8 s |
| separate warm kernel output using shared ccache | 23.5 s |
| fresh external-toolchain Buildroot target | 26.5 s |
| rootfs repack and ELF audit after target/frontend outputs | 10.7 s |

The clean parallel kernel was about 76% faster wall-clock than the one-job
baseline. Across the clean and warm kernel runs ccache recorded 1,896
cacheable compilations and 1,095 hits; the second run was approximately 941
hits with only seven misses. The remaining warm-build time is primarily
archive/link/strip work, which ccache cannot eliminate.

The generated external toolchain occupied about 201 MiB, while the tested
Buildroot target output was about 115 MiB. The kernel source remains the large
input (about 1.7 GiB after extraction); reducing that requires a separate
careful kernel-source pruning decision and is not safely replaced by a build
flag.

Validation completed in the isolated worktree:

- clean external-toolchain kernel build;
- fresh Buildroot target and rootfs generation;
- `make ROOTFS=buildroot elf-audit`;
- ASD pack integrity check;
- the repository QEMU ASD smoke assertions, including loader handoff,
  userspace, GE readiness, screen-ready, UART IRQ, and negative fault checks.

## 5. Compatibility notes

- The frontend must be built from a complete checkout. Its compiler identity
  stamp prevents stale objects from being linked with a different toolchain;
  its private nested core repositories are still required for full SD staging.
- `linux-buildroot-test-asd` intentionally skips SD core staging, making it the
  fast kernel/rootfs/QEMU validation path.
- QEMU linux-user cannot execute these static-PIE binaries; use the project's
  system-emulation smoke targets.
- `EXTERNAL_TOOLCHAIN=0` remains available for comparison or recovery, but its
  host build is deliberately no longer on the default path.

## 6. Rollback

To use the legacy Buildroot compiler for a single build, pass
`EXTERNAL_TOOLCHAIN=0` and give it a separate `BUILDROOT_WORK`/
`BUILDROOT_OUT`. To restore the previous repository behavior permanently,
revert the toolchain-default checkpoint; no Buildroot source files are needed
by the normal frog-toolchain path.
