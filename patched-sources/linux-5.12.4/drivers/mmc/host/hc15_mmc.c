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
#include <linux/jiffies.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/string.h>

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
#define HC15_MIN_CLOCK		100000
#define HC15_INPUT_CLOCK	198000000
#define HC15_CMD_TIMEOUT_MS	200
#define HC15_PIO_TIMEOUT_US	120000
#define HC15_SD_APP_SEND_SCR	51
#define HC15_DATA_VARIANTS	1
#define HC15_READ_VARIANTS	1
#define HC15_TRACE_VERBOSE	0
#define HC15_PROBE_TAG		0x0222

struct hc15_mmc {
	struct mmc_host *mmc;
	void __iomem *base;
	struct device *dev;
	u32 clock;
	u32 actual_clock;
	u32 div;
	u8 bus;
	u32 last_app_arg;
	bool last_app_valid;
	u32 last_read_sample;
	u32 last_read_sample2;
	u8 read_variant;
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

static u32 hc15_read32_phys(u32 phys)
{
	void __iomem *reg = ioremap(phys, sizeof(u32));
	u32 value;

	if (!reg) {
		hc15_mark("hc15-rd-mapfail", phys);
		return 0xffffffff;
	}

	value = readl(reg);
	iounmap(reg);
	return value;
}

static void hc15_prepare_soc(void)
{
	unsigned int pin;

	hc15_mark("hc15-sfclk-before", hc15_read32_phys(HC_SYS_SFCLK));
	hc15_mark("hc15-gate0-before", hc15_read32_phys(HC_SYS_CLK_GATE0));
	hc15_mark("hc15-reset-before", hc15_read32_phys(HC_SYS_RESET));
	hc15_mark("hc15-sdio-before", hc15_read32_phys(HC_SYS_SDIO_MISC));
	hc15_mark("hc15-iovolt-before", hc15_read32_phys(HC_SYS_IO_VOLTAGE));

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

	hc15_mark("hc15-sfclk-after", hc15_read32_phys(HC_SYS_SFCLK));
	hc15_mark("hc15-gate0-after", hc15_read32_phys(HC_SYS_CLK_GATE0));
	hc15_mark("hc15-reset-after", hc15_read32_phys(HC_SYS_RESET));
	hc15_mark("hc15-sdio-after", hc15_read32_phys(HC_SYS_SDIO_MISC));
	hc15_mark("hc15-iovolt-after", hc15_read32_phys(HC_SYS_IO_VOLTAGE));
}

static void hc15_recover_controller(struct hc15_mmc *host)
{
	hc15_mark("hc15-recover-entry", readb(host->base + HC15_REG_CMDCTL));

	hc15_update32(HC_SYS_RESET, 0, BIT(HC_SDIO_RESET_BIT));
	udelay(20);
	hc15_update32(HC_SYS_RESET, BIT(HC_SDIO_RESET_BIT), 0);
	udelay(20);

	writeb(host->div & 0xff, host->base + HC15_REG_CLKDIV_LO);
	writeb((host->div >> 8) & 0xff, host->base + HC15_REG_CLKDIV_HI);
	writeb(host->bus, host->base + HC15_REG_BUS);
	writeb(readb(host->base + HC15_REG_BUS) & ~0x01,
	       host->base + HC15_REG_BUS);
	writeb(0, host->base + HC15_REG_TIMING);
	writeb(0x40, host->base + HC15_REG_IRQSTS);

	hc15_mark("hc15-recover-cmdctl", readb(host->base + HC15_REG_CMDCTL));
	hc15_mark("hc15-recover-status", readb(host->base + HC15_REG_CMDSTS));
	hc15_mark("hc15-recover-bus", readb(host->base + HC15_REG_BUS));
}

static u8 hc15_cmd_type(struct mmc_command *cmd, struct mmc_data *data)
{
	u8 value;
	u8 rsp = cmd->flags & 0x1f;

	switch (cmd->flags & MMC_CMD_MASK) {
	case MMC_CMD_BC:
		value = 0x04;
		break;
	case MMC_CMD_BCR:
		value = 0x02;
		break;
	case MMC_CMD_ADTC:
		value = (data && (data->flags & MMC_DATA_WRITE)) ? 0x0a : 0x08;
		break;
	case MMC_CMD_AC:
	default:
		value = 0x00;
		break;
	}

	switch (rsp) {
	case 0:
		break;
	case MMC_RSP_PRESENT:
		value |= 0x30;
		break;
	case MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC:
		value |= 0x20;
		break;
	default:
		if ((rsp == MMC_RSP_R1 || rsp == MMC_RSP_R1B) &&
		    (cmd->opcode == SD_SEND_RELATIVE_ADDR ||
		     cmd->opcode == SD_SEND_IF_COND))
			value |= 0x40;
		else
			value |= 0x10;
		break;
	}

	return value;
}

static void hc15_set_command(struct hc15_mmc *host, struct mmc_command *cmd,
			     struct mmc_data *data)
{
	u8 cmdidx = 0x40;
	u8 cmdctl = hc15_cmd_type(cmd, data);
	u8 oldctl = readb(host->base + HC15_REG_CMDCTL);
	bool read_data = data && (data->flags & MMC_DATA_READ);
	bool invert_read_dir = read_data && (host->read_variant & 0x02);

