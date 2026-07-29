// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
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
#define PERFORMANCE_MARKER "/run/sf2000-performance-active"
#define PERFORMANCE_READY_MARKER "/run/sf2000-performance-ready"
#define DEVTEST_PATH "/usr/sbin/sf2000-devtest"
#define SCREEN_PID_PATH "/run/sf2000-screen.pid"
#define SCREEN_PAUSED_MARKER "/run/sf2000-screen-paused"
#define BATTERY_RATE_MIN_MS 300000u
#define BATTERY_SAMPLE_MS 60000

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
	if (last_battery_ms && now - last_battery_ms >= BATTERY_RATE_MIN_MS &&
			last_battery_mv > mv)
		rate = (unsigned)(((uint64_t)(last_battery_mv - mv) * 3600000u) /
			(now - last_battery_ms));
	snprintf(line, sizeof(line),
		"sf2000-powerd: battery mode=%s profile=%s raw=%u query_raw=%u generic_raw=%u mv=%u discharge_mv_h=%u interval_ms=%llu adc0=%08x adc1=%08x adc2=%08x adc3=%08x poll_ms=%u\n",
		mode, profile, raw, query_raw, generic_raw, mv, rate,
		(unsigned long long)(last_battery_ms ? now - last_battery_ms : 0),
		adc[0], adc[1], adc[2], adc[3], standby_poll_ms);
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

static int signal_screen(int signal_number)
{
	char value[24], comm[32], path[64], *end;
	long pid;
	int fd = open(SCREEN_PID_PATH, O_RDONLY | O_CLOEXEC);
	ssize_t got;

	if (fd < 0)
		return -1;
	got = read(fd, value, sizeof(value) - 1u);
	close(fd);
	if (got <= 0)
		return -1;
	value[got] = 0;
	pid = strtol(value, &end, 10);
	if (end == value || pid <= 1 || pid == getpid())
		return -1;
	snprintf(path, sizeof(path), "/proc/%ld/comm", pid);
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	got = read(fd, comm, sizeof(comm) - 1u);
	close(fd);
	if (got <= 0)
		return -1;
	comm[got] = 0;
	if (comm[got - 1] == '\n')
		comm[got - 1] = 0;
	if (strcmp(comm, "sf2000-screen"))
		return -1;
	return kill((pid_t)pid, signal_number);
}

static int pause_screen(void)
{
	unsigned wait;

	(void)unlink(SCREEN_PAUSED_MARKER);
	if (signal_screen(SIGUSR1) < 0)
		return -1;
	for (wait = 0; wait < 100u; wait++) {
		if (access(SCREEN_PAUSED_MARKER, F_OK) == 0)
			return 0;
		sleep_ms(1);
	}
	errno = ETIMEDOUT;
	return -1;
}

static int resume_screen(void)
{
	return signal_screen(SIGUSR2);
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
		if (pause_screen() < 0)
			log_line("sf2000-powerd: screen stop failed\n");
	} else {
		battery_sample("standby-exit");
		if (resume_screen() < 0)
			log_line("sf2000-powerd: screen resume failed\n");
		backlight_set(1);
		(void)unlink(STANDBY_MARKER);
		log_line("sf2000-powerd: display standby resumed\n");
	}
}

static void run_frontend(void)
{
	static char *const argv[] = { (char *)FRONTEND_PATH, NULL };
	pid_t pid;
	int status;
	int visible = 0;

	log_line("sf2000-powerd: frontend launch START+R\n");
	(void)unlink(FRONTEND_READY_MARKER);
	(void)unlink(PERFORMANCE_READY_MARKER);
	{
		int marker = open(PERFORMANCE_MARKER,
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		unsigned attempts;

		if (marker >= 0)
			close(marker);
		for (attempts = 0; attempts < 100u &&
				access(PERFORMANCE_READY_MARKER, F_OK) != 0; attempts++)
			sleep_ms(10);
		if (access(PERFORMANCE_READY_MARKER, F_OK) != 0)
			log_line("sf2000-powerd: performance journal acknowledgement timeout\n");
	}
	backlight_set(0);
	if (pause_screen() < 0)
		log_line("sf2000-powerd: frontend screen stop failed\n");
	pid = vfork();
	if (pid == 0) {
		execv(FRONTEND_PATH, argv);
		_exit(127);
	}
	if (pid > 0) {
		/*
		 * Interactive emulation is the only CPU-bound foreground job.
		 * Keep low-rate logging and battery housekeeping from winning a
		 * timeslice when the core is close to its frame deadline.  Do this
		 * in the parent after vfork has completed its exec hand-off.
		 */
		(void)setpriority(PRIO_PROCESS, (id_t)pid, -20);
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
			if (visible) {
				/*
				 * The ready marker has completed the only asynchronous
				 * hand-off.  A blocking wait now eliminates the old 10 ms
				 * poll, which preempted the emulator 100 times per second
				 * for the entire play session.
				 */
				do {
					result = waitpid(pid, &status, 0);
				} while (result < 0 && errno == EINTR);
				break;
			}
			sleep_ms(10);
		}
		{
			char line[128];
			if (WIFEXITED(status))
				snprintf(line, sizeof(line),
					"sf2000-powerd: frontend exit status=%d\n",
					WEXITSTATUS(status));
			else if (WIFSIGNALED(status))
				snprintf(line, sizeof(line),
					"sf2000-powerd: frontend signal=%d\n", WTERMSIG(status));
			else
				snprintf(line, sizeof(line),
					"sf2000-powerd: frontend wait status=0x%x\n", status);
			log_line(line);
		}
	}
	backlight_set(0);
	(void)unlink(PERFORMANCE_MARKER);
	(void)unlink(FRONTEND_READY_MARKER);
	if (resume_screen() < 0)
		log_line("sf2000-powerd: frontend screen resume failed\n");
	/* Let the console publish a complete replacement frame while blanked. */
	sleep_ms(50);
	backlight_set(1);
	log_line("sf2000-powerd: frontend returned to console\n");
}

