/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_VDEC_H
#define HC15XX_VDEC_H

#include <stdint.h>
#include <stddef.h>

struct hc15xx_vdec_io {
	void *cookie;
	uint32_t (*read32)(void *cookie, uintptr_t address);
	void (*write32)(void *cookie, uintptr_t address, uint32_t value);
	void (*delay_us)(void *cookie, unsigned int usec);
	void (*cache_flush)(void *cookie, uintptr_t address, size_t len);
	void (*cache_invalidate)(void *cookie, uintptr_t address, size_t len);
};

enum hc15xx_vdec_codec_id {
	HC15XX_VDEC_CODEC_H264 = 0,
	HC15XX_VDEC_CODEC_MPEG2,
	HC15XX_VDEC_CODEC_MPEG4,
	HC15XX_VDEC_CODEC_VC1,
	HC15XX_VDEC_CODEC_VP8,
	HC15XX_VDEC_CODEC_JPEG,
};

struct hc15xx_vdec_config {
	enum hc15xx_vdec_codec_id codec;
	uint32_t width;
	uint32_t height;
	uint32_t frame_rate;
	uintptr_t input_buf;
	uint32_t input_buf_size;
	uintptr_t output_buf;
	uint32_t output_buf_size;
};

struct hc15xx_vdec_status {
	uint32_t decode_status;
	uint32_t pic_width;
	uint32_t pic_height;
	uint32_t frames_decoded;
	uint32_t frames_displayed;
	uint32_t decode_error;
};

/* Register offsets from base (0x18810000) */
#define HC15XX_VDEC_REG_CTRL		0x000
#define HC15XX_VDEC_REG_CTRL2		0x030
#define HC15XX_VDEC_REG_CONFIG		0x084
#define HC15XX_VDEC_REG_DMA_BASE	0x0c0
#define HC15XX_VDEC_REG_DMA_END		0x0fc
#define HC15XX_VDEC_REG_CHAN_BASE	0x140
#define HC15XX_VDEC_REG_CHAN_END	0x180
#define HC15XX_VDEC_REG_EXT_BASE	0x240
#define HC15XX_VDEC_REG_CODEC_BASE	0x300
#define HC15XX_VDEC_REG_CODEC_END	0x400
#define HC15XX_VDEC_REG_IRQ_STATUS	0xf78
#define HC15XX_VDEC_REG_STATUS0		0xf7c
#define HC15XX_VDEC_REG_STATUS1		0xf80
#define HC15XX_VDEC_REG_STATUS2		0xf84
#define HC15XX_VDEC_REG_DOORBELL_BASE	0xf88
#define HC15XX_VDEC_REG_DOORBELL_END	0xfec
#define HC15XX_VDEC_REG_CMD		0xff0

/* Command doorbell values */
#define HC15XX_VDEC_CMD_RESET		0xff
#define HC15XX_VDEC_CMD_START		0x10

/* IRQ status bits (write-1-to-clear) */
#define HC15XX_VDEC_IRQ_DECODE_DONE	BIT(0)
#define HC15XX_VDEC_IRQ_ERROR		BIT(1)
#define HC15XX_VDEC_IRQ_FRAME_READY	BIT(2)

/* Status register bits */
#define HC15XX_VDEC_STATUS_BUSY		BIT(0)
#define HC15XX_VDEC_STATUS_DONE		BIT(1)
#define HC15XX_VDEC_STATUS_ERROR	BIT(2)

/* System control registers (base 0x18800000) */
#define HC15XX_SYS_REG_CHIP_ID		0x00
#define HC15XX_SYS_REG_VDEC_CFG		0x7c
#define HC15XX_SYS_REG_CTRL0		0x80
#define HC15XX_SYS_REG_CTRL1		0x84

#define HC15XX_CHIP_ID_HC1512		0x1512

/* Clock gate IDs */
#define HC15XX_CLK_VDEC			10
#define HC15XX_CLK_VE			27

#define HC15XX_VDEC_POLL_LIMIT		5000
#define HC15XX_VDEC_POLL_DELAY_US	1000
#define HC15XX_VDEC_RESET_DELAY_US	10000

struct hc15xx_vdec {
	const struct hc15xx_vdec_io *io;
	uintptr_t base;
	uintptr_t sys_base;
	struct hc15xx_vdec_config config;
	uint32_t frames_decoded;
	uint32_t frames_displayed;
};

void hc15xx_vdec_init(struct hc15xx_vdec *vdec, const struct hc15xx_vdec_io *io,
		      uintptr_t base, uintptr_t sys_base);
int hc15xx_vdec_reset(struct hc15xx_vdec *vdec);
int hc15xx_vdec_configure(struct hc15xx_vdec *vdec,
			  const struct hc15xx_vdec_config *config);
int hc15xx_vdec_start(struct hc15xx_vdec *vdec);
int hc15xx_vdec_poll_done(struct hc15xx_vdec *vdec);
int hc15xx_vdec_get_status(struct hc15xx_vdec *vdec,
			   struct hc15xx_vdec_status *status);
void hc15xx_vdec_ack_irq(struct hc15xx_vdec *vdec);
int hc15xx_vdec_decode_frame(struct hc15xx_vdec *vdec,
			     uintptr_t data, uint32_t size);

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

#endif /* HC15XX_VDEC_H */
