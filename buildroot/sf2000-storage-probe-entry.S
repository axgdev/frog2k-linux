/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.align 4
	.globl _start
	.ent _start
	.text
_start:
	li	$2, 4020		/* getpid: first physical userspace marker */
	syscall
	nop
	jal	main
	nop
	move	$4, $2
	li	$2, 4001		/* exit */
	syscall
	nop
1:
	b	1b
	nop
	.end _start
