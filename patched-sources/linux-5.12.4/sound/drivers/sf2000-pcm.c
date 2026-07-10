// SPDX-License-Identifier: GPL-2.0-or-later
/* Data Frog SF2000 / HC15xx SND0 PCM playback driver. */

#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#define SF2000_SND_CTL08	0x08
#define SF2000_SND_CTL0C	0x0c
#define SF2000_SND_DMA_BASE	0x30
#define SF2000_SND_DMA_SIZE	0x34
#define SF2000_SND_DMA_POS	0x38
#define SF2000_SND_I2S_CTL	0x3c
#define SF2000_SND_CTL50	0x50
#define SF2000_SND_FADE		0x90

#define SF2000_DAC_CTL		0x00
#define SF2000_RATE		32000
#define SF2000_BUFFER_MAX	(128 * 1024)
#define SF2000_PERIOD_MIN	256
#define SF2000_PERIOD_MAX	8192

struct sf2000_pcm {
	struct device *dev;
	void __iomem *snd;
	void __iomem *dac;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm_substream *substream;
	struct timer_list timer;
	spinlock_t lock;
	snd_pcm_uframes_t last_period;
	bool running;
};

static const struct snd_pcm_hardware sf2000_pcm_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_BATCH,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_32000,
	.rate_min = SF2000_RATE,
	.rate_max = SF2000_RATE,
	.channels_min = 1,
	.channels_max = 1,
	.buffer_bytes_max = SF2000_BUFFER_MAX,
	.period_bytes_min = SF2000_PERIOD_MIN,
	.period_bytes_max = SF2000_PERIOD_MAX,
	.periods_min = 2,
	.periods_max = 32,
};

static void sf2000_update_bits(void __iomem *reg, u32 mask, u32 value)
{
	u32 old = readl(reg);

	writel((old & ~mask) | (value & mask), reg);
}

static snd_pcm_uframes_t sf2000_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct sf2000_pcm *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	size_t pos = (readl(chip->snd + SF2000_SND_DMA_POS) & 0xffff) << 4;

	if (pos >= frames_to_bytes(runtime, runtime->buffer_size))
		pos = 0;
	return bytes_to_frames(runtime, pos);
}

static void sf2000_pcm_timer(struct timer_list *timer)
{
	struct sf2000_pcm *chip = from_timer(chip, timer, timer);
	struct snd_pcm_substream *substream;
	snd_pcm_uframes_t period;
	unsigned long flags;
	bool elapsed = false;

	spin_lock_irqsave(&chip->lock, flags);
	substream = chip->substream;
	if (chip->running && substream) {
		period = sf2000_pcm_pointer(substream) /
			substream->runtime->period_size;
		if (period != chip->last_period) {
			chip->last_period = period;
			elapsed = true;
		}
		mod_timer(&chip->timer, jiffies + 1);
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	if (elapsed)
		snd_pcm_period_elapsed(substream);
}

static int sf2000_pcm_open(struct snd_pcm_substream *substream)
{
	int ret;

	substream->runtime->hw = sf2000_pcm_hardware;
	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);
	if (ret)
		return ret;
	return snd_pcm_hw_constraint_step(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 16);
}

static int sf2000_pcm_close(struct snd_pcm_substream *substream)
{
	struct sf2000_pcm *chip = snd_pcm_substream_chip(substream);
	unsigned long flags;

	del_timer_sync(&chip->timer);
	spin_lock_irqsave(&chip->lock, flags);
	chip->running = false;
	chip->substream = NULL;
	spin_unlock_irqrestore(&chip->lock, flags);
	return 0;
}

static int sf2000_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

static int sf2000_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int sf2000_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct sf2000_pcm *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	size_t bytes = frames_to_bytes(runtime, runtime->buffer_size);

	if (!runtime->dma_area || !bytes || bytes > SF2000_BUFFER_MAX ||
	    bytes & 0xf || upper_32_bits(runtime->dma_addr))
		return -EINVAL;

	writel(0x4200039e, chip->dac + SF2000_DAC_CTL);
	writel(0x0000ff41, chip->snd + SF2000_SND_I2S_CTL);
	writel(0x008f0000, chip->snd + SF2000_SND_FADE);
	writel(lower_32_bits(runtime->dma_addr),
		chip->snd + SF2000_SND_DMA_BASE);
	writel(bytes >> 4, chip->snd + SF2000_SND_DMA_SIZE);
	writel(0, chip->snd + SF2000_SND_DMA_POS);
	chip->last_period = 0;
	return 0;
}

