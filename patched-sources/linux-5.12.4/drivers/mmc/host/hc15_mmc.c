// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Hichip HC15xx SD/MMC host for SF2000.
 *
 * The hcRTOS HC15 host library uses a compact byte-register layout at the
 * SDIO base address.  This is not the Synopsys DW-MMC register layout even
 * though vendor device trees call the block "dw-mshc".
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>

#define HC15_REG_CMDCTL		0x00
#define HC15_REG_CMDSTS		0x01
#define HC15_REG_CMDIDX		0x02
#define HC15_REG_CLKDIV_LO	0x03
#define HC15_REG_CMDARG		0x04
#define HC15_REG_BLKSIZ		0x08
#define HC15_REG_BLKCNT_LO	0x0a
#define HC15_REG_FIFO		0x0c
#define HC15_REG_PIO		0x0e
#define HC15_REG_RESP0		0x10
#define HC15_REG_IRQSTS		0x30
#define HC15_REG_CLKDIV_HI	0x34
#define HC15_REG_BLKCNT_HI	0x36
#define HC15_REG_BUS		0x0b
#define HC15_REG_TIMING		0x50

#define HC_SYS_SFCLK		0x1880007c
#define HC_SYS_RESET		0x18800084
#define HC_SYS_SDIO_MISC	0x18800094
#define HC_SYS_IO_VOLTAGE	0x18800184
#define HC_SYS_CLK_GATE0	0x18800060
#define HC_SYS_PINMUX_L		0x188004a0

#define HC_SDIO_CLK_GATE_BIT	1
#define HC_SDIO_RESET_BIT	18
#define HC_SDIO_CLK_SEL_SHIFT	20
#define HC_SDIO_CLK_SEL_MASK	(0x3 << HC_SDIO_CLK_SEL_SHIFT)
#define HC_SDIO_CLK_198MHZ	(0x3 << HC_SDIO_CLK_SEL_SHIFT)

#define HC15_MAX_CLOCK		25000000
#define HC15_MIN_CLOCK		400000
#define HC15_INPUT_CLOCK	198000000

struct hc15_mmc {
	struct mmc_host *mmc;
	void __iomem *base;
	struct device *dev;
	u32 clock;
	u32 actual_clock;
};

#ifdef CONFIG_MIPS_SF2000
extern void sf2000_progress_mark(const char *name, unsigned int kind,
				 unsigned int value);

static void hc15_mark(const char *name, u32 value)
{
	sf2000_progress_mark(name, 0x35, value);
}
#else
static void hc15_mark(const char *name, u32 value) { }
#endif

static void hc15_update32(u32 phys, u32 clear, u32 set)
{
	void __iomem *reg = ioremap(phys, sizeof(u32));
	u32 value;

	if (!reg) {
		hc15_mark("hc15-mapfail", phys);
		return;
	}

	value = readl(reg);
	value &= ~clear;
	value |= set;
	writel(value, reg);
	iounmap(reg);
}

static void hc15_write8_phys(u32 phys, u8 value)
{
	void __iomem *reg = ioremap(phys, sizeof(u8));

	if (!reg)
		return;
	writeb(value, reg);
	iounmap(reg);
}

static void hc15_prepare_soc(void)
{
	unsigned int pin;

	for (pin = 16; pin <= 21; pin++)
		hc15_write8_phys(HC_SYS_PINMUX_L + pin, 4);
	hc15_write8_phys(HC_SYS_PINMUX_L + 22, 0);

	hc15_update32(HC_SYS_CLK_GATE0, BIT(HC_SDIO_CLK_GATE_BIT), 0);
	hc15_update32(HC_SYS_SFCLK, HC_SDIO_CLK_SEL_MASK,
		      HC_SDIO_CLK_198MHZ | BIT(HC_SDIO_CLK_GATE_BIT));
	hc15_update32(HC_SYS_SDIO_MISC, 0, BIT(18) | BIT(19));
	hc15_update32(HC_SYS_IO_VOLTAGE, 0x3 << 24, 0x1 << 24);

	hc15_update32(HC_SYS_RESET, 0, BIT(HC_SDIO_RESET_BIT));
	udelay(5);
	hc15_update32(HC_SYS_RESET, BIT(HC_SDIO_RESET_BIT), 0);
	udelay(5);
}

