#!/bin/sh
# kernel-slim.sh — prune the Linux source tree to what the sf2000 port builds.
#
# The full linux-next tarball is ~1.8 GB but this port only compiles a small
# fraction of it (arch/mips, a handful of drivers, sound core + sf2000-pcm,
# and the VFS/core fs, mm, kernel, lib).  Everything else is pruned here so a
# fresh checkout extracts and builds quickly and uses far less disk.
#
# Rule: Kconfig and Makefile files are kept *everywhere* — the kconfig parser
# descends the whole tree (missing sourced files are fatal) and kbuild
# descends through every Makefile.  Only C sources/headers of subsystems this
# port never enables are removed.
#
# The keep-lists below are derived from the built .o files of the reference
# build (build/linux-sf2000-full), i.e. what the sf2000 defconfig +
# patches actually compile.  If a new driver is enabled later, add it here.
set -eu

SRC=${1:?usage: kernel-slim.sh <linux-src-dir>}
cd "$SRC"

# Keep only Kconfig*/Makefile*/Kbuild files under $1, then drop empty dirs.
prune() {
    find "$1" -type f ! -name 'Kconfig*' ! -name 'Makefile*' ! -name 'Kbuild' -delete
}

# --- top-level dirs kept in full (recursively) -------------------------
KEEP_FULL="include scripts init kernel mm lib block usr security \
           arch/mips sound/core sound/drivers"

# --- fs subdirs that compile (fs root *.c files are kept as-is) --------
FS_KEEP="devpts exfat exportfs fat iomap kernfs nls nullfs proc ramfs sysfs"

# --- driver top-level dirs that compile --------------------------------
DRV_FULL="auxdisplay base bus char clk input irqchip leds mfd misc mmc of \
          pci pinctrl power pps reset rtc tty video"
# --- driver subdirs (under the dirs above) that compile -----------------
DRV_SUB="base/firmware_loader base/power base/regmap tty/serial \
         mmc/core mmc/host video/fbdev input/misc power/reset misc/eeprom"

# --- arch: keep only mips (arch/Kconfig stays) --------------------------
# scripts/checksyscalls.sh diffs the arch's syscall set against the i386
# reference table under arch/x86, so save it before pruning that dir.
SYSCALL_REF=arch/x86/entry/syscalls/syscall_32.tbl
if [ -f "$SYSCALL_REF" ]; then
    cp "$SYSCALL_REF" .syscall_32.tbl.save
fi
for d in arch/*/; do
    [ -d "$d" ] || continue
    case "$d" in arch/mips/) ;; *) prune "${d%/}" ;; esac
done
if [ -f .syscall_32.tbl.save ]; then
    mkdir -p arch/x86/entry/syscalls
    mv .syscall_32.tbl.save "$SYSCALL_REF"
fi

# --- drivers: prune dirs never built, then subdirs never built -----------
# USB is not supported on this port: the config disables USB_SUPPORT/USB and
# the Hichip musb glue was removed from the patch series.  drivers/usb is not
# in DRV_FULL, so it is reduced to its Kconfig/Makefile shells here — the
# files must stay because drivers/Kconfig sources drivers/usb/Kconfig
# unconditionally, but every line of USB driver code is dropped.
for d in drivers/*/; do
    [ -d "$d" ] || continue
    b=$(basename "$d")
    case " $DRV_FULL " in *" $b "*) ;; *) prune "${d%/}" ;; esac
done
for d in $DRV_FULL; do
    for sub in drivers/$d/*/; do
        [ -d "$sub" ] || continue
        s=${sub#drivers/}
        s=${s%/}
        case " $DRV_SUB " in *" $s "*) ;; *) prune "${sub%/}" ;; esac
    done
done

# --- fs: prune subdirs never built ---------------------------------------
for d in fs/*/; do
    [ -d "$d" ] || continue
    b=$(basename "$d")
    case " $FS_KEEP " in *" $b "*) ;; *) prune "${d%/}" ;; esac
done

# --- sound: keep core + drivers, prune the rest --------------------------
for d in sound/*/; do
    [ -d "$d" ] || continue
    case "$d" in sound/core/|sound/drivers/) ;; *) prune "${d%/}" ;; esac
done

# kernel/sched/core.c includes ../../io_uring/io-wq.h unconditionally, so
# keep that one header before pruning the rest of io_uring.
if [ -f io_uring/io-wq.h ]; then
    cp io_uring/io-wq.h .io-wq.h.save
fi

# --- top-level dirs reduced to Kconfig/Makefile only ---------------------
for d in net crypto samples rust virt ipc io_uring certs Documentation; do
    [ -d "$d" ] && prune "$d"
done
if [ -f .io-wq.h.save ]; then
    mkdir -p io_uring
    mv .io-wq.h.save io_uring/io-wq.h
fi

# tools/: only the host build tools' headers are needed.  scripts/elf-parse.h
# includes <tools/be_byteshift.h> (via -I$(srctree)/tools/include) and
# sorttable.o is built with -I$(srctree)/tools/arch/$(SRCARCH)/include
# (CONFIG_BUILDTIME_TABLE_SORT=y).  Keep those two header trees, drop the
# rest (perf, selftests, lib, etc. are not part of the kernel build).
for d in tools/*/; do
    [ -d "$d" ] || continue
    case "$d" in tools/include/|tools/arch/) ;; *) rm -rf "$d" ;; esac
done
# tools/arch: keep only mips (the SRCARCH this port builds).
for d in tools/arch/*/; do
    [ -d "$d" ] || continue
    case "$d" in tools/arch/mips/) ;; *) rm -rf "$d" ;; esac
done

# --- remove empty dirs left by pruning -----------------------------------
find . -type d -empty -delete
