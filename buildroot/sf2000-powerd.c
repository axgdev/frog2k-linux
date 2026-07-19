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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SYSIO_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define PINMUX_R_OFF 0x4e0u
#define GPIO_R_OUT_OFF 0xf4u
#define GPIO_R_DIR_OFF 0xf8u
#define PIN_R05 5u
#define STANDBY_MARKER "/run/sf2000-display-standby"
#define POWER_CONFIG "/etc/sf2000-power.conf"
#define ADC_BASE ((volatile uint32_t *)(uintptr_t)0xb8818400u)
#define DEFAULT_STANDBY_POLL_MS 2000u
#define FRONTEND_PATH "/usr/bin/sf2000-frontend"
#define FRONTEND_READY_MARKER "/run/sf2000-frontend-ready"

static volatile uint8_t *sysio;
static unsigned standby_poll_ms = DEFAULT_STANDBY_POLL_MS;
static unsigned last_battery_mv;
static uint64_t last_battery_ms;

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

static uint64_t monotonic_ms(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 0;
	return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static void load_config(void)
{
	char data[128];
	int fd = open(POWER_CONFIG, O_RDONLY | O_CLOEXEC);
	ssize_t got;
	char *value, *end;
	unsigned long parsed;

	if (fd < 0)
		return;
	got = read(fd, data, sizeof(data) - 1u);
	close(fd);
	if (got <= 0)
		return;
	data[got] = 0;
	value = strstr(data, "standby_poll_ms=");
	if (!value)
		return;
	value += strlen("standby_poll_ms=");
	parsed = strtoul(value, &end, 10);
	if (end != value && parsed >= 100u && parsed <= 10000u)
		standby_poll_ms = (unsigned)parsed;
}

static void battery_sample(const char *mode)
{
	volatile uint32_t *adc = ADC_BASE;
	uint64_t now = monotonic_ms();
	unsigned raw, query_raw, generic_raw = 0, mv, rate = 0;
	const char *profile = "query";
	char line[256];

	/* Proven HC15xx ADC setup from the vendor-compatible UniFrog source. */
	adc[3] = (adc[3] & ~(3u << 16)) | (1u << 16);
	adc[2] = (adc[2] & ~0xffffu) | 0x00ffu;
	adc[1] = (adc[1] & ~0xff000f01u) | 0x01000f01u;
	adc[1] = (adc[1] & ~0x0000ff01u) | 0x0000ff01u;
	sleep_ms(1);
	query_raw = (adc[0] >> 16) & 0xffu;
	raw = query_raw;
	if (!raw) {
		/* The general HC1512 driver uses clock 0 and a longer average. */
		adc[3] &= ~((3u << 16) | 1u);
		adc[2] = (adc[2] & ~0xffffu) | 0x0ff8u;
		adc[1] = (adc[1] & ~0xff00ff01u) | 0x0f00ff01u;
		sleep_ms(20);
		generic_raw = (adc[0] >> 16) & 0xffu;
		if (generic_raw) {
			raw = generic_raw;
			profile = "generic";
		} else {
			profile = "unavailable";
		}
	}
	mv = raw * 20u;
	if (last_battery_ms && now > last_battery_ms && last_battery_mv > mv)
		rate = (unsigned)(((uint64_t)(last_battery_mv - mv) * 3600000u) /
			(now - last_battery_ms));
	snprintf(line, sizeof(line),
		"sf2000-powerd: battery mode=%s profile=%s raw=%u query_raw=%u generic_raw=%u mv=%u discharge_mv_h=%u adc0=%08x adc1=%08x adc2=%08x adc3=%08x poll_ms=%u\n",
		mode, profile, raw, query_raw, generic_raw, mv, rate, adc[0], adc[1],
		adc[2], adc[3], standby_poll_ms);
	log_line(line);
	last_battery_mv = mv;
	last_battery_ms = now;
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
		battery_sample("normal-exit");
		{
			int fd = open(STANDBY_MARKER,
				O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

			if (fd >= 0) {
				char value[16];
				int length = snprintf(value, sizeof(value), "%u\n",
					standby_poll_ms);
				(void)write(fd, value, (size_t)length);
				close(fd);
			}
		}
		/* Give logd one polling interval to flush before quiescing GE. */
		sleep_ms(100);
		backlight_set(0);
		signal_named("sf2000-screen", SIGSTOP);
	} else {
		battery_sample("standby-exit");
		signal_named("sf2000-screen", SIGCONT);
		backlight_set(1);
		(void)unlink(STANDBY_MARKER);
		log_line("sf2000-powerd: display standby resumed\n");
	}
}

static void run_frontend(void)
{
	static char *const argv[] = { (char *)FRONTEND_PATH,
		(char *)"/mnt/sd/sf2000-demo.rom", NULL };
	pid_t pid;
	int status;
	int visible = 0;

	log_line("sf2000-powerd: frontend launch START+X\n");
	(void)unlink(FRONTEND_READY_MARKER);
	backlight_set(0);
	signal_named("sf2000-screen", SIGSTOP);
	pid = vfork();
	if (pid == 0) {
		execv(FRONTEND_PATH, argv);
		_exit(127);
	}
	if (pid > 0) {
		for (;;) {
			pid_t result = waitpid(pid, &status, WNOHANG);

			if (result == pid)
				break;
			if (result < 0 && errno != EINTR)
				break;
			if (!visible && access(FRONTEND_READY_MARKER, F_OK) == 0) {
				backlight_set(1);
				visible = 1;
				log_line("sf2000-powerd: frontend first frame visible\n");
			}
			sleep_ms(10);
		}
	}
	backlight_set(0);
	(void)unlink(FRONTEND_READY_MARKER);
	signal_named("sf2000-screen", SIGCONT);
	/* Let the console publish a complete replacement frame while blanked. */
	sleep_ms(50);
	backlight_set(1);
	log_line("sf2000-powerd: frontend returned to console\n");
}

int main(void)
{
	int memfd, input = -1;
	int start = 0, y = 0, x = 0, standby = 0, released = 1;
	int launch_released = 1;
	struct input_event event;

	memfd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (memfd >= 0) {
		sysio = mmap(NULL, SYSIO_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, memfd, SYSIO_PHYS);
		close(memfd);
	}
	if (!sysio || sysio == MAP_FAILED)
		sysio = (volatile uint8_t *)(uintptr_t)0xb8800000u;
	load_config();
	battery_sample("normal");

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
		if (event.code == BTN_NORTH)
			x = event.value != 0;
		if (!start && !y)
			released = 1;
		if (!start && !x)
			launch_released = 1;
		if (!standby && released && start && y) {
			released = 0;
			standby = 1;
			set_standby(1);
		} else if (standby && released && event.value) {
			released = 0;
			standby = 0;
			set_standby(0);
		} else if (!standby && launch_released && start && x) {
			launch_released = 0;
			run_frontend();
			start = y = x = 0;
		}
	}
	if (standby)
		set_standby(0);
	return 1;
}
