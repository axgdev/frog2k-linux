// SPDX-License-Identifier: MIT
#include "hc15xx_retained.h"

int hc15xx_retained_mark(volatile struct hc15xx_retained_log *log,
	const char *name, uint32_t kind, uint32_t value)
{
	volatile struct hc15xx_retained_entry *entry;
	uint32_t index;
	uint32_t seq;
	unsigned i;

	if (!log || !name || log->magic != HC15XX_RETAINED_MAGIC ||
			log->version != HC15XX_RETAINED_VERSION)
		return -1;
	index = log->write_index;
	if (index >= HC15XX_RETAINED_ENTRIES)
		index = 0;
	seq = log->seq + 1u;
	entry = &log->entries[index];
	entry->kind = kind;
	entry->value = value;
	entry->name_ptr = 0;
	for (i = 0; i + 1u < HC15XX_RETAINED_NAME_LEN && name[i]; i++)
		entry->name[i] = name[i];
	for (; i < HC15XX_RETAINED_NAME_LEN; i++)
		entry->name[i] = 0;
	/* Publish the entry payload before advancing the ring header. */
	__asm__ volatile ("" : : : "memory");
	entry->seq = seq;
	index++;
	if (index >= HC15XX_RETAINED_ENTRIES) {
		index = 0;
		log->wrapped = 1;
	}
	log->write_index = index;
	log->seq = seq;
	return 0;
}
