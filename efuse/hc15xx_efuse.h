/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_EFUSE_H
#define HC15XX_EFUSE_H

#include <stdint.h>
#include <stddef.h>

struct hc15xx_efuse_io {
	void *cookie;
	uint32_t (*read32)(void *cookie, uintptr_t address);
	void (*write32)(void *cookie, uintptr_t address, uint32_t value);
	void (*delay_us)(void *cookie, unsigned int usec);
};

struct hc15xx_efuse {
	const struct hc15xx_efuse_io *io;
	uintptr_t base;
};

#define HC15XX_EFUSE_DATA_WORDS 8
#define HC15XX_EFUSE_DATA_BYTES (HC15XX_EFUSE_DATA_WORDS * 4)

void hc15xx_efuse_init(struct hc15xx_efuse *efuse,
		       const struct hc15xx_efuse_io *io, uintptr_t base);
int hc15xx_efuse_read(struct hc15xx_efuse *efuse,
		      uint8_t data[HC15XX_EFUSE_DATA_BYTES]);
int hc15xx_efuse_write(struct hc15xx_efuse *efuse,
		       const uint8_t data[HC15XX_EFUSE_DATA_BYTES]);
int hc15xx_efuse_bits_write(struct hc15xx_efuse *efuse, unsigned int bit_offset,
			    const uint8_t *bits, unsigned int bit_count);

#endif
