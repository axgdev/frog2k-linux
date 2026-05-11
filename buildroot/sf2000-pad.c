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
#define KEY_SHIFTER_LOAD_US 4u
#define KEY_SHIFTER_SETTLE_US 4u
#define KEY_SHIFTER_CLOCK_LOW_US 3u
#define KEY_SHIFTER_CLOCK_HIGH_US 3u
#define POLL_INTERVAL_MS 20u

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

static void sleep_us(unsigned usec)
{
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = (long)usec * 1000L;
	while (nanosleep(&ts, &ts) < 0 && errno == EINTR)
		;
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
	sleep_us(KEY_SHIFTER_LOAD_US);
	gpio_l_configure(PIN_L23, 0);
	sleep_us(KEY_SHIFTER_SETTLE_US);

	for (i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (1 ^ gpio_l_get(PIN_L23))
			raw_mask |= 1u << i;

		gpio_l_set(PIN_L24, 0);
		sleep_us(KEY_SHIFTER_CLOCK_LOW_US);
		gpio_l_set(PIN_L24, 1);
		sleep_us(KEY_SHIFTER_CLOCK_HIGH_US);
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
	sleep_us(KEY_SHIFTER_LOAD_US);

	gpio_l_configure(PIN_L27, 0);
	gpio_l_configure(PIN_L25, 0);
	sleep_us(KEY_SHIFTER_SETTLE_US);

	for (i = 0; i < KEY_SHIFTER_BITS; i++) {
		int raw0 = 1 ^ gpio_l_get(PIN_L27);
		int raw1 = 1 ^ gpio_l_get(PIN_L25);

		if (raw0 || raw1)
			raw_mask |= 1u << gb300_stock_bit_for_shift[i];

		gpio_l_set(PIN_L26, 0);
		sleep_us(KEY_SHIFTER_CLOCK_LOW_US);
		gpio_l_set(PIN_L26, 1);
		sleep_us(KEY_SHIFTER_CLOCK_HIGH_US);
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

int main(int argc, char **argv)
{
	enum pad_profile profile = parse_profile(getenv("SF2000_PAD_PROFILE"));
	uint32_t state = 0;
	uint32_t raw_prev = 0;
	uint32_t raw_count = 0;
	int uinput_fd;
	char line[96];

	if (argc > 1)
		profile = parse_profile(argv[1]);

	if (map_sysio() != 0)
		return 1;

	uinput_fd = setup_uinput();
	if (uinput_fd < 0)
		return 1;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	snprintf(line, sizeof(line), "sf2000-pad: userspace input bridge ready profile=%s\n",
		profile_name(profile));
	log_line(line);

	while (!stopping) {
		uint32_t raw = scan_buttons(profile);

		if (raw == raw_prev) {
			if (raw_count < 3)
				raw_count++;
		} else {
			raw_prev = raw;
			raw_count = 0;
		}

		if (raw_count >= 2 && raw != state) {
			emit_changes(uinput_fd, state, raw);
			state = raw;
			log_button_state(state);
		}

		sleep_ms(POLL_INTERVAL_MS);
	}

	(void)ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
	if (sysio)
		munmap((void *)sysio, SYSIO_SIZE);
	log_line("sf2000-pad: stopped\n");
	return 0;
}
