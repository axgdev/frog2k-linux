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

static bool hcge_format_supported(HCGESurfacePixelFormat format, bool source)
{
	switch (format) {
	case HCGE_DSPF_ARGB1555:
	case HCGE_DSPF_RGB16:
	case HCGE_DSPF_RGB24:
	case HCGE_DSPF_RGB32:
	case HCGE_DSPF_ARGB:
	case HCGE_DSPF_A8:
	case HCGE_DSPF_LUT8:
	case HCGE_DSPF_ARGB4444:
		return true;
	default:
		(void)source;
		return false;
	}
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

static uint32_t hcge_rgb565(HCGEColor color)
{
	return ((uint32_t)(color.r & 0xf8u) << 8) |
		((uint32_t)(color.g & 0xfcu) << 3) | color.b >> 3;
}

static bool hcge_rgb16_surface_valid(const HCGE_CoreSurface *surface,
	const HCGE_CoreSurfaceBuffer *buffer)
{
	return surface && buffer && surface->config.format == HCGE_DSPF_RGB16 &&
		surface->config.size.w > 0 && surface->config.size.h > 0 &&
		buffer->phys && !(buffer->phys & 1u) && buffer->pitch >= 2u &&
		!(buffer->pitch & 1u) && buffer->pitch <= HCGE_IMAGE_PITCH_MAX;
}

static uint32_t hcge_rgb16_buffer(uint32_t pitch)
{
	return 0x00006000u | ((pitch / 2u) & 0xfffu);
}

static bool hcge_rectangle_valid(const HCGERectangle *rectangle)
{
	return rectangle && rectangle->x >= 0 && rectangle->y >= 0 &&
		rectangle->w > 0 && rectangle->h > 0 && rectangle->x <= 0xfff &&
		rectangle->y <= 0xfff && rectangle->w <= 0xfff &&
		rectangle->h <= 0xfff;
}

int hcge_open(hcge_context **pctx)
{
	struct hcge_cmdq_buf_info queue = { 0, 0 };
	uint32_t registers = 0;
	hcge_context *ctx;
	int fd;

	if (!pctx)
		return -EINVAL;
	*pctx = NULL;
	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return -ENOMEM;
	fd = open("/dev/ge", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		free(ctx);
		return -errno;
	}
	ctx->ge_fd = fd;
	if (ioctl(fd, HCGE_GET_GE_REGISTER, &registers) < 0 ||
	    ioctl(fd, HCGE_GET_CMDQ_BUFINFO, &queue) < 0 || !queue.addr ||
	    queue.size <= 1056u) {
		int error = errno ? -errno : -ENODEV;

		close(fd);
		free(ctx);
		return error;
	}
	ctx->ge_regs = (volatile struct ge_reg_buf *)(uintptr_t)registers;
	ctx->cmdq_buf_map_phyaddr = queue.addr;
	ctx->cmdq_buf_map_size = queue.size;
	ctx->cmdq_buf_phyaddr = queue.addr + 1056u;
	ctx->cmdq_buf_size = queue.size - 1056u;
	ctx->clut_tbl_paddr = queue.addr + 32u;
	ctx->nd_ctx = calloc(1, 680u);
	if (!ctx->nd_ctx || ioctl(fd, HCGE_REQUEST_IRQ, 0) < 0) {
		int error = errno ? -errno : -ENOMEM;

		free(ctx->nd_ctx);
		close(fd);
		free(ctx);
		return error;
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

int hcge_linux_submit(hcge_context *ctx, const uint32_t *node, size_t words)
{
	struct hcge_linux_submit submit;

	if (hcge_context_fd(ctx) < 0 || !node || !words ||
	    words > ctx->cmdq_buf_size / sizeof(*node))
		return -EINVAL;
	submit.data = (uint32_t)(uintptr_t)node;
	submit.length = (uint32_t)(words * sizeof(*node));
	return ioctl(ctx->ge_fd, HCGE_SUBMIT, &submit) < 0 ? -errno : 0;
}

void hcge_check_state(hcge_state *state, HCGEAccelerationMask accel)
{
	if (!state || !(accel & (HCGE_DFXL_FILLRECTANGLE | HCGE_DFXL_BLIT |
				 HCGE_DFXL_STRETCHBLIT)))
		return;
	if (!hcge_format_supported(state->destination.config.format, false))
		return;
	if (HCGE_DFB_DRAWING_FUNCTION(accel)) {
		if (state->render_options != HCGE_DSRO_NONE &&
		    state->render_options != HCGE_DSRO_MATRIX)
			return;
		if ((accel & HCGE_DFXL_FILLRECTANGLE) &&
		    SUPPORTED_DRAW_FLAG(state->drawingflags))
			state->accel |= accel & HCGE_DFXL_FILLRECTANGLE;
	}
	if (HCGE_DFB_BLITTING_FUNCTION(accel) &&
	    hcge_format_supported(state->source.config.format, true) &&
	    SUPPORTED_BLIT_FLAG(state->blittingflags))
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
	    !hcge_rgb16_surface_valid(&state->destination, &state->dst))
		return false;
	node[0] = 0x02008367u;
	node[1] = 0x00a00003u;
	node[2] = (uint32_t)state->dst.phys;
	node[3] = hcge_rgb16_buffer(state->dst.pitch);
	node[4] = node[2];
	node[5] = node[3];
	node[6] = hcge_rgb565(state->color);
	node[7] = 0;
	node[8] = 0;
	node[9] = 6;
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
	    !hcge_rgb16_surface_valid(&state->destination, &state->dst) ||
	    !hcge_rgb16_surface_valid(&state->source, &state->src))
		return false;
	node[0] = 0x02000307u;
	node[1] = 0x00000002u;
	node[2] = (uint32_t)state->dst.phys;
	node[3] = hcge_rgb16_buffer(state->dst.pitch);
	node[4] = (uint32_t)state->src.phys;
	node[5] = hcge_rgb16_buffer(state->src.pitch);
	node[6] = hcge_xy(dx, dy);
	node[7] = hcge_wh(source->w, source->h);
	node[8] = hcge_xy(source->x, source->y);
	return hcge_linux_submit(ctx, node, sizeof(node) / sizeof(node[0])) == 0;
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
