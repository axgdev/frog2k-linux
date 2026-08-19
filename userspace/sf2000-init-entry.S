/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.align 4
	.globl _start
	.ent _start
_start:
	jal	sf2000_init_entry_mark
	nop
	jal	main
	nop
	move	$4, $2
	li	$2, 4001		/* exit */
	syscall
	nop
3:
	b	3b
	nop
	.end _start
