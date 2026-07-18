/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdint.h>
#include <stdio.h>

static void dump(const char *name, const double matrix[6])
{
	unsigned int i;
	printf("%s", name);
	for (i = 0; i < 6; ++i) {
		union { double d; uint64_t u; } bits;
		bits.d = matrix[i];
		printf(" %016llx", (unsigned long long)bits.u);
	}
	putchar('\n');
}

static void dump_rect(const char *name, int result, const HCGERectangle *first,
		      const HCGERectangle *second, int x, int y)
{
	printf("%s %d %d,%d,%d,%d %d,%d,%d,%d %d,%d\n", name, result,
	       first->x, first->y, first->w, first->h,
	       second->x, second->y, second->w, second->h, x, y);
}

int main(void)
{
	static const double coefficients[] = {
		-2.0, -1.0, -0.75, -0.5, -0.00001, 0.0,
		0.00001, 0.25, 0.5, 0.999999, 1.0, 2.0,
	};
	double left[6] = { 1.25, -0.5, 7.0, 0.75, 2.0, -3.0 };
	double right[6] = { -2.0, 0.25, 9.0, 1.5, -1.0, 4.0 };
	double matrix[6], inverse[6];
	unsigned int i;
	setbuf(stdout, NULL);

	for (i = 0; i < sizeof(coefficients) / sizeof(coefficients[0]); ++i)
		printf("coeff %08x\n", hcge_ge_coeff(coefficients[i]));
	hcge_matrix_multiply(left, right, matrix);
	dump("multiply", matrix);
	hcge_matrix_translate_left_multiply(11.5, -6.25, matrix);
	dump("left", matrix);
	hcge_matrix_translate_right_multiply(-3.5, 8.25, matrix);
	dump("right", matrix);
	hcge_get_inverse_matrix(left, inverse);
	dump("inverse", inverse);
	for (i = 0; i < 4; ++i) {
		hcge_matrix_init_rotate(matrix, i * 90.0);
		dump("rotate", matrix);
	}
	{
		hcge_state state = { 0 };
		HCGERectangle a, b;
		int x, y, result;
		state.destination.config.size.w = 320;
		state.destination.config.size.h = 240;
		state.clip = (HCGERegion){ 10, 20, 299, 219 };
		a = (HCGERectangle){ 3, 5, 19, 11 };
		b = (HCGERectangle){ 0 };
		result = hcge_clip_qw(&a, &b);
		dump_rect("qw", result, &a, &b, 0, 0);
		a = (HCGERectangle){ 0, 0, 40, 50 };
		b = (HCGERectangle){ 0 };
		result = hcge_clip_rect(&state, &a);
		dump_rect("rect", result, &a, &b, 0, 0);
		a = (HCGERectangle){ 7, 9, 80, 60 };
		b = (HCGERectangle){ 0 };
		x = -15; y = 190;
		result = hcge_clip_blit(&state, &a, &x, &y);
		dump_rect("blit", result, &a, &b, x, y);
		a = (HCGERectangle){ 5, 7, 160, 120 };
		b = (HCGERectangle){ -30, 180, 240, 100 };
		result = hcge_clip_stretch_blit(&state, &a, &b);
		dump_rect("stretch", result, &a, &b, 0, 0);
	}
	return 0;
}
