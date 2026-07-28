// SPDX-License-Identifier: MIT
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROM_SIZE (4u * 1024u * 1024u)
#define BLOCK_SIZE 4096u
#define PROGRAM_OFFSET 0xc0u
#define GBA_ENTRY 0x080000c0u
#define MAX_WORDS 256u
#define MAX_LITERALS 80u

struct literal_fixup {
	unsigned instruction;
	uint32_t value;
};

struct program {
	uint32_t words[MAX_WORDS];
	struct literal_fixup literals[MAX_LITERALS];
	unsigned count;
	unsigned literal_count;
};

static void put_le32(uint8_t *dst, uint32_t value)
{
	dst[0] = value;
	dst[1] = value >> 8;
	dst[2] = value >> 16;
	dst[3] = value >> 24;
}

static unsigned emit(struct program *program, uint32_t instruction)
{
	unsigned at = program->count;

	if (at < MAX_WORDS)
		program->words[program->count++] = instruction;
	return at;
}

static unsigned emit_ldr(struct program *program, unsigned reg, uint32_t value)
{
	unsigned at = emit(program, 0xe59f0000u | (reg << 12));
	unsigned literal = program->literal_count;

	if (program->literal_count < MAX_LITERALS) {
		program->literals[program->literal_count].instruction = at;
		program->literals[program->literal_count].value = value;
		program->literal_count++;
	}
	return literal;
}

static uint32_t gba_address(unsigned instruction)
{
	return GBA_ENTRY + instruction * 4u;
}

static void patch_branch(struct program *program, unsigned instruction,
			 unsigned target, unsigned condition)
{
	int32_t displacement = (int32_t)target - (int32_t)instruction - 2;

	program->words[instruction] =
		(condition << 28) | 0x0a000000u |
		((uint32_t)displacement & 0x00ffffffu);
}

static int finish_literals(struct program *program)
{
	unsigned i;

	for (i = 0; i < program->literal_count; i++) {
		unsigned instruction = program->literals[i].instruction;
		unsigned literal = program->count;
		int32_t byte_offset =
			((int32_t)literal - (int32_t)instruction - 2) * 4;

		if (program->count >= MAX_WORDS ||
		    byte_offset < 0 || byte_offset > 4095)
			return -1;
		program->words[program->count++] = program->literals[i].value;
		program->words[instruction] |= (uint32_t)byte_offset;
	}
	return 0;
}

static void emit_call_arm(struct program *program, unsigned target_reg)
{
	emit(program, 0xe1a0e00fu); /* mov lr, pc */
	emit(program, 0xe12fff10u | target_reg); /* bx target */
}

static void emit_call_thumb(struct program *program, unsigned target_reg,
			    unsigned scratch_reg)
{
	emit(program, 0xe3800001u | (target_reg << 16) |
		     (scratch_reg << 12)); /* orr scratch, target, #1 */
	emit_call_arm(program, scratch_reg);
}

static void emit_store_word(struct program *program, unsigned value_reg,
			    unsigned base_reg, unsigned offset)
{
	emit(program, 0xe5800000u | (base_reg << 16) |
		     (value_reg << 12) | offset);
}

static void emit_stack_return(struct program *program, uint32_t stack,
			      uint32_t value, unsigned *return_literal)
{
	emit_ldr(program, 2, stack);
	emit_ldr(program, 1, value);
	emit_store_word(program, 1, 2, 0);
	*return_literal = emit_ldr(program, 1, 0);
	emit_store_word(program, 1, 2, 4);
	emit_ldr(program, 13, stack);
}

static void emit_stack_thumb_return(struct program *program, uint32_t stack,
				    uint32_t value, uint32_t trampoline)
{
	emit_ldr(program, 2, stack);
	emit_ldr(program, 1, value);
	emit_store_word(program, 1, 2, 0);
	emit_ldr(program, 1, trampoline | 1u);
	emit_store_word(program, 1, 2, 4);
	emit_ldr(program, 13, stack);
}

static void emit_common_setup(struct program *program)
{
	emit_ldr(program, 0, 0x04000000u);
	emit_ldr(program, 1, 0x00000403u);
	emit(program, 0xe1c010b0u); /* strh r1, [r0] */
}

static void emit_result_screen(struct program *program, unsigned expected)
{
	unsigned failure;
	unsigned have_color;
	unsigned fill;

	emit(program, 0xe3540000u | expected); /* cmp r4, #expected */
	failure = emit(program, 0);
	emit_ldr(program, 1, 0x000003e0u); /* green */
	have_color = emit(program, 0);
	patch_branch(program, failure, program->count, 1); /* bne failure */
	emit_ldr(program, 1, 0x0000001fu); /* red */
	patch_branch(program, have_color, program->count, 14);

	emit_ldr(program, 0, 0x06000000u);
	emit_ldr(program, 2, 38400u);
	fill = program->count;
	emit(program, 0xe0c010b2u); /* strh r1, [r0], #2 */
	emit(program, 0xe2522001u); /* subs r2, r2, #1 */
	patch_branch(program, emit(program, 0), fill, 1);
	patch_branch(program, emit(program, 0), program->count - 1, 14);
}

