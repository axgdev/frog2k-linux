# SF2000 low-power states

## Implemented modes

`clocked-standby` preserves kernel time: the ST7789 sleeps, its backlight is
off, persistent logging is quiesced, the CPU uses the verified 198 MHz
selector, and keypad polling slows to 200 ms. Any key restores 918 MHz and the
existing framebuffer. Real battery current still needs physical measurement.

`shutdown` flushes logging, syncs and unmounts the SD card, blanks the display,
and halts. The physical switch can then remove power for minimum consumption;
datetime is not retained.

## Current hardware facts

The SF2000 keypad is a serial scan chain. Linux drives its load and clock pins
and samples a data pin every 20 ms in `sf2000-pad`; a key does not currently
arrive at the interrupt controller. Consequently the ordinary keys cannot wake
a CPU whose clock has stopped. A periodic always-on timer, or a separately
wired GPIO interrupt, is required for key wake.

The HC15xx also cannot use Linux's generic MIPS32 4K `WAIT` idle path. The
log91 physical experiment enabled it and stopped immediately after GMA
descriptor setup: timer interrupts did not restart execution. The port must
therefore continue to leave `cpu_wait` unset until an HC15xx-specific idle
sequence and wake source have been recovered. Enabling `CONFIG_SUSPEND` alone
does not solve this hardware contract.

The CP0 Count clocksource runs at 459 MHz at the normal CPU clock. Vendor CPU
selectors can change the CPU to 594, 396, 297, or 198 MHz, but a raw SYSIO
write would also change the rate seen by Linux timekeeping. A Linux cpufreq
driver must bracket the transition and update the MIPS clocksource and
clockevent rates. Direct register writes from userspace would make timeouts and
wall time incorrect.

The available reference schematics are generic H15/DB-B210 documents, not a
verified SF2000 production schematic. They do not establish a controllable SoC
connection to the charge indicator. Photographic evidence is consistent with
that LED being owned by the charger/power-bank IC. It must not be driven as a
guessed GPIO; if no SoC net exists, software cannot blink it with the power
switch on.

## Meaningful states

1. **Display standby** can safely turn off the backlight and panel, stop RGB
   scanout, idle the GE/audio engines, quiesce the SD card, and reduce keypad
   polling. RAM and Linux remain live and time continues, but the spinning CPU
   means this is not yet a low-current suspend.
2. **Clocked standby** additionally needs an HC15xx cpufreq driver. Running at
   198 MHz while polling the keypad slowly should reduce consumption and still
   keep time. It is the first state worth measuring on a current meter.
3. **Suspend-to-RAM** needs the HC15xx DDR self-refresh/clock-gating sequence
   plus an always-on wake timer or key IRQ. Neither contract has been recovered
   from the vendor firmware yet. This can approach minimal current while
   retaining RAM only if the DDR rail stays powered.
4. **Power off** loses RAM and time. Without an RTC, software can only restore
   approximate time from a value persisted before shutdown plus information
   supplied at the next boot.

Linux `CLOCK_REALTIME` can remain correct through states 1 and 2 because the
CP0 clock continues to count. A true suspend-to-RAM requires a counter in an
always-on clock domain; otherwise Linux cannot know how long it slept. Battery
charge strategy must therefore wait for that counter or use an external RTC.

## Implementation order and acceptance tests

- Recover and emulate the HC15xx CPU/DDR power registers and identify an
  always-on interrupt source from the original firmware.
- Add a kernel cpufreq driver with clocksource/clockevent transition handling;
  verify `CLOCK_MONOTONIC`, timer frequency, SD I/O, and GE operation at every
  selector before allowing automatic scaling.
- Add display/audio/MMC suspend and resume callbacks, preserving the proven
  panel ownership sequence on resume.
- Add a kernel keypad driver only after a real interrupt-capable input net is
  identified. Otherwise use documented periodic-wake polling and account for
  its higher power.
- Measure current in normal idle, display standby, 198 MHz standby, and
  suspend-to-RAM. A state is not called suspend until it wakes repeatedly,
  advances time correctly, preserves SD data, and survives at least 100 cycles.

This sequencing intentionally does not expose `/sys/power/state` before the
platform callbacks and wake source exist: doing so would advertise a state
that can enter but cannot wake, which was already observed with generic
`WAIT`.
