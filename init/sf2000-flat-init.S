/* SPDX-License-Identifier: MIT */

	.set noreorder
	.set noat
	.text
	.global _start
	.ent _start
_start:
	bal	1f
	 nop
1:
	move	$16, $31

	addiu	$4, $16, path_kmsg - 1b
	li	$5, 1
	move	$6, $0
	li	$2, 4005
	syscall
	 nop
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
	syscall
	 nop

	move	$4, $17
	addiu	$5, $16, msg_nommu - 1b
	li	$6, msg_nommu_end - msg_nommu
	li	$2, 4004
	syscall
	 nop

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
	syscall
	 nop

	li	$4, 0
	li	$2, 4001
	syscall
	 nop

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

