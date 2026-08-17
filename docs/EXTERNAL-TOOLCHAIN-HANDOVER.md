# External Toolchain (frog-toolchain) Integration — Handover

**Date:** 2026-08-17
**Scope:** The prebuilt crosstool-ng uClibc toolchain from `axgdev/frog-toolchain`
(`mipsel-unknown-linux-uclibc`, gcc 16.2.0 / binutils 2.47 / uClibc-ng 1.0.59)
replaces the multi-GB Buildroot toolchain build, so `sf2000_linux` can be built
from a fresh checkout with ~2.5 GB of disk instead of ~10 GB.

---

## 1. How to use it

Download one of the v1.3.2 uclibc artifacts (host arch matters — arm64 or
x86_64):

```
https://github.com/axgdev/frog-toolchain/releases/download/v1.3.2/toolchain-uclibc-static-<host>-gcc16.2.0-binutils2.47-uclibc-ng1.0.59.tar.xz
```

Extract and build:

```sh
tar -xf toolchain-uclibc-static-*.tar.xz        # yields the mipsel-unknown-linux-uclibc/ prefix dir
make EXTERNAL_TOOLCHAIN=1 \
     TOOLCHAIN_DIR=/path/to/mipsel-unknown-linux-uclibc \
     linux-buildroot-asd
```

`BUILDROOT_TARGET_TUPLE` defaults to `mipsel-unknown-linux-uclibc` in external
mode (override only for a differently-named toolchain). `LINUX_SRC`,
`BUILDROOT_WORK` and `FRONTEND_PROJECT` remain overridable. For QEMU-only
testing use `linux-buildroot-test-asd` (skips SD-card core staging).

## 2. What changed

| File | Change |
|---|---|
| `Makefile` | `EXTERNAL_TOOLCHAIN=1` mode: `BUILDROOT_CC`/`PIE_SYSROOT`/`BUILDROOT_TOOLCHAIN_STAMP` switch to the prebuilt prefix; the toolchain stamp becomes a verification gate (checks `rcrt1.o`, `crti.o`, `crtn.o`, `libc.a`, `crtbeginS.o`); the Buildroot `.config` selects the custom external toolchain and injects a generated gcc specs override into `BR2_TARGET_LDFLAGS`; the `.config` rule depends on `Makefile` |
| `buildroot/patches/buildroot/0006-add-gcc16-external-toolchain.patch` | Add `BR2_TOOLCHAIN_GCC_AT_LEAST_16` / `BR2_TOOLCHAIN_EXTERNAL_GCC_16` — Buildroot 2026.05.1's custom-external choice stops at GCC 15, so its probe would reject a gcc 16.x toolchain |
| `docs/BUILD-LAYOUT.md` | Document the mode, the specs override and the disk savings |
| `docs/EXTERNAL-TOOLCHAIN-HANDOVER.md` | This document |

## 3. Why the specs override is required (verified)

Both toolchains' gcc startfile spec lists `static:crt1.o` before
`static-pie:rcrt1.o`. Buildroot packages (BusyBox) link with
`-static -static-pie` (`BR2_TARGET_LDFLAGS=-static-pie` + BusyBox
`CONFIG_STATIC=y`), so `crt1.o` (non-PIC) is selected and the MIPS static-PIE
link fails with `R_MIPS_HI16 against _gp`.

The Buildroot-built `crt1.o` happens to be PIC-compatible so the failure never
surfaced there; the crosstool-ng `crt1.o` is not, so it fails immediately.
`-static-pie` **alone** selects `rcrt1.o` and works on both.

The fix generates `build/external-toolchain/static-pie.specs` from the
toolchain's own `-dumpspecs` with the two clauses swapped, and passes
`-specs=<abs path>` through `BR2_TARGET_LDFLAGS`. Verified: BusyBox links as
`ET_DYN` static-PIE with `1 R_MIPS_NONE / 527 R_MIPS_REL32` — identical to the
Buildroot-built rootfs.

## 4. Toolchain equivalence (verified on QEMU)

The frog toolchain differs from the Buildroot one in a few ways that were all
accommodated:

