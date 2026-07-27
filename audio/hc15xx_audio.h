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
	HC15XX_STC_TICKS_PER_MS = 45,
};

enum hc15xx_stc_id {
	HC15XX_STC0 = 0,
	HC15XX_STC1 = 1,
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
uint32_t hc15xx_audio_stc_tick(const struct hc15xx_audio *audio,
	enum hc15xx_stc_id id);
uint32_t hc15xx_audio_stc_ms(const struct hc15xx_audio *audio,
	enum hc15xx_stc_id id);
void hc15xx_audio_set_stc_tick(struct hc15xx_audio *audio,
	enum hc15xx_stc_id id, uint32_t tick);
void hc15xx_audio_set_stc_ms(struct hc15xx_audio *audio,
	enum hc15xx_stc_id id, uint32_t ms);
void hc15xx_audio_set_stc_divisor(struct hc15xx_audio *audio,
	enum hc15xx_stc_id id, uint16_t divisor);
void hc15xx_audio_pause_stc(struct hc15xx_audio *audio,
	enum hc15xx_stc_id id, int resume);
uint32_t hc15xx_audio_ms_to_stc_tick(uint32_t ms);
uint32_t hc15xx_audio_stc_tick_to_ms(uint32_t tick);

#endif
