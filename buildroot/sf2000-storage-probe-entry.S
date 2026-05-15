/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.globl _start
	.ent _start
_start:
	move	$31, $0
	lw	$4, 0($29)		/* argc */
	addiu	$5, $29, 4		/* argv */
	sll	$6, $4, 2
	addu	$6, $6, $5
	addiu	$6, $6, 4		/* envp = argv + argc + 1 */
	li	$8, -8
	and	$29, $29, $8
	addiu	$29, $29, -32
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
