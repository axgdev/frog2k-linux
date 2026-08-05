/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.set mips32

	.section .text.start, "ax"
	.globl _start
	.ent _start
_start:
	/* This is deliberately the first side effect after a direct fastboot
	 * handoff.  The bootloader retains it across a quick powercycle, so a
	 * missing marker distinguishes a pre-entry fault from a loader C failure. */
	li	$t0, 0xa6820000
	li	$t1, 0x86820000
	cache	0x11, 0xc0($t1)
	sync
	li	$t1, 0x4c494e58	/* LINX */
	sw	$t1, 0xc0($t0)
	mfc0	$t1, $12
	sw	$t1, 0xc4($t0)
	sync
	/* Keep the transient loader stack below the persistent recovery range. */
	li	$sp, 0x879f0000
	li	$t0, 0xa6820000
	li	$t1, 0x86820000
	cache	0x11, 0xd0($t1)
	sync
	li	$t1, 0x4c4a414c	/* LJAL: assembly entry is about to call C */
	sw	$t1, 0xd0($t0)
	mfc0	$t1, $12
	sw	$t1, 0xd4($t0)
	sync
	jal	linux_loader_main
	nop

1:
	b	1b
	nop
	.end _start

	/* Keep an assembly-owned boundary immediately before the compiled C
	 * function.  The complete cache sweep is already performed by the high
	 * handoff stub, but re-establish the target's D/I visibility here as well:
	 * this is the first code fetch after replacing a resident NuttX image. */
	.globl linux_loader_main
	.ent linux_loader_main
linux_loader_main:
	li	$t0, 0xa6820000
	li	$t1, 0x86820000
	cache	0x11, 0x120($t1)
	sync
	li	$t1, 0x4c575250	/* LWRP: assembly C-wrapper reached */
	sw	$t1, 0x120($t0)
	mfc0	$t1, $12
	sw	$t1, 0x124($t0)
	sync
	/* Refresh the first four cache lines of the compiled C prologue and its
	 * initial trace call.  This is intentionally MIPS32r1-compatible and does
	 * not use synci or any GP-relative address. */
	lui	$t0, %hi(linux_loader_main_impl)
	addiu	$t0, $t0, %lo(linux_loader_main_impl)
	li	$t1, 4
1:
	cache	0x15, 0($t0)
	cache	0x10, 0($t0)
	addiu	$t0, $t0, 16
	addiu	$t1, $t1, -1
	bne	$t1, $zero, 1b
	nop
	sync
	li	$t0, 0xa6820000
	li	$t1, 0x4c574346	/* LWCF: wrapper cache refresh complete */
	sw	$t1, 0x128($t0)
	mfc0	$t1, $12
	sw	$t1, 0x12c($t0)
	sync
	j	linux_loader_main_impl
	nop
	.end linux_loader_main