static void build_default(struct program *program)
{
	unsigned fill;

	emit_common_setup(program);
	emit_ldr(program, 0, 0x06000000u);
	emit(program, 0xe3a01000u); /* mov r1, #0 */
	emit_ldr(program, 2, 38400u);
	fill = program->count;
	emit(program, 0xe0c010b2u); /* strh r1, [r0], #2 */
	emit(program, 0xe2811001u); /* add r1, r1, #1 */
	emit(program, 0xe2522001u); /* subs r2, r2, #1 */
	patch_branch(program, emit(program, 0), fill, 1);
	patch_branch(program, emit(program, 0), program->count - 1, 14);
}

static void build_smc_ab(struct program *program)
{
	unsigned loop;

	emit_common_setup(program);
	emit_ldr(program, 0, 0x03000000u);
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_ldr(program, 1, 0xe12fff1eu);
	emit_store_word(program, 1, 0, 4);
	emit(program, 0xe3a04000u); /* mov r4, #0 */
	emit_call_arm(program, 0); /* prime version A: +1 */
	emit(program, 0xe3a05040u); /* mov r5, #64 */
	loop = program->count;
	emit_ldr(program, 1, 0xe2844002u);
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 0);
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 0);
	emit(program, 0xe2555001u); /* subs r5, r5, #1 */
	patch_branch(program, emit(program, 0), loop, 1);
	emit_result_screen(program, 193u);
}

static void build_smc_range(struct program *program)
{
	emit_common_setup(program);
	emit_ldr(program, 0, 0x03000100u);
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_ldr(program, 1, 0xe2844002u);
	emit_store_word(program, 1, 0, 4);
	emit_ldr(program, 1, 0xe2844004u);
	emit_store_word(program, 1, 0, 8);
	emit_ldr(program, 1, 0xe12fff1eu);
	emit_store_word(program, 1, 0, 12);
	emit(program, 0xe3a04000u); /* mov r4, #0 */
	emit_call_arm(program, 0); /* baseline +7 */

	/* An identical first-word write must not damage the active block. */
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 0); /* +7 */

	emit_ldr(program, 1, 0xe2844008u);
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 0); /* first: +14 */

	emit(program, 0xe3a01010u); /* mov r1, #0x10 */
	emit(program, 0xe5c01004u); /* strb r1, [r0, #4] */
	emit_call_arm(program, 0); /* middle byte: +28 */

	emit_ldr(program, 1, 0x00004020u);
	emit(program, 0xe1c010b8u); /* strh r1, [r0, #8] */
	emit_call_arm(program, 0); /* last halfword: +56 */
	emit_result_screen(program, 112u);
}

static void build_smc_isa(struct program *program)
{
	unsigned loop;

	emit_common_setup(program);
	emit_ldr(program, 0, 0x03000200u);
	emit_ldr(program, 1, 0xe12fff1eu);
	emit_store_word(program, 1, 0, 4);
	emit(program, 0xe3a04000u); /* mov r4, #0 */
	emit(program, 0xe3a05040u); /* mov r5, #64 */
	loop = program->count;
	emit_ldr(program, 1, 0xe2844001u); /* ARM add r4, #1 */
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 0);
	emit_ldr(program, 1, 0x47703402u); /* Thumb add r4,#2; bx lr */
	emit_store_word(program, 1, 0, 0);
	emit_call_thumb(program, 0, 2);
	emit(program, 0xe2555001u); /* subs r5, r5, #1 */
	patch_branch(program, emit(program, 0), loop, 1);
	emit_result_screen(program, 192u);
}

static void build_smc_mirror(struct program *program)
{
	emit_common_setup(program);
	emit(program, 0xe3a04000u); /* mov r4, #0 */

	emit_ldr(program, 0, 0x03000300u);
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_ldr(program, 1, 0xe12fff1eu);
	emit_store_word(program, 1, 0, 4);
	emit_call_arm(program, 0);
	emit_ldr(program, 2, 0x03008300u);
	emit_ldr(program, 1, 0xe2844002u);
	emit_store_word(program, 1, 2, 0);
	emit_call_arm(program, 0);
	emit_ldr(program, 1, 0xe2844004u);
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 2);

	emit_ldr(program, 0, 0x02000100u);
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_ldr(program, 1, 0xe12fff1eu);
	emit_store_word(program, 1, 0, 4);
	emit_call_arm(program, 0);
	emit_ldr(program, 2, 0x02040100u);
	emit_ldr(program, 1, 0xe2844002u);
	emit_store_word(program, 1, 2, 0);
	emit_call_arm(program, 0);
	emit_ldr(program, 1, 0xe2844004u);
	emit_store_word(program, 1, 0, 0);
	emit_call_arm(program, 2);
	emit_result_screen(program, 14u);
}

