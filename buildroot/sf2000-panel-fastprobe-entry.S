/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.align 4
	.globl _start
	.ent _start
_start:
	li	$4, 1
	lui	$5, %hi(entry_banner)
	addiu	$5, $5, %lo(entry_banner)
	li	$6, entry_banner_end - entry_banner
	li	$2, 4004		/* write */
	syscall
	nop
	li	$4, 77
	li	$2, 4001		/* exit */
	syscall
	nop
1:
	b	1b
	nop
	.end _start
	.align 2
	.section .rodata
entry_banner:
	.ascii	"sf2000_panel_fastprobe: entry stdout\n"
entry_banner_end:
