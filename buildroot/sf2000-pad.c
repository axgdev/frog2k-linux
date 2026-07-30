// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 0x220
#define BTN_DPAD_DOWN 0x221
#define BTN_DPAD_LEFT 0x222
#define BTN_DPAD_RIGHT 0x223
#endif

#ifndef BTN_NORTH
#define BTN_NORTH BTN_X
#define BTN_SOUTH BTN_A
#define BTN_WEST BTN_Y
#define BTN_EAST BTN_B
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define SYSIO_BASE_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define KSEG1ADDR(x) ((volatile uint8_t *)((uintptr_t)(x) | 0xa0000000u))
#define PINMUX_L_OFF 0x4a0u
#define GPIO_L_IN_OFF 0x50u
#define GPIO_L_OUT_OFF 0x54u
#define GPIO_L_DIR_OFF 0x58u

#define PIN_L23 23u
#define PIN_L24 24u
#define PIN_L25 25u
#define PIN_L26 26u
#define PIN_L27 27u

#define KEY_SHIFTER_BITS 16u
#define KEY_SHIFTER_LOAD_SPINS 300u
#define KEY_SHIFTER_SETTLE_SPINS 300u
#define KEY_SHIFTER_CLOCK_LOW_SPINS 180u
#define KEY_SHIFTER_CLOCK_HIGH_SPINS 180u
#define POLL_INTERVAL_MS 4u
#define NORMAL_DEBOUNCE_SCANS 1u
#define STANDBY_CHECK_SCANS 64u
#define STANDBY_MARKER "/run/sf2000-display-standby"
#define PERFORMANCE_MARKER "/run/sf2000-performance-active"
#define STORAGE_MARKER "/run/sf2000-storage-mounted"
#define SYSTEM_CONFIG "/etc/sf2000.conf"
#define USER_CONFIG "/mnt/sd/sf2000.conf"
#define DEFAULT_STANDBY_POLL_MS 2000u

enum pad_profile {
	PAD_PROFILE_SF2000,
	PAD_PROFILE_STOCK_BITS,
};

struct button_def {
	const char *name;
	int code;
};

static const struct button_def buttons[] = {
	{ "R", BTN_TR },
	{ "Y", BTN_WEST },
	{ "X", BTN_NORTH },
	{ "L", BTN_TL },
	{ "A", BTN_EAST },
	{ "B", BTN_SOUTH },
	{ "SELECT", BTN_SELECT },
	{ "START", BTN_START },
	{ "UP", BTN_DPAD_UP },
	{ "DOWN", BTN_DPAD_DOWN },
	{ "LEFT", BTN_DPAD_LEFT },
	{ "RIGHT", BTN_DPAD_RIGHT },
};

static const uint8_t stock_bit_for_button[ARRAY_SIZE(buttons)] = {
	11, 10, 12, 13, 14, 0, 3, 4, 6, 7, 5, 1,
};

static const uint8_t gb300_stock_bit_for_shift[KEY_SHIFTER_BITS] = {
	15, 11, 10, 12, 13, 14, 0, 3, 4, 6, 7, 5, 1, 2, 8, 9,
};

enum button_index {
	BUTTON_R,
	BUTTON_Y,
	BUTTON_X,
	BUTTON_L,
	BUTTON_A,
	BUTTON_B,
	BUTTON_SELECT,
	BUTTON_START,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,
};

static volatile uint8_t *sysio_mapping;
#define sysio (sysio_mapping ? sysio_mapping : KSEG1ADDR(SYSIO_BASE_PHYS))
static volatile sig_atomic_t stopping;
static unsigned normal_poll_ms = POLL_INTERVAL_MS;

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

static void delay_spins(unsigned count)
{
	volatile unsigned i;

	for (i = 0; i < count; i++)
		__asm__ volatile ("nop");
}

static void timespec_add_ms(struct timespec *time, unsigned msec)
{
	time->tv_nsec += (long)msec * 1000000L;
	while (time->tv_nsec >= 1000000000L) {
		time->tv_nsec -= 1000000000L;
		time->tv_sec++;
	}
}

static void sleep_until(struct timespec *deadline, unsigned msec)
{
	int result;

	timespec_add_ms(deadline, msec);
	do {
		result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
			deadline, NULL);
	} while (result == EINTR && !stopping);
	if (result && result != EINTR)
		(void)clock_gettime(CLOCK_MONOTONIC, deadline);
}

static void load_scan_config_file(const char *path)
{
	char data[4096];
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t got;
	char *line;

	if (fd < 0)
		return;
	got = read(fd, data, sizeof(data) - 1u);
	close(fd);
	if (got <= 0)
		return;
	data[got] = 0;
	for (line = data; line && *line;) {
		char *next = strchr(line, '\n');
		char *value;

		if (next)
			*next++ = 0;
		while (*line == ' ' || *line == '\t')
			line++;
		value = strchr(line, '=');
		if (value) {
			char *end;
			unsigned long parsed;

			*value++ = 0;
			end = line + strlen(line);
			while (end > line && (end[-1] == ' ' || end[-1] == '\t'))
				*--end = 0;
			while (*value == ' ' || *value == '\t')
				value++;
			if (!strcmp(line, "input_scan_ms")) {
				parsed = strtoul(value, &end, 10);
				if (end != value && parsed >= 2u && parsed <= 20u)
					normal_poll_ms = (unsigned)parsed;
			}
		}
		line = next;
	}
}

static void load_scan_config(void)
{
	load_scan_config_file(SYSTEM_CONFIG);
	load_scan_config_file(USER_CONFIG);
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

static void pinmux_l_gpio(unsigned pin)
{
	mmio_write8(PINMUX_L_OFF + pin, 0);
}

static void gpio_l_configure(unsigned pin, int output)
{
	uint32_t bit = 1u << pin;
	uint32_t dir;

	pinmux_l_gpio(pin);
	dir = mmio_read32(GPIO_L_DIR_OFF);
	if (output)
		dir |= bit;
	else
		dir &= ~bit;
	mmio_write32(GPIO_L_DIR_OFF, dir);
}

static void gpio_l_set(unsigned pin, int high)
{
	uint32_t bit = 1u << pin;
	uint32_t out = mmio_read32(GPIO_L_OUT_OFF);

	if (high)
		out |= bit;
	else
		out &= ~bit;
	mmio_write32(GPIO_L_OUT_OFF, out);
}

static int gpio_l_get(unsigned pin)
{
	return (mmio_read32(GPIO_L_IN_OFF) >> pin) & 1u;
}

static uint32_t normalize_stock_bits(uint32_t raw)
{
	uint32_t normalized = 0;
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (raw & (1u << stock_bit_for_button[i]))
			normalized |= 1u << i;
	}

	return normalized;
}

static uint32_t scan_sf2000(void)
{
	uint32_t raw_mask = 0;
	unsigned i;

	gpio_l_configure(PIN_L24, 1);
	gpio_l_set(PIN_L24, 1);
	gpio_l_configure(PIN_L23, 1);
	gpio_l_set(PIN_L23, 0);
	delay_spins(KEY_SHIFTER_LOAD_SPINS);
	gpio_l_configure(PIN_L23, 0);
	delay_spins(KEY_SHIFTER_SETTLE_SPINS);

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (1 ^ gpio_l_get(PIN_L23))
			raw_mask |= 1u << i;

		gpio_l_set(PIN_L24, 0);
		delay_spins(KEY_SHIFTER_CLOCK_LOW_SPINS);
		gpio_l_set(PIN_L24, 1);
		delay_spins(KEY_SHIFTER_CLOCK_HIGH_SPINS);
	}

	return raw_mask;
}

static uint32_t scan_stock_bits(void)
{
	uint32_t raw_mask = 0;
	unsigned i;

	gpio_l_configure(PIN_L26, 1);
	gpio_l_configure(PIN_L27, 1);
	gpio_l_configure(PIN_L25, 1);
	gpio_l_set(PIN_L27, 0);
	gpio_l_set(PIN_L25, 0);
	gpio_l_set(PIN_L26, 0);
	delay_spins(KEY_SHIFTER_LOAD_SPINS);

	gpio_l_configure(PIN_L27, 0);
	gpio_l_configure(PIN_L25, 0);
	delay_spins(KEY_SHIFTER_SETTLE_SPINS);

	for (i = 0; i < KEY_SHIFTER_BITS; i++) {
		int raw0 = 1 ^ gpio_l_get(PIN_L27);
		int raw1 = 1 ^ gpio_l_get(PIN_L25);

		if (raw0 || raw1)
			raw_mask |= 1u << gb300_stock_bit_for_shift[i];

		gpio_l_set(PIN_L26, 0);
		delay_spins(KEY_SHIFTER_CLOCK_LOW_SPINS);
		gpio_l_set(PIN_L26, 1);
		delay_spins(KEY_SHIFTER_CLOCK_HIGH_SPINS);
	}

	return normalize_stock_bits(raw_mask);
}

