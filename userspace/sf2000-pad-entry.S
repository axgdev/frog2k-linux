/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.section .bss
	.align 4
pad_stack:
	.space 65536
pad_stack_top:
	.text
	.globl _start
	.ent _start
_start:
	li	$2, 4020		/* getpid: establish the first syscall return */
	syscall
	nop
	lui	$29, %hi(pad_stack_top)
	addiu	$29, $29, %lo(pad_stack_top)
	move	$4, $0		/* default SF2000 profile needs no argv */
	move	$5, $0
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
