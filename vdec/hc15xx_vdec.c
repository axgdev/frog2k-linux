// SPDX-License-Identifier: MIT
#include "hc15xx_vdec.h"

static uint32_t vdec_read(struct hc15xx_vdec *vdec, uint32_t offset)
{
	return vdec->io->read32(vdec->io->cookie, vdec->base + offset);
}

static void vdec_write(struct hc15xx_vdec *vdec, uint32_t offset, uint32_t value)
{
	vdec->io->write32(vdec->io->cookie, vdec->base + offset, value);
}

static uint32_t sys_read(struct hc15xx_vdec *vdec, uint32_t offset)
{
	return vdec->io->read32(vdec->io->cookie, vdec->sys_base + offset);
}

static void sys_write(struct hc15xx_vdec *vdec, uint32_t offset, uint32_t value)
{
	vdec->io->write32(vdec->io->cookie, vdec->sys_base + offset, value);
}

void hc15xx_vdec_init(struct hc15xx_vdec *vdec, const struct hc15xx_vdec_io *io,
		      uintptr_t base, uintptr_t sys_base)
{
	vdec->io = io;
	vdec->base = base;
	vdec->sys_base = sys_base;
	vdec->frames_decoded = 0;
	vdec->frames_displayed = 0;
}

int hc15xx_vdec_reset(struct hc15xx_vdec *vdec)
{
	uint32_t chip_id;
	uint32_t ctrl;

	chip_id = sys_read(vdec, HC15XX_SYS_REG_CHIP_ID) >> 16;
	if (chip_id != HC15XX_CHIP_ID_HC1512)
		return -1;

	/* Clear bits 0-1 in system control register 0x80 */
	ctrl = sys_read(vdec, HC15XX_SYS_REG_CTRL0);
	ctrl &= ~3u;
	sys_write(vdec, HC15XX_SYS_REG_CTRL0, ctrl);

	/* Clear bit 31 in system control register 0x84 */
	ctrl = sys_read(vdec, HC15XX_SYS_REG_CTRL1);
	ctrl &= BIT(31) - 1;
	sys_write(vdec, HC15XX_SYS_REG_CTRL1, ctrl);

	/* Issue hardware reset via command doorbell */
	vdec_write(vdec, HC15XX_VDEC_REG_CMD, HC15XX_VDEC_CMD_RESET);
	vdec->io->delay_us(vdec->io->cookie, HC15XX_VDEC_RESET_DELAY_US);

	/* Acknowledge any pending IRQs */
	vdec_write(vdec, HC15XX_VDEC_REG_IRQ_STATUS, 0xffffffff);

	vdec->frames_decoded = 0;
	vdec->frames_displayed = 0;

	return 0;
}

int hc15xx_vdec_configure(struct hc15xx_vdec *vdec,
			  const struct hc15xx_vdec_config *config)
{
	uint32_t ctrl;

	vdec->config = *config;

	/* Set codec type in control register */
	ctrl = vdec_read(vdec, HC15XX_VDEC_REG_CTRL);
	ctrl &= ~0x7u;
	ctrl |= (uint32_t)config->codec & 0x7u;
	vdec_write(vdec, HC15XX_VDEC_REG_CTRL, ctrl);

	/* Configure dimensions */
	vdec_write(vdec, HC15XX_VDEC_REG_CONFIG,
		   (config->width & 0xfff) | ((config->height & 0xfff) << 16));

	/* Set up DMA input buffer */
	vdec_write(vdec, HC15XX_VDEC_REG_DMA_BASE, (uint32_t)config->input_buf);
	vdec_write(vdec, HC15XX_VDEC_REG_DMA_BASE + 4, config->input_buf_size);

	/* Set up DMA output buffer */
	vdec_write(vdec, HC15XX_VDEC_REG_DMA_BASE + 8, (uint32_t)config->output_buf);
	vdec_write(vdec, HC15XX_VDEC_REG_DMA_BASE + 12, config->output_buf_size);

	/* Frame rate configuration */
	vdec_write(vdec, HC15XX_VDEC_REG_CHAN_BASE, config->frame_rate);

	return 0;
}

int hc15xx_vdec_start(struct hc15xx_vdec *vdec)
{
	vdec_write(vdec, HC15XX_VDEC_REG_CMD, HC15XX_VDEC_CMD_START);
	return 0;
}

int hc15xx_vdec_poll_done(struct hc15xx_vdec *vdec)
{
	unsigned int i;
	uint32_t status;

	for (i = 0; i < HC15XX_VDEC_POLL_LIMIT; i++) {
		status = vdec_read(vdec, HC15XX_VDEC_REG_STATUS2);
		if (status & HC15XX_VDEC_STATUS_ERROR)
			return -1;
		if (status & HC15XX_VDEC_STATUS_DONE)
			return 0;
		vdec->io->delay_us(vdec->io->cookie, HC15XX_VDEC_POLL_DELAY_US);
	}

	return -2; /* timeout */
}

int hc15xx_vdec_get_status(struct hc15xx_vdec *vdec,
			   struct hc15xx_vdec_status *status)
{
	uint32_t raw;

	raw = vdec_read(vdec, HC15XX_VDEC_REG_STATUS2);
	status->decode_status = raw;
	status->pic_width = vdec->config.width;
	status->pic_height = vdec->config.height;
	status->frames_decoded = vdec->frames_decoded;
	status->frames_displayed = vdec->frames_displayed;
	status->decode_error = (raw & HC15XX_VDEC_STATUS_ERROR) ? 1 : 0;

	return 0;
}

void hc15xx_vdec_ack_irq(struct hc15xx_vdec *vdec)
{
	vdec_write(vdec, HC15XX_VDEC_REG_IRQ_STATUS, 0xffffffff);
}

int hc15xx_vdec_decode_frame(struct hc15xx_vdec *vdec,
			     uintptr_t data, uint32_t size)
{
	int ret;

	/* Flush input data from cache to memory for DMA access */
	if (vdec->io->cache_flush)
		vdec->io->cache_flush(vdec->io->cookie, data, size);

	/* Load input buffer address and size into DMA registers */
	vdec_write(vdec, HC15XX_VDEC_REG_DMA_BASE, (uint32_t)data);
	vdec_write(vdec, HC15XX_VDEC_REG_DMA_BASE + 4, size);

	/* Trigger decode */
	vdec_write(vdec, HC15XX_VDEC_REG_CMD, HC15XX_VDEC_CMD_START);

	/* Wait for completion */
	ret = hc15xx_vdec_poll_done(vdec);
	if (ret != 0)
		return ret;

	/* Acknowledge IRQ */
	hc15xx_vdec_ack_irq(vdec);

	/* Invalidate output buffer cache so CPU sees DMA results */
	if (vdec->io->cache_invalidate)
		vdec->io->cache_invalidate(vdec->io->cookie,
					   vdec->config.output_buf,
					   vdec->config.output_buf_size);

	vdec->frames_decoded++;
	vdec->frames_displayed++;

	return 0;
}
