/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.section .bss
	.align 4
audio_stack:
	.space 65536
audio_stack_top:
	.text
	.globl _start
	.ent _start
_start:
	li	$2, 4020
	syscall
	nop
	lui	$29, %hi(audio_stack_top)
	addiu	$29, $29, %lo(audio_stack_top)
	move	$4, $0
	move	$5, $0
	jal	main
	nop
	move	$4, $2
	li	$2, 4001
	syscall
	nop
1:
	b	1b
	nop
	.end _start
