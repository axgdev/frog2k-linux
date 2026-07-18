# Performance and fidelity measurements

The physically verified reference is the log90/loglinux0025 run based on
commit `2a58ded`.  Performance changes must preserve that display and storage
contract; a faster boot with a damaged raster or filesystem is a regression.

## Reproducible commands

`make metrics-linux` reads `METRICS_LOG` (loglinux0025 by default) and reports
screen startup, panel initialization, the 256 KiB write/verify test, sampled
guest CPU use, and the new retained display milestones when present.

`make benchmark-qemu-linux` runs the same Buildroot ASD for 15 seconds and
records wall time, host user/system CPU, CPU percentage, and maximum RSS in
`build/metrics/qemu-linux.txt`.  Set `QEMU_BENCH_SECONDS` to change the window.

`make metrics-qemu-fidelity` compares 15 hardware-derived display values in a
physical retained log against the latest QEMU display run.  Override
`PHYSICAL_CONTRACT_LOG` and `QEMU_CONTRACT_LOG` to compare other runs.

## 2026-07-18 baseline and checkpoint

The log90 physical baseline measured:

- screen process spawn to main: 1512.382 ms
- guarded panel initialization: 4146.055 ms
- screen spawn to ready: 6722.799 ms
- 256 KiB storage write and verification: 80.128 ms
- sampled guest CPU busy time: 1.21%

Before this checkpoint a settled QEMU Linux run used an entire host core:
15.01 seconds user CPU in a 15.00 second window (100%), with roughly 62.6 MiB
RSS.  Live GDB inspection showed `cpu_wait == NULL`: early hardware bring-up
had disabled `check_wait()` and Linux spun in `arch_cpu_idle`.

Restoring the standard MIPS 4Kc interrupt-driven WAIT idle reduced the same
benchmark to 2.55 seconds user plus 0.20 seconds system CPU in 15.02 seconds,
or 18% host CPU.  This is an 83% reduction in host CPU percentage.  The kernel
option `nowait` remains available as a recovery path if a physical CPU revision
has a defective WAIT implementation.

QEMU originally reproduced 13 of the 15 retained display values (86.67%).  Its
panel model shifted one serial bit on every GPIO read, while the SF2000 uses an
8080-style parallel bus split across GPIO L and T and advances data on RD.  The
parallel readback implementation plus the measured B3/F2 responses now matches
15 of 15 values (100%).

The screen logger formerly opened `/dev/kmsg`, issued two writes, and closed it
for every line.  Log90 shows roughly 49--52 ms between each GPIO diagnostic;
43 such messages consumed about 2.1 seconds.  The screen now keeps one kmsg
descriptor, emits one write per record, and summarizes GPIO directions by
bank.  Retained progress records remain the detailed source of truth.  The
next physical log will quantify the resulting startup reduction.

QEMU's continuous 30 Hz scanout now tracks dirty pages in the reserved GMA
arena and skips composition and hashing when no framebuffer data changed.  It
does not skip panel timers or alter guest-visible raster state.
