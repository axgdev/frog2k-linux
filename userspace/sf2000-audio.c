// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sound/asound.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SAMPLE_RATE 32000u
#define PERIOD_FRAMES 1024u
#define PERIODS 8u
#define TONE_HZ 440u

/* Rates the HC15xx SND0 APLL/I2S can generate natively (mirror of the
 * kernel driver's sf2000_pcm_rate_table).  Keeps the tone test able to
 * exercise the multi-rate path via SF2000_AUDIO_RATE= on the cmdline. */
static unsigned int cmdline_rate(void)
{
	static const unsigned int rates[] = {
		8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	};
	char line[256];
	char *needle;
	char *end;
	unsigned long value;
	int fd;
	size_t n;
	unsigned int i;

	fd = open("/proc/cmdline", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return SAMPLE_RATE;
	n = read(fd, line, sizeof(line) - 1);
	close(fd);
	if (n <= 0 || n >= sizeof(line))
		return SAMPLE_RATE;
	line[n] = '\0';
	needle = strstr(line, "SF2000_AUDIO_RATE=");
	if (!needle)
		return SAMPLE_RATE;
	needle += strlen("SF2000_AUDIO_RATE=");
	value = strtoul(needle, &end, 10);
	for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
		if (rates[i] == value)
			return (unsigned int)value;
	(void)end;
	return SAMPLE_RATE;
}

static int16_t samples[PERIOD_FRAMES * 2u];

static void log_line(const char *line)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		(void)write(fd, "<6>", 3);
		(void)write(fd, line, strlen(line));
		close(fd);
	}
}

static struct snd_mask *param_mask(struct snd_pcm_hw_params *p,
		unsigned int n)
{
	return &p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}

static struct snd_interval *param_interval(struct snd_pcm_hw_params *p,
		unsigned int n)
{
	return &p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

static void params_any(struct snd_pcm_hw_params *p)
{
	unsigned int i;

	memset(p, 0, sizeof(*p));
	for (i = 0; i < sizeof(p->masks) / sizeof(p->masks[0]); i++)
		memset(&p->masks[i], 0xff, sizeof(p->masks[i]));
	for (i = 0; i < sizeof(p->intervals) / sizeof(p->intervals[0]); i++) {
		p->intervals[i].max = UINT_MAX;
		/*
		 * Leave integer clear: forcing it on PERIOD_TIME makes
		 * 44.1 kHz fail (1024 frames at 44100 Hz is 23219.95 us,
		 * not an integer).  The kernel forces integer where it
		 * matters.
		 */
	}
	/*
	 * Request only the fixed parameters.  Including the derived time
	 * parameters makes ALSA reject 44.1 kHz (1024 frames at 44100 Hz is
	 * 23219.95 us, not representable in integer microseconds).
	 */
	p->rmask = (1u << SNDRV_PCM_HW_PARAM_ACCESS) |
		(1u << SNDRV_PCM_HW_PARAM_FORMAT) |
		(1u << SNDRV_PCM_HW_PARAM_SUBFORMAT) |
		(1u << SNDRV_PCM_HW_PARAM_CHANNELS) |
		(1u << SNDRV_PCM_HW_PARAM_RATE) |
		(1u << SNDRV_PCM_HW_PARAM_PERIOD_SIZE) |
		(1u << SNDRV_PCM_HW_PARAM_PERIODS) |
		(1u << SNDRV_PCM_HW_PARAM_BUFFER_SIZE);
}

static void param_set_mask(struct snd_pcm_hw_params *p, unsigned int n,
		unsigned int value)
{
	struct snd_mask *mask = param_mask(p, n);

	memset(mask, 0, sizeof(*mask));
	mask->bits[value >> 5] = 1u << (value & 31);
}

static void param_set_interval(struct snd_pcm_hw_params *p, unsigned int n,
		unsigned int value)
{
	struct snd_interval *interval = param_interval(p, n);

	memset(interval, 0, sizeof(*interval));
	interval->min = value;
	interval->max = value;
	interval->integer = 1;
}

static int configure_pcm(int fd, unsigned int channels,
		unsigned int rate)
{
	struct snd_pcm_hw_params hw;
	struct snd_pcm_sw_params sw;

	params_any(&hw);
	param_set_mask(&hw, SNDRV_PCM_HW_PARAM_ACCESS,
		SNDRV_PCM_ACCESS_RW_INTERLEAVED);
	param_set_mask(&hw, SNDRV_PCM_HW_PARAM_FORMAT,
		SNDRV_PCM_FORMAT_S16_LE);
	param_set_mask(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT,
		SNDRV_PCM_SUBFORMAT_STD);
	param_set_interval(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, channels);
	param_set_interval(&hw, SNDRV_PCM_HW_PARAM_RATE, rate);
	param_set_interval(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, PERIOD_FRAMES);
	param_set_interval(&hw, SNDRV_PCM_HW_PARAM_PERIODS, PERIODS);
	param_set_interval(&hw, SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
		PERIOD_FRAMES * PERIODS);
	if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw) < 0) {
		char detail[96];

		snprintf(detail, sizeof(detail),
			"sf2000-audio: hw_params failed rate=%u channels=%u errno=%d\n",
			rate, channels, errno);
		log_line(detail);
		return -1;
	}

	memset(&sw, 0, sizeof(sw));
	sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
	sw.period_step = 1;
	sw.avail_min = PERIOD_FRAMES;
	sw.start_threshold = PERIOD_FRAMES;
	sw.stop_threshold = PERIOD_FRAMES * PERIODS;
	sw.boundary = 0x40000000u;
	sw.proto = SNDRV_PCM_VERSION;
	if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0)
		return -1;
	return ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
}

