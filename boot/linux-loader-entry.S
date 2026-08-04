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
