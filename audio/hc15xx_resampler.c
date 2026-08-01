// SPDX-License-Identifier: MIT
#include "hc15xx_resampler.h"

#include <stddef.h>

int hc15xx_resampler_init(struct hc15xx_resampler *state,
	uint32_t input_rate, uint32_t output_rate)
{
	if (!state || !input_rate || !output_rate)
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
	if (!state || !output_rate)
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
		!state->input_rate || !state->output_rate || !capacity)
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
		uint64_t advanced;
		uint64_t outputs;
		uint64_t output_index;

		if (!have_previous) {
			if (produced >= capacity)
				break;
			previous = current;
			have_previous = 1;
			mono[produced++] = (int16_t)current;
			continue;
		}
		old_phase = phase;
		advanced = (uint64_t)old_phase + output_rate;
		outputs = advanced / input_rate;
		/* A slow core can require multiple outputs between two inputs. */
		if (outputs > (uint64_t)capacity - produced)
			break;
		for (output_index = 0; output_index < outputs; output_index++) {
			uint64_t fraction = (uint64_t)input_rate - old_phase +
				output_index * input_rate;
			int64_t delta = (int64_t)current - previous;

			mono[produced++] = (int16_t)(previous +
				delta * (int64_t)fraction / output_rate);
		}
		phase = (uint32_t)(advanced - outputs * input_rate);
		previous = current;
	}
	state->phase = phase;
	state->previous = previous;
	state->have_previous = (uint8_t)have_previous;
	return produced;
}
