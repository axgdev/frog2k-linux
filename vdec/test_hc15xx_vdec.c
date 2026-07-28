// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hc15xx_vdec.h"

#define REG_COUNT 1024
#define SYS_REG_COUNT 256

struct fake {
	uint32_t regs[REG_COUNT];
	uint32_t sys_regs[SYS_REG_COUNT];
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
	return 0;
}

static void fake_write32(void *cookie, uintptr_t address, uint32_t value)
{
	struct fake *f = cookie;

	if (address >= 0x18810000 && address < 0x18810000 + REG_COUNT * 4)
		f->regs[(address - 0x18810000) / 4] = value;
	else if (address >= 0x18800000 && address < 0x18800000 + SYS_REG_COUNT * 4)
		f->sys_regs[(address - 0x18800000) / 4] = value;
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

static void setup_vdec(struct hc15xx_vdec *vdec, struct fake *f)
{
	memset(f, 0, sizeof(*f));
	f->io.cookie = f;
	f->io.read32 = fake_read32;
	f->io.write32 = fake_write32;
	f->io.delay_us = fake_delay_us;
	f->io.cache_flush = fake_cache_flush;
	f->io.cache_invalidate = fake_cache_invalidate;

	/* Set chip ID to HC1512 */
	f->sys_regs[HC15XX_SYS_REG_CHIP_ID / 4] = 0x1512a501;

	hc15xx_vdec_init(vdec, &f->io, 0x18810000, 0x18800000);
}

static void test_init(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;

	setup_vdec(&vdec, &f);

	assert(vdec.base == 0x18810000);
	assert(vdec.sys_base == 0x18800000);
	assert(vdec.frames_decoded == 0);
	assert(vdec.frames_displayed == 0);

	printf("test_init: PASS\n");
}

static void test_reset(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	int ret;

	setup_vdec(&vdec, &f);
	f.sys_regs[HC15XX_SYS_REG_CTRL0 / 4] = 0x03;
	f.sys_regs[HC15XX_SYS_REG_CTRL1 / 4] = 0x80000001;

	ret = hc15xx_vdec_reset(&vdec);
	assert(ret == 0);

	/* Bits 0-1 cleared in CTRL0 */
	assert(f.sys_regs[HC15XX_SYS_REG_CTRL0 / 4] == 0x00);
	/* Bit 31 cleared in CTRL1 */
	assert(f.sys_regs[HC15XX_SYS_REG_CTRL1 / 4] == 0x00000001);
	/* Reset command written to doorbell */
	assert(f.regs[HC15XX_VDEC_REG_CMD / 4] == HC15XX_VDEC_CMD_RESET);
	/* IRQ acknowledged */
	assert(f.regs[HC15XX_VDEC_REG_IRQ_STATUS / 4] == 0xffffffff);
	/* Delay was called for reset timing */
	assert(f.delay_count == 1);

	printf("test_reset: PASS\n");
}

static void test_reset_wrong_chip(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	int ret;

	setup_vdec(&vdec, &f);
	f.sys_regs[HC15XX_SYS_REG_CHIP_ID / 4] = 0x16000001;

	ret = hc15xx_vdec_reset(&vdec);
	assert(ret == -1);

	printf("test_reset_wrong_chip: PASS\n");
}

static void test_configure(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_config config;
	int ret;

	setup_vdec(&vdec, &f);
	memset(&config, 0, sizeof(config));
	config.codec = HC15XX_VDEC_CODEC_H264;
	config.width = 320;
	config.height = 240;
	config.frame_rate = 30;
	config.input_buf = 0x01000000;
	config.input_buf_size = 65536;
	config.output_buf = 0x02000000;
	config.output_buf_size = 320 * 240 * 2;

	ret = hc15xx_vdec_configure(&vdec, &config);
	assert(ret == 0);

	/* Codec set in control register */
	assert((f.regs[HC15XX_VDEC_REG_CTRL / 4] & 0x7) == HC15XX_VDEC_CODEC_H264);
	/* Dimensions packed into config register */
	assert(f.regs[HC15XX_VDEC_REG_CONFIG / 4] == (320 | (240 << 16)));
	/* DMA buffers configured */
	assert(f.regs[HC15XX_VDEC_REG_DMA_BASE / 4] == 0x01000000);
	assert(f.regs[(HC15XX_VDEC_REG_DMA_BASE + 4) / 4] == 65536);
	assert(f.regs[(HC15XX_VDEC_REG_DMA_BASE + 8) / 4] == 0x02000000);
	assert(f.regs[(HC15XX_VDEC_REG_DMA_BASE + 12) / 4] == 320 * 240 * 2);
	/* Frame rate */
	assert(f.regs[HC15XX_VDEC_REG_CHAN_BASE / 4] == 30);

	printf("test_configure: PASS\n");
}

static void test_start(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	int ret;

	setup_vdec(&vdec, &f);

	ret = hc15xx_vdec_start(&vdec);
	assert(ret == 0);
	assert(f.regs[HC15XX_VDEC_REG_CMD / 4] == HC15XX_VDEC_CMD_START);

	printf("test_start: PASS\n");
}

static void test_poll_done_immediate(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	int ret;

	setup_vdec(&vdec, &f);
	f.regs[HC15XX_VDEC_REG_STATUS2 / 4] = HC15XX_VDEC_STATUS_DONE;

	ret = hc15xx_vdec_poll_done(&vdec);
	assert(ret == 0);
	assert(f.delay_count == 0);

	printf("test_poll_done_immediate: PASS\n");
}

static void test_poll_done_timeout(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	int ret;

	setup_vdec(&vdec, &f);
	f.regs[HC15XX_VDEC_REG_STATUS2 / 4] = HC15XX_VDEC_STATUS_BUSY;

	ret = hc15xx_vdec_poll_done(&vdec);
	assert(ret == -2);
	assert(f.delay_count == HC15XX_VDEC_POLL_LIMIT);

	printf("test_poll_done_timeout: PASS\n");
}

static void test_poll_done_error(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	int ret;

	setup_vdec(&vdec, &f);
	f.regs[HC15XX_VDEC_REG_STATUS2 / 4] = HC15XX_VDEC_STATUS_ERROR;

	ret = hc15xx_vdec_poll_done(&vdec);
	assert(ret == -1);

	printf("test_poll_done_error: PASS\n");
}

static void test_decode_frame(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_config config;
	int ret;

	setup_vdec(&vdec, &f);
	memset(&config, 0, sizeof(config));
	config.codec = HC15XX_VDEC_CODEC_JPEG;
	config.width = 320;
	config.height = 240;
	config.output_buf = 0x02000000;
	config.output_buf_size = 320 * 240 * 2;
	hc15xx_vdec_configure(&vdec, &config);

	/* Simulate hardware completing decode immediately */
	f.regs[HC15XX_VDEC_REG_STATUS2 / 4] = HC15XX_VDEC_STATUS_DONE;

	ret = hc15xx_vdec_decode_frame(&vdec, 0x01000000, 4096);
	assert(ret == 0);
	assert(vdec.frames_decoded == 1);
	assert(vdec.frames_displayed == 1);
	assert(f.flush_count == 1);
	assert(f.invalidate_count == 1);
	/* Start command was written */
	assert(f.regs[HC15XX_VDEC_REG_CMD / 4] == HC15XX_VDEC_CMD_START);
	/* IRQ was acknowledged */
	assert(f.regs[HC15XX_VDEC_REG_IRQ_STATUS / 4] == 0xffffffff);

	printf("test_decode_frame: PASS\n");
}

static void test_get_status(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_status status;
	struct hc15xx_vdec_config config;
	int ret;

	setup_vdec(&vdec, &f);
	memset(&config, 0, sizeof(config));
	config.codec = HC15XX_VDEC_CODEC_MPEG2;
	config.width = 720;
	config.height = 480;
	hc15xx_vdec_configure(&vdec, &config);
	vdec.frames_decoded = 42;
	vdec.frames_displayed = 40;

	f.regs[HC15XX_VDEC_REG_STATUS2 / 4] = HC15XX_VDEC_STATUS_DONE;

	ret = hc15xx_vdec_get_status(&vdec, &status);
	assert(ret == 0);
	assert(status.decode_status == HC15XX_VDEC_STATUS_DONE);
	assert(status.pic_width == 720);
	assert(status.pic_height == 480);
	assert(status.frames_decoded == 42);
	assert(status.frames_displayed == 40);
	assert(status.decode_error == 0);

	printf("test_get_status: PASS\n");
}

static void test_ack_irq(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;

	setup_vdec(&vdec, &f);
	f.regs[HC15XX_VDEC_REG_IRQ_STATUS / 4] = 0x07;

	hc15xx_vdec_ack_irq(&vdec);
	assert(f.regs[HC15XX_VDEC_REG_IRQ_STATUS / 4] == 0xffffffff);

	printf("test_ack_irq: PASS\n");
}

int main(void)
{
	test_init();
	test_reset();
	test_reset_wrong_chip();
	test_configure();
	test_start();
	test_poll_done_immediate();
	test_poll_done_timeout();
	test_poll_done_error();
	test_decode_frame();
	test_get_status();
	test_ack_irq();

	printf("vdec: all tests passed\n");
	return 0;
}
