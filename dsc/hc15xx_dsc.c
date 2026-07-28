// SPDX-License-Identifier: MIT
/*
 * HC15xx Display System Controller (DSC) driver.
 *
 * Recovered from libdsc.a (2 exported stubs + 15 internal functions).
 * The DSC provides VSync interrupt handling and DAC/CVBS output
 * configuration for the HC15xx display pipeline.
 *
 * Register protocol (from ISR disassembly):
 * - base+0x00: Control (bit 0 = enable, bit 1 = VBlank IRQ enable)
 * - base+0x04: IRQ status (read to get, write to acknowledge)
 * - base+0x08: VSync counter (read-only, incremented by hardware)
 *
 * The ISR (rRliPpIatzEEnmBQOPRqRPnHkZhnVrUU) reads IRQ status,
 * ORs it into an accumulator, writes it back to acknowledge,
 * increments a software VSync counter, and wakes waiting tasks.
 *
 * Init sequence (izMkuhyflPJcWGZWUHCnZrkjjyseRfLX):
 * 1. hc_clk_enable(3) - enable display clock
 * 2. memalign(32, 16384) - allocate DMA descriptor ring
 * 3. fdt_node_probe_by_path() - find DT node
 * 4. fdt_get_property_u_32_index(node, "reg", 0) - get register base
 * 5. xQueueCreateCountingSemaphoreStatic(1, 1) - VSync semaphore
 * 6. xPortInterruptInstallISR(base, isr, ctx) - install VSync ISR
 */
#include "hc15xx_dsc.h"

static uint32_t dsc_read(struct hc15xx_dsc *dsc, uint32_t offset)
{
	return dsc->io->read32(dsc->io->cookie, dsc->base + offset);
}

static void dsc_write(struct hc15xx_dsc *dsc, uint32_t offset, uint32_t value)
{
	dsc->io->write32(dsc->io->cookie, dsc->base + offset, value);
}

void hc15xx_dsc_init(struct hc15xx_dsc *dsc, const struct hc15xx_dsc_io *io,
		     uintptr_t base)
{
	dsc->io = io;
	dsc->base = base;
	dsc->vsync_count = 0;
	dsc->irq_accum = 0;
	dsc->enabled = 0;
}

int hc15xx_dsc_enable(struct hc15xx_dsc *dsc)
{
	dsc_write(dsc, HC15XX_DSC_REG_CTRL,
		  HC15XX_DSC_CTRL_ENABLE | HC15XX_DSC_CTRL_VBLANK_IRQ);
	dsc->enabled = 1;
	return 0;
}

void hc15xx_dsc_disable(struct hc15xx_dsc *dsc)
{
	dsc_write(dsc, HC15XX_DSC_REG_CTRL, 0);
	dsc->enabled = 0;
}

int hc15xx_dsc_handle_irq(struct hc15xx_dsc *dsc)
{
	uint32_t status;

	status = dsc_read(dsc, HC15XX_DSC_REG_IRQ_STATUS);
	if (status == 0)
		return 0;

	/* Acknowledge by writing status back */
	dsc_write(dsc, HC15XX_DSC_REG_IRQ_STATUS, status);

	/* Accumulate and count VBlank events */
	dsc->irq_accum |= status;
	if (status & HC15XX_DSC_IRQ_VBLANK)
		dsc->vsync_count++;

	return 1;
}

int hc15xx_dsc_wait_vblank(struct hc15xx_dsc *dsc, uint32_t target_count)
{
	unsigned int i;
	uint32_t hw_count;

	for (i = 0; i < 100000; i++) {
		hw_count = dsc_read(dsc, HC15XX_DSC_REG_VSYNC_COUNT);
		if (hw_count >= target_count)
			return 0;
		dsc->io->delay_us(dsc->io->cookie, 100);
	}

	return -1; /* timeout */
}

uint32_t hc15xx_dsc_get_vsync_count(struct hc15xx_dsc *dsc)
{
	return dsc_read(dsc, HC15XX_DSC_REG_VSYNC_COUNT);
}

int hc15xx_dsc_set_dac(struct hc15xx_dsc *dsc,
		       const struct hc15xx_dsc_dac_config *cfg)
{
	uint32_t ctrl;

	if (!cfg)
		return -1;

	ctrl = dsc_read(dsc, HC15XX_DSC_REG_CTRL);

	if (cfg->enable)
		ctrl |= HC15XX_DSC_CTRL_ENABLE;
	else
		ctrl &= ~HC15XX_DSC_CTRL_ENABLE;

	dsc_write(dsc, HC15XX_DSC_REG_CTRL, ctrl);
	return 0;
}

int hc15xx_dsc_set_bg_color(struct hc15xx_dsc *dsc,
			    const struct hc15xx_dsc_bg_color *color)
{
	(void)dsc;
	(void)color;
	/* Background color is set via the GMA compositor, not DSC registers */
	return 0;
}

int hc15xx_dsc_suspend(struct hc15xx_dsc *dsc)
{
	hc15xx_dsc_disable(dsc);
	return 0;
}

int hc15xx_dsc_resume(struct hc15xx_dsc *dsc)
{
	return hc15xx_dsc_enable(dsc);
}