| Feature | Buildroot 2026.05.1 | frog-toolchain uclibc |
|---|---|---|
| gcc | 16.1.0 | 16.2.0 |
| binutils | 2.46.1 | 2.47 |
| uClibc-ng | wchar **off** | wchar **on** (`__UCLIBC_HAS_WCHAR__`) |
| uClibc-ng SSP | off | on |
| kernel headers | 7.1.4 | 7.1.0 |
| tuple | mipsel-buildroot-linux-uclibc | mipsel-unknown-linux-uclibc |

- **wchar on** broke `mufrog_wchar_compat.h` (frontend): the shim unconditionally
  remapped `wcslen`/`wcstombs` macros, colliding with the libc's own
  declarations (`std::istream::seekg`-style `fpos<mbstate_t>` conflicts in
  libstdc++ too). Fixed in the frontend repo: the ASCII helper functions always
  exist (cores call them by name); only the macro remaps are conditional on
  `!__UCLIBC_HAS_WCHAR__` (loaded via `<features.h>`).
- **Stale-object mixing** (frontend): the frontend's core/browser rules never
  tracked the compiler, so switching toolchains linked old objects against the
  new libstdc++ (`undefined reference to std::istream::seekg(...)` — a real
  failure, hit on stella2014). Fixed in the frontend repo with a
  `TOOLCHAIN_STAMP` (records `SF2000_CC`/`SF2000_CXX` + compiler stat) that all
  cross-compiled rules depend on.
- **Headers 7.1.0 vs 7.1.4**: userspace compiles against the toolchain's
  sysroot headers; the kernel builds its own. Minor version delta, no ABI
  impact observed.

### Validation performed (this handover)

All of the following passed with `EXTERNAL_TOOLCHAIN=1` using the v1.3.2 arm64
artifact:

| Check | Result |
|---|---|
| `make linux-buildroot-test-asd` (kernel + buildroot rootfs + browser + js2300) | PASS |
| `make smoke-linux-buildroot-asd` (full QEMU boot) | PASS — all markers, no unexpected IRQ / data bus error |
| `make smoke-linux-buildroot-superfloppy-storage` | PASS |
| `make smoke-linux-buildroot-partitioned-storage` | PASS |
| All frontend libretro cores (incl. stella2014, fbalpha2012, fake08, qpsx) | PASS — rebuilt with the frog toolchain |
| `elf-audit` equivalent check on rootfs binaries | PASS — `ET_DYN`, no interpreter, `R_MIPS_NONE`/`R_MIPS_REL32` only |
| Direct toolchain equivalence (`-static-pie -fPIC` hello world) | PASS — same ELF class/relocs; both run identically under qemu (linux-user static-PIE limitation applies to both equally) |

Disk usage with the external toolchain: Buildroot work ~312 MB (no toolchain
build), kernel source 1.7 GB, toolchain 207 MB extracted, build outputs ~210 MB
→ **~2.5 GB total** instead of ~9.7 GB.

## 5. Caveats

- **First build still downloads/patches the kernel source** (`LINUX_SRC`, 1.7 GB
  extracted). The user separately plans to shrink this (e.g. drop
  `drivers/gpu/drm/amd`, ~520 MB); not part of this change.
- **Frontend repo changes are required** for the toolchain switch to be safe
  (see §4). The frontend Makefile now rebuilds everything when the compiler
  identity changes. Both repos should be updated together.
- **`js2300-ui`/mufrog cores need the private `mufrog`/`js2300` checkouts** in
  the frontend `.deps/` — still private; make them public before external
  contributors can build the full SD layout. The boot ASD itself only needs
  browser + js2300-ui + the init binaries, all of which build without the
  private repos if the frontend `.deps` are pre-populated.
- **QEMU linux-user cannot run static-PIE binaries** (both toolchains
  identically); use the project's system-emulation smoke targets, which is how
  the validation above was done.
- **Building with the Buildroot toolchain still works** (default mode
  unchanged); switching modes invalidates the frontend outputs once via the new
  toolchain stamp, then settles.

## 6. Rollback

The change is additive: the default (`EXTERNAL_TOOLCHAIN=0`) path is
byte-identical to before. Removing `EXTERNAL_TOOLCHAIN=1` from the command line
(or reverting the commit) restores the Buildroot-toolchain build.
