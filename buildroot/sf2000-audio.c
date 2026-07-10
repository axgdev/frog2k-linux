// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define KSEG1ADDR(x) ((volatile uint32_t *)((uintptr_t)(x) | 0xa0000000u))
#define I2S_BASE_PHYS 0x1880a000u
#define DAC_BASE_PHYS 0x1880b000u
#define DMA_BYTES 8192u
#define SAMPLE_RATE 32000u
#define TONE_HZ 440u

static int16_t pcm[DMA_BYTES / sizeof(int16_t)] __attribute__((aligned(32)));

static void log_line(const char *line)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		(void)write(fd, "<6>", 3);
		(void)write(fd, line, strlen(line));
		close(fd);
	}
}

static void fill_tone(void)
{
	uint32_t phase = 0;
	uint32_t step = (uint32_t)(((uint64_t)TONE_HZ << 32) / SAMPLE_RATE);
	unsigned int i;

	for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i++) {
		pcm[i] = (phase & 0x80000000u) ? -4096 : 4096;
		phase += step;
	}
}

int main(void)
{
	volatile uint32_t *i2s = KSEG1ADDR(I2S_BASE_PHYS);
	volatile uint32_t *dac = KSEG1ADDR(DAC_BASE_PHYS);
	struct timespec delay = { .tv_sec = 1, .tv_nsec = 0 };
	uint32_t physical;

	fill_tone();
	physical = (uint32_t)(uintptr_t)pcm & 0x1fffffffu;

	dac[0] = 0x4200039eu;
	i2s[0x3c / 4] = 0x0000ff41u;
	i2s[0x90 / 4] = 0x008f0000u;
	i2s[0x30 / 4] = physical;
	i2s[0x34 / 4] = DMA_BYTES >> 4;
	i2s[0x38 / 4] = 0;
	i2s[0x08 / 4] |= 1u;
	i2s[0x0c / 4] |= 1u;
	i2s[0x50 / 4] |= 1u << 29;
	log_line("sf2000-audio: PCM DMA tone active\n");

	for (;;)
		while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
			;
}