static int sf2000_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct sf2000_pcm *chip = snd_pcm_substream_chip(substream);
	unsigned long flags;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		spin_lock_irqsave(&chip->lock, flags);
		chip->substream = substream;
		chip->running = true;
		spin_unlock_irqrestore(&chip->lock, flags);
		sf2000_update_bits(chip->snd + SF2000_SND_CTL08, BIT(0), BIT(0));
		sf2000_update_bits(chip->snd + SF2000_SND_CTL0C, BIT(0), BIT(0));
		sf2000_update_bits(chip->snd + SF2000_SND_CTL50, BIT(29), BIT(29));
		mod_timer(&chip->timer, jiffies + 1);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sf2000_update_bits(chip->snd + SF2000_SND_CTL50, BIT(29), 0);
		sf2000_update_bits(chip->snd + SF2000_SND_CTL0C, BIT(0), 0);
		sf2000_update_bits(chip->snd + SF2000_SND_CTL08, BIT(0), 0);
		spin_lock_irqsave(&chip->lock, flags);
		chip->running = false;
		spin_unlock_irqrestore(&chip->lock, flags);
		del_timer(&chip->timer);
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct snd_pcm_ops sf2000_pcm_ops = {
	.open = sf2000_pcm_open,
	.close = sf2000_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sf2000_pcm_hw_params,
	.hw_free = sf2000_pcm_hw_free,
	.prepare = sf2000_pcm_prepare,
	.trigger = sf2000_pcm_trigger,
	.pointer = sf2000_pcm_pointer,
	.mmap = snd_pcm_lib_default_mmap,
};

static int sf2000_pcm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sf2000_pcm *chip;
	struct snd_card *card;
	struct resource *res;
	int ret;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(29));
	if (ret)
		return dev_err_probe(dev, ret, "cannot set 29-bit DMA mask\n");

	ret = snd_card_new(dev, -1, NULL, THIS_MODULE, sizeof(*chip), &card);
	if (ret)
		return ret;
	chip = card->private_data;
	chip->dev = dev;
	chip->card = card;
	spin_lock_init(&chip->lock);
	timer_setup(&chip->timer, sf2000_pcm_timer, 0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chip->snd = devm_ioremap_resource(dev, res);
	if (IS_ERR(chip->snd)) {
		ret = PTR_ERR(chip->snd);
		goto err_card;
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	chip->dac = devm_ioremap_resource(dev, res);
	if (IS_ERR(chip->dac)) {
		ret = PTR_ERR(chip->dac);
		goto err_card;
	}

	ret = snd_pcm_new(card, "SF2000 PCM", 0, 1, 0, &chip->pcm);
	if (ret)
		goto err_card;
	snd_pcm_set_ops(chip->pcm, SNDRV_PCM_STREAM_PLAYBACK,
		&sf2000_pcm_ops);
	chip->pcm->private_data = chip;
	strscpy(chip->pcm->name, "SF2000 PCM", sizeof(chip->pcm->name));
	snd_pcm_lib_preallocate_pages_for_all(chip->pcm, SNDRV_DMA_TYPE_DEV,
		dev, 32 * 1024, SF2000_BUFFER_MAX);

	strscpy(card->driver, "SF2000", sizeof(card->driver));
	strscpy(card->shortname, "SF2000 Audio", sizeof(card->shortname));
	strscpy(card->longname, "Data Frog SF2000 HC15xx PCM",
		sizeof(card->longname));
	ret = snd_card_register(card);
	if (ret)
		goto err_card;
	platform_set_drvdata(pdev, card);
	dev_info(dev, "32 kHz S16_LE mono PCM playback ready\n");
	return 0;

err_card:
	snd_card_free(card);
	return ret;
}

static int sf2000_pcm_remove(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	struct sf2000_pcm *chip = card->private_data;

	del_timer_sync(&chip->timer);
	snd_card_free(card);
	return 0;
}

static const struct of_device_id sf2000_pcm_of_match[] = {
	{ .compatible = "hichip,hc15xx-snd0" },
	{ }
};
MODULE_DEVICE_TABLE(of, sf2000_pcm_of_match);

static struct platform_driver sf2000_pcm_driver = {
	.probe = sf2000_pcm_probe,
	.remove = sf2000_pcm_remove,
	.driver = {
		.name = "sf2000-pcm",
		.of_match_table = sf2000_pcm_of_match,
	},
};
module_platform_driver(sf2000_pcm_driver);

MODULE_DESCRIPTION("Data Frog SF2000 HC15xx PCM playback");
MODULE_AUTHOR("SF2000 Linux project");
MODULE_LICENSE("GPL");
