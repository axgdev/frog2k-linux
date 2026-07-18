/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define HCGE_REQUEST_IRQ 0x20002301u
#define HCGE_FREE_IRQ 0x20002302u
#define HCGE_RESET 0x20002303u
#define HCGE_SYNC_TIMEOUT 0x20002304u
#define HCGE_GET_CMDQ_BUFINFO 0x20002305u
#define HCGE_SET_CLOCK 0x20002306u
#define HCGE_GET_GE_REGISTER 0x20002307u
#define HCGE_SUBMIT 0x20002308u
#define QUEUE_PHYS 0x01000000u
#define QUEUE_SIZE 0x0003c000u

struct command_queue_info {
	uint32_t addr;
	uint32_t size;
};

struct linux_submit {
	uint32_t data;
	uint32_t length;
};

static uint32_t fake_registers[64];
static unsigned char fake_queue[QUEUE_SIZE] __attribute__((aligned(16)));
static uint32_t submitted_node[178];
static unsigned int submitted_words;

int __wrap_open(const char *path, int flags, ...)
{
	(void)path;
	(void)flags;
	return 7;
}

int __wrap_close(int fd)
{
	(void)fd;
	return 0;
}

int __wrap_ioctl(int fd, unsigned long command, ...)
{
	va_list args;
	void *argument;

	(void)fd;
	va_start(args, command);
	argument = va_arg(args, void *);
	va_end(args);
	switch (command) {
	case HCGE_GET_GE_REGISTER:
		*(uint32_t *)argument = (uint32_t)(uintptr_t)fake_registers;
		return 0;
	case HCGE_GET_CMDQ_BUFINFO: {
		struct command_queue_info *info = argument;

		info->addr = QUEUE_PHYS;
		info->size = QUEUE_SIZE;
		return 0;
	}
	case HCGE_REQUEST_IRQ:
	case HCGE_FREE_IRQ:
	case HCGE_RESET:
	case HCGE_SYNC_TIMEOUT:
	case HCGE_SET_CLOCK:
		return 0;
	case HCGE_SUBMIT: {
		const struct linux_submit *submit = argument;

		if (!submit || !submit->data || (submit->length & 3u) ||
		    submit->length > sizeof(submitted_node))
			return -1;
		submitted_words = submit->length / sizeof(uint32_t);
		memcpy(submitted_node, (const void *)(uintptr_t)submit->data,
			submit->length);
		return 0;
	}
	default:
		return 0;
	}
}

void *__wrap_mmap(void *address, size_t length, int protection, int flags,
	int fd, off_t offset)
{
	(void)address;
	(void)length;
	(void)protection;
	(void)flags;
	(void)fd;
	(void)offset;
	return fake_queue;
}

int __wrap_munmap(void *address, size_t length)
{
	(void)address;
	(void)length;
	return 0;
}

int __wrap_usleep(unsigned int usec)
{
	(void)usec;
	return 0;
}

static void setup_surface(HCGE_CoreSurface *surface,
	HCGE_CoreSurfaceBuffer *buffer, uint32_t physical, int width, int height)
{
	memset(surface, 0, sizeof(*surface));
	surface->config.format = HCGE_DSPF_RGB16;
	surface->config.size.w = width;
	surface->config.size.h = height;
	buffer->phys = physical;
	buffer->pitch = (unsigned int)width * 2u;
}

