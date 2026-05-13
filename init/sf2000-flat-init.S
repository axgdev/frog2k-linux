/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.global _start
	.ent _start
_start:
	li	$8, 0xa1400000
	li	$9, 0x51510410
	sw	$9, 0($8)
	sw	$29, 4($8)
	sw	$31, 8($8)
	mfc0	$9, $12
	sw	$9, 12($8)
	sync

	li	$9, 0x51510420
	sw	$9, 64($8)

	bal	1f
	 nop
1:
	move	$16, $31

	li	$8, 0xa1400000
	li	$9, 0x51510421
	sw	$9, 64($8)
	addiu	$4, $16, path_kmsg - 1b
	li	$5, 1
	move	$6, $0
	li	$2, 4005
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 68($8)
	sw	$7, 72($8)

	bnez	$7, 2f
	 nop
	move	$17, $2
	b	3f
	 nop
2:
	li	$17, 1

3:
	move	$4, $17
	addiu	$5, $16, msg_alive - 1b
	li	$6, msg_alive_end - msg_alive
	li	$2, 4004
	li	$8, 0xa1400000
	li	$9, 0x51510422
	sw	$9, 76($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 80($8)
	sw	$7, 84($8)

	move	$4, $17
	addiu	$5, $16, msg_nommu - 1b
	li	$6, msg_nommu_end - msg_nommu
	li	$2, 4004
	li	$8, 0xa1400000
	li	$9, 0x51510423
	sw	$9, 88($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 92($8)
	sw	$7, 96($8)

	slti	$8, $17, 3
	bnez	$8, 4f
	 nop
	move	$4, $17
	li	$2, 4006
	syscall
	 nop

4:
	li	$4, 0xfee1dead
	li	$5, 672274793
	li	$6, 0x4321fedc
	li	$2, 4088
	li	$8, 0xa1400000
	li	$9, 0x51510424
	sw	$9, 100($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 104($8)
	sw	$7, 108($8)

	li	$4, 0
	li	$2, 4001
	li	$8, 0xa1400000
	li	$9, 0x51510425
	sw	$9, 112($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 116($8)
	sw	$7, 120($8)

5:
	b	5b
	 nop

path_kmsg:
	.asciz	"/dev/kmsg"
msg_alive:
	.ascii	"<6>sf2000_linux: flat init alive\n"
msg_alive_end:
msg_nommu:
	.ascii	"<6>sf2000_linux: nommu userspace reached\n"
msg_nommu_end:
	.end _start
