/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define HCGE_IOCBASE 35
#define HCGE_REQUEST_IRQ _IO(HCGE_IOCBASE, 0x01)
#define HCGE_FREE_IRQ _IO(HCGE_IOCBASE, 0x02)
#define HCGE_RESET _IO(HCGE_IOCBASE, 0x03)
#define HCGE_SYNC_TIMEOUT _IO(HCGE_IOCBASE, 0x04)
#define HCGE_GET_CMDQ_BUFINFO _IO(HCGE_IOCBASE, 0x05)
#define HCGE_SET_CLOCK _IO(HCGE_IOCBASE, 0x06)
#define HCGE_GET_GE_REGISTER _IO(HCGE_IOCBASE, 0x07)
#define HCGE_SUBMIT _IO(HCGE_IOCBASE, 0x08)

struct hcge_cmdq_buf_info {
	uint32_t addr;
	uint32_t size;
};

struct hcge_linux_submit {
	uint32_t data;
	uint32_t length;
};

struct hcge_format {
	HCGESurfacePixelFormat format;
	uint8_t code;
	uint8_t bytes;
};

static const struct hcge_format hcge_formats[] = {
	{ HCGE_DSPF_ARGB1555, 5, 2 },
	{ HCGE_DSPF_RGB16, 6, 2 },
	{ HCGE_DSPF_RGB32, 0, 4 },
	{ HCGE_DSPF_ARGB, 1, 4 },
	{ HCGE_DSPF_ARGB4444, 3, 2 },
};

static const struct hcge_format *hcge_get_format(HCGESurfacePixelFormat format)
{
	unsigned int i;

	for (i = 0; i < sizeof(hcge_formats) / sizeof(hcge_formats[0]); i++)
		if (hcge_formats[i].format == format)
			return &hcge_formats[i];
	return NULL;
}

static int hcge_context_fd(const hcge_context *ctx)
{
	return ctx ? ctx->ge_fd : -1;
}

static uint32_t hcge_xy(int x, int y)
{
	return ((uint32_t)y & 0xfffu) << 16 | ((uint32_t)x & 0xfffu);
}

static uint32_t hcge_wh(int width, int height)
{
	return ((uint32_t)height & 0xfffu) << 16 |
		((uint32_t)width & 0xfffu);
}

static uint32_t hcge_surface_color(HCGESurfacePixelFormat format,
	HCGEColor color)
{
	switch (format) {
	case HCGE_DSPF_ARGB1555:
		return ((uint32_t)(color.a >> 7) << 15) |
			((uint32_t)(color.r >> 3) << 10) |
			((uint32_t)(color.g >> 3) << 5) | (color.b >> 3);
	case HCGE_DSPF_RGB16:
		/* Bit 16 is the vendor RGB565 paint-format tag. */
		return 0x00010000u | ((uint32_t)(color.r & 0xf8u) << 8) |
			((uint32_t)(color.g & 0xfcu) << 3) | color.b >> 3;
	case HCGE_DSPF_ARGB4444:
		return ((uint32_t)(color.a >> 4) << 12) |
			((uint32_t)(color.r >> 4) << 8) |
			((uint32_t)(color.g >> 4) << 4) | (color.b >> 4);
	default:
		return (uint32_t)color.a << 24 | (uint32_t)color.r << 16 |
			(uint32_t)color.g << 8 | color.b;
	}
}

static bool hcge_surface_valid(const HCGE_CoreSurface *surface,
	const HCGE_CoreSurfaceBuffer *buffer)
{
	const struct hcge_format *format;

	if (!surface || !buffer)
		return false;
	format = hcge_get_format(surface->config.format);
	return format && surface->config.size.w > 0 &&
		surface->config.size.h > 0 && buffer->phys &&
		!(buffer->phys & (format->bytes - 1u)) &&
		buffer->pitch >= format->bytes &&
		!(buffer->pitch % format->bytes) &&
		buffer->pitch <= HCGE_IMAGE_PITCH_MAX;
}

static uint32_t hcge_surface_buffer(HCGESurfacePixelFormat format,
	uint32_t pitch)
{
	const struct hcge_format *description = hcge_get_format(format);

	return description ? (uint32_t)description->code << 12 |
		((pitch / description->bytes) & 0xfffu) : 0;
}