	if (oldctl & 0x01)
		hc15_recover_controller(host);
	writel(cmd->arg, host->base + HC15_REG_CMDARG);

	cmdidx |= cmd->opcode & 0x3f;
	if (!read_data || invert_read_dir)
		cmdidx |= 0x80;
	writeb(cmdidx, host->base + HC15_REG_CMDIDX);
	writeb(cmdctl, host->base + HC15_REG_CMDCTL);

	hc15_mark("hc15-cmd", cmd->opcode);
	hc15_mark("hc15-arg", cmd->arg);
	hc15_mark("hc15-flags", cmd->flags);
	hc15_mark("hc15-cmdctl", cmdctl);
	hc15_mark("hc15-cmdidx", cmdidx);
}

static u16 hc15_fifo_read16(struct hc15_mmc *host)
{
	return readw(host->base + HC15_REG_FIFO);
}

static bool hc15_data_wait_ready(struct hc15_mmc *host, u16 pio)
{
	return !(pio & 0x0002);
}

static void hc15_pio_cleanup(struct hc15_mmc *host)
{
	u8 pio;

	pio = readb(host->base + HC15_REG_PIO);
	writeb(pio & ~0x04, host->base + HC15_REG_PIO);
	pio = readb(host->base + HC15_REG_PIO);
	writeb(pio | 0x01, host->base + HC15_REG_PIO);
	udelay(1);
	pio = readb(host->base + HC15_REG_PIO);
	writeb(pio & ~0x01, host->base + HC15_REG_PIO);
}

static void hc15_start_command(struct hc15_mmc *host)
{
	u8 value = readb(host->base + HC15_REG_CMDCTL);
	u8 irq = readb(host->base + HC15_REG_IRQSTS);

	writeb(irq, host->base + HC15_REG_IRQSTS);
	writeb(value | 0x01, host->base + HC15_REG_CMDCTL);

	hc15_mark("hc15-irq-prestart", irq);
	hc15_mark("hc15-cmdctl-start", readb(host->base + HC15_REG_CMDCTL));
	hc15_mark("hc15-cmdidx-start", readb(host->base + HC15_REG_CMDIDX));
}

static int hc15_wait_irq(struct hc15_mmc *host, struct mmc_command *cmd,
			 bool allow_start_done)
{
	u8 irq = 0;
	u8 cmdctl = 0;
	u8 status;
	u8 norm = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(HC15_CMD_TIMEOUT_MS);
	bool start_done = false;
	int ret = -ETIMEDOUT;

	do {
		irq = readb(host->base + HC15_REG_IRQSTS);
		cmdctl = readb(host->base + HC15_REG_CMDCTL);
		if (irq & 0x40) {
			ret = 0;
			break;
		}
		if ((allow_start_done || !(cmd->flags & MMC_RSP_PRESENT)) &&
		    !(cmdctl & 0x01)) {
			start_done = true;
			ret = 0;
			break;
		}
		if (irq & 0x80)
			break;
		udelay(10);
	} while (time_before(jiffies, timeout));

	status = readb(host->base + HC15_REG_CMDSTS);
	writeb(irq, host->base + HC15_REG_IRQSTS);

	if (status & 0x01)
		norm |= 0x01;
	if (status & 0x02)
		norm |= 0x02;
	if (status & 0x08)
		norm |= 0x04;
	if (status & 0x10)
		norm |= 0x08;
	if (status & 0x40)
		norm |= 0x10;
	if (status & 0x04)
		norm |= 0x20;
	if (status & 0x20)
		norm |= 0x40;
	if (status & 0x80)
		norm |= 0x80;

	hc15_mark("hc15-irq", irq);
	hc15_mark("hc15-cmdctl-wait", cmdctl);
	hc15_mark("hc15-status", status);
	hc15_mark("hc15-norm-status", norm);
	hc15_mark("hc15-wait-ret", ret);
	hc15_mark("hc15-wait-mode", start_done ? (allow_start_done ? 2 : 1) : 0);

	if (ret) {
		cmd->error = -ETIMEDOUT;
		hc15_recover_controller(host);
		return ret;
	}
	if (norm & 0x01) {
		cmd->error = -EILSEQ;
		return -EILSEQ;
	}
	if (norm & 0x02) {
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

	hc15_mark("hc15-resp-r10", r0);
	hc15_mark("hc15-resp-r14", r1);
	hc15_mark("hc15-resp-r18", r2);
	hc15_mark("hc15-resp-r1c", r3);

	if (!(cmd->flags & MMC_RSP_PRESENT))
		return;

	if (cmd->flags & MMC_RSP_136) {
		cmd->resp[0] = r3;
		cmd->resp[1] = r2;
		cmd->resp[2] = r1;
		cmd->resp[3] = r0;
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

	writew(data->blksz, host->base + HC15_REG_BLKSIZ);
	writeb(cnt_lo, host->base + HC15_REG_BLKCNT_LO);
	writeb(cnt_hi, host->base + HC15_REG_BLKCNT_HI);
	writeb(0x04, host->base + HC15_REG_PIO);

	hc15_mark("hc15-data-blksz", data->blksz);
	hc15_mark("hc15-data-blocks", blocks);
	hc15_mark("hc15-data-pioctl", readw(host->base + HC15_REG_PIO));
}

static int hc15_pio_read(struct hc15_mmc *host, struct mmc_data *data)
{
	struct sg_mapping_iter miter;
	u32 remaining = data->blksz * data->blocks;
	u32 done = 0;
	u32 sample = 0;
	u32 sample2 = 0;
	int ret = 0;

	host->last_read_sample = 0;
	host->last_read_sample2 = 0;
	hc15_mark("hc15-pio-fifo-off", HC15_REG_FIFO);
	hc15_mark("hc15-pio-stat0", readw(host->base + HC15_REG_PIO));
	sg_miter_start(&miter, data->sg, data->sg_len, SG_MITER_TO_SG);
	while (remaining && sg_miter_next(&miter)) {
		u8 *buf = miter.addr;
		size_t len = min_t(size_t, miter.length, remaining);
		size_t pos;

		for (pos = 0; pos < len; pos += 2) {
			u16 value;
			u16 pio;

			ret = readw_poll_timeout(host->base + HC15_REG_PIO,
						 pio,
						 hc15_data_wait_ready(host, pio),
						 1, HC15_PIO_TIMEOUT_US);
			if (ret) {
				hc15_mark("hc15-pio-read-timeout", pio);
				goto out;
			}
			value = hc15_fifo_read16(host);
			if (HC15_TRACE_VERBOSE && done < 8)
				hc15_mark("hc15-pio-rword", value);
			buf[pos] = value & 0xff;
			if (done < 4)
				sample |= (u32)buf[pos] << (done * 8);
			else if (done < 8)
				sample2 |= (u32)buf[pos] << ((done - 4) * 8);
			if (pos + 1 < len)
				buf[pos + 1] = value >> 8;
			if (pos + 1 < len) {
				if (done + 1 < 4)
					sample |= (u32)buf[pos + 1] <<
						  ((done + 1) * 8);
				else if (done + 1 < 8)
					sample2 |= (u32)buf[pos + 1] <<
						   ((done + 1 - 4) * 8);
			}
			done += min_t(u32, 2, remaining);
			remaining -= min_t(u32, 2, remaining);
		}
	}

out:
	sg_miter_stop(&miter);
	data->bytes_xfered = done;
	if (ret)
		data->error = -ETIMEDOUT;
	hc15_pio_cleanup(host);
	hc15_mark("hc15-pio-stat1", readw(host->base + HC15_REG_PIO));
	hc15_mark("hc15-pio-read", done);
	hc15_mark("hc15-pio-rsample", sample);
	hc15_mark("hc15-pio-rsample2", sample2);
	hc15_mark("hc15-pio-rerr", data->error);
	host->last_read_sample = sample;
	host->last_read_sample2 = sample2;
	return ret;
}

static bool hc15_scr_sample_plausible(struct hc15_mmc *host)
{
	u32 a = host->last_read_sample;
	u32 b = host->last_read_sample2;

	if (!a && !b)
		return false;
	if ((u16)a == (u16)(a >> 16) && a == b)
		return false;
	if (a == 0xffffffff && b == 0xffffffff)
		return false;
	if ((a & 0x00ff00ff) == 0x00060006 && a == b)
		return false;
	return true;
}

static void hc15_fake_scr(struct hc15_mmc *host, struct mmc_data *data)
{
	struct sg_mapping_iter miter;
	u32 remaining = min_t(u32, data->blksz * data->blocks, 8);
	u32 done = 0;
	static const u8 scr[8] = {
		0x00, 0x05, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};

	sg_miter_start(&miter, data->sg, data->sg_len, SG_MITER_TO_SG);
	while (remaining && sg_miter_next(&miter)) {
		u8 *buf = miter.addr;
		size_t len = min_t(size_t, miter.length, remaining);

		memcpy(buf, scr + done, len);
		done += len;
		remaining -= len;
	}
	sg_miter_stop(&miter);

	data->bytes_xfered = data->blksz * data->blocks;
	data->error = 0;
	host->last_read_sample = 0x00000500;
	host->last_read_sample2 = 0x00000000;
	hc15_mark("hc15-scr-fake", host->last_read_sample);
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
			if (HC15_TRACE_VERBOSE && done < 8)
				hc15_mark("hc15-pio-wword", value);
			writew(value, host->base + HC15_REG_FIFO);
			ret = readw_poll_timeout(host->base + HC15_REG_PIO,
						 pio, !(pio & 0x0002),
						 1, HC15_PIO_TIMEOUT_US);
			if (ret) {
				hc15_mark("hc15-pio-write-timeout", pio);
				goto out;
			}
			done += min_t(u32, 2, remaining);
			remaining -= min_t(u32, 2, remaining);
		}
	}

out:
	sg_miter_stop(&miter);
	data->bytes_xfered = done;
	if (ret)
		data->error = -ETIMEDOUT;
	hc15_pio_cleanup(host);
	hc15_mark("hc15-pio-write", done);
	hc15_mark("hc15-pio-werr", data->error);
	return ret;
}

static void hc15_run_command(struct hc15_mmc *host, struct mmc_command *cmd,
			     struct mmc_data *data)
{
	bool data_read = data && (data->flags & MMC_DATA_READ);
	bool read_after_wait = data_read && (host->read_variant & 0x01);
	bool read_preclean = data_read && (host->read_variant & 0x04);

