/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_VDEC_CODEC_H
#define HC15XX_VDEC_CODEC_H

#include "hc15xx_vdec.h"

/*
 * Hardware DMA/display backend for use with ffmpeg software decoders.
 *
 * The SF2000 VDEC hardware is NOT a hardware decoder.  It provides:
 * 1. DMA engine for fast frame buffer transfers
 * 2. GMA compositor for scaling/overlay to the display panel
 * 3. VSync-synchronized buffer swap for tear-free output
 *
 * ffmpeg (libavcodec) handles all bitstream parsing and frame
 * reconstruction in software.  This backend handles the hardware
 * display path: DMA transfer of decoded frames to the framebuffer
 * and GMA compositor configuration for scaling/positioning.
 *
 * Integration with ffmpeg:
 *   - Decode: ffmpeg software decoders (libx264, mpeg2video, etc.)
 *   - Display: write decoded AVFrame data via hc15xx_vdec_display_frame()
 *   - The DMA engine transfers the frame to the GMA framebuffer
 *   - The GMA compositor scales/positions it on the 320x240 panel
 */

/* Display layer configuration for GMA compositor */
struct hc15xx_vdec_display_config {
	uint32_t fb_phys;	/* Physical address of framebuffer */
	uint32_t fb_size;	/* Framebuffer size in bytes */
	uint32_t width;		/* Source frame width */
	uint32_t height;	/* Source frame height */
	uint32_t stride;	/* Source stride in bytes */
	uint32_t dst_x;		/* Display window X */
	uint32_t dst_y;		/* Display window Y */
	uint32_t dst_w;		/* Display window width */
	uint32_t dst_h;		/* Display window height */
	int format;		/* Pixel format (0=RGB565, 1=YUV420) */
};

/* DMA transfer descriptor */
struct hc15xx_vdec_dma_xfer {
	uintptr_t src;		/* Source physical address */
	uintptr_t dst;		/* Destination physical address */
	uint32_t size;		/* Transfer size in bytes */
};

int hc15xx_vdec_display_init(struct hc15xx_vdec *vdec,
			     const struct hc15xx_vdec_display_config *cfg);
int hc15xx_vdec_display_frame(struct hc15xx_vdec *vdec,
			      const uint8_t *frame_data, uint32_t size);
int hc15xx_vdec_display_flush(struct hc15xx_vdec *vdec);
void hc15xx_vdec_display_close(struct hc15xx_vdec *vdec);

/* DMA operations */
int hc15xx_vdec_dma_submit(struct hc15xx_vdec *vdec,
			   const struct hc15xx_vdec_dma_xfer *xfer);
int hc15xx_vdec_dma_wait(struct hc15xx_vdec *vdec);

/* GMA compositor control */
int hc15xx_vdec_gma_configure(struct hc15xx_vdec *vdec,
			      const struct hc15xx_vdec_display_config *cfg);
int hc15xx_vdec_gma_swap(struct hc15xx_vdec *vdec);

#endif /* HC15XX_VDEC_CODEC_H */
