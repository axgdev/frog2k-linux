/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_DSC_H
#define HC15XX_DSC_H

#include <stdint.h>

struct hc15xx_dsc_io {
	void *cookie;
	uint32_t (*read32)(void *cookie, uintptr_t address);
	void (*write32)(void *cookie, uintptr_t address, uint32_t value);
	void (*delay_us)(void *cookie, unsigned int usec);
};

/* Register offsets from base (read from device tree "reg" property) */
#define HC15XX_DSC_REG_CTRL		0x00
#define HC15XX_DSC_REG_IRQ_STATUS	0x04
#define HC15XX_DSC_REG_VSYNC_COUNT	0x08

/* IRQ status bits */
#define HC15XX_DSC_IRQ_VBLANK		BIT(0)
#define HC15XX_DSC_IRQ_LINE		BIT(1)

/* Control bits */
#define HC15XX_DSC_CTRL_ENABLE		BIT(0)
#define HC15XX_DSC_CTRL_VBLANK_IRQ	BIT(1)

/* TV system types (from hcuapi/tvtype.h) */
enum hc15xx_dsc_tvtype {
	HC15XX_DSC_TV_PAL = 0,
	HC15XX_DSC_TV_NTSC,
	HC15XX_DSC_TV_PAL_M,
	HC15XX_DSC_TV_PAL_N,
	HC15XX_DSC_TV_NTSC_443,
	HC15XX_DSC_TV_PAL_60,
	HC15XX_DSC_TV_SECAM,
};

/* DAC output types (from hcuapi/dis.h) */
enum hc15xx_dsc_dac_type {
	HC15XX_DSC_DAC_CVBS = 0,
	HC15XX_DSC_DAC_SVIDEO,
	HC15XX_DSC_DAC_YUV,
	HC15XX_DSC_DAC_RGB,
};

/* DAC indices */
#define HC15XX_DSC_DAC_0		(1 << 0)
#define HC15XX_DSC_DAC_1		(1 << 1)
#define HC15XX_DSC_DAC_2		(1 << 2)

struct hc15xx_dsc_dac_config {
	enum hc15xx_dsc_dac_type type;
	int enable;
	int progressive;
	uint32_t dac_mask;	/* Which DACs to use (HC15XX_DSC_DAC_x) */
};

struct hc15xx_dsc_bg_color {
	uint8_t y;
	uint8_t cb;
	uint8_t cr;
};

struct hc15xx_dsc {
	const struct hc15xx_dsc_io *io;
	uintptr_t base;
	uint32_t vsync_count;
	uint32_t irq_accum;
	int enabled;
};

void hc15xx_dsc_init(struct hc15xx_dsc *dsc, const struct hc15xx_dsc_io *io,
		     uintptr_t base);
int hc15xx_dsc_enable(struct hc15xx_dsc *dsc);
void hc15xx_dsc_disable(struct hc15xx_dsc *dsc);
int hc15xx_dsc_handle_irq(struct hc15xx_dsc *dsc);
int hc15xx_dsc_wait_vblank(struct hc15xx_dsc *dsc, uint32_t target_count);
uint32_t hc15xx_dsc_get_vsync_count(struct hc15xx_dsc *dsc);
int hc15xx_dsc_set_dac(struct hc15xx_dsc *dsc,
		       const struct hc15xx_dsc_dac_config *cfg);
int hc15xx_dsc_set_bg_color(struct hc15xx_dsc *dsc,
			    const struct hc15xx_dsc_bg_color *color);
int hc15xx_dsc_suspend(struct hc15xx_dsc *dsc);
int hc15xx_dsc_resume(struct hc15xx_dsc *dsc);

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

#endif /* HC15XX_DSC_H */