static uint32_t scan_buttons(enum pad_profile profile)
{
	if (profile == PAD_PROFILE_STOCK_BITS)
		return scan_stock_bits();
	return scan_sf2000();
}

static int open_uinput(void)
{
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);

	if (fd < 0)
		fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	return fd;
}

static int setup_uinput(void)
{
	struct uinput_user_dev dev;
	int fd;
	unsigned i;

	fd = open_uinput();
	if (fd < 0) {
		perror("open uinput");
		return -1;
	}

	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
			ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0) {
		perror("uinput evbit");
		close(fd);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (ioctl(fd, UI_SET_KEYBIT, buttons[i].code) < 0) {
			perror("uinput keybit");
			close(fd);
			return -1;
		}
	}

	memset(&dev, 0, sizeof(dev));
	snprintf(dev.name, sizeof(dev.name), "SF2000 GPIO keypad");
	dev.id.bustype = BUS_HOST;
	dev.id.vendor = 0x5346;
	dev.id.product = 0x2000;
	dev.id.version = 1;

	if (write(fd, &dev, sizeof(dev)) != (ssize_t)sizeof(dev)) {
		perror("uinput write");
		close(fd);
		return -1;
	}
	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		perror("uinput create");
		close(fd);
		return -1;
	}

	return fd;
}

static void emit_event(int fd, uint16_t type, uint16_t code, int32_t value)
{
	struct input_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = type;
	ev.code = code;
	ev.value = value;
	(void)write(fd, &ev, sizeof(ev));
}

static void emit_changes(int fd, uint32_t old_mask, uint32_t new_mask)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		uint32_t bit = 1u << i;

		if ((old_mask & bit) == (new_mask & bit))
			continue;
		emit_event(fd, EV_KEY, (uint16_t)buttons[i].code,
			(new_mask & bit) != 0);
	}
	emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

static void log_button_state(uint32_t mask)
{
	char line[192];
	size_t used;
	unsigned i;

	used = (size_t)snprintf(line, sizeof(line), "sf2000-pad: state=0x%03x",
		mask);
	for (i = 0; i < ARRAY_SIZE(buttons) && used + 2 < sizeof(line); i++) {
		if (!(mask & (1u << i)))
			continue;
		used += (size_t)snprintf(line + used, sizeof(line) - used,
			" %s", buttons[i].name);
	}
	if (used + 2 < sizeof(line))
		snprintf(line + used, sizeof(line) - used, "\n");
	else
		line[sizeof(line) - 1] = '\0';

	/*
	 * During emulation the frontend records evdev latency directly. Avoid
	 * synchronous printk/serial work on every transition on the one CPU.
	 * Keep the verbose state trace for boot diagnostics and QEMU input tests.
	 */
	if (access(PERFORMANCE_MARKER, F_OK) != 0)
		log_line(line);
}

static enum pad_profile parse_profile(const char *name)
{
	if (name && (!strcmp(name, "stock-bits") || !strcmp(name, "gb300") ||
			!strcmp(name, "dy12") || !strcmp(name, "dy14")))
		return PAD_PROFILE_STOCK_BITS;
	return PAD_PROFILE_SF2000;
}

static const char *profile_name(enum pad_profile profile)
{
	return profile == PAD_PROFILE_STOCK_BITS ? "stock-bits" : "sf2000";
}

static int map_sysio(void)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);

	if (fd < 0) {
		log_line("sf2000-pad: using direct SYSIO mapping\n");
		sysio_mapping = KSEG1ADDR(SYSIO_BASE_PHYS);
		return 0;
	}

	sysio_mapping = mmap(NULL, SYSIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
		fd, SYSIO_BASE_PHYS);
	close(fd);
	if (sysio_mapping == MAP_FAILED) {
		perror("mmap sysio");
		sysio_mapping = KSEG1ADDR(SYSIO_BASE_PHYS);
		log_line("sf2000-pad: using direct SYSIO mapping\n");
		return 0;
	}

	return 0;
}

static void handle_signal(int sig)
{
	(void)sig;
	stopping = 1;
}

