/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.align 4
	.globl _start
	.ent _start
_start:
	jal	main
	nop
	move	$4, $2
	li	$2, 4001		/* exit */
	syscall
	nop
2:
	b	2b
	nop
	.end _start