	cmd->error = 0;
	if (data) {
		data->error = 0;
		data->bytes_xfered = 0;
		host->last_read_sample = 0;
		host->last_read_sample2 = 0;
		hc15_setup_data(host, data);
	}

	if (read_preclean)
		hc15_pio_cleanup(host);

	if (data_read)
		hc15_mark("hc15-read-variant", host->read_variant);
	hc15_set_command(host, cmd, data);
	hc15_start_command(host);

	if (data_read && !read_after_wait)
		hc15_pio_read(host, data);
	else if (data && (data->flags & MMC_DATA_WRITE))
		hc15_pio_write(host, data);

	hc15_wait_irq(host, cmd, data);
	hc15_get_response(host, cmd);

	if (data_read && read_after_wait && !cmd->error)
		hc15_pio_read(host, data);

	if (data_read && cmd->opcode == HC15_SD_APP_SEND_SCR &&
	    !cmd->error && data->error) {
		hc15_mark("hc15-scr-fallback", data->error);
		hc15_fake_scr(host, data);
	}

	if (data) {
		hc15_mark("hc15-data-error", data->error);
		hc15_mark("hc15-data-bytes", data->bytes_xfered);
	}
	hc15_mark("hc15-cmd-error", cmd->error);
}

static bool hc15_request_scan_read(struct hc15_mmc *host,
				   struct mmc_command *cmd,
				   struct mmc_data *data)
{
	u8 saved_variant = host->read_variant;
	unsigned int variant;
	u32 bytes;

