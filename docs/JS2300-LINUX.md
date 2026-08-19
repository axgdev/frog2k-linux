# JS2300 on Linux

The Linux port supports the three JS2300 execution modes without linking the
HCRTOS SDK:

* Libretro mode uses `sf2000/cores/sf2000-js2300-core`. JavaScript and CHIP-8
  content under a `JAVASCRIPT`, `JS2300`, or `CHIP8` directory is routed to
  this core. The packaged `sf2000/js2300-cores/chip8.js` is the default CHIP-8
  script.
* UI mode uses `/usr/bin/sf2000-js2300`. Put `.js` or `.mjs` scripts below a
  directory named `SCRIPTS`, `APPS`, or `JS2300UI`, then select the script in
  the browser. These scripts run with JS2300 mode `extension` and have the
  RGB565 UI, text, rectangles, input, and filesystem services.
* Background mode uses the same binary without opening the framebuffer:

  ```text
  /usr/bin/sf2000-js2300 --background /mnt/sd/sf2000/background.js
  ```

  It runs with JS2300 mode `background` and is intended for polling, logging,
  and filesystem tasks. The normal boot supervisor does not start an arbitrary
  user script automatically.

The interpreter and MQuickJS are ordinary Linux static objects built from the
pinned [frog2k-javascript repository](https://github.com/axgdev/frog2k-javascript).
The frontend supplies the JS2300 host callbacks locally: process allocation
uses the JS2300 calloc heap, script/bytecode loading uses the file callback
pair, and directory enumeration uses POSIX `opendir`/`readdir`. A fresh
frontend checkout obtains the runtime with `make setup`; the JS2300 source is
independent of the emulator-core source cache and carries its own pinned public
revision.
