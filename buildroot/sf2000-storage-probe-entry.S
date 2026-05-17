/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.globl _start
	.ent _start
	.rdata
1:
	.ascii "/dev/console\0"
2:
	.ascii "sf2000_storage_probe: _start\n"
3:
	.text
_start:
	li	$2, 4005		/* open */
	lui	$4, %hi(1b)
	addiu	$4, $4, %lo(1b)
	li	$5, 1			/* O_WRONLY */
	move	$6, $0
	syscall
	nop
	bltz	$2, 1f
	nop
	move	$4, $2
	li	$2, 4004		/* write */
	lui	$5, %hi(2b)
	addiu	$5, $5, %lo(2b)
	li	$6, 29
	syscall
	nop
1:
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