static bool hcge_rectangle_valid(const HCGERectangle *rectangle)
{
	return rectangle && rectangle->x >= 0 && rectangle->y >= 0 &&
		rectangle->w > 0 && rectangle->h > 0 && rectangle->x <= 0xfff &&
		rectangle->y <= 0xfff && rectangle->w <= 0xfff &&
		rectangle->h <= 0xfff;
}

int hcge_open_context(hcge_context *ctx)
{
	struct hcge_cmdq_buf_info queue = { 0, 0 };
	uint32_t registers = 0;
	int fd;

	if (!ctx)
		return -EINVAL;
	memset(ctx, 0, sizeof(*ctx));
	ctx->ge_fd = -1;
	fd = open("/dev/ge", O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	ctx->ge_fd = fd;
	if (ioctl(fd, HCGE_GET_GE_REGISTER, &registers) < 0 ||
	    ioctl(fd, HCGE_GET_CMDQ_BUFINFO, &queue) < 0 || !queue.addr ||
	    queue.size <= 1056u) {
		int error = errno ? -errno : -ENODEV;

		close(fd);
		ctx->ge_fd = -1;
		return error;
	}
	ctx->ge_regs = (volatile struct ge_reg_buf *)(uintptr_t)registers;
	ctx->cmdq_buf_map_phyaddr = queue.addr;
	ctx->cmdq_buf_map_size = queue.size;
	ctx->cmdq_buf_phyaddr = queue.addr + 1056u;
	ctx->cmdq_buf_size = queue.size - 1056u;
	ctx->clut_tbl_paddr = queue.addr + 32u;
	/* The kernel probe owns clock/reset; a second reset wedges HC15xx. */
	if (ioctl(fd, HCGE_REQUEST_IRQ, 0) < 0) {
		int error = errno ? -errno : -EIO;

		close(fd);
		ctx->ge_fd = -1;
		return error;
	}
	return 0;
}

int hcge_open(hcge_context **pctx)
{
	hcge_context *ctx;
	int ret;

	if (!pctx)
		return -EINVAL;
	*pctx = NULL;
	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return -ENOMEM;
	ret = hcge_open_context(ctx);
	if (ret) {
		free(ctx);
		return ret;
	}
	*pctx = ctx;
	return 0;
}

void hcge_close(hcge_context *ctx)
{
	if (!ctx)
		return;
	if (ctx->ge_fd >= 0) {
		(void)hcge_engine_sync(ctx);
		(void)ioctl(ctx->ge_fd, HCGE_FREE_IRQ, 0);
		close(ctx->ge_fd);
	}
	free(ctx->nd_ctx);
	free(ctx);
}

void hcge_hw_reset(hcge_context *ctx)
{
	if (hcge_context_fd(ctx) >= 0)
		(void)ioctl(ctx->ge_fd, HCGE_RESET, 0);
}

void hcge_reset(hcge_context *ctx)
{
	if (!ctx)
		return;
	hcge_hw_reset(ctx);
	if (ctx->nd_ctx)
		memset(ctx->nd_ctx, 0, 680u);
	ctx->blit_direct = false;
	ctx->clip_en = false;
	ctx->msk_en = false;
	ctx->matrix_en = false;
}

int hcge_engine_sync(hcge_context *ctx)
{
	if (hcge_context_fd(ctx) < 0)
		return -EINVAL;
	return ioctl(ctx->ge_fd, HCGE_SYNC_TIMEOUT, 1000ul) < 0 ? -errno : 0;
}

int hcge_set_clock(hcge_context *ctx, unsigned int selector)
{
	if (hcge_context_fd(ctx) < 0 || selector > 3u)
		return -EINVAL;
	return ioctl(ctx->ge_fd, HCGE_SET_CLOCK, (unsigned long)selector) < 0 ?
		-errno : 0;
}

int hcge_linux_submit(hcge_context *ctx, const uint32_t *node, size_t words)
{
	struct hcge_linux_submit submit;

	if (hcge_context_fd(ctx) < 0 || !node || !words ||
	    words > ctx->cmdq_buf_size / sizeof(*node))
		return -EINVAL;
	if (hcge_engine_sync(ctx) < 0)
		return -EIO;
	submit.data = (uint32_t)(uintptr_t)node;
	submit.length = (uint32_t)(words * sizeof(*node));
	return ioctl(ctx->ge_fd, HCGE_SUBMIT, &submit) < 0 ? -errno : 0;
}

void hcge_check_state(hcge_state *state, HCGEAccelerationMask accel)
{
	if (!state || !(accel & (HCGE_DFXL_FILLRECTANGLE | HCGE_DFXL_BLIT |
				 HCGE_DFXL_STRETCHBLIT)))
		return;
	if (!hcge_get_format(state->destination.config.format))
		return;
	if (HCGE_DFB_DRAWING_FUNCTION(accel)) {
		if (state->render_options != HCGE_DSRO_NONE ||
		    state->drawingflags != HCGE_DSDRAW_NOFX)
			return;
		if (accel & HCGE_DFXL_FILLRECTANGLE)
			state->accel |= accel & HCGE_DFXL_FILLRECTANGLE;
	}
	if (HCGE_DFB_BLITTING_FUNCTION(accel) &&
	    hcge_get_format(state->source.config.format) &&
	    state->source.config.format == state->destination.config.format &&
	    state->render_options == HCGE_DSRO_NONE &&
	    state->blittingflags == HCGE_DSBLIT_NOFX)
		state->accel |= accel & (HCGE_DFXL_BLIT | HCGE_DFXL_STRETCHBLIT);
}

void hcge_set_state(hcge_context *ctx, hcge_state *state,
	HCGEAccelerationMask accel)
{
	if (!ctx || !state)
		return;
	if (state != &ctx->state)
		ctx->state = *state;
	ctx->state.accel = accel;
	state->mod_hw = HCGE_SMF_NONE;
}

bool hcge_fill_rect(hcge_context *ctx, HCGERectangle *rectangle)
{
	const hcge_state *state;
	uint32_t node[14];

	if (!ctx || !hcge_rectangle_valid(rectangle))
		return false;
	state = &ctx->state;
	if (state->accel != HCGE_DFXL_FILLRECTANGLE ||
	    state->drawingflags != HCGE_DSDRAW_NOFX ||
	    !hcge_surface_valid(&state->destination, &state->dst))
		return false;
	node[0] = 0x02008367u;
	node[1] = 0x00a00003u;
	node[2] = (uint32_t)state->dst.phys;
	node[3] = hcge_surface_buffer(state->destination.config.format,
		state->dst.pitch);
	node[4] = node[2];
	node[5] = node[3];
	node[6] = hcge_surface_color(state->destination.config.format,
		state->color);
	node[7] = 0;
	node[8] = 0;
	node[9] = hcge_get_format(state->destination.config.format)->code;
	node[10] = hcge_xy(rectangle->x, rectangle->y);
	node[11] = hcge_wh(rectangle->w, rectangle->h);
	node[12] = node[10];
	node[13] = 0x00030000u;
	return hcge_linux_submit(ctx, node, sizeof(node) / sizeof(node[0])) == 0;
}

bool hcge_blit(hcge_context *ctx, HCGERectangle *source, int dx, int dy)
{
	const hcge_state *state;
	uint32_t node[9];

	if (!ctx || !hcge_rectangle_valid(source) || dx < 0 || dy < 0 ||
	    dx > 0xfff || dy > 0xfff)
		return false;
	state = &ctx->state;
	if (state->accel != HCGE_DFXL_BLIT ||
	    state->blittingflags != HCGE_DSBLIT_NOFX ||
	    state->destination.config.format != state->source.config.format ||
	    !hcge_surface_valid(&state->destination, &state->dst) ||
	    !hcge_surface_valid(&state->source, &state->src))
		return false;
	node[0] = 0x02000307u;
	node[1] = 0x00000002u;
	node[2] = (uint32_t)state->dst.phys;
	node[3] = hcge_surface_buffer(state->destination.config.format,
		state->dst.pitch);
	node[4] = (uint32_t)state->src.phys;
	node[5] = hcge_surface_buffer(state->source.config.format,
		state->src.pitch);
	node[6] = hcge_xy(dx, dy);
	node[7] = hcge_wh(source->w, source->h);
	node[8] = hcge_xy(source->x, source->y);
	return hcge_linux_submit(ctx, node, sizeof(node) / sizeof(node[0])) == 0;
}

bool hcge_stretch_blit(hcge_context *ctx, HCGERectangle *source,
	HCGERectangle *destination)
{
	const hcge_state *state;
	uint32_t node[22];

	if (!ctx || !hcge_rectangle_valid(source) ||
	    !hcge_rectangle_valid(destination) || source->x || source->y ||
	    destination->x || destination->y)
		return false;
	state = &ctx->state;
	if (state->accel != HCGE_DFXL_STRETCHBLIT ||
	    state->blittingflags != HCGE_DSBLIT_NOFX ||
	    state->destination.config.format != state->source.config.format ||
	    !hcge_surface_valid(&state->destination, &state->dst) ||
	    !hcge_surface_valid(&state->source, &state->src))
		return false;
	node[0] = 0x0206870fu;
	node[1] = 0x00a03009u;
	node[2] = (uint32_t)state->dst.phys;
	node[3] = hcge_surface_buffer(state->destination.config.format,
		state->dst.pitch);
	node[4] = node[2];
	node[5] = node[3];
	node[6] = (uint32_t)state->src.phys;
	node[7] = 0x40000000u |
		hcge_surface_buffer(state->source.config.format, state->src.pitch);
	node[8] = 0;
	node[9] = hcge_wh(destination->w, destination->h);
	node[10] = 0;
	node[11] = 0;
	node[12] = hcge_wh(source->w, source->h);
	node[13] = 0x00004000u;
	node[14] = hcge_surface_color(HCGE_DSPF_ARGB, state->color);
	node[15] = 0x00000080u;
	node[16] = ((uint32_t)source->w << 16) / (uint32_t)destination->w;
	node[17] = 0;
	node[18] = (uint32_t)source->w << 15;
	node[19] = 0;
	node[20] = ((uint32_t)source->h << 16) / (uint32_t)destination->h;
	node[21] = (uint32_t)source->h << 15;
	return hcge_linux_submit(ctx, node, sizeof(node) / sizeof(node[0])) == 0;
}

bool hcge_draw_rect(hcge_context *ctx, HCGERectangle *rectangle)
{
	HCGERectangle edge;
	HCGEAccelerationMask saved;
	bool ok;

	if (!ctx || !hcge_rectangle_valid(rectangle))
		return false;
	saved = ctx->state.accel;
	ctx->state.accel = HCGE_DFXL_FILLRECTANGLE;
	edge = *rectangle;
	edge.h = 1;
	ok = hcge_fill_rect(ctx, &edge);
	if (rectangle->h > 1) {
		edge.y = rectangle->y + rectangle->h - 1;
		ok = hcge_fill_rect(ctx, &edge) && ok;
	}
	if (rectangle->h > 2) {
		edge.x = rectangle->x;
		edge.y = rectangle->y + 1;
		edge.w = 1;
		edge.h = rectangle->h - 2;
		ok = hcge_fill_rect(ctx, &edge) && ok;
		if (rectangle->w > 1) {
			edge.x = rectangle->x + rectangle->w - 1;
			ok = hcge_fill_rect(ctx, &edge) && ok;
		}
	}
	ctx->state.accel = saved;
	return ok;
}

void hcge_fill_rect_ext(hcge_context *ctx, HCGE_CoreSurfaceBuffer *dst,
	HCGE_CoreSurface *surface, HCGERectangle *rectangle, HCGEColor *color)
{
	if (!ctx || !dst || !surface || !rectangle || !color)
		return;
	memset(&ctx->state, 0, sizeof(ctx->state));
	ctx->state.destination = *surface;
	ctx->state.dst = *dst;
	ctx->state.color = *color;
	ctx->state.drawingflags = HCGE_DSDRAW_NOFX;
	ctx->state.accel = HCGE_DFXL_FILLRECTANGLE;
	(void)hcge_fill_rect(ctx, rectangle);
}

void hcge_engine_reset(void *drv, void *dev)
{
	(void)dev;
	hcge_hw_reset(drv);
}

void hcge_flush_texture_cache(void *drv, void *dev)
{
	(void)drv;
	(void)dev;
}

void hcge_emit_jommands(void *drv, void *dev)
{
	(void)drv;
	(void)dev;
}
