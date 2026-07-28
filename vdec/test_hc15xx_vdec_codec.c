// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hc15xx_vdec_codec.h"

#define REG_COUNT 1024
#define SYS_REG_COUNT 256
#define GMA_REG_COUNT 64

struct fake {
	uint32_t regs[REG_COUNT];
	uint32_t sys_regs[SYS_REG_COUNT];
	uint32_t gma_regs[GMA_REG_COUNT];
	unsigned int delay_count;
	unsigned int flush_count;
	unsigned int invalidate_count;
	struct hc15xx_vdec_io io;
};

static uint32_t fake_read32(void *cookie, uintptr_t address)
{
	struct fake *f = cookie;

	if (address >= 0x18810000 && address < 0x18810000 + REG_COUNT * 4)
		return f->regs[(address - 0x18810000) / 4];
	if (address >= 0x18800000 && address < 0x18800000 + SYS_REG_COUNT * 4)
		return f->sys_regs[(address - 0x18800000) / 4];
	if (address >= 0x18808000 && address < 0x18808000 + GMA_REG_COUNT * 4)
		return f->gma_regs[(address - 0x18808000) / 4];
	return 0;
}

static void fake_write32(void *cookie, uintptr_t address, uint32_t value)
{
	struct fake *f = cookie;

	if (address >= 0x18810000 && address < 0x18810000 + REG_COUNT * 4)
		f->regs[(address - 0x18810000) / 4] = value;
	else if (address >= 0x18800000 && address < 0x18800000 + SYS_REG_COUNT * 4)
		f->sys_regs[(address - 0x18800000) / 4] = value;
	else if (address >= 0x18808000 && address < 0x18808000 + GMA_REG_COUNT * 4)
		f->gma_regs[(address - 0x18808000) / 4] = value;
}

static void fake_delay_us(void *cookie, unsigned int usec)
{
	struct fake *f = cookie;

	(void)usec;
	f->delay_count++;
}

static void fake_cache_flush(void *cookie, uintptr_t address, size_t len)
{
	struct fake *f = cookie;

	(void)address;
	(void)len;
	f->flush_count++;
}

static void fake_cache_invalidate(void *cookie, uintptr_t address, size_t len)
{
	struct fake *f = cookie;

	(void)address;
	(void)len;
	f->invalidate_count++;
}

static void setup(struct hc15xx_vdec *vdec, struct fake *f)
{
	memset(f, 0, sizeof(*f));
	f->io.cookie = f;
	f->io.read32 = fake_read32;
	f->io.write32 = fake_write32;
	f->io.delay_us = fake_delay_us;
	f->io.cache_flush = fake_cache_flush;
	f->io.cache_invalidate = fake_cache_invalidate;
	f->sys_regs[HC15XX_SYS_REG_CHIP_ID / 4] = 0x1512a501;
	hc15xx_vdec_init(vdec, &f->io, 0x18810000, 0x18800000);
}

static void test_display_init(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_display_config cfg;
	int ret;

	setup(&vdec, &f);
	memset(&cfg, 0, sizeof(cfg));
	cfg.fb_phys = 0x00f10000;
	cfg.fb_size = 320 * 240 * 2;
	cfg.width = 320;
	cfg.height = 240;
	cfg.stride = 640;
	cfg.dst_x = 0;
	cfg.dst_y = 0;
	cfg.dst_w = 320;
	cfg.dst_h = 240;
	cfg.format = 0;

	ret = hc15xx_vdec_display_init(&vdec, &cfg);
	assert(ret == 0);

	/* GMA should be configured */
	assert(f.gma_regs[0x04 / 4] == 0x00f10000); /* src addr */
	assert(f.gma_regs[0x08 / 4] == 640); /* stride */
	assert(f.gma_regs[0x00 / 4] == 1); /* enabled */

	printf("test_display_init: PASS\n");
}

