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

int hc15xx_resampler_set_output_rate(struct hc15xx_resampler *state,
	uint32_t output_rate)
{
	if (!state || !output_rate || output_rate > state->input_rate)
		return -1;
	state->output_rate = output_rate;
	return 0;
}

int hc15xx_resampler_push_stereo_s16(struct hc15xx_resampler *state,
	int16_t left, int16_t right, int16_t *mono)
{
	int16_t stereo[2] = { left, right };

	if (!state || !mono || !state->input_rate || !state->output_rate)
		return -1;
	return (int)hc15xx_resampler_process_stereo_s16(state, stereo, 1,
		mono, 1);
}

size_t hc15xx_resampler_process_stereo_s16(struct hc15xx_resampler *state,
	const int16_t *stereo, size_t frames, int16_t *mono, size_t capacity)
{
	uint32_t input_rate;
	uint32_t output_rate;
	uint32_t phase;
	int32_t previous;
	unsigned have_previous;
	size_t input;
	size_t produced = 0;

	if (!state || (!stereo && frames) || (!mono && capacity) ||
			!state->input_rate || !state->output_rate ||
			capacity < frames)
		return 0;
	input_rate = state->input_rate;
	output_rate = state->output_rate;
	phase = state->phase;
	previous = state->previous;
	have_previous = state->have_previous;
	for (input = 0; input < frames; ++input) {
		int32_t current = ((int32_t)stereo[input * 2] +
			(int32_t)stereo[input * 2 + 1]) / 2;
		uint32_t old_phase;
		uint32_t fraction;

		if (!have_previous) {
			previous = current;
			have_previous = 1;
			mono[produced++] = (int16_t)current;
			continue;
		}
		old_phase = phase;
		phase += output_rate;
		if (phase < input_rate) {
			previous = current;
			continue;
		}
		phase -= input_rate;
		fraction = input_rate - old_phase;
		/*
		 * output_rate <= input_rate, so there is at most one output per
		 * input interval and a frames-sized output is always sufficient.
		 */
		mono[produced++] = (int16_t)(previous +
			(current - previous) * (int32_t)fraction /
			(int32_t)output_rate);
		previous = current;
	}
	state->phase = phase;
	state->previous = previous;
	state->have_previous = (uint8_t)have_previous;
	return produced;
}
