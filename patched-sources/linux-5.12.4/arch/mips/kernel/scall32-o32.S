/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995-99, 2000- 02, 06 Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2001 MIPS Technologies, Inc.
 * Copyright (C) 2004 Thiemo Seufer
 * Copyright (C) 2014 Imagination Technologies Ltd.
 */
#include <linux/errno.h>
#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/irqflags.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/isadep.h>
#include <asm/sysmips.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>
#include <asm/war.h>
#include <asm/asm-offsets.h>

	.align	5
NESTED(handle_sys, PT_SIZE, sp)
	.set	noat
	SAVE_SOME
#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510030
	sw	k1, 20(k0)
	sw	v0, 24(k0)
	mfc0	k1, CP0_EPC
	sw	k1, 28(k0)
	li	k0, __NR_O32_Linux + 162
	beq	v0, k0, sf2000_syscall_irq_on
	 nop
	li	k0, __NR_O32_Linux + 166
	beq	v0, k0, sf2000_syscall_irq_on
	 nop
	li	k0, __NR_O32_Linux + 118
	beq	v0, k0, sf2000_syscall_irq_on
	 nop
	lw	k1, PT_STATUS(sp)
	andi	k1, ST0_ERL
	bnez	k1, sf2000_erl_syscall_irq_off
	 nop
	lw	k1, PT_EPC(sp)
	li	k0, 0xe0000000
	and	k1, k0
	li	k0, 0x80000000
	beq	k1, k0, sf2000_erl_syscall_irq_off
	 nop
sf2000_syscall_irq_on:
#endif
	TRACE_IRQS_ON_RELOAD
	STI
#ifdef CONFIG_MIPS_SF2000
	b	sf2000_after_syscall_irq_state
	 nop
sf2000_erl_syscall_irq_off:
	CLI
	li	k0, 0xa1400000
	li	k1, 0x51510430
	sw	k1, 20(k0)
	lw	k1, PT_STATUS(sp)
	sw	k1, 24(k0)
	mfc0	k1, CP0_STATUS
	sw	k1, 28(k0)
sf2000_after_syscall_irq_state:
#endif
	.set	at

	lw	t1, PT_EPC(sp)		# skip syscall on return

	addiu	t1, 4			# skip to next instruction
	sw	t1, PT_EPC(sp)

#ifdef CONFIG_MIPS_SF2000
	lw	t2, PT_STATUS(sp)
	andi	t2, ST0_ERL
	beqz	t2, 1f
	 nop
	mtc0	t1, CP0_ERROREPC
	li	k0, 0xa1400000
	li	k1, 0x51510032
	sw	k1, 20(k0)
	sw	t1, 24(k0)
	mfc0	k1, CP0_ERROREPC
	sw	k1, 28(k0)
	sync
1:
#endif

	sw	a3, PT_R26(sp)		# save a3 for syscall restarting

	/*
	 * More than four arguments.  Try to deal with it by copying the
	 * stack arguments from the user stack to the kernel stack.
	 * This Sucks (TM).
	 */
	lw	t0, PT_R29(sp)		# get old user stack pointer

#ifdef CONFIG_MIPS_SF2000
	li	t5, 0xc0000000
	and	t4, t0, t5
	li	t5, 0x80000000
	beq	t4, t5, sf2000_kseg_stack_args
	nop
#endif

	/*
	 * We intentionally keep the kernel stack a little below the top of
	 * userspace so we don't have to do a slower byte accurate check here.
	 */
	lw	t5, TI_ADDR_LIMIT($28)
	addu	t4, t0, 32
	and	t5, t4
	bltz	t5, bad_stack		# -> sp is bad

	/*
	 * Ok, copy the args from the luser stack to the kernel stack.
	 */

	.set    push
	.set    noreorder
	.set	nomacro

load_a4: user_lw(t5, 16(t0))		# argument #5 from usp
load_a5: user_lw(t6, 20(t0))		# argument #6 from usp
load_a6: user_lw(t7, 24(t0))		# argument #7 from usp
load_a7: user_lw(t8, 28(t0))		# argument #8 from usp
loads_done:

#ifdef CONFIG_MIPS_SF2000
	b	sf2000_stack_args_ready
	nop
sf2000_kseg_stack_args:
	move	t5, zero
	move	t6, zero
	move	t7, zero
	move	t8, zero
sf2000_stack_args_ready:
#endif

	sw	t5, 16(sp)		# argument #5 to ksp
	sw	t6, 20(sp)		# argument #6 to ksp
	sw	t7, 24(sp)		# argument #7 to ksp
	sw	t8, 28(sp)		# argument #8 to ksp
	.set	pop

#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510031
	sw	k1, 20(k0)
	sw	v0, 24(k0)
	lw	k1, PT_R29(sp)
	sw	k1, 28(k0)
	li	k1, 0x51510032
	sw	k1, 20(k0)
	move	a0, v0
	jal	sf2000_syscall_mark
	nop
	li	k0, 0xa1400000
	li	k1, 0x51510033
	sw	k1, 20(k0)
	sw	v0, 24(k0)
	lw	v0, PT_R2(sp)
	lw	a0, PT_R4(sp)
	lw	a1, PT_R5(sp)
	lw	a2, PT_R6(sp)
	lw	a3, PT_R7(sp)
