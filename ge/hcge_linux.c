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
		surface->config.size.w <= 0xfff &&
		surface->config.size.h > 0 && surface->config.size.h <= 0xfff &&
		buffer->phys &&
		!(buffer->phys & (format->bytes - 1u)) &&
		buffer->pitch >= (uint32_t)surface->config.size.w * format->bytes &&
		!(buffer->pitch % format->bytes) &&
		buffer->pitch / format->bytes <= 0xfff;
}

static bool hcge_mask_surface_valid(const hcge_state *state)
{
	return state && state->source_mask.config.format == HCGE_DSPF_A8 &&
		state->source_mask.config.size.w > 0 &&
		state->source_mask.config.size.w <= 0xfff &&
		state->source_mask.config.size.h > 0 &&
		state->source_mask.config.size.h <= 0xfff &&
		state->src_mask.phys && state->src_mask.pitch >=
			(uint32_t)state->source_mask.config.size.w &&
		state->src_mask.pitch <= 0xfff;
}

static uint32_t hcge_mask_buffer(const hcge_state *state)
{
	/* A8 is hardware format 29; bit 16 enables mask-alpha extraction. */
	return 0x0001d000u | (state->src_mask.pitch & 0xfffu);
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

static bool hcge_rectangle_in_surface(const HCGERectangle *rectangle,
	const HCGE_CoreSurface *surface)
{
	return hcge_rectangle_valid(rectangle) && surface &&
		rectangle->x + rectangle->w <= surface->config.size.w &&
		rectangle->y + rectangle->h <= surface->config.size.h;
}

static uint32_t hcge_surface_offset(const HCGE_CoreSurface *surface,
	const HCGE_CoreSurfaceBuffer *buffer, int x, int y)
{
	const struct hcge_format *format = hcge_get_format(surface->config.format);

	return (uint32_t)buffer->phys + (uint32_t)y * buffer->pitch +
		(uint32_t)x * format->bytes;
}

static bool hcge_blit_effects_supported(HCGESurfaceBlittingFlags flags)
{
	const uint32_t supported = HCGE_DSBLIT_BLEND_ALPHACHANNEL |
		HCGE_DSBLIT_BLEND_COLORALPHA | HCGE_DSBLIT_COLORIZE |
		HCGE_DSBLIT_SRC_COLORKEY | HCGE_DSBLIT_DST_COLORKEY |
		HCGE_DSBLIT_SRC_PREMULTIPLY | HCGE_DSBLIT_DST_PREMULTIPLY |
		HCGE_DSBLIT_DEMULTIPLY | HCGE_DSBLIT_SRC_PREMULTCOLOR |
		HCGE_DSBLIT_XOR | HCGE_DSBLIT_COLORKEY_PROTECT |
		HCGE_DSBLIT_SRC_MASK_ALPHA |
		HCGE_DSBLIT_FLIP_HORIZONTAL |
		HCGE_DSBLIT_FLIP_VERTICAL | HCGE_DSBLIT_ROTATE90 |
		HCGE_DSBLIT_ROTATE180 | HCGE_DSBLIT_ROTATE270 |
		HCGE_CUST_DST_COLORKEY | HCGE_CUST_SRC_COLORKEY;
	uint32_t rotations = flags & (HCGE_DSBLIT_ROTATE90 |
		HCGE_DSBLIT_ROTATE180 | HCGE_DSBLIT_ROTATE270);

	return !(flags & ~supported) && (!rotations || !(rotations & (rotations - 1u)));
}

static bool hcge_drawing_effects_supported(HCGESurfaceDrawingFlags flags)
{
	const uint32_t supported = HCGE_DSDRAW_BLEND |
		HCGE_DSDRAW_DST_COLORKEY | HCGE_DSDRAW_SRC_PREMULTIPLY |
		HCGE_DSDRAW_DST_PREMULTIPLY | HCGE_DSDRAW_DEMULTIPLY |
		HCGE_DSDRAW_XOR;

	return !(flags & ~supported);
}

static HCGEColor hcge_premultiply_color(HCGEColor color)
{
	unsigned int alpha = (unsigned int)color.a + 1u;

	color.r = (uint8_t)((unsigned int)color.r * alpha >> 8);
	color.g = (uint8_t)((unsigned int)color.g * alpha >> 8);
	color.b = (uint8_t)((unsigned int)color.b * alpha >> 8);
	return color;
}

static bool hcge_blend_functions_valid(const hcge_state *state, bool enabled)
{
	return !enabled || (state->src_blend >= HCGE_DSBF_ZERO &&
		state->src_blend <= HCGE_DSBF_SRCALPHASAT &&
		state->dst_blend >= HCGE_DSBF_ZERO &&
		state->dst_blend <= HCGE_DSBF_SRCALPHASAT);
}

static uint32_t hcge_color_key_argb(HCGESurfacePixelFormat format,
	uint32_t color)
{
	uint32_t red;
	uint32_t green;
	uint32_t blue;

	switch (format) {
	case HCGE_DSPF_ARGB1555:
		blue = color & 0x1fu;
		/* Preserve the vendor's bit-spread operation, including alpha. */
		return ((color << 4) & 0x00070000u) |
			((color << 1) & 0x00000700u) | (blue >> 2) |
			((color << 9) & 0x07f80000u) |
			((color << 6) & 0x0007f800u) | (blue << 3);
	case HCGE_DSPF_RGB16:
		red = color >> 11 & 0x1fu;
		green = color >> 5 & 0x3fu;
		blue = color & 0x1fu;
		return ((red << 3) | (red >> 2)) << 16 |
			((green << 2) | (green >> 4)) << 8 |
			(blue << 3) | (blue >> 2);
	case HCGE_DSPF_ARGB4444:
		red = color >> 8 & 0xfu;
		green = color >> 4 & 0xfu;
		blue = color & 0xfu;
		return (red << 4 | red) << 16 | (green << 4 | green) << 8 |
			(blue << 4 | blue);
	default:
		return color;
	}
}

static uint32_t hcge_blit_rop(const hcge_state *state)
{
	uint32_t flags = state->blittingflags;
	uint32_t blend = flags & (HCGE_DSBLIT_BLEND_ALPHACHANNEL |
		HCGE_DSBLIT_BLEND_COLORALPHA);
	uint32_t rop = flags & HCGE_DSBLIT_SRC_MASK_ALPHA ?
		0x00008421u : 0x00004000u;

	if (blend) {
		rop = 0x00008000u | (((uint32_t)state->src_blend & 0xfu) << 4) |
			((uint32_t)state->dst_blend & 0xfu);
		if (blend == HCGE_DSBLIT_BLEND_COLORALPHA)
			rop |= 0x00000200u;
		else if (blend == (HCGE_DSBLIT_BLEND_ALPHACHANNEL |
				HCGE_DSBLIT_BLEND_COLORALPHA))
			rop |= 0x00000400u;
		else if (flags & HCGE_DSBLIT_SRC_MASK_ALPHA)
			rop |= 0x00000400u;
	}
	if (flags & HCGE_DSBLIT_COLORIZE)
		rop += 0x00000600u;
	if (flags & HCGE_DSBLIT_SRC_PREMULTIPLY)
		rop |= 0x00001000u;
	if (flags & HCGE_DSBLIT_DST_PREMULTIPLY)
		rop |= 0x00002000u;
	if (flags & HCGE_DSBLIT_SRC_PREMULTCOLOR)
		rop |= 0x00000100u;
	if (flags & HCGE_DSBLIT_DEMULTIPLY)
		rop |= 0x00040000u;
	if (flags & HCGE_DSBLIT_XOR)
		rop |= 0x00080000u;
	if (flags & HCGE_DSBLIT_SRC_COLORKEY)
		rop |= 0xe0800000u;
	if (flags & HCGE_DSBLIT_DST_COLORKEY)
		rop |= 0x0e100000u;
	if (flags & HCGE_CUST_SRC_COLORKEY) {
		static const uint32_t operations[HCGE_CKEY_OP_MAX] = {
			0xe0400000u, 0xe0800000u, 0x10800000u,
			0x10400000u, 0xe0800000u, 0xe0400000u,
		};

		rop |= operations[state->src_colorkey_opt];
	}
	if (flags & HCGE_CUST_DST_COLORKEY) {
		static const uint32_t operations[HCGE_CKEY_OP_MAX] = {
			0x0e100000u, 0x0e200000u, 0x01200000u,
			0x01100000u, 0x0e200000u, 0x0e100000u,
		};

		rop |= operations[state->dst_colorkey_opt];
	}
	return rop;
}

static bool hcge_blit_state_valid(const hcge_state *state)
{
	uint32_t flags = state->blittingflags;

	if ((flags & HCGE_DSBLIT_SRC_MASK_ALPHA) &&
	    !hcge_mask_surface_valid(state))
		return false;
	if ((flags & HCGE_CUST_SRC_COLORKEY) &&
	    state->src_colorkey_opt >= HCGE_CKEY_OP_MAX)
		return false;
	if ((flags & HCGE_CUST_DST_COLORKEY) &&
	    state->dst_colorkey_opt >= HCGE_CKEY_OP_MAX)
		return false;
	return true;
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

void hcge_close_context(hcge_context *ctx)
{
	if (!ctx)
		return;
	if (ctx->ge_fd >= 0) {
		(void)hcge_engine_sync(ctx);
		(void)ioctl(ctx->ge_fd, HCGE_FREE_IRQ, 0);
		close(ctx->ge_fd);
	}
	free(ctx->nd_ctx);
	memset(ctx, 0, sizeof(*ctx));
	ctx->ge_fd = -1;
}

void hcge_close(hcge_context *ctx)
{
	if (!ctx)
		return;
	hcge_close_context(ctx);
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
		    !hcge_drawing_effects_supported(state->drawingflags) ||
		    !hcge_blend_functions_valid(state,
			state->drawingflags & HCGE_DSDRAW_BLEND))
			return;
		if (accel & HCGE_DFXL_FILLRECTANGLE)
			state->accel |= accel & HCGE_DFXL_FILLRECTANGLE;
	}
	if (HCGE_DFB_BLITTING_FUNCTION(accel) &&
	    hcge_get_format(state->source.config.format) &&
	    state->render_options == HCGE_DSRO_NONE &&
	    hcge_blit_effects_supported(state->blittingflags) &&
	    hcge_blit_state_valid(state) &&
	    hcge_blend_functions_valid(state, state->blittingflags &
		(HCGE_DSBLIT_BLEND_ALPHACHANNEL |
		 HCGE_DSBLIT_BLEND_COLORALPHA))) {
		state->accel |= accel & HCGE_DFXL_BLIT;
		if (!(state->blittingflags & (HCGE_DSBLIT_ROTATE90 |
				HCGE_DSBLIT_ROTATE180 | HCGE_DSBLIT_ROTATE270)))
			state->accel |= accel & HCGE_DFXL_STRETCHBLIT;
	}
}

