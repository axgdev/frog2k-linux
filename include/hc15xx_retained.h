// SPDX-License-Identifier: MIT
#ifndef HC15XX_RETAINED_H
#define HC15XX_RETAINED_H

#include <stdint.h>

#define HC15XX_RETAINED_PHYS 0x07a00000u
#define HC15XX_RETAINED_UNCACHED 0xa7a00000u
#define HC15XX_RETAINED_MAGIC 0x52504653u
#define HC15XX_RETAINED_VERSION 1u
#define HC15XX_RETAINED_ENTRIES 1024u
#define HC15XX_RETAINED_NAME_LEN 32u

struct hc15xx_retained_entry {
	uint32_t seq;
	uint32_t kind;
	uint32_t value;
	uint32_t name_ptr;
	char name[HC15XX_RETAINED_NAME_LEN];
};

struct hc15xx_retained_log {
	uint32_t magic;
	uint32_t version;
	uint32_t seq;
	uint32_t write_index;
	uint32_t wrapped;
	uint32_t reserved[3];
	struct hc15xx_retained_entry entries[HC15XX_RETAINED_ENTRIES];
};

int hc15xx_retained_mark(volatile struct hc15xx_retained_log *log,
	const char *name, uint32_t kind, uint32_t value);

#endif
