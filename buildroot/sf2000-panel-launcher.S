/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.globl _start
	.ent _start
_start:
	lui	$4, %hi(path_kmsg)
	addiu	$4, $4, %lo(path_kmsg)
	li	$5, 1
	move	$6, $0
	li	$2, 4005		/* open */
	syscall
	nop
	move	$8, $2
	bgez	$2, 1f
	nop
	lui	$4, %hi(path_console)
	addiu	$4, $4, %lo(path_console)
	li	$5, 1
	move	$6, $0
	li	$2, 4005		/* open */
	syscall
	nop
	move	$8, $2
1:
	move	$4, $8
	lui	$5, %hi(start_msg)
	addiu	$5, $5, %lo(start_msg)
	li	$6, 32
	li	$2, 4004		/* write */
	syscall
	nop
	lui	$4, %hi(panel_probe_path)
	addiu	$4, $4, %lo(panel_probe_path)
	lui	$5, %hi(panel_argv)
	addiu	$5, $5, %lo(panel_argv)
	lui	$6, %hi(panel_envp)
	addiu	$6, $6, %lo(panel_envp)
	li	$2, 4011		/* execve */
	syscall
	nop
	li	$2, 127
	li	$4, 127
	li	$7, 0
	li	$2, 4001		/* exit */
	syscall
	nop
1:
	b	1b
	nop
	.end _start

	.section .rodata, "a"
	.align 4
panel_probe_path:
	.asciiz "/usr/sbin/sf2000-panel-probe"
	.align 2
panel_argv:
	.word panel_probe_path
	.word 0
	.align 2
env_home:
	.asciiz "HOME=/"
	.align 2
env_path:
	.asciiz "PATH=/bin:/sbin:/usr/bin:/usr/sbin"
	.align 2
env_term:
	.asciiz "TERM=linux"
	.align 2
env_pad_profile:
	.asciiz "SF2000_PAD_PROFILE=sf2000"
	.align 2
env_screen:
	.asciiz "SF2000_SCREEN=0"
	.align 2
env_heartbeat:
	.asciiz "SF2000_HEARTBEAT=0"
	.align 2
env_panel_probe:
	.asciiz "SF2000_PANEL_PROBE=1"
	.align 2
panel_envp:
	.word env_home
	.word env_path
	.word env_term
	.word env_pad_profile
	.word env_screen
	.word env_heartbeat
	.word env_panel_probe
	.word 0

start_msg:
	.asciiz "<6>sf2000-panel-launcher: start\n"
path_kmsg:
	.asciiz "/dev/kmsg"
path_console:
	.asciiz "/dev/console"
