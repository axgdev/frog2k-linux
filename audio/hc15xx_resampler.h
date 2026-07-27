// SPDX-License-Identifier: MIT
#ifndef HC15XX_RESAMPLER_H
#define HC15XX_RESAMPLER_H

#include <stddef.h>
#include <stdint.h>

struct hc15xx_resampler {
	uint32_t input_rate;
	uint32_t output_rate;
	uint32_t phase;
	int32_t previous;
	uint8_t have_previous;
};

int hc15xx_resampler_init(struct hc15xx_resampler *state,
	uint32_t input_rate, uint32_t output_rate);
int hc15xx_resampler_set_output_rate(struct hc15xx_resampler *state,
	uint32_t output_rate);
int hc15xx_resampler_push_stereo_s16(struct hc15xx_resampler *state,
	int16_t left, int16_t right, int16_t *mono);
size_t hc15xx_resampler_process_stereo_s16(struct hc15xx_resampler *state,
	const int16_t *stereo, size_t frames, int16_t *mono, size_t capacity);

#endif