static void fill_tone(unsigned int channels, unsigned int rate)
{
	static uint32_t phase;
	uint32_t step = (uint32_t)(((uint64_t)TONE_HZ << 32) / rate);
	unsigned int i;

	for (i = 0; i < PERIOD_FRAMES; i++) {
		int16_t sample = (phase & 0x80000000u) ? -4096 : 4096;

		samples[i * channels] = sample;
		if (channels == 2u)
			samples[i * channels + 1u] = sample;
		phase += step;
	}
}

int main(void)
{
	int fd;
	bool announced = false;
	unsigned int channels = 2;
	unsigned int rate = cmdline_rate();

	fd = open("/dev/snd/pcmC0D0p", O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		log_line("sf2000-audio: cannot open ALSA PCM\n");
		return 1;
	}
	if (configure_pcm(fd, channels, rate) < 0) {
		/* SF2000 is physically mono; GB300 uses the vendor stereo path. */
		close(fd);
		channels = 1;
		fd = open("/dev/snd/pcmC0D0p", O_WRONLY | O_CLOEXEC);
		if (fd < 0 || configure_pcm(fd, channels, rate) < 0) {
			char detail[128];

			snprintf(detail, sizeof(detail),
				"sf2000-audio: cannot configure ALSA PCM "
				"channels=%u rate=%u errno=%d\n",
				channels, rate, errno);
			log_line(detail);
			if (fd >= 0)
				close(fd);
			return 1;
		}
	}

	for (;;) {
		ssize_t done;

		size_t bytes = PERIOD_FRAMES * channels * sizeof(samples[0]);

		fill_tone(channels, rate);
		done = write(fd, samples, bytes);
		if (done == (ssize_t)bytes) {
			if (!announced) {
				char detail[96];

				snprintf(detail, sizeof(detail),
					"sf2000-audio: ALSA PCM DMA tone active rate=%u\n",
					rate);
				log_line(detail);
				announced = true;
			}
			continue;
		}
		if (done < 0 && errno == EPIPE) {
			(void)ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
			continue;
		}
		log_line("sf2000-audio: ALSA PCM write failed\n");
		close(fd);
		return 1;
	}
}
