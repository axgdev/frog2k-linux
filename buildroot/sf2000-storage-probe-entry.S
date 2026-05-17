/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.globl _start
	.ent _start
	.text
_start:
	lui	$8, 0x013f
	ori	$8, $8, 0x0000
	li	$9, 0x52504653		/* PROGRESS_MAGIC */
	sw	$9, 0($8)
	li	$9, 1
	sw	$9, 4($8)
	sw	$0, 8($8)
	sw	$9, 12($8)
	sw	$0, 16($8)
	sw	$0, 20($8)
	sw	$0, 24($8)
	sw	$0, 28($8)
	sw	$9, 32($8)
	li	$9, 0x11
	sw	$9, 36($8)
	li	$9, 0x656e7472		/* "entr" */
	sw	$9, 40($8)
	sw	$0, 44($8)
	li	$9, 0x72746e65		/* "entr" */
	sw	$9, 48($8)
	li	$9, 0x74732d79		/* "y-st" */
	sw	$9, 52($8)
	li	$9, 0x00747261		/* "art\0" */
	sw	$9, 56($8)
	li	$9, 1
	sw	$9, 8($8)
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
