/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

short extract_coef(const short *coefficients, int phases, int taps,
		   int phase, int tap)
{
	(void)taps;
	return coefficients[(taps - 1 - tap) * phases + phase];
}

void extract_phase(short *output, const short *coefficients, int phases,
		   int taps, int phase)
{
	int tap;

	for (tap = 0; tap < taps; ++tap)
		output[tap] = coefficients[(taps - 1 - tap) * phases + phase];
}
