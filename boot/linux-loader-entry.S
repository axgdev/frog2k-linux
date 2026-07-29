/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.set mips32

	.section .text.start, "ax"
	.globl _start
	.ent _start
_start:
	/* Keep the transient loader stack below the persistent recovery range. */
	li	$sp, 0x879f0000
	jal	linux_loader_main
	nop

1:
	b	1b
	nop
	.end _start
