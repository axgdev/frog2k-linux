// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define SYSIO_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define PINMUX_R_OFF 0x4e0u
#define GPIO_R_OUT_OFF 0xf4u
#define GPIO_R_DIR_OFF 0xf8u
#define PIN_R05 5u
#define STANDBY_MARKER "/run/sf2000-display-standby"

static volatile uint8_t *sysio;

static void log_line(const char *text)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return;
	(void)write(fd, "<6>", 3);
	(void)write(fd, text, strlen(text));
	close(fd);
}

static void sleep_ms(unsigned ms)
{
	struct timespec delay = { ms / 1000u, (long)(ms % 1000u) * 1000000L };
	while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
		;
}

static void backlight_set(int on)
{
	volatile uint32_t *dir = (volatile uint32_t *)(sysio + GPIO_R_DIR_OFF);
	volatile uint32_t *out = (volatile uint32_t *)(sysio + GPIO_R_OUT_OFF);
	uint32_t bit = 1u << PIN_R05;

	*(volatile uint8_t *)(sysio + PINMUX_R_OFF + PIN_R05) = 0;
	*dir |= bit;
	if (on)
		*out &= ~bit;
	else
		*out |= bit;
}

static void signal_named(const char *wanted, int signal_number)
{
	DIR *proc = opendir("/proc");
	struct dirent *entry;

	if (!proc)
		return;
	while ((entry = readdir(proc))) {
		char path[64], comm[32];
		char *end;
		long pid = strtol(entry->d_name, &end, 10);
		int fd;
		ssize_t got;

		if (!entry->d_name[0] || *end || pid <= 1 || pid == getpid())
			continue;
		snprintf(path, sizeof(path), "/proc/%ld/comm", pid);
		fd = open(path, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;
		got = read(fd, comm, sizeof(comm) - 1u);
		close(fd);
		if (got <= 0)
			continue;
		comm[got] = 0;
		if (comm[got - 1] == '\n')
			comm[got - 1] = 0;
		if (!strcmp(comm, wanted))
			(void)kill((pid_t)pid, signal_number);
	}
	closedir(proc);
}

static void set_standby(int standby)
{
	if (standby) {
		log_line("sf2000-powerd: display standby entering\n");
		{
			int fd = open(STANDBY_MARKER,
				O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

			if (fd >= 0)
				close(fd);
		}
		/* Give logd one polling interval to flush before quiescing GE. */
		sleep_ms(100);
		backlight_set(0);
		signal_named("sf2000-screen", SIGSTOP);
	} else {
		signal_named("sf2000-screen", SIGCONT);
		backlight_set(1);
		(void)unlink(STANDBY_MARKER);
		log_line("sf2000-powerd: display standby resumed\n");
	}
}

int main(void)
{
	int memfd, input = -1;
	int start = 0, y = 0, standby = 0, released = 1;
	struct input_event event;

	memfd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (memfd >= 0) {
		sysio = mmap(NULL, SYSIO_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, memfd, SYSIO_PHYS);
		close(memfd);
	}
	if (!sysio || sysio == MAP_FAILED)
		sysio = (volatile uint8_t *)(uintptr_t)0xb8800000u;

	while (input < 0) {
		input = open("/dev/input/event0", O_RDONLY | O_CLOEXEC);
		if (input < 0)
			sleep_ms(100);
	}
	log_line("sf2000-powerd: ready START+Y enters display standby\n");
	while (read(input, &event, sizeof(event)) == sizeof(event)) {
		if (event.type != EV_KEY)
			continue;
		if (event.code == BTN_START)
			start = event.value != 0;
		if (event.code == BTN_WEST)
			y = event.value != 0;
		if (!start && !y)
			released = 1;
		if (!standby && released && start && y) {
			released = 0;
			standby = 1;
			set_standby(1);
		} else if (standby && released && event.value) {
			released = 0;
			standby = 0;
			set_standby(0);
		}
	}
	if (standby)
		set_standby(0);
	return 1;
}