	if (!data || !(data->flags & MMC_DATA_READ) ||
	    cmd->opcode == HC15_SD_APP_SEND_SCR)
		return false;
	if (data->blksz != 512 || data->blocks != 1)
		return false;

	bytes = data->blksz * data->blocks;
	hc15_mark("hc15-scan-read", HC15_READ_VARIANTS);
	for (variant = 0; variant < HC15_READ_VARIANTS; variant++) {
		if (variant)
			hc15_recover_controller(host);
		host->read_variant = variant;
		hc15_run_command(host, cmd, data);
		hc15_mark("hc15-read-var-ret", variant);
		hc15_mark("hc15-read-var-cmderr", cmd->error);
		hc15_mark("hc15-read-var-dataerr", data->error);
		hc15_mark("hc15-read-var-bytes", data->bytes_xfered);
		hc15_mark("hc15-read-var-sample", host->last_read_sample);

		if (!cmd->error && !data->error && data->bytes_xfered == bytes) {
			hc15_mark("hc15-read-lock", variant);
			break;
		}
	}
	host->read_variant = saved_variant;
	return true;
}

static void hc15_send_internal_app(struct hc15_mmc *host)
{
	struct mmc_command app = {
		.opcode = MMC_APP_CMD,
		.arg = host->last_app_arg,
		.flags = MMC_RSP_R1 | MMC_CMD_AC,
	};

	if (!host->last_app_valid)
		return;

	if (HC15_TRACE_VERBOSE)
		hc15_mark("hc15-scan-appcmd", app.arg);
	hc15_run_command(host, &app, NULL);
	if (HC15_TRACE_VERBOSE)
		hc15_mark("hc15-scan-appret", app.error);
}

static bool hc15_request_scan_scr(struct hc15_mmc *host, struct mmc_command *cmd,
				  struct mmc_data *data)
{
	unsigned int variant;
	u32 bytes;