static void build_smc_dma(struct program *program)
{
	/*
	 * DMA3 copies one ARM opcode from fixed ROM data to IWRAM.  gpSP's
	 * conservative DMA invalidation must establish a new semantic epoch.
	 */
	emit_common_setup(program);
	emit(program, 0xe3a04000u); /* mov r4, #0 */
	emit_ldr(program, 0, 0x03000400u);
	emit_ldr(program, 1, 0xe2844001u);
	emit_store_word(program, 1, 0, 0);
	emit_ldr(program, 1, 0xe12fff1eu);
	emit_store_word(program, 1, 0, 4);
	emit_call_arm(program, 0); /* +1 */

	emit_ldr(program, 3, 0x040000d4u);
	emit_ldr(program, 1, 0x08000404u);
	emit_store_word(program, 1, 3, 0); /* DMA3 source */
	emit_store_word(program, 0, 3, 4); /* destination */
	emit(program, 0xe3a01001u);
	emit(program, 0xe1c310b8u); /* strh count, [r3, #8] */
	emit_ldr(program, 1, 0x00008400u);
	emit(program, 0xe1c310bau); /* strh control, [r3, #10] */
	emit_call_arm(program, 0); /* +2 */

	emit_ldr(program, 1, 0x08000400u);
	emit_store_word(program, 1, 3, 0);
	emit(program, 0xe3a01001u);
	emit(program, 0xe1c310b8u);
	emit_ldr(program, 1, 0x00008400u);
	emit(program, 0xe1c310bau);
	emit_call_arm(program, 0); /* +1 */
	emit_result_screen(program, 4u);
}

static void emit_code_word(struct program *program, uint32_t address,
				   uint32_t value)
{
	emit_ldr(program, 0, address);
	emit_ldr(program, 1, value);
	emit_store_word(program, 1, 0, 0);
}

static void emit_arm_stm_triplet(struct program *program, uint32_t address,
					 uint32_t first, uint32_t middle,
					 uint32_t last)
{
	emit_ldr(program, 0, address);
	emit_ldr(program, 1, first);
	emit_ldr(program, 2, middle);
	emit_ldr(program, 3, last);
	emit(program, 0xe8a0000eu); /* stmia r0!, {r1-r3} */
}

static void emit_smc_check(struct program *program, unsigned reg,
				   unsigned expected)
{
	emit(program, 0xe3500000u | (reg << 16) | expected); /* cmp reg, #expected */
	emit(program, 0x13a0a000u); /* movne r10, #0 */
}

