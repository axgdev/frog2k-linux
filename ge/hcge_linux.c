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
