/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <math.h>
#include <stdint.h>

uint32_t hcge_ge_coeff(double value)
{
	double magnitude = value < 0.0 ? -value : value;
	uint32_t coefficient;

	coefficient = magnitude >= 32768.0 ? 0x7fffffffu :
		(uint32_t)(magnitude * 65536.0);
	return value < 0.0 ? coefficient | 0x80000000u : coefficient;
}

void hcge_matrix_init_rotate(double *matrix, double degree)
{
	/* Match the vendor's promoted single-precision pi constant. */
	double radians = degree * (3.141592502593994140625 / 180.0);
	double sine = sin(radians);
	double cosine = cos(radians);

	matrix[0] = cosine;
	matrix[1] = -sine;
	matrix[2] = 0.0;
	matrix[3] = sine;
	matrix[4] = cosine;
	matrix[5] = 0.0;
}

void hcge_matrix_multiply(const double *left, const double *right,
			  double *output)
{
	double result[6];

	result[0] = left[0] * right[0] + left[1] * right[3];
	result[1] = left[0] * right[1] + left[1] * right[4];
	result[2] = left[0] * right[2] + left[1] * right[5];
	result[3] = left[3] * right[0] + left[4] * right[3];
	result[4] = left[3] * right[1] + left[4] * right[4];
	result[5] = left[3] * right[2] + left[4] * right[5];
	for (unsigned int i = 0; i < 6; ++i)
		output[i] = result[i];
}

void hcge_matrix_translate_left_multiply(float x, float y, double *matrix)
{
	matrix[2] += x;
	matrix[5] += y;
}

void hcge_matrix_translate_right_multiply(float x, float y, double *matrix)
{
	matrix[2] += matrix[0] * x + matrix[1] * y;
	matrix[5] += matrix[3] * x + matrix[4] * y;
}

void hcge_get_inverse_matrix(const double *matrix, double *inverse)
{
	double determinant = matrix[0] * matrix[4] - matrix[1] * matrix[3];

	inverse[0] = matrix[4] / determinant;
	inverse[1] = -matrix[1] / determinant;
	inverse[2] = (matrix[1] * matrix[5] - matrix[4] * matrix[2]) /
		determinant;
	inverse[3] = -matrix[3] / determinant;
	inverse[4] = matrix[0] / determinant;
	inverse[5] = (matrix[3] * matrix[2] - matrix[0] * matrix[5]) /
		determinant;
}

void hcge_process_matrix(hcge_context *ctx, HCGEAccelerationMask accel)
{
	const int32_t *m;
	unsigned int i;

	if (!ctx)
		return;
	ctx->matrix_en = false;
	ctx->x_translate = 0;
	ctx->y_translate = 0;
	if (accel != HCGE_DFXL_BLIT && accel != HCGE_DFXL_STRETCHBLIT)
		return;
	m = ctx->state.matrix;
	if (accel == HCGE_DFXL_STRETCHBLIT &&
	    ctx->state.render_options != HCGE_DSRO_MATRIX) {
		ctx->matrix_en = true;
		ctx->matrix[0] = 1.0;
		ctx->matrix[1] = 0.0;
		ctx->matrix[2] = 0.0;
		ctx->matrix[3] = 0.0;
		ctx->matrix[4] = 1.0;
		ctx->matrix[5] = 0.0;
		return;
	}
	if (ctx->state.render_options != HCGE_DSRO_MATRIX)
		return;
	if (m[0] == 0x10000 && m[1] == 0 && m[3] == 0 && m[4] == 0x10000) {
		ctx->x_translate = (uint32_t)((m[2] + 0x8000) >> 16);
		ctx->y_translate = (uint32_t)((m[5] + 0x8000) >> 16);
		return;
	}
	ctx->matrix_en = true;
	for (i = 0; i < 6; ++i)
		ctx->matrix[i] = (double)m[i] / 65536.0;
}

static int hcge_floor_double(double value)
{
	int integer = (int)value;
	return value < (double)integer ? integer - 1 : integer;
}

static int hcge_ceil_double(double value)
{
	int integer = (int)value;
	return value > (double)integer ? integer + 1 : integer;
}

