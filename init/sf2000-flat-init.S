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

	addiu	$29, $29, -192

	addiu	$4, $16, path_proc_src - 1b
	addiu	$5, $16, path_proc - 1b
	addiu	$6, $16, path_proc_type - 1b
	move	$7, $0
	sw	$0, 16($29)
	li	$2, 4021
	li	$8, 0xa1400000
	li	$9, 0x51510427
	sw	$9, 256($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 260($8)
	sw	$7, 264($8)

	addiu	$4, $16, path_meminfo - 1b
	move	$5, $0
	move	$6, $0
	li	$2, 4005
	li	$8, 0xa1400000
	li	$9, 0x51510428
	sw	$9, 268($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 272($8)
	sw	$7, 276($8)
	bnez	$7, 8f
	 nop
	move	$18, $2
	move	$4, $18
	addiu	$5, $29, 32
	li	$6, 96
	li	$2, 4003
	li	$8, 0xa1400000
	li	$9, 0x51510429
	sw	$9, 280($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 284($8)
	sw	$7, 288($8)
	bnez	$7, 7f
	 nop
	blez	$2, 7f
	 nop
	move	$4, $17
	addiu	$5, $29, 32
	move	$6, $2
	li	$2, 4004
	syscall
	 nop
7:
	move	$4, $18
	li	$2, 4006
	syscall
	 nop
	b	9f
	 nop
8:
	li	$8, 0xa1400000
	li	$9, 0x51510429
	sw	$9, 280($8)
	sw	$0, 284($8)
	sw	$0, 288($8)

9:
	addiu	$29, $29, -64
	addiu	$8, $16, arg_init - 1b
	sw	$8, 0($29)
	sw	$0, 4($29)
	addiu	$8, $16, env_home - 1b
	sw	$8, 16($29)
	addiu	$8, $16, env_path - 1b
	sw	$8, 20($29)
	addiu	$8, $16, env_term - 1b
	sw	$8, 24($29)
	sw	$0, 28($29)
	addiu	$4, $16, path_busybox - 1b
	move	$5, $29
	addiu	$6, $29, 16
	li	$2, 4011
	li	$8, 0xa1400000
	li	$9, 0x5151042a
	sw	$9, 292($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 296($8)
	sw	$7, 300($8)

4:
	li	$4, 0xfee1dead
	li	$5, 672274793
	li	$6, 0x01234567
	li	$2, 4088
	li	$8, 0xa1400000
	li	$9, 0x51510424
	sw	$9, 100($8)
	syscall
	 nop
	li	$8, 0xa1400000
	sw	$2, 104($8)
	sw	$7, 108($8)
	li	$9, 0x51510426
	sw	$9, 124($8)
	li	$10, 0xb8818500
	li	$9, 0
	sb	$9, 4($10)
	li	$9, 0xfffff000
	sw	$9, 0($10)
	li	$9, 0x67
	sb	$9, 4($10)

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
path_proc_src:
	.asciz	"proc"
path_proc:
	.asciz	"/proc"
path_proc_type:
	.asciz	"proc"
path_meminfo:
	.asciz	"/proc/meminfo"
path_busybox:
	.asciz	"/bin/busybox"
arg_init:
	.asciz	"init"
env_home:
	.asciz	"HOME=/"
env_path:
	.asciz	"PATH=/bin:/sbin:/usr/bin:/usr/sbin"
env_term:
	.asciz	"TERM=linux"
msg_alive:
	.ascii	"<6>sf2000_linux: flat init alive\n"
msg_alive_end:
msg_nommu:
	.ascii	"<6>sf2000_linux: nommu userspace reached\n"
msg_nommu_end:
	.end _start