static u8 hc15_cmd_type(struct mmc_command *cmd, struct mmc_data *data)
{
	u8 value;

	switch (cmd->flags & MMC_CMD_MASK) {
	case MMC_CMD_BC:
		value = 0x00;
		break;
	case MMC_CMD_BCR:
		value = 0x02;
		break;
	case MMC_CMD_ADTC:
		value = (data && (data->flags & MMC_DATA_WRITE)) ? 0x0a : 0x08;
		break;
	case MMC_CMD_AC:
	default:
		value = 0x04;
		break;
	}

	switch (cmd->opcode) {
	case MMC_GO_IDLE_STATE:
		break;
	case MMC_SEND_OP_COND:
	case MMC_ALL_SEND_CID:
	case SD_SEND_RELATIVE_ADDR:
	case SD_SEND_IF_COND:
		value |= 0x30;
		break;
	case MMC_SELECT_CARD:
		value |= 0x20;
		break;
	default:
		value |= 0x10;
		break;
	}

	return value;
}

static void hc15_set_command(struct hc15_mmc *host, struct mmc_command *cmd,
			     struct mmc_data *data)
{
	u8 cmdidx = readb(host->base + HC15_REG_CMDIDX) & 0x40;
	u8 cmdctl = hc15_cmd_type(cmd, data);

	writel(cmd->arg, host->base + HC15_REG_CMDARG);

	cmdidx |= cmd->opcode & 0x3f;
	if (!data || !(data->flags & MMC_DATA_READ))
		cmdidx |= 0x80;
	writeb(cmdidx, host->base + HC15_REG_CMDIDX);
	writeb(cmdctl, host->base + HC15_REG_CMDCTL);

	hc15_mark("hc15-cmd", cmd->opcode);
	hc15_mark("hc15-cmdctl", cmdctl);
	hc15_mark("hc15-cmdidx", cmdidx);
}

static void hc15_start_command(struct hc15_mmc *host)
{
	u8 value = readb(host->base + HC15_REG_CMDCTL);

	writeb(0xff, host->base + HC15_REG_IRQSTS);
	writeb(value | 0x01, host->base + HC15_REG_CMDCTL);
}

static int hc15_wait_irq(struct hc15_mmc *host, struct mmc_command *cmd)
{
	u8 irq = 0;
	u8 status;
	int ret;

	ret = readb_poll_timeout(host->base + HC15_REG_IRQSTS, irq,
				 irq & 0xc0, 10, 1000000);
	status = readb(host->base + HC15_REG_CMDSTS);
	writeb(irq, host->base + HC15_REG_IRQSTS);

	hc15_mark("hc15-irq", irq);
	hc15_mark("hc15-status", status);

	if (ret) {
		cmd->error = -ETIMEDOUT;
		return ret;
	}
	if (status & 0x01) {
		cmd->error = -EILSEQ;
		return -EILSEQ;
	}
	if (status & 0x02) {
		cmd->error = -ETIMEDOUT;
		return -ETIMEDOUT;
	}

	cmd->error = 0;
	return 0;
}

static void hc15_get_response(struct hc15_mmc *host, struct mmc_command *cmd)
{
	u32 r0 = readl(host->base + HC15_REG_RESP0);
	u32 r1 = readl(host->base + HC15_REG_RESP0 + 4);
	u32 r2 = readl(host->base + HC15_REG_RESP0 + 8);
	u32 r3 = readl(host->base + HC15_REG_RESP0 + 12);

	if (!(cmd->flags & MMC_RSP_PRESENT))
		return;

	if (cmd->flags & MMC_RSP_136) {
		cmd->resp[0] = (r3 << 24) | (r2 >> 8);
		cmd->resp[1] = (r2 << 24) | (r1 >> 8);
		cmd->resp[2] = (r1 << 24) | (r0 >> 8);
		cmd->resp[3] = r0 << 24;
	} else {
		cmd->resp[0] = (r1 << 24) | (r0 >> 8);
	}

	hc15_mark("hc15-resp0", cmd->resp[0]);
}