static unsigned poll_interval_ms(void)
{
	int fd = open(STANDBY_MARKER, O_RDONLY | O_CLOEXEC);
	char value[16];
	ssize_t got;

	if (fd < 0)
		return normal_poll_ms;
	got = read(fd, value, sizeof(value) - 1u);
	close(fd);
	if (got > 0) {
		char *end;
		unsigned long configured;

		value[got] = 0;
		configured = strtoul(value, &end, 10);
		if (end != value && configured >= 100u && configured <= 10000u)
			return (unsigned)configured;
	}
	return DEFAULT_STANDBY_POLL_MS;
}

int main(int argc, char **argv)
{
	enum pad_profile profile = parse_profile(getenv("SF2000_PAD_PROFILE"));
	uint32_t state = 0;
	uint32_t raw_prev = 0;
	uint32_t raw_count = 0;
	unsigned standby_check = 0;
	unsigned poll_ms;
	int user_config_loaded = 0;
	int uinput_fd;
	char line[128];
	struct timespec deadline;

	if (argc > 1)
		profile = parse_profile(argv[1]);

	if (map_sysio() != 0)
		return 1;

	uinput_fd = setup_uinput();
	if (uinput_fd < 0)
		return 1;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
	/*
	 * The CPU-bound frontend runs at -20. Give the sleeping, bounded scan the
	 * same priority so it runs promptly on wake even in uncapped mode. Each
	 * scan performs a fixed 12-bit transaction and then sleeps again.
	 */
	(void)setpriority(PRIO_PROCESS, 0, -20);
	(void)clock_gettime(CLOCK_MONOTONIC, &deadline);
	load_scan_config();
	poll_ms = normal_poll_ms;

	snprintf(line, sizeof(line),
		"sf2000-pad: userspace input bridge ready profile=%s scan_ms=%u debounce=adaptive-confirm priority=-20\n",
		profile_name(profile), normal_poll_ms);
	log_line(line);

	while (!stopping) {
		uint32_t raw = scan_buttons(profile);

		if (raw == raw_prev) {
			if (raw_count < 3)
				raw_count++;
		} else {
			raw_prev = raw;
			raw_count = 0;
			/*
			 * Confirm an edge immediately instead of waiting one complete
			 * polling period. This removes up to scan_ms from press/release
			 * latency while leaving the idle GPIO transaction rate exactly
			 * unchanged. Mechanical transitions are rare, so the extra
			 * shift-register read has negligible CPU cost.
			 */
			if (poll_ms == normal_poll_ms) {
				uint32_t confirmed = scan_buttons(profile);

				if (confirmed == raw)
					raw_count = NORMAL_DEBOUNCE_SCANS;
				else {
					raw = confirmed;
					raw_prev = confirmed;
				}
			}
		}

		/* Normal scans debounce; a low-rate standby scan is already stable. */
		if (raw_count >= (poll_ms == normal_poll_ms ?
				NORMAL_DEBOUNCE_SCANS : 0u) && raw != state) {
			emit_changes(uinput_fd, state, raw);
			state = raw;
			log_button_state(state);
		}
		/*
		 * Opening the standby marker every 4 ms cost more than the GPIO
		 * transaction. Check it at a low duty cycle in normal operation;
		 * while in standby, every wake scan also checks for its removal.
		 */
		if (poll_ms != normal_poll_ms ||
				++standby_check >= STANDBY_CHECK_SCANS) {
			poll_ms = poll_interval_ms();
			standby_check = 0;
			if (!user_config_loaded &&
					access(STORAGE_MARKER, F_OK) == 0) {
				unsigned old_poll_ms = normal_poll_ms;

				load_scan_config_file(USER_CONFIG);
				user_config_loaded = 1;
				if (poll_ms == old_poll_ms)
					poll_ms = normal_poll_ms;
				if (normal_poll_ms != old_poll_ms) {
					snprintf(line, sizeof(line),
						"sf2000-pad: user scan_ms=%u debounce_ms=%u\n",
						normal_poll_ms, normal_poll_ms *
						NORMAL_DEBOUNCE_SCANS);
					log_line(line);
				}
			}
			(void)clock_gettime(CLOCK_MONOTONIC, &deadline);
		}
		sleep_until(&deadline, poll_ms);
	}

	(void)ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
	if (sysio_mapping && sysio_mapping != KSEG1ADDR(SYSIO_BASE_PHYS))
		munmap((void *)sysio_mapping, SYSIO_SIZE);
	log_line("sf2000-pad: stopped\n");
	return 0;
}
