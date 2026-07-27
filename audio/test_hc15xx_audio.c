/* SPDX-License-Identifier: MIT */
#include "hc15xx_audio.h"

#include <assert.h>
#include <string.h>

struct fake {
	uint32_t regs[0x300 / 4];
	unsigned int delay;
	unsigned int barriers;
};

static uint32_t fake_read(void *cookie, uintptr_t address)
{
	struct fake *fake = cookie;

	return fake->regs[address / 4];
}

static void fake_write(void *cookie, uintptr_t address, uint32_t value)
{
	struct fake *fake = cookie;

	fake->regs[address / 4] = value;
}

static void fake_delay(void *cookie, unsigned int usec)
{
	((struct fake *)cookie)->delay += usec;
}

static void fake_barrier(void *cookie)
{
	((struct fake *)cookie)->barriers++;
}

int main(void)
{
	struct fake fake;
	struct hc15xx_audio audio;
	struct hc15xx_audio_io io = {
		.cookie = &fake,
		.read32 = fake_read,
		.write32 = fake_write,
		.delay_us = fake_delay,
		.dma_barrier = fake_barrier,
	};

	memset(&fake, 0, sizeof(fake));
	hc15xx_audio_init(&audio, &io, 0x000, 0x100, 0x180);
	hc15xx_audio_clock_reset(&audio);
	assert(fake.delay == 1000);
	assert(!(fake.regs[(0x180 + 0x80) / 4] & (1u << 5)));
	hc15xx_audio_configure_output(&audio);
	assert(fake.regs[(0x180 + 0x148) / 4] == 0x0c000120);
	assert(hc15xx_audio_configure_ring(&audio, 0x01234000, 16384,
		1024) == 0);
	assert((fake.regs[0x34 / 4] & 0x0f330000) == 0x09100000);

	/* A full ring publishes 64 bytes short, then releases the tail. */
	hc15xx_audio_set_target(&audio, 0, 1);
	assert((fake.regs[0x38 / 4] & 0xffff) == (16384 - 64) / 16);
	fake.regs[0x38 / 4] |= 4u << 16;
	assert(hc15xx_audio_service_ring(&audio));
	assert((fake.regs[0x38 / 4] & 0xffff) == 0);
	assert(fake.barriers >= 2);
	return 0;
}
