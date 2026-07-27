// SPDX-License-Identifier: MIT
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROM_SIZE (4u * 1024u * 1024u)
#define BLOCK_SIZE 4096u

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
			/* ARM B . plus the minimum gpSP header signature. */
			block[0] = 0xfe;
			block[1] = 0xff;
			block[2] = 0xff;
			block[3] = 0xea;
			memcpy(block + 0xa0, "SF2000 GBA TEST", 15);
			memcpy(block + 0xac, "S2KT", 4);
			block[0xb2] = 0x96;
		}
		if (fwrite(block, 1, sizeof(block), output) != sizeof(block)) {
			fclose(output);
			return 1;
		}
	}
	return fclose(output) != 0;
}