static void hc15_setup_data(struct hc15_mmc *host, struct mmc_data *data)
{
	u32 blocks = data->blocks ? data->blocks : 1;
	u8 cnt_hi = (blocks - 1) >> 8;
	u8 cnt_lo = (blocks - 1) & 0xff;
	u8 cmdidx = readb(host->base + HC15_REG_CMDIDX);

	writew(data->blksz, host->base + HC15_REG_BLKSIZ);
	writeb(cnt_lo, host->base + HC15_REG_BLKCNT_LO);
	writeb(cnt_hi, host->base + HC15_REG_BLKCNT_HI);
	writeb(0x04, host->base + HC15_REG_PIO);

	if (data->blksz == 256)
		cmdidx |= 0x80;
	else
		cmdidx &= ~0x80;
	writeb(cmdidx, host->base + HC15_REG_CMDIDX);

	hc15_mark("hc15-data-blksz", data->blksz);
	hc15_mark("hc15-data-blocks", blocks);
}

static int hc15_pio_read(struct hc15_mmc *host, struct mmc_data *data)
{
	struct sg_mapping_iter miter;
	u32 remaining = data->blksz * data->blocks;
	u32 done = 0;
	int ret = 0;

	sg_miter_start(&miter, data->sg, data->sg_len, SG_MITER_TO_SG);
	while (remaining && sg_miter_next(&miter)) {
		u8 *buf = miter.addr;
		size_t len = min_t(size_t, miter.length, remaining);
		size_t pos;

		for (pos = 0; pos < len; pos += 2) {
			u16 value;
			u16 pio;

			ret = readw_poll_timeout(host->base + HC15_REG_PIO,
						 pio, pio & 0x0002,
						 1, 1000000);
			if (ret)
				goto out;
			value = readw(host->base + HC15_REG_FIFO);
			buf[pos] = value & 0xff;
			if (pos + 1 < len)
				buf[pos + 1] = value >> 8;
			done += min_t(u32, 2, remaining);
			remaining -= min_t(u32, 2, remaining);
		}
	}

out:
	sg_miter_stop(&miter);
	data->bytes_xfered = done;
	if (ret)
		data->error = -ETIMEDOUT;
	hc15_mark("hc15-pio-read", done);
	return ret;
}

static int hc15_pio_write(struct hc15_mmc *host, struct mmc_data *data)
{
	struct sg_mapping_iter miter;
	u32 remaining = data->blksz * data->blocks;
	u32 done = 0;
	int ret = 0;

	sg_miter_start(&miter, data->sg, data->sg_len, SG_MITER_FROM_SG);
	while (remaining && sg_miter_next(&miter)) {
		u8 *buf = miter.addr;
		size_t len = min_t(size_t, miter.length, remaining);
		size_t pos;

		for (pos = 0; pos < len; pos += 2) {
			u16 value = buf[pos];
			u16 pio;

			if (pos + 1 < len)
				value |= (u16)buf[pos + 1] << 8;
			writew(value, host->base + HC15_REG_FIFO);
			ret = readw_poll_timeout(host->base + HC15_REG_PIO,
						 pio, !(pio & 0x0002),
						 1, 1000000);
			if (ret)
				goto out;
			done += min_t(u32, 2, remaining);
			remaining -= min_t(u32, 2, remaining);
		}
	}

out:
	sg_miter_stop(&miter);
	data->bytes_xfered = done;
	if (ret)
		data->error = -ETIMEDOUT;
	hc15_mark("hc15-pio-write", done);
	return ret;
}

