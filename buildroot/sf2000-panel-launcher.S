/* SPDX-License-Identifier: MIT */

	.set noreorder
	.text
	.align 4
	.globl panel_launcher_main
	.ent panel_launcher_main
panel_launcher_main:
	lui	$4, %hi(panel_path)
	addiu	$4, $4, %lo(panel_path)
	lui	$5, %hi(panel_argv)
	addiu	$5, $5, %lo(panel_argv)
	lui	$6, %hi(panel_envp)
	addiu	$6, $6, %lo(panel_envp)
	li	$2, 4011		/* execve */
	syscall
	nop
	li	$2, 127
	jr	$31
	nop
	.end panel_launcher_main

	.section .rodata, "a"
	.align 4
panel_path:
	.asciiz "/usr/sbin/sf2000-screen"
	.align 2
panel_argv:
	.word panel_path
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
