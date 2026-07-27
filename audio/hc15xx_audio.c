/* SPDX-License-Identifier: MIT */
#include "hc15xx_audio.h"

#define BIT(n)			(UINT32_C(1) << (n))
#define SND(off)		(audio->snd + (off))
#define DAC(off)		(audio->dac + (off))
#define SYS(off)		(audio->sys + (off))

static uint32_t rd(const struct hc15xx_audio *audio, uintptr_t address)
{
	return audio->io.read32(audio->io.cookie, address);
}

static void wr(const struct hc15xx_audio *audio, uintptr_t address,
	uint32_t value)
{
	audio->io.write32(audio->io.cookie, address, value);
}

static void update(struct hc15xx_audio *audio, uintptr_t address,
	uint32_t mask, uint32_t value)
{
	wr(audio, address, (rd(audio, address) & ~mask) | (value & mask));
}

void hc15xx_audio_init(struct hc15xx_audio *audio,
	const struct hc15xx_audio_io *io, uintptr_t snd, uintptr_t dac,
	uintptr_t sys)
{
	audio->io = *io;
	audio->snd = snd;
	audio->dac = dac;
	audio->sys = sys;
	audio->ring_bytes = 0;
	audio->target = 0;
	audio->published = 0;
}

void hc15xx_audio_clock_reset(struct hc15xx_audio *audio)
{
	/* Exact apll_dai_init reset pulse recovered from libauddrv.a. */
	update(audio, SYS(0x80), BIT(5), BIT(5));
	audio->io.delay_us(audio->io.cookie, 1000);
	update(audio, SYS(0x80), BIT(5), 0);
}

void hc15xx_audio_configure_output(struct hc15xx_audio *audio)
{
	uint32_t value = rd(audio, DAC(0));

	/* Exact pwm_dai_init RMW sequence, preserving undocumented reset bits. */
	value &= ~BIT(10);
	value |= BIT(9) | BIT(8);
	value &= ~(BIT(14) | BIT(15) | BIT(22) | BIT(17) | BIT(20));
	value |= BIT(23) | BIT(18);
	value &= ~(UINT32_C(0xf) << 24);
	value |= BIT(25);
	value &= ~(UINT32_C(7) << 28);
	value |= BIT(30);
	value &= ~(BIT(23) | BIT(18));
	wr(audio, DAC(0), value);

	/* 294.912 MHz / 24, then 12.288 MHz / (32 kHz * 32). */
	wr(audio, SYS(0x148), UINT32_C(0x0c000120));
	update(audio, SYS(0x154), BIT(14), BIT(14));
	update(audio, SND(0x50), BIT(4) | BIT(5) | BIT(10) | BIT(11), 0);
	update(audio, SND(0x04), BIT(0) | BIT(8), BIT(0) | BIT(8));
	wr(audio, SND(0x3c), UINT32_C(0x0000ff41));
}

int hc15xx_audio_configure_ring(struct hc15xx_audio *audio,
	uint32_t dma_address, uint32_t bytes, uint32_t period_bytes)
{
	uint32_t config;

	if (!bytes || (bytes & 15) || bytes > UINT32_C(0xffff0) ||
	    !period_bytes || period_bytes > UINT32_C(0xffff))
		return -1;
	audio->ring_bytes = bytes;
	audio->target = 0;
	audio->published = 0;
	wr(audio, SND(0x30), dma_address);
	config = rd(audio, SND(0x34));
	config &= ~UINT32_C(0x1ff30000);
	config |= UINT32_C(0x09100000);
	wr(audio, SND(0x34), config);
	/* Both cursors are in 16-byte units. */
	wr(audio, SND(0x34), (rd(audio, SND(0x34)) & UINT32_C(0xffff0000)) |
		(bytes >> 4));
	wr(audio, SND(0x5c), (rd(audio, SND(0x5c)) & UINT32_C(0xffff0000)) |
		period_bytes);
	wr(audio, SND(0x38), 0);
	return 0;
}

