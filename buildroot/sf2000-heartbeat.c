// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SYSIO_BASE_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define PINMUX_R_OFF 0x4e0u
#define GPIO_R_OUT_OFF 0xf4u
#define GPIO_R_DIR_OFF 0xf8u
#define PIN_R05 5u
#define BACKLIGHT_R05 (1u << PIN_R05)

static volatile uint8_t *sysio;
static volatile sig_atomic_t stopping;

static void log_line(const char *line)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		(void)write(fd, "<6>", 3);
		(void)write(fd, line, strlen(line));
		close(fd);
		return;
	}

	(void)write(STDERR_FILENO, line, strlen(line));
}

static void sleep_ms(unsigned msec)
{
	struct timespec ts;

	ts.tv_sec = msec / 1000u;
	ts.tv_nsec = (long)(msec % 1000u) * 1000000L;
	while (nanosleep(&ts, &ts) < 0 && errno == EINTR && !stopping)
		;
}

static uint32_t mmio_read32(uint32_t off)
{
	return *(volatile uint32_t *)(sysio + off);
}

static void mmio_write32(uint32_t off, uint32_t value)
{
	*(volatile uint32_t *)(sysio + off) = value;
}

static void mmio_write8(uint32_t off, uint8_t value)
{
	*(volatile uint8_t *)(sysio + off) = value;
}

static void backlight_set(int on)
{
	uint32_t bit = BACKLIGHT_R05;
	uint32_t out;
	uint32_t dir;

	mmio_write8(PINMUX_R_OFF + PIN_R05, 0);
	dir = mmio_read32(GPIO_R_DIR_OFF);
	mmio_write32(GPIO_R_DIR_OFF, dir | bit);

	out = mmio_read32(GPIO_R_OUT_OFF);
	if (on)
		out &= ~bit;
	else
		out |= bit;

	mmio_write32(GPIO_R_OUT_OFF, out);
}

static void pulse_backlight(unsigned count, unsigned on_ms, unsigned off_ms)
{
	unsigned i;

	for (i = 0; i < count && !stopping; i++) {
		if (access("/run/sf2000-screen-own-backlight", F_OK) == 0)
			break;
		backlight_set(1);
		sleep_ms(on_ms);
		backlight_set(0);
		sleep_ms(off_ms);
	}
	backlight_set(1);
}

static int map_sysio(void)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);

	if (fd < 0) {
		perror("open /dev/mem");
		return -1;
	}

	sysio = mmap(NULL, SYSIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
		fd, SYSIO_BASE_PHYS);
	close(fd);
	if (sysio == MAP_FAILED) {
		sysio = NULL;
		perror("mmap sysio");
		return -1;
	}

	return 0;
}

static void handle_signal(int sig)
{
	(void)sig;
	stopping = 1;
}

int main(void)
{
	if (map_sysio() != 0)
		return 1;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	log_line("sf2000-heartbeat: backlight heartbeat ready\n");
	pulse_backlight(3, 800, 800);

	while (!stopping) {
		if (access("/run/sf2000-screen-own-backlight", F_OK) == 0) {
			sleep_ms(500);
			continue;
		}

		if (access("/run/sf2000-screen-ready", F_OK) == 0) {
			backlight_set(1);
			sleep_ms(2000);
			continue;
		}

		backlight_set(0);
		sleep_ms(1000);
		backlight_set(1);
		sleep_ms(1500);
	}

	backlight_set(1);
	if (sysio)
		munmap((void *)sysio, SYSIO_SIZE);
	log_line("sf2000-heartbeat: stopped\n");
	return 0;
}
