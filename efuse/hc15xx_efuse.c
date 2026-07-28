// SPDX-License-Identifier: MIT
#include "hc15xx_efuse.h"

#define EFUSE_CTRL	0x00
#define EFUSE_STATUS	0x08
#define EFUSE_DATA0	0x10
#define EFUSE_TIMING	0x40

#define EFUSE_CTRL_READ		0x01
#define EFUSE_CTRL_WRITE	0x02
#define EFUSE_CTRL_MODE_MASK	0x03

#define EFUSE_STATUS_BUSY	0x80000000u
#define EFUSE_STATUS_ERR_MASK	0x00004c00u

#define EFUSE_TIMING_VALUE	0x16004843u

#define EFUSE_POLL_LIMIT	5000
#define EFUSE_POLL_DELAY_US	1000

static uint32_t efuse_reg_read(struct hc15xx_efuse *efuse, unsigned int offset)
{
	return efuse->io->read32(efuse->io->cookie, efuse->base + offset);
}

static void efuse_reg_write(struct hc15xx_efuse *efuse, unsigned int offset,
			    uint32_t value)
{
	efuse->io->write32(efuse->io->cookie, efuse->base + offset, value);
}

static void efuse_clear_status(struct hc15xx_efuse *efuse)
{
	uint32_t status = efuse_reg_read(efuse, EFUSE_STATUS);

	efuse_reg_write(efuse, EFUSE_STATUS, status);
}

static int efuse_poll_ready(struct hc15xx_efuse *efuse)
{
	unsigned int i;

	for (i = 0; i < EFUSE_POLL_LIMIT; i++) {
		uint32_t status = efuse_reg_read(efuse, EFUSE_STATUS);

		if (!(status & EFUSE_STATUS_BUSY))
			return 0;
		efuse->io->delay_us(efuse->io->cookie, EFUSE_POLL_DELAY_US);
	}
	return -16;
}

static int efuse_setup_read(struct hc15xx_efuse *efuse)
{
	uint32_t ctrl = efuse_reg_read(efuse, EFUSE_CTRL);

	ctrl &= ~EFUSE_CTRL_MODE_MASK;
	ctrl |= EFUSE_CTRL_READ;
	efuse_reg_write(efuse, EFUSE_CTRL, ctrl);

	return efuse_poll_ready(efuse);
}

void hc15xx_efuse_init(struct hc15xx_efuse *efuse,
		       const struct hc15xx_efuse_io *io, uintptr_t base)
{
	efuse->io = io;
	efuse->base = base;
}

int hc15xx_efuse_read(struct hc15xx_efuse *efuse,
		      uint8_t data[HC15XX_EFUSE_DATA_BYTES])
{
	int ret;
	unsigned int i;

	ret = efuse_setup_read(efuse);
	if (ret < 0)
		return ret;

	for (i = 0; i < HC15XX_EFUSE_DATA_WORDS; i++) {
		uint32_t word = efuse_reg_read(efuse,
					       EFUSE_DATA0 + i * 4);
		data[i * 4 + 0] = (uint8_t)(word);
		data[i * 4 + 1] = (uint8_t)(word >> 8);
		data[i * 4 + 2] = (uint8_t)(word >> 16);
		data[i * 4 + 3] = (uint8_t)(word >> 24);
	}

	efuse_clear_status(efuse);
	return HC15XX_EFUSE_DATA_BYTES;
}

static int efuse_raw_write(struct hc15xx_efuse *efuse,
			   const uint32_t words[HC15XX_EFUSE_DATA_WORDS])
{
	uint32_t ctrl;
	unsigned int i;
	int ret;

	ret = efuse_poll_ready(efuse);
	if (ret < 0)
		return ret;

	for (i = 0; i < HC15XX_EFUSE_DATA_WORDS; i++)
		efuse_reg_write(efuse, EFUSE_DATA0 + i * 4, words[i]);

	efuse_reg_write(efuse, EFUSE_TIMING, EFUSE_TIMING_VALUE);

	ctrl = efuse_reg_read(efuse, EFUSE_CTRL);
	ctrl &= ~EFUSE_CTRL_MODE_MASK;
	ctrl |= EFUSE_CTRL_WRITE;
	efuse_reg_write(efuse, EFUSE_CTRL, ctrl);

	for (i = 0; i < EFUSE_POLL_LIMIT; i++) {
		uint32_t status = efuse_reg_read(efuse, EFUSE_STATUS);

		if (status & EFUSE_STATUS_ERR_MASK)
			return -1;
		if (!(status & EFUSE_STATUS_BUSY))
			return 0;
		efuse->io->delay_us(efuse->io->cookie, EFUSE_POLL_DELAY_US);
	}
	return -16;
}

int hc15xx_efuse_write(struct hc15xx_efuse *efuse,
		       const uint8_t data[HC15XX_EFUSE_DATA_BYTES])
{
	uint8_t current[HC15XX_EFUSE_DATA_BYTES];
	uint32_t words[HC15XX_EFUSE_DATA_WORDS];
	unsigned int i;
	int ret;

	ret = hc15xx_efuse_read(efuse, current);
	if (ret < 0)
		return ret;

	for (i = 0; i < HC15XX_EFUSE_DATA_WORDS; i++) {
		uint32_t new_word = (uint32_t)data[i * 4] |
				    ((uint32_t)data[i * 4 + 1] << 8) |
				    ((uint32_t)data[i * 4 + 2] << 16) |
				    ((uint32_t)data[i * 4 + 3] << 24);
		uint32_t cur_word = (uint32_t)current[i * 4] |
				    ((uint32_t)current[i * 4 + 1] << 8) |
				    ((uint32_t)current[i * 4 + 2] << 16) |
				    ((uint32_t)current[i * 4 + 3] << 24);
		words[i] = new_word & ~cur_word;
	}

	for (i = 0; i < HC15XX_EFUSE_DATA_WORDS; i++) {
		if (words[i] != 0)
			return efuse_raw_write(efuse, words);
	}

	return 0;
}

int hc15xx_efuse_bits_write(struct hc15xx_efuse *efuse, unsigned int bit_offset,
			    const uint8_t *bits, unsigned int bit_count)
{
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	unsigned int i;

	if (bit_offset + bit_count > HC15XX_EFUSE_DATA_BYTES * 8)
		return -1;

	for (i = 0; i < HC15XX_EFUSE_DATA_BYTES; i++)
		data[i] = 0;

	for (i = 0; i < bit_count; i++) {
		unsigned int src_byte = i / 8;
		unsigned int src_bit = i % 8;
		unsigned int dst_bit = bit_offset + i;
		unsigned int dst_byte = dst_bit / 8;
		unsigned int dst_off = dst_bit % 8;

		if (bits[src_byte] & (1u << src_bit))
			data[dst_byte] |= (uint8_t)(1u << dst_off);
	}

	return hc15xx_efuse_write(efuse, data);
}