bool hcge_get_bounding_rect(hcge_context *ctx, HCGERectangle *source,
			    HCGERectangle *bounding)
{
	double x[4], y[4];
	int min_x, min_y, max_x, max_y, i;

	if (!ctx || !source || !bounding || source->w <= 0 || source->h <= 0)
		return false;
	x[0] = source->x; y[0] = source->y;
	x[1] = source->x + source->w - 1; y[1] = source->y;
	x[2] = source->x; y[2] = source->y + source->h - 1;
	x[3] = source->x + source->w - 1;
	y[3] = source->y + source->h - 1;
	for (i = 0; i < 4; ++i) {
		double tx = ctx->matrix[0] * x[i] + ctx->matrix[1] * y[i] +
			ctx->matrix[2];
		double ty = ctx->matrix[3] * x[i] + ctx->matrix[4] * y[i] +
			ctx->matrix[5];
		x[i] = tx;
		y[i] = ty;
	}
	min_x = max_x = hcge_floor_double(x[0]);
	min_y = max_y = hcge_floor_double(y[0]);
	for (i = 1; i < 4; ++i) {
		int low_x = hcge_floor_double(x[i]);
		int low_y = hcge_floor_double(y[i]);
		int high_x = hcge_ceil_double(x[i]);
		int high_y = hcge_ceil_double(y[i]);
		if (low_x < min_x) min_x = low_x;
		if (low_y < min_y) min_y = low_y;
		if (high_x > max_x) max_x = high_x;
		if (high_y > max_y) max_y = high_y;
	}
	if (max_x < 0 || max_y < 0 || min_x >= ctx->state.destination.config.size.w ||
	    min_y >= ctx->state.destination.config.size.h)
		return false;
	if (min_x < 0) min_x = 0;
	if (min_y < 0) min_y = 0;
	if (max_x >= ctx->state.destination.config.size.w)
		max_x = ctx->state.destination.config.size.w - 1;
	if (max_y >= ctx->state.destination.config.size.h)
		max_y = ctx->state.destination.config.size.h - 1;
	*bounding = (HCGERectangle){ min_x, min_y,
		max_x - min_x + 1, max_y - min_y + 1 };
	return true;
}

static bool hcge_intersect(HCGERectangle *rectangle, int left, int top,
			   int right, int bottom)
{
	int x2 = rectangle->x + rectangle->w;
	int y2 = rectangle->y + rectangle->h;

	if (rectangle->x < left)
		rectangle->x = left;
	if (rectangle->y < top)
		rectangle->y = top;
	if (x2 > right)
		x2 = right;
	if (y2 > bottom)
		y2 = bottom;
	rectangle->w = x2 - rectangle->x;
	rectangle->h = y2 - rectangle->y;
	return rectangle->w > 0 && rectangle->h > 0;
}

bool hcge_clip_qw(HCGERectangle *rectangle, HCGERectangle *hardware_clip)
{
	int aligned_right;

	if (!rectangle || !hardware_clip || rectangle->w <= 0 || rectangle->h <= 0)
		return false;
	*hardware_clip = *rectangle;
	aligned_right = (rectangle->x + rectangle->w + 3) & ~3;
	rectangle->w = aligned_right - rectangle->x;
	return true;
}

bool hcge_clip_rect(hcge_state *state, HCGERectangle *rectangle)
{
	int left, top, right, bottom;

	if (!state || !rectangle)
		return false;
	left = state->clip.x1 > 0 ? state->clip.x1 : 0;
	top = state->clip.y1 > 0 ? state->clip.y1 : 0;
	right = state->clip.x2 + 1;
	bottom = state->clip.y2 + 1;
	if (right > state->destination.config.size.w)
		right = state->destination.config.size.w;
	if (bottom > state->destination.config.size.h)
		bottom = state->destination.config.size.h;
	return hcge_intersect(rectangle, left, top, right, bottom);
}

bool hcge_clip_blit(hcge_state *state, HCGERectangle *source,
		    int *destination_x, int *destination_y)
{
	HCGERectangle destination;
	int source_x, source_y;

	if (!source || !destination_x || !destination_y)
		return false;
	destination.x = *destination_x;
	destination.y = *destination_y;
	destination.w = source->w;
	destination.h = source->h;
	source_x = source->x;
	source_y = source->y;
	if (!hcge_clip_rect(state, &destination))
		return false;
	source->x = source_x + destination.x - *destination_x;
	source->y = source_y + destination.y - *destination_y;
	source->w = destination.w;
	source->h = destination.h;
	*destination_x = destination.x;
	*destination_y = destination.y;
	return true;
}

bool hcge_clip_stretch_blit(hcge_state *state, HCGERectangle *source,
			    HCGERectangle *destination)
{
	HCGERectangle original;
	int left, top, right, bottom;

	if (!source || !destination || destination->w <= 0 || destination->h <= 0)
		return false;
	original = *destination;
	if (!hcge_clip_rect(state, destination))
		return false;
	left = destination->x - original.x;
	top = destination->y - original.y;
	right = original.x + original.w - destination->x - destination->w;
	bottom = original.y + original.h - destination->y - destination->h;
	source->x += (left * source->w) / original.w;
	source->y += (top * source->h) / original.h;
	source->w -= (left * source->w) / original.w +
		(right * source->w) / original.w;
	source->h -= (top * source->h) / original.h +
		(bottom * source->h) / original.h;
	if ((left || right) && source->w > 0)
		--source->w;
	return source->w > 0 && source->h > 0;
}
