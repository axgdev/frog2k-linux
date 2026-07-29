// SPDX-License-Identifier: MIT
/*
 * sf2000-player - WAV audio player for SF2000.
 *
 * Self-contained RIFF/WAV parser with linear resampling to 32kHz mono S16.
 * No external libraries — keeps the binary small enough for bFLT relocations
 * to work reliably on this NOMMU platform.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sound/asound.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define AUDIO_RATE 32000u
#define AUDIO_PERIOD 1024u
#define AUDIO_PERIODS 8u

static int pcm_fd = -1;
static int input_fd = -1;

static void log_kmsg(const char *msg)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		write(fd, msg, strlen(msg));
		close(fd);
	}
}

/* --- ALSA --- */

static struct snd_mask *pcm_param_mask(struct snd_pcm_hw_params *p,
	unsigned n)
{
	return &p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}

static struct snd_interval *pcm_param_interval(struct snd_pcm_hw_params *p,
	unsigned n)
{
	return &p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

static void pcm_set_mask(struct snd_pcm_hw_params *p, unsigned n, unsigned v)
{
	struct snd_mask *m = pcm_param_mask(p, n);

	memset(m, 0, sizeof(*m));
	m->bits[v >> 5] = 1u << (v & 31);
}

static void pcm_set_interval(struct snd_pcm_hw_params *p, unsigned n,
	unsigned v)
{
	struct snd_interval *iv = pcm_param_interval(p, n);

	memset(iv, 0, sizeof(*iv));
	iv->min = v;
	iv->max = v;
	iv->integer = 1;
}

static int open_audio(void)
{
	struct snd_pcm_hw_params hw;
	struct snd_pcm_sw_params sw;
	unsigned i;

	pcm_fd = open("/dev/snd/pcmC0D0p", O_WRONLY | O_CLOEXEC);
	if (pcm_fd < 0)
		return -1;
	memset(&hw, 0, sizeof(hw));
	for (i = 0; i < sizeof(hw.masks) / sizeof(hw.masks[0]); i++)
		memset(&hw.masks[i], 0xff, sizeof(hw.masks[i]));
	for (i = 0; i < sizeof(hw.intervals) / sizeof(hw.intervals[0]); i++) {
		hw.intervals[i].max = UINT32_MAX;
		hw.intervals[i].integer = 1;
	}
	hw.rmask = ~0u;
	pcm_set_mask(&hw, SNDRV_PCM_HW_PARAM_ACCESS,
		SNDRV_PCM_ACCESS_RW_INTERLEAVED);
	pcm_set_mask(&hw, SNDRV_PCM_HW_PARAM_FORMAT,
		SNDRV_PCM_FORMAT_S16_LE);
	pcm_set_mask(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT,
		SNDRV_PCM_SUBFORMAT_STD);
	pcm_set_interval(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, 1);
	pcm_set_interval(&hw, SNDRV_PCM_HW_PARAM_RATE, AUDIO_RATE);
	pcm_set_interval(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, AUDIO_PERIOD);
	pcm_set_interval(&hw, SNDRV_PCM_HW_PARAM_PERIODS, AUDIO_PERIODS);
	pcm_set_interval(&hw, SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
		AUDIO_PERIOD * AUDIO_PERIODS);
	if (ioctl(pcm_fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw) < 0)
		goto fail;
	memset(&sw, 0, sizeof(sw));
	sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
	sw.period_step = 1;
	sw.avail_min = AUDIO_PERIOD;
	sw.start_threshold = AUDIO_PERIOD * (AUDIO_PERIODS - 1);
	sw.stop_threshold = AUDIO_PERIOD * AUDIO_PERIODS;
	sw.boundary = 0x40000000u;
	sw.proto = SNDRV_PCM_VERSION;
	if (ioctl(pcm_fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0 ||
			ioctl(pcm_fd, SNDRV_PCM_IOCTL_PREPARE) < 0)
		goto fail;
	return 0;
fail:
	close(pcm_fd);
	pcm_fd = -1;
	return -1;
}

/* --- Input --- */

static int open_input(void)
{
	input_fd = open("/dev/input/event0", O_RDONLY | O_CLOEXEC);
	return input_fd < 0 ? -1 : 0;
}

static int stop_pressed(void)
{
	struct input_event ev;
	struct pollfd pfd = { .fd = input_fd, .events = POLLIN };

	while (poll(&pfd, 1, 0) > 0) {
		if (read(input_fd, &ev, sizeof(ev)) != sizeof(ev))
			break;
		if (ev.type == EV_KEY && ev.code == BTN_SOUTH && ev.value == 0)
			return 1;
	}
	return 0;
}

/* --- WAV parsing --- */

struct wav_info {
	uint16_t channels;
	uint32_t sample_rate;
	uint16_t bits_per_sample;
	uint32_t data_offset;
	uint32_t data_size;
};

static uint32_t rd_le32(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_le16(const unsigned char *p)
{
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int parse_wav(int fd, struct wav_info *wi)
{
	unsigned char hdr[12];
	ssize_t n;

	n = read(fd, hdr, 12);
	if (n < 12)
		return -1;
	if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)
		return -1;

	memset(wi, 0, sizeof(*wi));

	for (;;) {
		unsigned char chunk[8];
		uint32_t chunk_size;

		n = read(fd, chunk, 8);
		if (n < 8)
			break;
		chunk_size = rd_le32(chunk + 4);

		if (memcmp(chunk, "fmt ", 4) == 0) {
			unsigned char fmt[40];
			uint32_t to_read = chunk_size < 40 ? chunk_size : 40;

			n = read(fd, fmt, to_read);
			if (n < 16)
				return -1;
			if (rd_le16(fmt) != 1)
				return -1; /* PCM only */
			wi->channels = rd_le16(fmt + 2);
			wi->sample_rate = rd_le32(fmt + 4);
			wi->bits_per_sample = rd_le16(fmt + 14);
			if (wi->bits_per_sample != 8 && wi->bits_per_sample != 16)
				return -1;
			if (chunk_size > to_read)
				lseek(fd, chunk_size - to_read, SEEK_CUR);
		} else if (memcmp(chunk, "data", 4) == 0) {
			wi->data_offset = (uint32_t)lseek(fd, 0, SEEK_CUR);
			wi->data_size = chunk_size;
			break;
		} else {
			lseek(fd, chunk_size, SEEK_CUR);
		}
		/* chunks are word-aligned */
		if (chunk_size & 1)
			lseek(fd, 1, SEEK_CUR);
	}

	if (!wi->data_size || !wi->channels || !wi->sample_rate)
		return -1;
	return 0;
}

/* --- Playback --- */

#define READ_BUF_SAMPLES 2048u

static int play_wav(int fd, const struct wav_info *wi)
{
	int16_t out_buf[AUDIO_PERIOD];
	unsigned src_bytes_per_sample = wi->bits_per_sample / 8;
	unsigned src_frame_size = src_bytes_per_sample * wi->channels;
	uint32_t total_frames = wi->data_size / src_frame_size;
	uint32_t out_total = 0;
	uint64_t src_pos = 0;
	char line[128];

	lseek(fd, wi->data_offset, SEEK_SET);

	snprintf(line, sizeof(line),
		"sf2000-player: %uHz %uch %ubit -> %uHz mono\n",
		wi->sample_rate, wi->channels, wi->bits_per_sample, AUDIO_RATE);
	log_kmsg(line);

	/*
	 * Linear resampling: for each output sample at AUDIO_RATE, compute
	 * the corresponding source position and interpolate.
	 */
	unsigned out_idx = 0;

	for (;;) {
		uint64_t src_frame_target;
		int16_t sample;

		if (stop_pressed())
			break;

		/* source frame index for this output sample */
		src_frame_target = (uint64_t)out_total * wi->sample_rate /
			AUDIO_RATE;
		if (src_frame_target >= total_frames)
			break;

		/* read the source frame */
		if (src_frame_target != src_pos) {
			lseek(fd, wi->data_offset +
				src_frame_target * src_frame_size, SEEK_SET);
			src_pos = src_frame_target;
		}

		/* decode one frame to mono S16 */
		if (wi->bits_per_sample == 16) {
			int32_t sum = 0;
			unsigned ch;
			unsigned char raw[8];

			if (read(fd, raw, src_frame_size) < (ssize_t)src_frame_size)
				break;
			for (ch = 0; ch < wi->channels; ch++)
				sum += (int16_t)rd_le16(raw + ch * 2);
			sample = (int16_t)(sum / (int32_t)wi->channels);
		} else {
			/* 8-bit unsigned PCM */
			int32_t sum = 0;
			unsigned ch;
			unsigned char raw[8];

			if (read(fd, raw, src_frame_size) < (ssize_t)src_frame_size)
				break;
			for (ch = 0; ch < wi->channels; ch++)
				sum += ((int32_t)raw[ch] - 128) << 8;
			sample = (int16_t)(sum / (int32_t)wi->channels);
		}
		src_pos++;

		out_buf[out_idx++] = sample;
		out_total++;

		if (out_idx == AUDIO_PERIOD) {
			ssize_t off = 0;
			ssize_t bytes = AUDIO_PERIOD * sizeof(int16_t);

			while (off < bytes) {
				ssize_t w = write(pcm_fd,
					(char *)out_buf + off, bytes - off);
				if (w < 0) {
					if (errno == EINTR)
						continue;
					goto done;
				}
				off += w;
			}
			out_idx = 0;
		}
	}

	/* flush remaining samples */
	if (out_idx > 0) {
		ssize_t off = 0;
		ssize_t bytes = out_idx * sizeof(int16_t);

		while (off < bytes) {
			ssize_t w = write(pcm_fd, (char *)out_buf + off,
				bytes - off);
			if (w < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			off += w;
		}
	}

done:
	log_kmsg("sf2000-player: playback complete\n");
	return 0;
}

/* --- Main --- */

int main(int argc, char **argv)
{
	struct wav_info wi;
	char line[128];
	int fd;
	int ret;

	if (argc < 2) {
		log_kmsg("sf2000-player: usage: sf2000-player <file.wav>\n");
		return 1;
	}

	snprintf(line, sizeof(line), "sf2000-player: opening %s\n", argv[1]);
	log_kmsg(line);

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		snprintf(line, sizeof(line),
			"sf2000-player: open errno=%d\n", errno);
		log_kmsg(line);
		return 1;
	}

	if (parse_wav(fd, &wi) < 0) {
		log_kmsg("sf2000-player: not a valid PCM WAV file\n");
		close(fd);
		return 1;
	}

	open_input();

	if (open_audio() < 0) {
		log_kmsg("sf2000-player: audio open failed\n");
		close(fd);
		return 1;
	}

	log_kmsg("sf2000-player: audio playback ready\n");
	ret = play_wav(fd, &wi);

	close(fd);
	if (pcm_fd >= 0)
		close(pcm_fd);
	if (input_fd >= 0)
		close(input_fd);
	return ret;
}