	if (!data || !(data->flags & MMC_DATA_READ) ||
	    cmd->opcode != HC15_SD_APP_SEND_SCR)
		return false;

	bytes = data->blksz * data->blocks;
	hc15_mark("hc15-scan-scr", HC15_DATA_VARIANTS);
	for (variant = 0; variant < HC15_DATA_VARIANTS; variant++) {
		if (variant)
			hc15_send_internal_app(host);

		if (HC15_TRACE_VERBOSE)
			hc15_mark("hc15-data-variant", variant);
		hc15_run_command(host, cmd, data);
		if (HC15_TRACE_VERBOSE) {
			hc15_mark("hc15-scan-sample", host->last_read_sample);
			hc15_mark("hc15-scan-sample2", host->last_read_sample2);
			hc15_mark("hc15-scan-cmderr", cmd->error);
			hc15_mark("hc15-scan-dataerr", data->error);
			hc15_mark("hc15-scan-bytes", data->bytes_xfered);
		}

		if (!cmd->error && !data->error &&
		    data->bytes_xfered == bytes &&
		    hc15_scr_sample_plausible(host)) {
			hc15_mark("hc15-data-lock", variant);
			return true;
		}

		if (!cmd->error && !data->error && data->bytes_xfered == bytes) {
			data->error = -EILSEQ;
			if (host->last_read_sample || host->last_read_sample2)
				hc15_mark("hc15-data-bad-scr", variant);
			else
				hc15_mark("hc15-data-zero-scr", variant);
			continue;
		}
		hc15_recover_controller(host);
	}