void hcge_check_blit_state(hcge_context *ctx, hcge_state *state,
			   HCGEAccelerationMask accel)
{
	(void)ctx;
	if (!state)
		return;
	/* The vendor entry point is the blitting half of CheckState. */
	hcge_check_state(state, accel & HCGE_DFXL_ALL_BLIT);
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
	uint32_t node[16];
	uint32_t flags;
	uint32_t rop;
	HCGEColor color;
	bool blend;
	unsigned int words = 0;

	if (!ctx)
		return false;
	state = &ctx->state;
	if (state->accel != HCGE_DFXL_FILLRECTANGLE ||
	    !hcge_drawing_effects_supported(state->drawingflags) ||
	    !hcge_blend_functions_valid(state,
		state->drawingflags & HCGE_DSDRAW_BLEND) ||
	    !hcge_rectangle_in_surface(rectangle, &state->destination) ||
	    !hcge_surface_valid(&state->destination, &state->dst))
		return false;
	flags = state->drawingflags;
	blend = flags & HCGE_DSDRAW_BLEND;
	color = state->color;
	if (flags & HCGE_DSDRAW_SRC_PREMULTIPLY)
		color = hcge_premultiply_color(color);
	rop = blend ? 0x00008000u |
		(((uint32_t)state->src_blend & 0xfu) << 4) |
		((uint32_t)state->dst_blend & 0xfu) : 0x00030000u;
	if (flags & HCGE_DSDRAW_DST_COLORKEY)
		rop |= 0x0e100000u;
	if (flags & HCGE_DSDRAW_DST_PREMULTIPLY)
		rop |= 0x00002000u;
	if (flags & HCGE_DSDRAW_DEMULTIPLY)
		rop |= 0x00040000u;
	if (flags & HCGE_DSDRAW_XOR)
		rop |= 0x00080000u;

	node[words++] = 0x02008367u |
		(flags & HCGE_DSDRAW_DST_COLORKEY ? 0x80u : 0);
	node[words++] = blend ? 0x00a00301u : 0x00a00003u;
	node[words++] = (uint32_t)state->dst.phys;
	node[words++] = hcge_surface_buffer(state->destination.config.format,
		state->dst.pitch);
	node[words++] = node[2];
	node[words++] = node[3];
	node[words++] = hcge_surface_color(blend ? HCGE_DSPF_ARGB :
		state->destination.config.format, color);
	node[words++] = 0;
	node[words++] = 0;
	node[words++] = blend ? 1u :
		hcge_get_format(state->destination.config.format)->code;
	if (flags & HCGE_DSDRAW_DST_COLORKEY) {
		node[words++] = state->dst_colorkey;
		node[words++] = 0;
	}
	node[words++] = hcge_xy(rectangle->x, rectangle->y);
	node[words++] = hcge_wh(rectangle->w, rectangle->h);
	node[words++] = hcge_xy(rectangle->x, rectangle->y);
	node[words++] = rop;
	return hcge_linux_submit(ctx, node, words) == 0;
}

