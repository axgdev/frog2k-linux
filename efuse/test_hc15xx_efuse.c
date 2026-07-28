// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hc15xx_efuse.h"

#define REG_COUNT 32

struct fake {
	uint32_t regs[REG_COUNT];
	unsigned int delay_count;
	struct hc15xx_efuse_io io;
};

static uint32_t fake_read32(void *cookie, uintptr_t address)
{
	struct fake *f = cookie;
	unsigned int offset = (unsigned int)(address & 0xff);

	assert(offset / 4 < REG_COUNT);
	return f->regs[offset / 4];
}

static void fake_write32(void *cookie, uintptr_t address, uint32_t value)
{
	struct fake *f = cookie;
	unsigned int offset = (unsigned int)(address & 0xff);

	assert(offset / 4 < REG_COUNT);
	f->regs[offset / 4] = value;
}

static void fake_delay_us(void *cookie, unsigned int usec)
{
	struct fake *f = cookie;

	(void)usec;
	f->delay_count++;
}

static void setup_efuse(struct hc15xx_efuse *efuse, struct fake *f)
{
	memset(f, 0, sizeof(*f));
	f->io.cookie = f;
	f->io.read32 = fake_read32;
	f->io.write32 = fake_write32;
	f->io.delay_us = fake_delay_us;
	hc15xx_efuse_init(efuse, &f->io, 0x18818e00);
}

static void test_read(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	int ret;
	unsigned int i;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0;
	for (i = 0; i < 8; i++)
		f.regs[(0x10 + i * 4) / 4] = 0x11223344u + i;

	ret = hc15xx_efuse_read(&efuse, data);
	assert(ret == HC15XX_EFUSE_DATA_BYTES);

	assert(f.regs[0x00 / 4] == 0x01);

	for (i = 0; i < 8; i++) {
		uint32_t expected = 0x11223344u + i;
		uint32_t got = (uint32_t)data[i * 4] |
			       ((uint32_t)data[i * 4 + 1] << 8) |
			       ((uint32_t)data[i * 4 + 2] << 16) |
			       ((uint32_t)data[i * 4 + 3] << 24);
		assert(got == expected);
	}

	printf("test_read: PASS\n");
}

static void test_read_busy_timeout(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	int ret;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0x80000000u;

	ret = hc15xx_efuse_read(&efuse, data);
	assert(ret == -16);
	assert(f.delay_count == 5000);

	printf("test_read_busy_timeout: PASS\n");
}

static void test_write_fresh(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	int ret;
	unsigned int i;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0;
	for (i = 0; i < 8; i++)
		f.regs[(0x10 + i * 4) / 4] = 0;

	memset(data, 0, sizeof(data));
	data[0] = 0xAB;
	data[1] = 0xCD;

	ret = hc15xx_efuse_write(&efuse, data);
	assert(ret == 0);

	assert(f.regs[0x40 / 4] == 0x16004843u);
	assert(f.regs[0x10 / 4] == 0x0000CDABu);

	printf("test_write_fresh: PASS\n");
}

static void test_write_otp_mask(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	int ret;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0;
	f.regs[0x10 / 4] = 0x0000FF00u;

	memset(data, 0, sizeof(data));
	data[0] = 0xFF;
	data[1] = 0xFF;

	ret = hc15xx_efuse_write(&efuse, data);
	assert(ret == 0);

	assert(f.regs[0x10 / 4] == 0x000000FFu);

	printf("test_write_otp_mask: PASS\n");
}

static void test_write_error(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	int ret;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0x00004000u;

	memset(data, 0, sizeof(data));
	data[0] = 0x01;

	ret = hc15xx_efuse_write(&efuse, data);
	assert(ret == -1);

	printf("test_write_error: PASS\n");
}

static void test_write_no_change(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t data[HC15XX_EFUSE_DATA_BYTES];
	int ret;
	unsigned int i;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0;
	for (i = 0; i < 8; i++)
		f.regs[(0x10 + i * 4) / 4] = 0xFFFFFFFFu;

	memset(data, 0xFF, sizeof(data));

	ret = hc15xx_efuse_write(&efuse, data);
	assert(ret == 0);

	assert(f.regs[0x40 / 4] == 0);

	printf("test_write_no_change: PASS\n");
}

static void test_bits_write(void)
{
	struct fake f;
	struct hc15xx_efuse efuse;
	uint8_t bits[1];
	int ret;

	setup_efuse(&efuse, &f);
	f.regs[0x08 / 4] = 0;

	bits[0] = 0x05;

	ret = hc15xx_efuse_bits_write(&efuse, 10, bits, 3);
	assert(ret == 0);

	assert(f.regs[0x10 / 4] == (0x05u << 10));

	printf("test_bits_write: PASS\n");
}

int main(void)
{
	test_read();
	test_read_busy_timeout();
	test_write_fresh();
	test_write_otp_mask();
	test_write_error();
	test_write_no_change();
	test_bits_write();

	printf("efuse: all tests passed\n");
	return 0;
}