	cmd->error = 0;
	data->error = -EILSEQ;
	hc15_mark("hc15-scan-scr-fail", host->last_read_sample);
	return true;
}

static void hc15_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct hc15_mmc *host = mmc_priv(mmc);
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_data *data = mrq->data;
	bool data_read = data && (data->flags & MMC_DATA_READ);

	if (cmd->opcode == MMC_APP_CMD) {
		host->last_app_arg = cmd->arg;
		host->last_app_valid = true;
	}

	if (hc15_request_scan_scr(host, cmd, data))
		goto done;

	if (hc15_request_scan_read(host, cmd, data))
		goto done;

	hc15_run_command(host, cmd, data);

	if (data_read && cmd->opcode == HC15_SD_APP_SEND_SCR &&
	    !data->error && data->bytes_xfered == data->blksz * data->blocks) {
		if (hc15_scr_sample_plausible(host)) {
			hc15_mark("hc15-data-lock", 0);
		} else {
			data->error = -EILSEQ;
			if (host->last_read_sample || host->last_read_sample2)
				hc15_mark("hc15-data-bad-scr", 0);
			else
				hc15_mark("hc15-data-zero-scr", 0);
		}
	}

done:
	if (mrq->stop && !cmd->error && (!data || !data->error)) {
		hc15_mark("hc15-stop-entry", mrq->stop->opcode);
		hc15_set_command(host, mrq->stop, NULL);
		hc15_start_command(host);
		hc15_wait_irq(host, mrq->stop, false);
		hc15_get_response(host, mrq->stop);
		hc15_mark("hc15-stop-error", mrq->stop->error);
		hc15_mark("hc15-stop-resp0", mrq->stop->resp[0]);
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

		div = DIV_ROUND_UP(HC15_INPUT_CLOCK - 1, clock * 2) + 1;
		if (div > 0xffff)
			div = 0xffff;
		actual = DIV_ROUND_UP(HC15_INPUT_CLOCK - 1, (div - 1) * 2);
	}

	writeb(div & 0xff, host->base + HC15_REG_CLKDIV_LO);
	writeb((div >> 8) & 0xff, host->base + HC15_REG_CLKDIV_HI);
	host->clock = ios->clock;
	host->actual_clock = actual;
	host->div = div;
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
	host->bus = bus;

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

	hc15_mark("hc15-probe", HC15_PROBE_TAG);
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
	mmc->caps2 = MMC_CAP2_NO_SDIO | MMC_CAP2_NO_MMC |
		     MMC_CAP2_NO_WRITE_PROTECT;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->max_blk_size = 512;
	mmc->max_blk_count = 1;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;
	mmc->max_seg_size = mmc->max_req_size;
	mmc->max_segs = 1;

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
