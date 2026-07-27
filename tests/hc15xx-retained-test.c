// SPDX-License-Identifier: MIT
#include "hc15xx_retained.h"

#include <assert.h>
#include <string.h>

int main(void)
{
	struct hc15xx_retained_log log;
	unsigned i;

	memset(&log, 0, sizeof(log));
	assert(hc15xx_retained_mark(&log, "invalid", 1, 2) == -1);
	log.magic = HC15XX_RETAINED_MAGIC;
	log.version = HC15XX_RETAINED_VERSION;
	assert(hc15xx_retained_mark(&log, "audio-health", 0x60, 0x12345678) == 0);
	assert(log.seq == 1);
	assert(log.write_index == 1);
	assert(log.entries[0].seq == 1);
	assert(log.entries[0].kind == 0x60);
	assert(log.entries[0].value == 0x12345678);
	assert(!strcmp(log.entries[0].name, "audio-health"));
	for (i = 1; i < HC15XX_RETAINED_ENTRIES; i++)
		assert(hc15xx_retained_mark(&log, "wrap", 0x61, i) == 0);
	assert(log.write_index == 0);
	assert(log.wrapped == 1);
	assert(hc15xx_retained_mark(&log, "newest", 0x62, 7) == 0);
	assert(log.write_index == 1);
	assert(!strcmp(log.entries[0].name, "newest"));
	return 0;
}