#endif

	.section __ex_table,"a"
	PTR	load_a4, bad_stack_a4
	PTR	load_a5, bad_stack_a5
	PTR	load_a6, bad_stack_a6
	PTR	load_a7, bad_stack_a7
	.previous

	lw	t0, TI_FLAGS($28)	# syscall tracing enabled?
	li	t1, _TIF_WORK_SYSCALL_ENTRY
	and	t0, t1
	bnez	t0, syscall_trace_entry # -> yes
syscall_common:
	subu	v0, v0, __NR_O32_Linux	# check syscall number
	sltiu	t0, v0, __NR_O32_Linux_syscalls
	beqz	t0, illegal_syscall

	sll	t0, v0, 2
	la	t1, sys_call_table
	addu	t1, t0
	lw	t2, (t1)		# syscall routine

#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510534
	sw	k1, 128(k0)
	sw	v0, 132(k0)
	sw	t2, 136(k0)
#endif
	beqz	t2, illegal_syscall

#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510034
	sw	k1, 20(k0)
	sw	v0, 24(k0)
	sw	t2, 28(k0)
#endif
	jalr	t2			# Do The Real Thing (TM)
	nop
#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510035
	sw	k1, 20(k0)
	sw	v0, 24(k0)
	sw	a3, 28(k0)
	li	k1, 0x51510535
	sw	k1, 140(k0)
	sw	v0, 144(k0)
	sw	a3, 148(k0)
#endif

	li	t0, -EMAXERRNO - 1	# error?
	sltu	t0, t0, v0
	sw	t0, PT_R7(sp)		# set error flag
	beqz	t0, 1f

	lw	t1, PT_R2(sp)		# syscall number
	negu	v0			# error
	sw	t1, PT_R0(sp)		# save it for syscall restarting
1:	sw	v0, PT_R2(sp)		# result

o32_syscall_exit:
	j	syscall_exit_partial

/* ------------------------------------------------------------------------ */

syscall_trace_entry:
	SAVE_STATIC
	move	a0, sp

	/*
	 * syscall number is in v0 unless we called syscall(__NR_###)
	 * where the real syscall number is in a0
	 */
	move	a1, v0
	subu	t2, v0,  __NR_O32_Linux
	bnez	t2, 1f /* __NR_syscall at offset 0 */
	lw	a1, PT_R4(sp)

1:	jal	syscall_trace_enter

	bltz	v0, 1f			# seccomp failed? Skip syscall

	RESTORE_STATIC
	lw	v0, PT_R2(sp)		# Restore syscall (maybe modified)
	lw	a0, PT_R4(sp)		# Restore argument registers
	lw	a1, PT_R5(sp)
	lw	a2, PT_R6(sp)
	lw	a3, PT_R7(sp)
	j	syscall_common

1:	j	syscall_exit

/* ------------------------------------------------------------------------ */

	/*
	 * Our open-coded access area sanity test for the stack pointer
	 * failed. We probably should handle this case a bit more drastic.
	 */
bad_stack:
#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510537
	sw	k1, 160(k0)
	lw	k1, PT_R29(sp)
	sw	k1, 164(k0)
#endif
	li	v0, EFAULT
	sw	v0, PT_R2(sp)
	li	t0, 1				# set error flag
	sw	t0, PT_R7(sp)
	j	o32_syscall_exit

bad_stack_a4:
	li	t5, 0
	b	load_a5

bad_stack_a5:
	li	t6, 0
	b	load_a6

bad_stack_a6:
	li	t7, 0
	b	load_a7

bad_stack_a7:
	li	t8, 0
	b	loads_done

	/*
	 * The system call does not exist in this kernel
	 */
illegal_syscall:
#ifdef CONFIG_MIPS_SF2000
	li	k0, 0xa1400000
	li	k1, 0x51510536
	sw	k1, 152(k0)
	sw	v0, 156(k0)
#endif
	li	v0, ENOSYS			# error
	sw	v0, PT_R2(sp)
	li	t0, 1				# set error flag
	sw	t0, PT_R7(sp)
	j	o32_syscall_exit
	END(handle_sys)

	LEAF(sys_syscall)
	subu	t0, a0, __NR_O32_Linux	# check syscall number
	sltiu	v0, t0, __NR_O32_Linux_syscalls
	beqz	t0, einval		# do not recurse
	sll	t1, t0, 2
	beqz	v0, einval
	lw	t2, sys_call_table(t1)		# syscall routine

	move	a0, a1				# shift argument registers
	move	a1, a2
	move	a2, a3
	lw	a3, 16(sp)
	lw	t4, 20(sp)
	lw	t5, 24(sp)
	lw	t6, 28(sp)
	sw	t4, 16(sp)
	sw	t5, 20(sp)
	sw	t6, 24(sp)
	jr	t2
	/* Unreached */

einval: li	v0, -ENOSYS
	jr	ra
	END(sys_syscall)

#ifdef CONFIG_MIPS_MT_FPAFF
	/*
	 * For FPU affinity scheduling on MIPS MT processors, we need to
	 * intercept sys_sched_xxxaffinity() calls until we get a proper hook
	 * in kernel/sched/core.c.  Considered only temporary we only support
	 * these hooks for the 32-bit kernel - there is no MIPS64 MT processor
	 * atm.
	 */
#define sys_sched_setaffinity	mipsmt_sys_sched_setaffinity
#define sys_sched_getaffinity	mipsmt_sys_sched_getaffinity
#endif /* CONFIG_MIPS_MT_FPAFF */

#define __SYSCALL(nr, entry) 	PTR entry
	.align	2
	.type	sys_call_table, @object
EXPORT(sys_call_table)
#include <asm/syscall_table_32_o32.h>
#undef __SYSCALL
