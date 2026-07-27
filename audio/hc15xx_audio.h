/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_AUDIO_H
#define HC15XX_AUDIO_H

#include <stddef.h>
#include <stdint.h>

/*
 * OS-neutral HC15xx SND0 output core.  Linux, HCRTOS and FreeRTOS adapters
 * provide only MMIO, delay and DMA-barrier operations.
 */
struct hc15xx_audio_io {
	void *cookie;
	uint32_t (*read32)(void *cookie, uintptr_t address);
	void (*write32)(void *cookie, uintptr_t address, uint32_t value);
	void (*delay_us)(void *cookie, unsigned int usec);
	void (*dma_barrier)(void *cookie);
};

struct hc15xx_audio {
	struct hc15xx_audio_io io;
	uintptr_t snd;
	uintptr_t dac;
	uintptr_t sys;
	uint32_t ring_bytes;
	uint16_t target;
	uint16_t published;
};

enum {
	HC15XX_AUDIO_RATE = 32000,
	HC15XX_AUDIO_RING_GUARD = 64,
};

void hc15xx_audio_init(struct hc15xx_audio *audio,
	const struct hc15xx_audio_io *io, uintptr_t snd, uintptr_t dac,
	uintptr_t sys);
void hc15xx_audio_clock_reset(struct hc15xx_audio *audio);
void hc15xx_audio_configure_output(struct hc15xx_audio *audio);
int hc15xx_audio_configure_ring(struct hc15xx_audio *audio,
	uint32_t dma_address, uint32_t bytes, uint32_t period_frames);
void hc15xx_audio_set_volume(struct hc15xx_audio *audio, uint8_t volume);
void hc15xx_audio_start(struct hc15xx_audio *audio);
void hc15xx_audio_stop(struct hc15xx_audio *audio);
void hc15xx_audio_set_target(struct hc15xx_audio *audio,
	uint32_t byte_offset, int data_pending);
int hc15xx_audio_service_ring(struct hc15xx_audio *audio);
int hc15xx_audio_ack_irq(struct hc15xx_audio *audio);
uint32_t hc15xx_audio_consumer(const struct hc15xx_audio *audio);

#endif