bool hcge_blit(hcge_context *ctx, HCGERectangle *source, int dx, int dy)
{
	const hcge_state *state;
	uint32_t node[28];
	uint32_t flags;
	uint32_t source_context;
	uint32_t destination_address;
	uint32_t source_address;
	uint32_t output_width;
	uint32_t output_height;
	unsigned int words = 0;
	const uint32_t *matrix = NULL;
	static const uint32_t rotate_90[7] = {
		0, 0, 0x8000ffffu, 0, 0x0000ffffu, 0, 0,
	};
	static const uint32_t rotate_180[7] = {
		0, 0x8000ffffu, 0x80000000u, 0, 0, 0x8000ffffu, 0,
	};
	static const uint32_t rotate_270[7] = {
		0, 0x80000000u, 0x0000ffffu, 0,
		0x8000ffffu, 0x80000000u, 0,
	};

	if (!ctx || dx < 0 || dy < 0 ||
	    dx > 0xfff || dy > 0xfff)
		return false;
	state = &ctx->state;
	if (state->accel != HCGE_DFXL_BLIT ||
	    !hcge_blit_effects_supported(state->blittingflags) ||
	    !hcge_blit_state_valid(state) ||
	    !hcge_blend_functions_valid(state, state->blittingflags &
		(HCGE_DSBLIT_BLEND_ALPHACHANNEL |
		 HCGE_DSBLIT_BLEND_COLORALPHA)) ||
	    !hcge_rectangle_in_surface(source, &state->source) ||
	    !hcge_surface_valid(&state->destination, &state->dst) ||
	    !hcge_surface_valid(&state->source, &state->src))
		return false;
	flags = state->blittingflags;
	if (flags & HCGE_DSBLIT_SRC_MASK_ALPHA) {
		int mask_x = state->src_mask_flags & HCGE_DSMF_STENCIL ?
			state->src_mask_offset.x : source->x;
		int mask_y = state->src_mask_flags & HCGE_DSMF_STENCIL ?
			state->src_mask_offset.y : source->y;

		if (mask_x < 0 || mask_y < 0 ||
		    mask_x + source->w > state->source_mask.config.size.w ||
		    mask_y + source->h > state->source_mask.config.size.h)
			return false;
	}
	output_width = source->w;
	output_height = source->h;
	if (flags & (HCGE_DSBLIT_ROTATE90 | HCGE_DSBLIT_ROTATE270)) {
		output_width = source->h;
		output_height = source->w;
	}
	if ((uint32_t)dx + output_width >
			(uint32_t)state->destination.config.size.w ||
		(uint32_t)dy + output_height >
			(uint32_t)state->destination.config.size.h)
		return false;

	if (flags == HCGE_DSBLIT_NOFX &&
	    state->destination.config.format == state->source.config.format) {
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
		return hcge_linux_submit(ctx, node, 9) == 0;
	}

	/* Generic compositor path: cropped surfaces are zero-origin views. */
	destination_address = hcge_surface_offset(&state->destination, &state->dst,
		dx, dy);
	source_address = hcge_surface_offset(&state->source, &state->src,
		source->x, source->y);
	source_context = hcge_surface_buffer(state->source.config.format,
		state->src.pitch);
	if (flags & HCGE_DSBLIT_FLIP_HORIZONTAL)
		source_context |= 0x00100000u;
	if (flags & HCGE_DSBLIT_FLIP_VERTICAL)
		source_context |= 0x00200000u;
	if (flags & HCGE_DSBLIT_ROTATE90)
		matrix = rotate_90;
	else if (flags & HCGE_DSBLIT_ROTATE180)
		matrix = rotate_180;
	else if (flags & HCGE_DSBLIT_ROTATE270)
		matrix = rotate_270;

	node[words++] = (matrix ? 0x0206870fu : 0x0202870fu) |
		(flags & HCGE_DSBLIT_SRC_MASK_ALPHA ? 0x00000810u : 0) |
		(flags & (HCGE_DSBLIT_SRC_COLORKEY | HCGE_DSBLIT_DST_COLORKEY |
			HCGE_CUST_SRC_COLORKEY | HCGE_CUST_DST_COLORKEY) ?
			0x80u : 0);
	node[words++] = (matrix ? 0x00a03009u : 0x00a00009u) |
		(flags & HCGE_DSBLIT_SRC_MASK_ALPHA ? 0x40u : 0);
	node[words++] = destination_address;
	node[words++] = hcge_surface_buffer(state->destination.config.format,
		state->dst.pitch);
	node[words++] = destination_address;
	node[words++] = node[3];
	node[words++] = source_address;
	node[words++] = (matrix ? 0x40000000u : 0) | source_context;
	if (flags & HCGE_DSBLIT_SRC_MASK_ALPHA) {
		node[words++] = (uint32_t)state->src_mask.phys;
		node[words++] = hcge_mask_buffer(state);
	}
	if (flags & (HCGE_DSBLIT_SRC_COLORKEY | HCGE_DSBLIT_DST_COLORKEY |
		HCGE_CUST_SRC_COLORKEY | HCGE_CUST_DST_COLORKEY)) {
		node[words++] = flags & (HCGE_DSBLIT_DST_COLORKEY |
			HCGE_CUST_DST_COLORKEY) ?
			hcge_color_key_argb(state->destination.config.format,
				state->dst_colorkey) : 0;
		node[words++] = flags & (HCGE_DSBLIT_SRC_COLORKEY |
			HCGE_CUST_SRC_COLORKEY) ?
			hcge_color_key_argb(state->source.config.format,
				state->src_colorkey) : 0;
	}
	node[words++] = 0;
	node[words++] = hcge_wh(output_width, output_height);
	node[words++] = 0;
	node[words++] = 0;
	node[words++] = hcge_wh(source->w, source->h);
	if (flags & HCGE_DSBLIT_SRC_MASK_ALPHA) {
		int mask_x = state->src_mask_flags & HCGE_DSMF_STENCIL ?
			state->src_mask_offset.x : source->x;
		int mask_y = state->src_mask_flags & HCGE_DSMF_STENCIL ?
			state->src_mask_offset.y : source->y;
		int margin_x = state->source_mask.config.size.w -
			((state->src_mask_flags & HCGE_DSMF_STENCIL) ?
			 mask_x : source->w);
		int margin_y = state->source_mask.config.size.h -
			((state->src_mask_flags & HCGE_DSMF_STENCIL) ?
			 mask_y : source->h);

		node[words++] = hcge_xy(mask_x, mask_y);
		node[words++] = hcge_wh(margin_x, margin_y);
	}
	node[words++] = hcge_blit_rop(state);
	node[words++] = hcge_surface_color(HCGE_DSPF_ARGB, state->color);
	if (matrix) {
		memcpy(node + words, matrix, 7 * sizeof(*node));
		words += 7;
	}
	return hcge_linux_submit(ctx, node, words) == 0;
}

