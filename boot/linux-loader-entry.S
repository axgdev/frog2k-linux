/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.set mips32

	.section .text.start, "ax"
	.globl _start
	.ent _start
_start:
	li	$sp, 0x87f00000
	jal	linux_loader_main
	nop

1:
	b	1b
	nop
	.end _start
