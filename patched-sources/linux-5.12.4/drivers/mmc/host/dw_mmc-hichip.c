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

#define HC_SDIO_CLK_GATE_BIT	1
#define HC_SDIO_CLK_SEL_SHIFT	20
#define HC_SDIO_CLK_SEL_MASK	(0x3 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_49MHZ	(0x0 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_99MHZ	(0x1 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_150MHZ	(0x2 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_198MHZ	(0x3 << HC_SDIO_CLK_SEL_SHIFT)

static void hc_update32(u32 phys, u32 clear, u32 set)
{
	void __iomem *reg = ioremap(phys, sizeof(u32));
	u32 value;

	if (!reg)
		return;

	value = readl(reg);
	value &= ~clear;
	value |= set;
	writel(value, reg);
	iounmap(reg);
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

	pr_info("dw_mmc-hichip: bus_hz=%u clk_sel=0x%x\n",
		host->bus_hz, clk_sel >> HC_SDIO_CLK_SEL_SHIFT);

	/* Gate bit polarity follows the vendor clock gate helper: clear enables. */
	hc_update32(HC_SYS_CLK_GATE0, BIT(HC_SDIO_CLK_GATE_BIT), 0);

	/* Select the SDIO source clock and enable the related source bit. */
	hc_update32(HC_SYS_SFCLK, HC_SDIO_CLK_SEL_MASK,
		clk_sel | BIT(HC_SDIO_CLK_GATE_BIT));

	/* Conservative 3.3 V signaling for the removable boot SD card. */
	hc_update32(HC_SYS_IO_VOLTAGE, 0x3 << 24, 0x1 << 24);

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

	if (pdev->dev.of_node) {
		match = of_match_node(dw_mci_hichip_match, pdev->dev.of_node);
		if (match && match->data)
			drv_data = match->data;
	}

	return dw_mci_pltfm_register(pdev, drv_data);
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
