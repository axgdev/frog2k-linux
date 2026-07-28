// SPDX-License-Identifier: MIT
/*
 * Hardware DMA/display backend for ffmpeg software decoders.
 *
 * Register protocol recovered from libviddrv.a disassembly:
 * - DMA transfers use registers 0x0c0-0x0fc (src/dst/size/ctrl)
 * - DMA completion signaled via status bit in 0xf84
 * - GMA compositor at 0x18808000 handles scaling/overlay
 * - VSync doorbell at 0xff0 with value 0x10 triggers buffer swap
 * - DMA doorbell at 0xff0 with value 0x20 triggers transfer
 */
#include <string.h>
#include "hc15xx_vdec_codec.h"

/* DMA register offsets within VDEC register space */
#define VDEC_DMA_SRC		0x0c0
#define VDEC_DMA_DST		0x0c4
#define VDEC_DMA_SIZE		0x0c8
#define VDEC_DMA_CTRL		0x0cc
#define VDEC_DMA_STATUS		0x0d0

/* DMA control bits */
#define VDEC_DMA_CTRL_START	BIT(0)
#define VDEC_DMA_CTRL_IRQ_EN	BIT(1)

/* DMA command doorbell value */
#define HC15XX_VDEC_CMD_DMA	0x20

/* GMA register base (display compositor) */
#define HC15XX_GMA_BASE		0x18808000
#define GMA_REG_CTRL		0x00
#define GMA_REG_SRC_ADDR	0x04
#define GMA_REG_SRC_STRIDE	0x08
#define GMA_REG_DST_RECT	0x0c
#define GMA_REG_SRC_RECT	0x10
#define GMA_REG_FORMAT		0x14
#define GMA_REG_SWAP		0x18

int hc15xx_vdec_display_init(struct hc15xx_vdec *vdec,
			     const struct hc15xx_vdec_display_config *cfg)
{
	int ret;

	/* Reset the VDEC hardware */
	ret = hc15xx_vdec_reset(vdec);
	if (ret != 0)
		return ret;

	/* Configure GMA compositor for the display */
	ret = hc15xx_vdec_gma_configure(vdec, cfg);
	if (ret != 0)
		return ret;

	/* Start the display engine */
	hc15xx_vdec_start(vdec);

	return 0;
}

int hc15xx_vdec_display_frame(struct hc15xx_vdec *vdec,
			      const uint8_t *frame_data, uint32_t size)
{
	struct hc15xx_vdec_dma_xfer xfer;
	int ret;

	/* Flush CPU cache so DMA sees the decoded frame data */
	if (vdec->io->cache_flush)
		vdec->io->cache_flush(vdec->io->cookie,
				      (uintptr_t)frame_data, size);

	/* DMA transfer from decode buffer to framebuffer */
	xfer.src = (uintptr_t)frame_data;
	xfer.dst = vdec->config.output_buf;
	xfer.size = size;

	ret = hc15xx_vdec_dma_submit(vdec, &xfer);
	if (ret != 0)
		return ret;

	ret = hc15xx_vdec_dma_wait(vdec);
	if (ret != 0)
		return ret;

	/* Trigger GMA buffer swap on next VSync */
	return hc15xx_vdec_gma_swap(vdec);
}

int hc15xx_vdec_display_flush(struct hc15xx_vdec *vdec)
{
	/* Wait for any pending DMA to complete */
	return hc15xx_vdec_dma_wait(vdec);
}

void hc15xx_vdec_display_close(struct hc15xx_vdec *vdec)
{
	/* Acknowledge any pending IRQs and stop */
	hc15xx_vdec_ack_irq(vdec);
	vdec->io->write32(vdec->io->cookie,
			  vdec->base + HC15XX_VDEC_REG_CMD,
			  HC15XX_VDEC_CMD_RESET);
}

int hc15xx_vdec_dma_submit(struct hc15xx_vdec *vdec,
			   const struct hc15xx_vdec_dma_xfer *xfer)
{
	/* Program DMA transfer registers */
	vdec->io->write32(vdec->io->cookie,
			  vdec->base + VDEC_DMA_SRC, (uint32_t)xfer->src);
	vdec->io->write32(vdec->io->cookie,
			  vdec->base + VDEC_DMA_DST, (uint32_t)xfer->dst);
	vdec->io->write32(vdec->io->cookie,
			  vdec->base + VDEC_DMA_SIZE, xfer->size);
	vdec->io->write32(vdec->io->cookie,
			  vdec->base + VDEC_DMA_CTRL,
			  VDEC_DMA_CTRL_START | VDEC_DMA_CTRL_IRQ_EN);

	/* Trigger DMA via command doorbell */
	vdec->io->write32(vdec->io->cookie,
			  vdec->base + HC15XX_VDEC_REG_CMD,
			  HC15XX_VDEC_CMD_DMA);

	return 0;
}

int hc15xx_vdec_dma_wait(struct hc15xx_vdec *vdec)
{
	unsigned int i;
	uint32_t status;

	for (i = 0; i < HC15XX_VDEC_POLL_LIMIT; i++) {
		status = vdec->io->read32(vdec->io->cookie,
					  vdec->base + VDEC_DMA_STATUS);
		if (status & HC15XX_VDEC_STATUS_ERROR)
			return -1;
		if (status & HC15XX_VDEC_STATUS_DONE)
			return 0;
		vdec->io->delay_us(vdec->io->cookie, HC15XX_VDEC_POLL_DELAY_US);
	}

	return -2; /* timeout */
}

int hc15xx_vdec_gma_configure(struct hc15xx_vdec *vdec,
			      const struct hc15xx_vdec_display_config *cfg)
{
	uint32_t dst_rect;
	uint32_t src_rect;

	/* Source framebuffer address and stride */
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_SRC_ADDR, cfg->fb_phys);
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_SRC_STRIDE, cfg->stride);

	/* Destination rectangle on screen */
	dst_rect = (cfg->dst_x & 0xfff) |
		   ((cfg->dst_y & 0xfff) << 12);
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_DST_RECT, dst_rect);

	/* Source rectangle (dimensions) */
	src_rect = (cfg->dst_w & 0xfff) |
		   ((cfg->dst_h & 0xfff) << 12);
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_SRC_RECT, src_rect);

	/* Pixel format */
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_FORMAT,
			  (uint32_t)cfg->format);

	/* Enable GMA layer */
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_CTRL, 1);

	return 0;
}

int hc15xx_vdec_gma_swap(struct hc15xx_vdec *vdec)
{
	/* Trigger buffer swap on next VSync */
	vdec->io->write32(vdec->io->cookie,
			  HC15XX_GMA_BASE + GMA_REG_SWAP, 1);
	return 0;
}