static void run_device_tests(void)
{
	static char *const argv[] = { (char *)DEVTEST_PATH, NULL };
	pid_t pid;
	int status = 0;
	char line[128];

	log_line("sf2000-powerd: device tests START+A\n");
	backlight_set(0);
	if (pause_screen() < 0)
		log_line("sf2000-powerd: devtest screen stop failed\n");
	pid = vfork();
	if (pid == 0) {
		execv(DEVTEST_PATH, argv);
		_exit(127);
	}
	if (pid > 0) {
		do {
			waitpid(pid, &status, 0);
		} while (errno == EINTR);
	}
	if (WIFEXITED(status))
		snprintf(line, sizeof(line),
			"sf2000-powerd: device tests returned status=%d\n",
			WEXITSTATUS(status));
	else
		snprintf(line, sizeof(line),
			"sf2000-powerd: device tests wait status=0x%x\n", status);
	log_line(line);
	if (resume_screen() < 0)
		log_line("sf2000-powerd: devtest screen resume failed\n");
	sleep_ms(50);
	backlight_set(1);
}

static void drain_input(int fd)
{
	struct input_event event;
	int flags = fcntl(fd, F_GETFL);
	unsigned count = 0;
	char line[96];

	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return;
	while (read(fd, &event, sizeof(event)) == sizeof(event))
		count++;
	(void)fcntl(fd, F_SETFL, flags);
	snprintf(line, sizeof(line),
		"sf2000-powerd: discarded %u stale frontend input events\n", count);
	log_line(line);
}

int main(void)
{
	int memfd, input = -1;
	int start = 0, y = 0, r = 0, a = 0, standby = 0, released = 1;
	int launch_released = 1, test_released = 1;
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
	log_line("sf2000-powerd: ready START+Y standby START+R browser START+A tests\n");
	for (;;) {
		struct pollfd wait = { .fd = input, .events = POLLIN };
		int ready = poll(&wait, 1, BATTERY_SAMPLE_MS);

		if (ready == 0) {
			battery_sample(standby ? "standby" : "normal");
			continue;
		}
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (read(input, &event, sizeof(event)) != sizeof(event))
			continue;
		if (event.type != EV_KEY)
			continue;
		if (event.code == BTN_START)
			start = event.value != 0;
		if (event.code == BTN_WEST)
			y = event.value != 0;
		if (event.code == BTN_TR)
			r = event.value != 0;
		if (event.code == BTN_EAST)
			a = event.value != 0;
		if (!start && !y)
			released = 1;
		if (!start && !r)
			launch_released = 1;
		if (!start && !a)
			test_released = 1;
		if (!standby && released && start && y) {
			released = 0;
			standby = 1;
			set_standby(1);
		} else if (standby && released && event.value) {
			released = 0;
			standby = 0;
			set_standby(0);
		} else if (!standby && launch_released && start && r) {
			launch_released = 0;
			run_frontend();
			drain_input(input);
			start = y = r = a = 0;
			released = launch_released = test_released = 1;
		} else if (!standby && test_released && start && a) {
			test_released = 0;
			run_device_tests();
			drain_input(input);
			start = y = r = a = 0;
			released = launch_released = test_released = 1;
		}
	}
	if (standby)
		set_standby(0);
	return 1;
}
