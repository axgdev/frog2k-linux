/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define HCGE_REQUEST_IRQ 0x20002301u
#define HCGE_FREE_IRQ 0x20002302u
#define HCGE_RESET 0x20002303u
#define HCGE_SYNC_TIMEOUT 0x20002304u
#define HCGE_GET_CMDQ_BUFINFO 0x20002305u
#define HCGE_SET_CLOCK 0x20002306u
#define HCGE_GET_GE_REGISTER 0x20002307u
#define QUEUE_PHYS 0x01000000u
#define QUEUE_SIZE 0x0003c000u

struct command_queue_info {
	uint32_t addr;
	uint32_t size;
};

extern void hcge_construct_nodes(void *context, uint32_t **output);

static uint32_t fake_registers[64];
static unsigned char fake_queue[QUEUE_SIZE] __attribute__((aligned(16)));

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
	state->color.a = 0xff;
	state->color.r = 0x12;
	state->color.g = 0x34;
	state->color.b = 0x56;
}

static void dump_nodes(const char *operation, hcge_context *ctx)
{
	uint32_t node[178];
	uint32_t *last = node;
	unsigned int words;
	unsigned int i;

	memset(node, 0, sizeof(node));
	hcge_construct_nodes(ctx, &last);
	words = (unsigned int)(last - node);

	printf("%s %u", operation, words);
	for (i = 0; i < words; i++)
		printf(" %08x", node[i]);
	putchar('\n');
}

int main(void)
{
	hcge_context *ctx;
	hcge_state state;
	HCGERectangle source = { 4, 6, 64, 48 };
	HCGERectangle destination = { 10, 12, 160, 120 };

	memset(fake_queue, 0, sizeof(fake_queue));
	if (hcge_open(&ctx) != 0 || !ctx)
		return 1;
	setup_state(&state);

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
	state.accel = HCGE_DFXL_STRETCHBLIT;
	ctx->state = state;
	hcge_set_state(ctx, &ctx->state, ctx->state.accel);
	if (!hcge_stretch_blit(ctx, &source, &destination))
		return 1;
	dump_nodes("stretch-rgb16", ctx);
	hcge_close(ctx);
	return 0;
}
