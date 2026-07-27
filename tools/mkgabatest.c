// SPDX-License-Identifier: MIT
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROM_SIZE (4u * 1024u * 1024u)
#define BLOCK_SIZE 4096u

static void put_le32(uint8_t *dst, uint32_t value)
{
	dst[0] = value;
	dst[1] = value >> 8;
	dst[2] = value >> 16;
	dst[3] = value >> 24;
}

int main(int argc, char **argv)
{
	uint8_t block[BLOCK_SIZE];
	FILE *output;
	unsigned offset;

	if (argc != 2)
		return 2;
	output = fopen(argv[1], "wb");
	if (!output)
		return 1;
	for (offset = 0; offset < ROM_SIZE; offset += sizeof(block)) {
		memset(block, 0, sizeof(block));
		if (!offset) {
			/*
			 * Enter at 0xc0, select GBA mode 3, and paint VRAM with a
			 * deterministic RGB555 ramp.  A self-branching ROM only
			 * produces a solid frame and therefore cannot distinguish a
			 * working GE scanout from the blank-screen regression.
			 */
			static const uint32_t program[] = {
				0xe59f0020, /* ldr  r0, =0x04000000 */
				0xe59f1020, /* ldr  r1, =0x00000403 */
				0xe1c010b0, /* strh r1, [r0] */
				0xe59f001c, /* ldr  r0, =0x06000000 */
				0xe3a01000, /* mov  r1, #0 */
				0xe59f2018, /* ldr  r2, =38400 */
				0xe0c010b2, /* strh r1, [r0], #2 */
				0xe2811001, /* add  r1, r1, #1 */
				0xe2522001, /* subs r2, r2, #1 */
				0x1afffffb, /* bne  fill */
				0x04000000,
				0x00000403,
				0x06000000,
				0x00009600,
				0xeafffffe, /* b . */
			};
			unsigned i;

			put_le32(block, 0xea00002e); /* b 0xc0 */
			memcpy(block + 0xa0, "SF2000 GBA TEST", 15);
			memcpy(block + 0xac, "S2KT", 4);
			block[0xb2] = 0x96;
			for (i = 0; i < sizeof(program) / sizeof(program[0]); i++)
				put_le32(block + 0xc0 + i * 4, program[i]);
		}
		if (fwrite(block, 1, sizeof(block), output) != sizeof(block)) {
			fclose(output);
			return 1;
		}
	}
	return fclose(output) != 0;
}
