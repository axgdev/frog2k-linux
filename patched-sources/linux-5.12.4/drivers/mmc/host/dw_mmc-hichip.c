// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal Hichip HC15xx/HC16xx glue for the Synopsys DW-MSHC controller.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define HC_SYS_SFCLK		0x1880007c
#define HC_SYS_IO_VOLTAGE	0x18800184
#define HC_SYS_CLK_GATE0	0x18800060
#define HC_SYS_PINMUX_L		0x188004a0

#define HC_SDIO_CLK_GATE_BIT	1
#define HC_SDIO_CLK_SEL_SHIFT	20
#define HC_SDIO_CLK_SEL_MASK	(0x3 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_49MHZ	(0x0 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_99MHZ	(0x1 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_150MHZ	(0x2 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_198MHZ	(0x3 << HC_SDIO_CLK_SEL_SHIFT)

#define HC_SDIO_PIN_FIRST	16
#define HC_SDIO_PIN_LAST	21
#define HC_SDIO_PIN_CD		22

#ifdef CONFIG_MIPS_SF2000
extern void sf2000_progress_mark(const char *name, unsigned int kind,
				 unsigned int value);

static void hc_mark(const char *name, u32 value)
{
	sf2000_progress_mark(name, 0x33, value);
}
#else
static void hc_mark(const char *name, u32 value) { }
#endif

static void hc_update32(u32 phys, u32 clear, u32 set)
{
	void __iomem *reg = ioremap(phys, sizeof(u32));
	u32 value;

	if (!reg) {
		hc_mark("mmc-mapfail", phys);
		return;
	}

	value = readl(reg);
	value &= ~clear;
	value |= set;
	writel(value, reg);
	iounmap(reg);
}

static void hc_write8(u32 phys, u8 value)
{
	void __iomem *reg = ioremap(phys, sizeof(u8));

	if (!reg)
		return;
	writeb(value, reg);
	iounmap(reg);
}

static u32 hc_read32(u32 phys)
{
	void __iomem *reg = ioremap(phys, sizeof(u32));
	u32 value = 0xffffffff;

	if (!reg)
		return value;
	value = readl(reg);
	iounmap(reg);
	return value;
}

static u8 hc_read8(u32 phys)
{
	void __iomem *reg = ioremap(phys, sizeof(u8));
	u8 value = 0xff;

	if (!reg)
		return value;
	value = readb(reg);
	iounmap(reg);
	return value;
}

static void hc_sdio_dump_regs(const char *where)
{
	pr_info("dw_mmc-hichip: %s sfclk=0x%08x gate0=0x%08x iov=0x%08x pinmux-l16-22=%02x %02x %02x %02x %02x %02x %02x\n",
		where, hc_read32(HC_SYS_SFCLK), hc_read32(HC_SYS_CLK_GATE0),
		hc_read32(HC_SYS_IO_VOLTAGE),
		hc_read8(HC_SYS_PINMUX_L + 16),
		hc_read8(HC_SYS_PINMUX_L + 17),
		hc_read8(HC_SYS_PINMUX_L + 18),
		hc_read8(HC_SYS_PINMUX_L + 19),
		hc_read8(HC_SYS_PINMUX_L + 20),
		hc_read8(HC_SYS_PINMUX_L + 21),
		hc_read8(HC_SYS_PINMUX_L + 22));
}

static void hc_sdio_mark_regs(const char *prefix)
{
	if (prefix[0] == 'b') {
		hc_mark("mmc-before-sfclk", hc_read32(HC_SYS_SFCLK));
		hc_mark("mmc-before-gate0", hc_read32(HC_SYS_CLK_GATE0));
		hc_mark("mmc-before-iov", hc_read32(HC_SYS_IO_VOLTAGE));
	} else {
		hc_mark("mmc-after-sfclk", hc_read32(HC_SYS_SFCLK));
		hc_mark("mmc-after-gate0", hc_read32(HC_SYS_CLK_GATE0));
		hc_mark("mmc-after-iov", hc_read32(HC_SYS_IO_VOLTAGE));
	}
}

static void hc_sdio_pinmux(void)
{
	unsigned int pin;

	for (pin = HC_SDIO_PIN_FIRST; pin <= HC_SDIO_PIN_LAST; pin++)
		hc_write8(HC_SYS_PINMUX_L + pin, 4);

	/* L22 is card-detect GPIO in the SF2000 hcrtos board description. */
	hc_write8(HC_SYS_PINMUX_L + HC_SDIO_PIN_CD, 0);

	pr_info("dw_mmc-hichip: pinmux L16-L21=sdio L22=gpio\n");
}

