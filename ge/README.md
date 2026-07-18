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
- Supported accelerated primitives: fill, blit, cropped stretch blit, pixel
  format conversion, horizontal/vertical flips, 90/180/270-degree blit
  rotation, alpha blending, A8 source masks, color modulation and standard or
  custom key operators, premultiply/demultiply, and XOR. Effects are encoded by
  the recovered compositor, mask, and key groups.

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
The driver establishes the physically proven selector 0 (198 MHz) during
probe. Selector 3 reports command completion but produced a blank physical
scanout in log85. SFCLK `0x1880007c` also holds the SDIO selector in adjacent
bits, so the display service does not retime GE while the MMC delayed-rescan
worker can be active.

QEMU recognizes the register block, walks multi-node command queues, and
executes the grouped fill, blit, conversion, flip, rotation, stretch, key, and
compositor forms in the five destination formats supported by the vendor
implementation.

The clean source tree now includes the complete command-node serializer. It
handles all twenty hardware group bits, including the four high-address base
records and the 512-byte filter group. `make test-ge-node-vendor` executes the
surviving MIPS vendor serializer under qemu-user and byte-compares its complete
172-word output with the source implementation.

The surviving public `ge_api.h` ABI is now mirrored in this directory. The
Linux backend implements context lifetime, reset, IRQ ownership, clock
selection, synchronous completion, validated copied-node submission, and
conservative acceleration capability checks without exposing kernel mappings
or registers to userspace. Heap contexts use `hcge_open()`/`hcge_close()`;
small no-MMU programs can pair `hcge_open_context()` with
`hcge_close_context()` for caller-owned storage. Fill, direct blit, and
stretch nodes are
byte-identical to the vendor library for ARGB1555, RGB565, XRGB8888, ARGB8888,
and ARGB4444. Cropped surfaces use validated physical-address views while
retaining the hardware pitch. Drawing/blitting blend factors, color alpha,
colorize, A8 source-mask alpha, all six vendor custom source/destination key
operators, premultiply/demultiply, XOR, flips, and rotations are byte-identical
to the vendor nodes. `make test-ge-formats`, `make test-ge-effects`, `make
test-ge-mask`, and `make test-ge-custom-keys` compare these paths, non-default
blend factors, mask offset modes and pitches, and key conversion in every
supported format against the original MIPS archive under qemu-user.

The remaining public-header flags are capabilities the surviving vendor header
itself marks unsupported: deinterlace, indexed translation, extended keys,
source-mask color, caller-supplied ROP/color matrices/convolution filters, and
the advanced DirectFB drawing primitives. They remain absent from
`hcge_check_state()` rather than silently producing an incorrect node. System
QEMU executes the complete supported compositor contract functionally; its GE
completion timing is intentionally functional rather than cycle-accurate.

The SF2000 console uses GE for its full-screen background paint and for every
render-to-scanout presentation. Idle counter frames update only their dynamic
title and anchor before the hardware BLIT, avoiding a CPU redraw of the full
320x240 surface.

No vendor object is linked into Linux. The eventual library will keep the
public `hcge_*` API so MuFrog can switch from `libge.a` to source without
changing its display backend.
