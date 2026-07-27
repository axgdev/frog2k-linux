// SPDX-License-Identifier: MIT
#include "hc15xx_resampler.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
	struct hc15xx_resampler state;
	struct hc15xx_resampler batch_state;
	int16_t output;
	int16_t stereo[32768 * 2];
	int16_t batch_output[32768];
	unsigned produced = 0;
	unsigned i;

	assert(hc15xx_resampler_init(&state, 32000, 32000) == 0);
	for (i = 0; i < 100; i++) {
		assert(hc15xx_resampler_push_stereo_s16(&state,
			(int16_t)i, (int16_t)i, &output) == 1);
		assert(output == (int16_t)i);
	}

	assert(hc15xx_resampler_init(&state, 32768, 32000) == 0);
	for (i = 0; i < 32768; i++) {
		int emitted = hc15xx_resampler_push_stereo_s16(&state,
			(int16_t)(i & 0x7fff), (int16_t)(i & 0x7fff), &output);

		assert(emitted >= 0);
		produced += (unsigned)emitted;
	}
	assert(produced == 32000);
	assert(hc15xx_resampler_init(&batch_state, 32768, 32000) == 0);
	for (i = 0; i < 32768; ++i) {
		stereo[i * 2] = (int16_t)(i & 0x7fff);
		stereo[i * 2 + 1] = (int16_t)(i & 0x7fff);
	}
	assert(hc15xx_resampler_process_stereo_s16(&batch_state, stereo,
		32768, batch_output, 32768) == 32000);
	assert(hc15xx_resampler_set_output_rate(&batch_state, 32256) == 0);
	assert(batch_state.output_rate == 32256);
	assert(hc15xx_resampler_set_output_rate(&batch_state, 32769) < 0);
	assert(hc15xx_resampler_process_stereo_s16(&batch_state, stereo,
		1, batch_output, 0) == 0);
	assert(hc15xx_resampler_init(&state, 16000, 32000) < 0);
	puts("hc15xx resampler tests: PASS");
	return 0;
}
