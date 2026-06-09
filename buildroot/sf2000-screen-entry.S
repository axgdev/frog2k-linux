/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.section .bss
	.align 4
screen_stack:
	.space 65536
screen_stack_top:
	.text
	.globl _start
	.ent _start
_start:
	li	$2, 4020		/* getpid: first physical userspace marker */
	syscall
	nop
	lui	$29, %hi(screen_stack_top)
	addiu	$29, $29, %lo(screen_stack_top)
	move	$4, $0
	move	$5, $0
	move	$6, $0
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