void hc15xx_audio_set_volume(struct hc15xx_audio *audio, uint8_t volume)
{
	uint32_t value = rd(audio, SND(0x90));

	value = (value & ~UINT32_C(0x00ff0000)) | ((uint32_t)volume << 16);
	wr(audio, SND(0x90), value);
}

void hc15xx_audio_start(struct hc15xx_audio *audio)
{
	update(audio, SND(0x08), BIT(0), BIT(0));
	update(audio, SND(0x0c), BIT(0), BIT(0));
	update(audio, SND(0x50), BIT(29), BIT(29));
	(void)hc15xx_audio_service_ring(audio);
}

void hc15xx_audio_stop(struct hc15xx_audio *audio)
{
	update(audio, SND(0x50), BIT(29), 0);
	update(audio, SND(0x04), BIT(16), 0);
	update(audio, SND(0x0c), BIT(0), 0);
	update(audio, SND(0x08), BIT(0) | BIT(8), 0);
	update(audio, SND(0x34), BIT(28), 0);
	wr(audio, SND(0x38), 0);
}

uint32_t hc15xx_audio_consumer(const struct hc15xx_audio *audio)
{
	return ((rd(audio, audio->snd + 0x38) >> 16) & UINT32_C(0xffff)) << 4;
}

void hc15xx_audio_set_target(struct hc15xx_audio *audio,
	uint32_t byte_offset, int data_pending)
{
	uint32_t consumer;
	uint32_t published;

	if (!audio->ring_bytes)
		return;
	byte_offset %= audio->ring_bytes;
	consumer = hc15xx_audio_consumer(audio);
	/*
	 * The hardware has no full flag: equal producer/consumer means empty.
	 * libauddrv therefore reserves 64 bytes (128 in its mode 2).  Publish
	 * the guard now and the true target after the consumer advances.
	 */
	if (data_pending && byte_offset == consumer)
		published = (byte_offset + audio->ring_bytes -
			HC15XX_AUDIO_RING_GUARD) % audio->ring_bytes;
	else
		published = byte_offset;
	audio->target = (uint16_t)(byte_offset >> 4);
	audio->published = (uint16_t)(published >> 4);
	audio->io.dma_barrier(audio->io.cookie);
	update(audio, SND(0x38), UINT32_C(0xffff),
		(uint32_t)audio->published);
	(void)hc15xx_audio_service_ring(audio);
}

int hc15xx_audio_service_ring(struct hc15xx_audio *audio)
{
	uint16_t consumer = (uint16_t)(hc15xx_audio_consumer(audio) >> 4);
	int changed = 0;

	if (audio->published != audio->target && consumer != audio->target) {
		audio->published = audio->target;
		audio->io.dma_barrier(audio->io.cookie);
		update(audio, SND(0x38), UINT32_C(0xffff),
			audio->published);
		changed = 1;
	}
	if (audio->published != consumer && !(rd(audio, SND(0x04)) & BIT(16))) {
		update(audio, SND(0x04), BIT(16), BIT(16));
		changed = 1;
	}
	return changed;
}

int hc15xx_audio_ack_irq(struct hc15xx_audio *audio)
{
	uint32_t ctl08 = rd(audio, SND(0x08));
	uint32_t ctl0c = rd(audio, SND(0x0c));
	uint32_t status08 = ctl08 & (BIT(16) | BIT(24) | BIT(27) | BIT(28));
	uint32_t status0c = ctl0c & (BIT(8) | BIT(24));
	int period = ((ctl08 & (BIT(16) | BIT(0))) == (BIT(16) | BIT(0))) ||
		((ctl0c & (BIT(8) | BIT(0))) == (BIT(8) | BIT(0)));

	if (status08)
		wr(audio, SND(0x08), (ctl08 & UINT32_C(0xffff)) | status08);
	if (status0c)
		wr(audio, SND(0x0c),
			(ctl0c & UINT32_C(0x00ff00ff)) | status0c);
	return period;
}
