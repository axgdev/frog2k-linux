# SF2000 software boundaries

The port is deliberately split into four layers.

## Linux kernel

Linux patches contain hardware mechanisms: HC15xx boot/interrupts, clocks and
CPU frequency, storage, framebuffer, input, audio, and the GE device ABI.
Programs use standard Linux interfaces; CPU policy uses cpufreq, not MMIO.

## QEMU machine

The separate `sf2000_qemu` repository models HC15xx hardware and contains no
Linux workarounds. CPU-selector writes change QEMU's CPU reference clock so
Linux cpufreq and timekeeping transitions can be tested.

## SF2000 platform

This repository builds the kernel, compact root filesystem, platform services,
and reusable source GE library. It owns display startup, power coordination,
storage-safe shutdown, and translating the keypad scan chain into evdev.

The provisional platform ABI uses `/run/sf2000-power-state` for
`display-standby` and `/run/sf2000-power-request` for `shutdown`. START+Y enters
standby, any key wakes, START+B shuts down, and START+SELECT restarts.

## Application OS

The separate `sf2000_os` repository owns menus, the game database, standalone
emulator/game processes, and the libretro host. It consumes evdev, ALSA,
framebuffer/GE, storage, and the platform power ABI. It never accesses physical
registers or carries kernel/QEMU patches.
