// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hc15xx_dsc.h"

#define REG_COUNT 16

struct fake {
	uint32_t regs[REG_COUNT];
	unsigned int delay_count;
	struct hc15xx_dsc_io io;
};

static uint32_t fake_read32(void *cookie, uintptr_t address)
{
	struct fake *f = cookie;
	unsigned int offset = (unsigned int)(address & 0x3f);

	assert(offset / 4 < REG_COUNT);
	return f->regs[offset / 4];
}

static void fake_write32(void *cookie, uintptr_t address, uint32_t value)
{
	struct fake *f = cookie;
	unsigned int offset = (unsigned int)(address & 0x3f);

	assert(offset / 4 < REG_COUNT);
	f->regs[offset / 4] = value;
}

static void fake_delay_us(void *cookie, unsigned int usec)
{
	struct fake *f = cookie;

	(void)usec;
	f->delay_count++;
}

static void setup_dsc(struct hc15xx_dsc *dsc, struct fake *f)
{
	memset(f, 0, sizeof(*f));
	f->io.cookie = f;
	f->io.read32 = fake_read32;
	f->io.write32 = fake_write32;
	f->io.delay_us = fake_delay_us;
	hc15xx_dsc_init(dsc, &f->io, 0x18870000);
}

static void test_init(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;

	setup_dsc(&dsc, &f);

	assert(dsc.base == 0x18870000);
	assert(dsc.vsync_count == 0);
	assert(dsc.irq_accum == 0);
	assert(dsc.enabled == 0);

	printf("test_init: PASS\n");
}

static void test_enable_disable(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;

	setup_dsc(&dsc, &f);

	hc15xx_dsc_enable(&dsc);
	assert(f.regs[HC15XX_DSC_REG_CTRL / 4] == 0x03);
	assert(dsc.enabled == 1);

	hc15xx_dsc_disable(&dsc);
	assert(f.regs[HC15XX_DSC_REG_CTRL / 4] == 0x00);
	assert(dsc.enabled == 0);

	printf("test_enable_disable: PASS\n");
}

static void test_handle_irq_vblank(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;
	int ret;

	setup_dsc(&dsc, &f);
	f.regs[HC15XX_DSC_REG_IRQ_STATUS / 4] = HC15XX_DSC_IRQ_VBLANK;

	ret = hc15xx_dsc_handle_irq(&dsc);
	assert(ret == 1);
	assert(dsc.vsync_count == 1);
	assert(dsc.irq_accum == HC15XX_DSC_IRQ_VBLANK);
	/* IRQ acknowledged (written back) */
	assert(f.regs[HC15XX_DSC_REG_IRQ_STATUS / 4] == HC15XX_DSC_IRQ_VBLANK);

	printf("test_handle_irq_vblank: PASS\n");
}

static void test_handle_irq_none(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;
	int ret;

	setup_dsc(&dsc, &f);
	f.regs[HC15XX_DSC_REG_IRQ_STATUS / 4] = 0;

	ret = hc15xx_dsc_handle_irq(&dsc);
	assert(ret == 0);
	assert(dsc.vsync_count == 0);

	printf("test_handle_irq_none: PASS\n");
}

static void test_handle_irq_multiple(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;
	unsigned int i;

	setup_dsc(&dsc, &f);

	for (i = 0; i < 5; i++) {
		f.regs[HC15XX_DSC_REG_IRQ_STATUS / 4] = HC15XX_DSC_IRQ_VBLANK;
		hc15xx_dsc_handle_irq(&dsc);
	}

	assert(dsc.vsync_count == 5);

	printf("test_handle_irq_multiple: PASS\n");
}

static void test_wait_vblank_immediate(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;
	int ret;

	setup_dsc(&dsc, &f);
	f.regs[HC15XX_DSC_REG_VSYNC_COUNT / 4] = 10;

	ret = hc15xx_dsc_wait_vblank(&dsc, 10);
	assert(ret == 0);
	assert(f.delay_count == 0);

	printf("test_wait_vblank_immediate: PASS\n");
}

static void test_wait_vblank_timeout(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;
	int ret;

	setup_dsc(&dsc, &f);
	f.regs[HC15XX_DSC_REG_VSYNC_COUNT / 4] = 0;

	ret = hc15xx_dsc_wait_vblank(&dsc, 100);
	assert(ret == -1);
	assert(f.delay_count == 100000);

	printf("test_wait_vblank_timeout: PASS\n");
}

static void test_get_vsync_count(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;

	setup_dsc(&dsc, &f);
	f.regs[HC15XX_DSC_REG_VSYNC_COUNT / 4] = 42;

	assert(hc15xx_dsc_get_vsync_count(&dsc) == 42);

	printf("test_get_vsync_count: PASS\n");
}

static void test_set_dac(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;
	struct hc15xx_dsc_dac_config cfg;
	int ret;

	setup_dsc(&dsc, &f);

	memset(&cfg, 0, sizeof(cfg));
	cfg.type = HC15XX_DSC_DAC_CVBS;
	cfg.enable = 1;
	cfg.dac_mask = HC15XX_DSC_DAC_0;

	ret = hc15xx_dsc_set_dac(&dsc, &cfg);
	assert(ret == 0);
	assert(f.regs[HC15XX_DSC_REG_CTRL / 4] & HC15XX_DSC_CTRL_ENABLE);

	cfg.enable = 0;
	ret = hc15xx_dsc_set_dac(&dsc, &cfg);
	assert(ret == 0);
	assert(!(f.regs[HC15XX_DSC_REG_CTRL / 4] & HC15XX_DSC_CTRL_ENABLE));

	printf("test_set_dac: PASS\n");
}

static void test_suspend_resume(void)
{
	struct fake f;
	struct hc15xx_dsc dsc;

	setup_dsc(&dsc, &f);

	hc15xx_dsc_enable(&dsc);
	assert(dsc.enabled == 1);

	hc15xx_dsc_suspend(&dsc);
	assert(dsc.enabled == 0);
	assert(f.regs[HC15XX_DSC_REG_CTRL / 4] == 0);

	hc15xx_dsc_resume(&dsc);
	assert(dsc.enabled == 1);
	assert(f.regs[HC15XX_DSC_REG_CTRL / 4] == 0x03);

	printf("test_suspend_resume: PASS\n");
}

int main(void)
{
	test_init();
	test_enable_disable();
	test_handle_irq_vblank();
	test_handle_irq_none();
	test_handle_irq_multiple();
	test_wait_vblank_immediate();
	test_wait_vblank_timeout();
	test_get_vsync_count();
	test_set_dac();
	test_suspend_resume();

	printf("dsc: all tests passed\n");
	return 0;
}