static void build_smc_block(struct program *program)
{
	const uint32_t stack = 0x03007f00u;
	const uint32_t arm_code = 0x03000504u;
	const uint32_t thumb_stm_code = 0x03000584u;
	const uint32_t thumb_push_code = 0x03000604u;
	const uint32_t thumb_stm_writer = 0x03000700u;
	const uint32_t thumb_push_writer = 0x03000720u;
	const uint32_t thumb_return_trampoline = 0x03000740u;
	unsigned return_literal;

	/* Every case starts with a valid status and turns it red on any mismatch. */
	emit_common_setup(program);
	emit(program, 0xe3a0a001u); /* mov r10, #1 */

	/* ARM STMIA: only the middle transfer (arm_code) is translated code. */
	emit_arm_stm_triplet(program, arm_code - 4u, 0xdeadbeefu,
			    0xe8bd8010u, 0xcafebabeu);
	emit_ldr(program, 0, arm_code);
	emit_stack_return(program, stack, 1u, &return_literal);
	emit_call_arm(program, 0);
	program->literals[return_literal].value = gba_address(program->count);
	emit(program, 0xe1a08004u); /* mov r8, r4 */

	emit_arm_stm_triplet(program, arm_code - 4u, 0xdeadbeefu,
			    0xe8bd8020u, 0xcafebabeu);
	emit_stack_return(program, stack, 2u, &return_literal);
	emit(program, 0xe3a05000u); /* mov r5, #0 */
	emit_ldr(program, 0, arm_code);
	emit_call_arm(program, 0);
	program->literals[return_literal].value = gba_address(program->count);
	emit_smc_check(program, 8, 1u);
	emit_smc_check(program, 5, 2u);

	/* Thumb STMIA: the middle word contains POP {r4,pc}, then POP {r5,pc}. */
	/* POP stays in Thumb state; BX LR in this trampoline returns to ARM. */
	emit_code_word(program, thumb_return_trampoline, 0x46c04770u);
	emit_code_word(program, thumb_stm_writer, 0x4770c01eu);
	emit_code_word(program, thumb_stm_code, 0x46c0bd10u);
	emit_stack_thumb_return(program, stack, 1u, thumb_return_trampoline);
	emit_ldr(program, 0, thumb_stm_code);
	emit_call_thumb(program, 0, 7);
	emit(program, 0xe1a08004u); /* mov r8, r4 */

	emit_ldr(program, 7, thumb_stm_writer);
	emit_ldr(program, 0, thumb_stm_code - 4u);
	emit_ldr(program, 1, 0xdeadbeefu);
	emit_ldr(program, 2, 0x46c0bd20u);
	emit_ldr(program, 3, 0xcafebabeu);
	emit_call_thumb(program, 7, 6);
	emit_stack_thumb_return(program, stack, 2u, thumb_return_trampoline);
	emit(program, 0xe3a05000u); /* mov r5, #0 */
	emit_ldr(program, 0, thumb_stm_code);
	emit_call_thumb(program, 0, 7);
	emit_smc_check(program, 8, 1u);
	emit_smc_check(program, 5, 2u);

	/* Thumb PUSH: SP=C+8 makes stores land at C-4, C, and C+4. */
	emit_code_word(program, thumb_push_writer, 0x4770b40eu);
	emit_code_word(program, thumb_push_code, 0x46c0bd10u);
	emit_stack_thumb_return(program, stack, 1u, thumb_return_trampoline);
	emit_ldr(program, 0, thumb_push_code);
	emit_call_thumb(program, 0, 7);
	emit(program, 0xe1a08004u); /* mov r8, r4 */

	emit_ldr(program, 7, thumb_push_writer);
	emit_ldr(program, 1, 0xdeadbeefu);
	emit_ldr(program, 2, 0x46c0bd20u);
	emit_ldr(program, 3, 0xcafebabeu);
	emit_ldr(program, 13, thumb_push_code + 8u);
	emit_call_thumb(program, 7, 6);
	emit_stack_thumb_return(program, stack, 2u, thumb_return_trampoline);
	emit(program, 0xe3a05000u); /* mov r5, #0 */
	emit_ldr(program, 0, thumb_push_code);
	emit_call_thumb(program, 0, 7);
	emit_smc_check(program, 8, 1u);
	emit_smc_check(program, 5, 2u);

	emit(program, 0xe1a0400au); /* mov r4, r10 */
	emit_result_screen(program, 1u);
}

static int build_program(struct program *program, const char *mode)
{
	memset(program, 0, sizeof(*program));
	if (!strcmp(mode, "default"))
		build_default(program);
	else if (!strcmp(mode, "smc-ab"))
		build_smc_ab(program);
	else if (!strcmp(mode, "smc-range"))
		build_smc_range(program);
	else if (!strcmp(mode, "smc-isa"))
		build_smc_isa(program);
	else if (!strcmp(mode, "smc-mirror"))
		build_smc_mirror(program);
	else if (!strcmp(mode, "smc-dma") ||
		 !strcmp(mode, "smc-dma-epoch"))
		build_smc_dma(program);
	else if (!strcmp(mode, "smc-block"))
		build_smc_block(program);
	else
		return -1;
	return finish_literals(program);
}

int main(int argc, char **argv)
{
	uint8_t block[BLOCK_SIZE];
	struct program program;
	const char *mode = argc == 3 ? argv[2] : "default";
	FILE *output;
	unsigned offset;

	if ((argc != 2 && argc != 3) || build_program(&program, mode) < 0)
		return 2;
	output = fopen(argv[1], "wb");
	if (!output)
		return 1;
	for (offset = 0; offset < ROM_SIZE; offset += sizeof(block)) {
		memset(block, 0, sizeof(block));
		if (!offset) {
			unsigned i;

			put_le32(block, 0xea00002e); /* b 0xc0 */
			memcpy(block + 0xa0, "SF2000 GBA TEST", 15);
			memcpy(block + 0xac, "S2KT", 4);
			block[0xb2] = 0x96;
			for (i = 0; i < program.count; i++)
				put_le32(block + PROGRAM_OFFSET + i * 4,
					 program.words[i]);
			/* DMA source words used by smc-dma. */
			put_le32(block + 0x400, 0xe2844001u);
			put_le32(block + 0x404, 0xe2844002u);
		}
		if (fwrite(block, 1, sizeof(block), output) != sizeof(block)) {
			fclose(output);
			return 1;
		}
	}
	return fclose(output) != 0;
}
