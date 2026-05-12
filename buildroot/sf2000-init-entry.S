/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.align 4
	.globl _start
	.ent _start
_start:
	li	$2, 4020		/* getpid: first physical userspace marker */
	syscall
	nop
	j	sf2000_init_main
	nop
	.end _start
