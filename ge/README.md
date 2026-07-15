# HC15xx Graphics Engine Reconstruction

The SF2000 graphics engine is a command-queue accelerator at `0x18806000`.
This directory tracks the clean source replacement for the vendor LGPL-2.0+
`libge`, with an ABI that can be shared by Linux and HCRTOS/MuFrog.

## Recovered hardware contract

- Registers: control `+0x00`, start `+0x04`, status/W1C IRQ `+0x08`, command
  queue first `+0x10`, and command queue last `+0x14`.
- SYSIO reset: `0x18800080` bit 4.
- Clock selector: `0x1880007c` bits 19:18 (198, 148, 225, or 238 MHz).
- SYSINT source: 4.
- Vendor command queue size: `0x3c000` bytes.
- Queue header: seven 32-bit control words followed by an aligned 256-entry
  CLUT. Command nodes begin after the 1,056-byte header.
- Supported accelerated primitives: RGB565/ARGB fill, blit, and stretch
  blit. Rotation and blending are encoded by optional node groups.

The HCRTOS kernel driver source survives under the read-only reference tree at
`components/kernel/source/drivers/ge/ge.c`. The userspace archive retains
symbols, relocations, source paths, line tables, and DWARF structure layouts.
Run `make reverse-ge` to reproduce disassembly, symbols, and decoded DWARF
under `build/reverse-ge/` without modifying the reference projects.

## Implementation status

Linux now has a native `hichip,hc15xx-ge` platform driver. It owns reset and
clock control, allocates the coherent vendor-sized queue, exposes `/dev/ge`,
and implements the vendor ioctl numbers for queue discovery and completion.
IRQ 4 remains masked until a client issues `HCGE_REQUEST_IRQ`; enabling it at
probe time can starve unrelated deferred probes on this interrupt controller.

QEMU already recognizes the register block, captures real command queues, and
executes the basic node forms used by stock firmware. It still needs exact
execution for every optional node group and pixel format.

The clean source tree now includes the complete command-node serializer. It
handles all twenty hardware group bits, including the four high-address base
records and the 512-byte filter group. `make test-ge-node-vendor` executes the
surviving MIPS vendor serializer under qemu-user and byte-compares its complete
172-word output with the source implementation.

The surviving public `ge_api.h` ABI is now mirrored in this directory. The
Linux backend implements context lifetime, reset, IRQ ownership, clock
selection, synchronous completion, validated copied-node submission, and
conservative acceleration capability checks without exposing kernel mappings
or registers to userspace. Fill, direct blit, and zero-origin stretch nodes are
byte-identical to the vendor library for ARGB1555, RGB565, XRGB8888, ARGB8888,
and ARGB4444; `make test-ge-formats` checks every operation and format against
the original MIPS archive under qemu-user.

The remaining API work is the optional DirectFB effects which are advertised by
the vendor header but are not used by the SF2000 native RGB565 presentation
path. They remain deliberately absent from `hcge_check_state()` until their
nodes are vendor-equivalent. Reconstruction order:

1. non-zero-origin stretch clipping and filter coefficients;
2. clip, rotation, alpha blend, color key, CLUT, mask, and matrix effects;
3. cycle-accurate execution of those optional groups in system QEMU.

The SF2000 console uses GE for its full-screen background paint and for every
render-to-scanout presentation. Idle counter frames update only their dynamic
title and anchor before the hardware BLIT, avoiding a CPU redraw of the full
320x240 surface.

No vendor object is linked into Linux. The eventual library will keep the
public `hcge_*` API so MuFrog can switch from `libge.a` to source without
changing its display backend.
