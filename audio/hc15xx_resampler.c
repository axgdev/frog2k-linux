// SPDX-License-Identifier: MIT
#include "hc15xx_resampler.h"

#include <stddef.h>

int hc15xx_resampler_init(struct hc15xx_resampler *state,
	uint32_t input_rate, uint32_t output_rate)
{
	if (!state || !input_rate || !output_rate || output_rate > input_rate)
		return -1;
	*state = (struct hc15xx_resampler){
		.input_rate = input_rate,
		.output_rate = output_rate,
	};
	return 0;
}

int hc15xx_resampler_push_stereo_s16(struct hc15xx_resampler *state,
	int16_t left, int16_t right, int16_t *mono)
{
	int32_t current;
	uint32_t old_phase;
	uint32_t fraction;

	if (!state || !mono || !state->input_rate || !state->output_rate)
		return -1;
	current = ((int32_t)left + (int32_t)right) / 2;
	if (!state->have_previous) {
		state->previous = current;
		state->have_previous = 1;
		*mono = (int16_t)current;
		return 1;
	}
	old_phase = state->phase;
	state->phase += state->output_rate;
	if (state->phase < state->input_rate) {
		state->previous = current;
		return 0;
	}
	state->phase -= state->input_rate;
	fraction = state->input_rate - old_phase;
	/*
	 * output_rate <= input_rate, so there is at most one output per input
	 * interval.  The product fits signed 32-bit for S16 input and the
	 * SF2000's 32 kHz output rate.
	 */
	*mono = (int16_t)(state->previous +
		(current - state->previous) * (int32_t)fraction /
		(int32_t)state->output_rate);
	state->previous = current;
	return 1;
}