static void setup_state(hcge_state *state)
{
	const char *value;
	unsigned int mask_width = 128;
	unsigned int mask_height = 96;
	unsigned int mask_pitch;
	unsigned int mask_bytes = 1;

	memset(state, 0, sizeof(*state));
	state->render_options = HCGE_DSRO_NONE;
	state->drawingflags = HCGE_DSDRAW_NOFX;
	state->blittingflags = HCGE_DSBLIT_NOFX;
	state->src_blend = HCGE_DSBF_SRCALPHA;
	state->dst_blend = HCGE_DSBF_ZERO;
	state->clip.x1 = 0;
	state->clip.y1 = 0;
	state->clip.x2 = 319;
	state->clip.y2 = 239;
	state->mod_hw = HCGE_SMF_CLIP;
	setup_surface(&state->destination, &state->dst, 0x00200000u, 320, 240);
	setup_surface(&state->source, &state->src, 0x00300000u, 128, 96);
	value = getenv("HCGE_CAPTURE_MASK_WIDTH");
	if (value)
		mask_width = (unsigned int)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_MASK_HEIGHT");
	if (value)
		mask_height = (unsigned int)strtoul(value, NULL, 0);
	mask_pitch = mask_width;
	value = getenv("HCGE_CAPTURE_MASK_PITCH");
	if (value)
		mask_pitch = (unsigned int)strtoul(value, NULL, 0);
	setup_surface(&state->source_mask, &state->src_mask, 0x00400000u,
		(int)mask_width, (int)mask_height);
	state->source_mask.config.format = HCGE_DSPF_A8;
	value = getenv("HCGE_CAPTURE_MASK_FORMAT");
	if (value) {
		unsigned int index = (unsigned int)strtoul(value, NULL, 0);
		static const HCGESurfacePixelFormat formats[] = {
			HCGE_DSPF_ARGB1555, HCGE_DSPF_RGB16, HCGE_DSPF_RGB24,
			HCGE_DSPF_RGB32, HCGE_DSPF_ARGB, HCGE_DSPF_A8,
			HCGE_DSPF_LUT8, HCGE_DSPF_ARGB4444,
		};

		if (index < sizeof(formats) / sizeof(formats[0])) {
			state->source_mask.config.format = formats[index];
			mask_bytes = index == 5 || index == 6 ? 1 :
				(index == 2 ? 3 :
				 (index == 3 || index == 4 ? 4 : 2));
		}
	}
	state->src_mask.pitch = mask_pitch * mask_bytes;
	state->color.a = 0xff;
	state->color.r = 0x12;
	state->color.g = 0x34;
	state->color.b = 0x56;
	value = getenv("HCGE_CAPTURE_SRC_KEY");
	if (value)
		state->src_colorkey = (uint32_t)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_DST_KEY");
	if (value)
		state->dst_colorkey = (uint32_t)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_SRC_BLEND");
	if (value)
		state->src_blend = (HCGESurfaceBlendFunction)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_DST_BLEND");
	if (value)
		state->dst_blend = (HCGESurfaceBlendFunction)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_COLOR");
	if (value) {
		uint32_t color = (uint32_t)strtoul(value, NULL, 0);

		state->color.a = color >> 24;
		state->color.r = color >> 16;
		state->color.g = color >> 8;
		state->color.b = color;
	}
	value = getenv("HCGE_CAPTURE_MASK_X");
	if (value)
		state->src_mask_offset.x = (int)strtol(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_MASK_Y");
	if (value)
		state->src_mask_offset.y = (int)strtol(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_MASK_FLAGS");
	if (value)
		state->src_mask_flags = (HCGESurfaceMaskFlags)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_SRC_KEY_OP");
	if (value)
		state->src_colorkey_opt = (HCGEColorKeyOp)strtoul(value, NULL, 0);
	value = getenv("HCGE_CAPTURE_DST_KEY_OP");
	if (value)
		state->dst_colorkey_opt = (HCGEColorKeyOp)strtoul(value, NULL, 0);
}

static void dump_nodes(const char *operation, hcge_context *ctx)
{
	uint32_t node[178];
	uint32_t *last = node;
	unsigned int words;
	unsigned int i;

	memset(node, 0, sizeof(node));
#ifdef HCGE_SOURCE_CAPTURE
	(void)ctx;
	words = submitted_words;
	memcpy(node, submitted_node, words * sizeof(uint32_t));
#else
	hcge_construct_nodes(ctx, &last);
	words = (unsigned int)(last - node);
#endif

	printf("%s %u", operation, words);
	for (i = 0; i < words; i++)
		printf(" %08x", node[i]);
	putchar('\n');
}

int main(int argc, char **argv)
{
	hcge_context *ctx;
	hcge_state state;
	HCGERectangle source = { 4, 6, 64, 48 };
	HCGERectangle destination = { 10, 12, 160, 120 };
	int *coordinates[] = {
		&source.x, &source.y, &source.w, &source.h,
		&destination.x, &destination.y, &destination.w, &destination.h,
	};
	unsigned int coordinate;
	static const HCGESurfacePixelFormat formats[] = {
		HCGE_DSPF_ARGB1555, HCGE_DSPF_RGB16, HCGE_DSPF_RGB24,
		HCGE_DSPF_RGB32, HCGE_DSPF_ARGB, HCGE_DSPF_A8,
		HCGE_DSPF_LUT8, HCGE_DSPF_ARGB4444,
	};
	HCGESurfacePixelFormat format = HCGE_DSPF_RGB16;
	unsigned int bytes_per_pixel = 2;
	HCGESurfaceBlittingFlags blittingflags = HCGE_DSBLIT_NOFX;
	HCGESurfaceDrawingFlags drawingflags = HCGE_DSDRAW_NOFX;

	if (argc != 1 && argc != 9 && argc != 10 && argc != 12)
		return 2;
	for (coordinate = 0; coordinate < 8 && argc >= 9; coordinate++)
		*coordinates[coordinate] = (int)strtol(argv[coordinate + 1], NULL, 0);
	if (argc >= 10) {
		unsigned long index = strtoul(argv[9], NULL, 0);

		if (index >= sizeof(formats) / sizeof(formats[0]))
			return 2;
		format = formats[index];
		bytes_per_pixel = index == 5 || index == 6 ? 1 :
			(index == 2 ? 3 : (index == 3 || index == 4 ? 4 : 2));
	}
	if (argc == 12) {
		blittingflags = (HCGESurfaceBlittingFlags)strtoul(argv[10], NULL, 0);
		drawingflags = (HCGESurfaceDrawingFlags)strtoul(argv[11], NULL, 0);
	}

	memset(fake_queue, 0, sizeof(fake_queue));
	if (hcge_open(&ctx) != 0 || !ctx)
		return 1;
	setup_state(&state);
	state.destination.config.format = format;
	state.dst.pitch = 320u * bytes_per_pixel;
	state.drawingflags = drawingflags;

	state.accel = HCGE_DFXL_FILLRECTANGLE;
	ctx->state = state;
	hcge_set_state(ctx, &ctx->state, ctx->state.accel);
	if (!hcge_fill_rect(ctx, &destination))
		return 1;
	dump_nodes("fill-rgb16", ctx);
	hcge_close(ctx);

	memset(fake_queue, 0, sizeof(fake_queue));
	memset(fake_registers, 0, sizeof(fake_registers));
	if (hcge_open(&ctx) != 0 || !ctx)
		return 1;
	setup_state(&state);
	state.destination.config.format = format;
	state.dst.pitch = 320u * bytes_per_pixel;
	state.source.config.format = format;
	state.src.pitch = 128u * bytes_per_pixel;
	state.blittingflags = blittingflags;
	state.accel = HCGE_DFXL_BLIT;
	ctx->state = state;
	hcge_set_state(ctx, &ctx->state, ctx->state.accel);
	if (!hcge_blit(ctx, &source, destination.x, destination.y))
		return 1;
	dump_nodes("blit-rgb16", ctx);
	hcge_close(ctx);

	memset(fake_queue, 0, sizeof(fake_queue));
	memset(fake_registers, 0, sizeof(fake_registers));
	if (hcge_open(&ctx) != 0 || !ctx)
		return 1;
	setup_state(&state);
	state.destination.config.format = format;
	state.dst.pitch = 320u * bytes_per_pixel;
	state.source.config.format = format;
	state.src.pitch = 128u * bytes_per_pixel;
	state.blittingflags = blittingflags;
	state.accel = HCGE_DFXL_STRETCHBLIT;
	ctx->state = state;
	hcge_set_state(ctx, &ctx->state, ctx->state.accel);
	if (!hcge_stretch_blit(ctx, &source, &destination))
		return 1;
	dump_nodes("stretch-rgb16", ctx);
	hcge_close(ctx);
#ifdef HCGE_SOURCE_CAPTURE
	/* Exercise the no-heap lifetime used by the SF2000 screen service. */
	{
		hcge_context storage;

		if (hcge_open_context(&storage) != 0)
			return 1;
		hcge_close_context(&storage);
		if (storage.ge_fd != -1 || storage.nd_ctx != NULL)
			return 1;
	}
#endif
	return 0;
}