static void test_dma_submit_and_wait(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_dma_xfer xfer;
	int ret;

	setup(&vdec, &f);

	xfer.src = 0x01000000;
	xfer.dst = 0x00f10000;
	xfer.size = 320 * 240 * 2;

	ret = hc15xx_vdec_dma_submit(&vdec, &xfer);
	assert(ret == 0);

	/* Verify DMA registers programmed */
	assert(f.regs[0x0c0 / 4] == 0x01000000);
	assert(f.regs[0x0c4 / 4] == 0x00f10000);
	assert(f.regs[0x0c8 / 4] == 320 * 240 * 2);
	assert(f.regs[0x0cc / 4] == 0x03); /* START | IRQ_EN */
	/* DMA doorbell triggered */
	assert(f.regs[HC15XX_VDEC_REG_CMD / 4] == 0x20);

	/* Simulate DMA completion */
	f.regs[0x0d0 / 4] = HC15XX_VDEC_STATUS_DONE;

	ret = hc15xx_vdec_dma_wait(&vdec);
	assert(ret == 0);

	printf("test_dma_submit_and_wait: PASS\n");
}

static void test_dma_timeout(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_dma_xfer xfer;
	int ret;

	setup(&vdec, &f);

	xfer.src = 0x01000000;
	xfer.dst = 0x00f10000;
	xfer.size = 1024;

	hc15xx_vdec_dma_submit(&vdec, &xfer);

	/* DMA never completes */
	f.regs[0x0d0 / 4] = HC15XX_VDEC_STATUS_BUSY;

	ret = hc15xx_vdec_dma_wait(&vdec);
	assert(ret == -2);
	assert(f.delay_count == HC15XX_VDEC_POLL_LIMIT);

	printf("test_dma_timeout: PASS\n");
}

static void test_display_frame(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_display_config cfg;
	uint8_t frame[153600]; /* 320*240*2 */
	int ret;

	setup(&vdec, &f);
	memset(&cfg, 0, sizeof(cfg));
	cfg.fb_phys = 0x00f10000;
	cfg.fb_size = sizeof(frame);
	cfg.width = 320;
	cfg.height = 240;
	cfg.stride = 640;
	cfg.dst_w = 320;
	cfg.dst_h = 240;

	vdec.config.output_buf = 0x00f10000;
	vdec.config.output_buf_size = sizeof(frame);

	ret = hc15xx_vdec_display_init(&vdec, &cfg);
	assert(ret == 0);

	/* Simulate DMA completing immediately */
	f.regs[0x0d0 / 4] = HC15XX_VDEC_STATUS_DONE;

	memset(frame, 0x42, sizeof(frame));
	ret = hc15xx_vdec_display_frame(&vdec, frame, sizeof(frame));
	assert(ret == 0);

	/* Cache flush was called for the frame data */
	assert(f.flush_count == 1);
	/* GMA swap was triggered */
	assert(f.gma_regs[0x18 / 4] == 1);

	printf("test_display_frame: PASS\n");
}

static void test_gma_configure(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_display_config cfg;
	int ret;

	setup(&vdec, &f);
	memset(&cfg, 0, sizeof(cfg));
	cfg.fb_phys = 0x02000000;
	cfg.stride = 1280;
	cfg.dst_x = 10;
	cfg.dst_y = 20;
	cfg.dst_w = 300;
	cfg.dst_h = 200;
	cfg.format = 1;

	ret = hc15xx_vdec_gma_configure(&vdec, &cfg);
	assert(ret == 0);

	assert(f.gma_regs[0x04 / 4] == 0x02000000);
	assert(f.gma_regs[0x08 / 4] == 1280);
	/* dst_rect: x=10, y=20 -> 10 | (20 << 12) */
	assert(f.gma_regs[0x0c / 4] == (10 | (20 << 12)));
	/* src_rect: w=300, h=200 -> 300 | (200 << 12) */
	assert(f.gma_regs[0x10 / 4] == (300 | (200 << 12)));
	assert(f.gma_regs[0x14 / 4] == 1);
	assert(f.gma_regs[0x00 / 4] == 1);

	printf("test_gma_configure: PASS\n");
}

static void test_display_close(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;

	setup(&vdec, &f);

	hc15xx_vdec_display_close(&vdec);

	/* IRQ acknowledged */
	assert(f.regs[HC15XX_VDEC_REG_IRQ_STATUS / 4] == 0xffffffff);
	/* Reset command issued */
	assert(f.regs[HC15XX_VDEC_REG_CMD / 4] == HC15XX_VDEC_CMD_RESET);

	printf("test_display_close: PASS\n");
}

int main(void)
{
	test_display_init();
	test_dma_submit_and_wait();
	test_dma_timeout();
	test_display_frame();
	test_gma_configure();
	test_display_close();

	printf("vdec-display: all tests passed\n");
	return 0;
}