static void hc15_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct hc15_mmc *host = mmc_priv(mmc);
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_data *data = mrq->data;

	if (data)
		hc15_setup_data(host, data);

	hc15_set_command(host, cmd, data);
	hc15_start_command(host);

	if (data && (data->flags & MMC_DATA_READ))
		hc15_pio_read(host, data);
	else if (data && (data->flags & MMC_DATA_WRITE))
		hc15_pio_write(host, data);

	hc15_wait_irq(host, cmd);
	hc15_get_response(host, cmd);

	if (mrq->stop && !cmd->error && (!data || !data->error)) {
		hc15_set_command(host, mrq->stop, NULL);
		hc15_start_command(host);
		hc15_wait_irq(host, mrq->stop);
		hc15_get_response(host, mrq->stop);
	}

	mmc_request_done(mmc, mrq);
}

static void hc15_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct hc15_mmc *host = mmc_priv(mmc);
	u32 div = 0;
	u32 actual = 0;
	u8 bus;

	if (ios->clock) {
		u32 clock = min_t(u32, ios->clock, HC15_MAX_CLOCK);

		div = DIV_ROUND_UP(HC15_INPUT_CLOCK, clock * 2);
		if (!div)
			div = 1;
		if (div > 0xffff)
			div = 0xffff;
		actual = HC15_INPUT_CLOCK / (div * 2);
	}

	writeb(div & 0xff, host->base + HC15_REG_CLKDIV_LO);
	writeb((div >> 8) & 0xff, host->base + HC15_REG_CLKDIV_HI);
	host->clock = ios->clock;
	host->actual_clock = actual;
	mmc->actual_clock = actual;

	bus = readb(host->base + HC15_REG_BUS) & 0xf1;
	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_8:
		bus |= 0x02;
		break;
	case MMC_BUS_WIDTH_4:
		bus |= 0x08;
		break;
	case MMC_BUS_WIDTH_1:
	default:
		bus |= 0x04;
		break;
	}
	writeb(bus, host->base + HC15_REG_BUS);
	writeb(0, host->base + HC15_REG_TIMING);

	hc15_mark("hc15-set-clock", ios->clock);
	hc15_mark("hc15-actual-clock", actual);
	hc15_mark("hc15-bus", bus);
}

static int hc15_get_cd(struct mmc_host *mmc)
{
	return 1;
}

static int hc15_get_ro(struct mmc_host *mmc)
{
	return 0;
}

static const struct mmc_host_ops hc15_ops = {
	.request = hc15_request,
	.set_ios = hc15_set_ios,
	.get_cd = hc15_get_cd,
	.get_ro = hc15_get_ro,
};

static int hc15_mmc_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct hc15_mmc *host;
	struct resource *res;
	int ret;

	hc15_mark("hc15-probe", 0x0180);
	hc15_prepare_soc();

	mmc = mmc_alloc_host(sizeof(*host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto err_free;
	}

	mmc->ops = &hc15_ops;
	mmc->f_min = HC15_MIN_CLOCK;
	mmc->f_max = HC15_MAX_CLOCK;
	mmc->caps = MMC_CAP_NEEDS_POLL;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->max_blk_size = 512;
	mmc->max_blk_count = 128;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_seg_size = mmc->max_req_size;
	mmc->max_segs = 16;

	platform_set_drvdata(pdev, mmc);
	ret = mmc_add_host(mmc);
	hc15_mark("hc15-add-host", (u32)ret);
	if (ret)
		goto err_free;

	dev_info(&pdev->dev, "HC15 SD/MMC host registered, 1-bit max %u Hz\n",
		 HC15_MAX_CLOCK);
	return 0;

err_free:
	mmc_free_host(mmc);
	return ret;
}

static int hc15_mmc_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	mmc_remove_host(mmc);
	mmc_free_host(mmc);
	return 0;
}

static const struct of_device_id hc15_mmc_match[] = {
	{ .compatible = "hichip,hc15-mmc" },
	{ }
};
MODULE_DEVICE_TABLE(of, hc15_mmc_match);

static struct platform_driver hc15_mmc_driver = {
	.probe = hc15_mmc_probe,
	.remove = hc15_mmc_remove,
	.driver = {
		.name = "hc15-mmc",
		.of_match_table = hc15_mmc_match,
	},
};
module_platform_driver(hc15_mmc_driver);

MODULE_DESCRIPTION("Hichip HC15xx SD/MMC host");
MODULE_LICENSE("GPL");
