# Agent Notes

Keep this repository small and direct.
Use lightweight build tools.
Compiling should be fast.
Don't hack things to work, do it properly.

## Editing Rules

- Prefer Makefile changes over shell scripts.
- Avoid Python, CMake, Autotools, and generated configure layers.
- Keep dependencies to a host C compiler, the MIPS cross toolchain, `make`,
  `dtc`, the SDK, fetched dependencies, and normal Unix utilities.
