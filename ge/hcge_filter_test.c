/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdio.h>

int main(void)
{
	short coefficients[32], phase[8];
	double real[33];
	int fixed[33];
	int i, p, t;

	for (i = 0; i < 32; ++i)
		coefficients[i] = (short)(i * 37 - 400);
	for (p = 0; p < 8; ++p) {
		extract_phase(phase, coefficients, 8, 4, p);
		printf("phase %d", p);
		for (t = 0; t < 4; ++t)
			printf(" %d/%d", phase[t],
			       extract_coef(coefficients, 8, 4, p, t));
		putchar('\n');
	}
	designfilter(320, 240, 8, 4, 1, 0.9, 8, real, coefficients);
	printf("double");
	for (i = 0; i < 32; ++i)
		printf(" %d", coefficients[i]);
	putchar('\n');
	designfilterff(320, 240, 8, 4, 1, 58982, 8, fixed, coefficients);
	printf("fixed-sums");
	for (p = 0; p < 8; ++p) {
		int sum = 0;
		for (t = 0; t < 4; ++t)
			sum += coefficients[t * 8 + p];
		printf(" %d", sum);
	}
	putchar('\n');
	return 0;
}