bool hcge_stretch_blit(hcge_context *ctx, HCGERectangle *source,
	HCGERectangle *destination)
{
	const hcge_state *state;
	uint32_t node[26];

	const struct hcge_format *source_format;
	const struct hcge_format *destination_format;
	uint32_t source_address;
	uint32_t destination_address;
	uint32_t flags;
	unsigned int words = 0;

	if (!ctx)
		return false;
	state = &ctx->state;
	if (state->accel != HCGE_DFXL_STRETCHBLIT ||
	    !hcge_blit_effects_supported(state->blittingflags) ||
	    !hcge_blit_state_valid(state) ||
	    !hcge_blend_functions_valid(state, state->blittingflags &
		(HCGE_DSBLIT_BLEND_ALPHACHANNEL |
		 HCGE_DSBLIT_BLEND_COLORALPHA)) ||
	    (state->blittingflags & (HCGE_DSBLIT_ROTATE90 |
		HCGE_DSBLIT_ROTATE180 | HCGE_DSBLIT_ROTATE270)) ||
	    !hcge_rectangle_in_surface(source, &state->source) ||
	    !hcge_rectangle_in_surface(destination, &state->destination) ||
	    !hcge_surface_valid(&state->destination, &state->dst) ||
	    !hcge_surface_valid(&state->source, &state->src))
		return false;
	source_format = hcge_get_format(state->source.config.format);
	destination_format = hcge_get_format(state->destination.config.format);
	source_address = (uint32_t)state->src.phys +
		(uint32_t)source->y * state->src.pitch +
		(uint32_t)source->x * source_format->bytes;
	destination_address = (uint32_t)state->dst.phys +
		(uint32_t)destination->y * state->dst.pitch +
		(uint32_t)destination->x * destination_format->bytes;
	flags = state->blittingflags;
	if (flags & HCGE_DSBLIT_SRC_MASK_ALPHA) {
		int mask_x = state->src_mask_flags & HCGE_DSMF_STENCIL ?
			state->src_mask_offset.x : source->x;
		int mask_y = state->src_mask_flags & HCGE_DSMF_STENCIL ?
			state->src_mask_offset.y : source->y;

		if (mask_x < 0 || mask_y < 0 ||
		    mask_x + source->w > state->source_mask.config.size.w ||
		    mask_y + source->h > state->source_mask.config.size.h)
			return false;
	}
	node[words++] = 0x0206870fu |
		(flags & HCGE_DSBLIT_SRC_MASK_ALPHA ? 0x10u : 0) |
		(flags & (HCGE_DSBLIT_SRC_COLORKEY | HCGE_DSBLIT_DST_COLORKEY |
			HCGE_CUST_SRC_COLORKEY | HCGE_CUST_DST_COLORKEY) ?
			0x80u : 0);
	node[words++] = 0x00a03009u |
		(flags & HCGE_DSBLIT_SRC_MASK_ALPHA ? 0x40u : 0);
	node[words++] = destination_address;
	node[words++] = hcge_surface_buffer(state->destination.config.format,
		state->dst.pitch);
	node[words++] = destination_address;
	node[words++] = node[3];
	node[words++] = source_address;
	node[words++] = 0x40000000u |
		hcge_surface_buffer(state->source.config.format, state->src.pitch);
	if (flags & HCGE_DSBLIT_FLIP_HORIZONTAL)
		node[7] |= 0x00100000u;
	if (flags & HCGE_DSBLIT_FLIP_VERTICAL)
		node[7] |= 0x00200000u;
	if (flags & HCGE_DSBLIT_SRC_MASK_ALPHA) {
		node[words++] = (uint32_t)state->src_mask.phys;
		node[words++] = hcge_mask_buffer(state);
	}
	if (flags & (HCGE_DSBLIT_SRC_COLORKEY | HCGE_DSBLIT_DST_COLORKEY |
		HCGE_CUST_SRC_COLORKEY | HCGE_CUST_DST_COLORKEY)) {
		node[words++] = flags & (HCGE_DSBLIT_DST_COLORKEY |
			HCGE_CUST_DST_COLORKEY) ?
			hcge_color_key_argb(state->destination.config.format,
				state->dst_colorkey) : 0;
		node[words++] = flags & (HCGE_DSBLIT_SRC_COLORKEY |
			HCGE_CUST_SRC_COLORKEY) ?
			hcge_color_key_argb(state->source.config.format,
				state->src_colorkey) : 0;
	}
	node[words++] = 0;
	node[words++] = hcge_wh(destination->w, destination->h);
	node[words++] = 0;
	node[words++] = 0;
	node[words++] = hcge_wh(source->w, source->h);
	node[words++] = hcge_blit_rop(state);
	node[words++] = hcge_surface_color(HCGE_DSPF_ARGB, state->color);
	node[words++] = 0x00000080u;
	node[words++] = ((uint32_t)source->w << 16) /
		(uint32_t)destination->w;
	node[words++] = 0;
	node[words++] = (uint32_t)source->w << 15;
	node[words++] = 0;
	node[words++] = ((uint32_t)source->h << 16) /
		(uint32_t)destination->h;
	node[words++] = (uint32_t)source->h << 15;
	return hcge_linux_submit(ctx, node, words) == 0;
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