static u32 hc_sdio_clock_select(unsigned long hz)
{
	if (hz >= 198000000)
		return HC_SDIO_CLK_198MHZ;
	if (hz >= 150000000)
		return HC_SDIO_CLK_150MHZ;
	if (hz >= 99000000)
		return HC_SDIO_CLK_99MHZ;
	return HC_SDIO_CLK_49MHZ;
}

static int dw_mci_hichip_init(struct dw_mci *host)
{
	u32 clk_sel = hc_sdio_clock_select(host->bus_hz);

	hc_mark("mmc-init-entry", host->bus_hz);
	hc_mark("mmc-init-hcon", mci_readl(host, HCON));
	hc_sdio_mark_regs("before");
	hc_sdio_dump_regs("before-init");
	hc_sdio_pinmux();

	pr_info("dw_mmc-hichip: bus_hz=%u clk_sel=0x%x\n",
		host->bus_hz, clk_sel >> HC_SDIO_CLK_SEL_SHIFT);

	/* Gate bit polarity follows the vendor clock gate helper: clear enables. */
	hc_update32(HC_SYS_CLK_GATE0, BIT(HC_SDIO_CLK_GATE_BIT), 0);

	/* Select the SDIO source clock and enable the related source bit. */
	hc_update32(HC_SYS_SFCLK, HC_SDIO_CLK_SEL_MASK,
		clk_sel | BIT(HC_SDIO_CLK_GATE_BIT));

	/* Conservative 3.3 V signaling for the removable boot SD card. */
	hc_update32(HC_SYS_IO_VOLTAGE, 0x3 << 24, 0x1 << 24);

	hc_sdio_dump_regs("after-init");
	hc_sdio_mark_regs("after");
	hc_mark("mmc-init-done", mci_readl(host, HCON));
	return 0;
}

static const struct dw_mci_drv_data hichip_data = {
	.init = dw_mci_hichip_init,
};

static const struct of_device_id dw_mci_hichip_match[] = {
	{
		.compatible = "hichip,dw-mshc",
		.data = &hichip_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_hichip_match);

static int dw_mci_hichip_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data = &hichip_data;
	const struct of_device_id *match;
	struct resource *res;
	int irq;
	int ret;

	hc_mark("mmc-probe-entry", 0x0177);
	hc_sdio_mark_regs("before");
	hc_sdio_pinmux();
	hc_update32(HC_SYS_CLK_GATE0, BIT(HC_SDIO_CLK_GATE_BIT), 0);
	hc_update32(HC_SYS_SFCLK, HC_SDIO_CLK_SEL_MASK,
		HC_SDIO_CLK_49MHZ | BIT(HC_SDIO_CLK_GATE_BIT));
	hc_update32(HC_SYS_IO_VOLTAGE, 0x3 << 24, 0x1 << 24);
	hc_sdio_mark_regs("after");
	if (pdev->dev.of_node) {
		match = of_match_node(dw_mci_hichip_match, pdev->dev.of_node);
		if (match && match->data)
			drv_data = match->data;
	}

	irq = platform_get_irq_optional(pdev, 0);
	hc_mark("mmc-probe-irq", (u32)irq);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		hc_mark("mmc-probe-res-start", (u32)res->start);
		hc_mark("mmc-probe-res-size", (u32)resource_size(res));
		dev_info(&pdev->dev, "precheck irq=%d res=%pa size=0x%llx\n",
			 irq, &res->start, (unsigned long long)resource_size(res));
	} else {
		hc_mark("mmc-probe-no-res", 0xffffffff);
		dev_info(&pdev->dev, "precheck irq=%d no IORESOURCE_MEM\n", irq);
	}

	dev_info(&pdev->dev, "registering Hichip DW-MSHC host\n");

	ret = dw_mci_pltfm_register(pdev, drv_data);
	hc_mark("mmc-probe-ret", (u32)ret);
	if (ret)
		dev_err(&pdev->dev, "Hichip DW-MSHC registration failed ret=%d\n",
			ret);
	return ret;
}

static struct platform_driver dw_mci_hichip_driver = {
	.probe = dw_mci_hichip_probe,
	.remove = dw_mci_pltfm_remove,
	.driver = {
		.name = "dw_mmc-hichip",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = dw_mci_hichip_match,
		.pm = &dw_mci_pltfm_pmops,
	},
};
module_platform_driver(dw_mci_hichip_driver);

MODULE_DESCRIPTION("Hichip HC15xx/HC16xx DW-MSHC glue");
MODULE_LICENSE("GPL");
