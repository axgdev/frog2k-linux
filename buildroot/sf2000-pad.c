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
#include <sys/reboot.h>
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
#define POLL_INTERVAL_MS 20u
#ifndef LINUX_REBOOT_CMD_RESTART
#define LINUX_REBOOT_CMD_RESTART 0x01234567
#endif

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

#define BUTTON_BIT(name) (1u << BUTTON_##name)

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

#define REBOOT_BUTTON_MASK (BUTTON_BIT(SELECT) | BUTTON_BIT(START))
#define STANDBY_BUTTON_MASK (BUTTON_BIT(START) | BUTTON_BIT(Y))
#define SHUTDOWN_BUTTON_MASK (BUTTON_BIT(START) | BUTTON_BIT(B))
#define POWER_STATE_PATH "/run/sf2000-power-state"
#define POWER_REQUEST_PATH "/run/sf2000-power-request"
#define CPU_GOVERNOR_PATH \
	"/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"

static volatile uint8_t *sysio_mapping;
#define sysio (sysio_mapping ? sysio_mapping : KSEG1ADDR(SYSIO_BASE_PHYS))
static volatile sig_atomic_t stopping;
static int clocked_standby;
static int standby_released;

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

static int write_text(const char *path, const char *value)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	ssize_t size = (ssize_t)strlen(value);
	ssize_t written;

	if (fd < 0)
		return -1;
	written = write(fd, value, (size_t)size);
	close(fd);
	return written == size ? 0 : -1;
}

static void set_governor(const char *name)
{
	int fd = open(CPU_GOVERNOR_PATH, O_WRONLY | O_CLOEXEC);

	if (fd < 0) {
		log_line("sf2000-power: cpufreq governor unavailable\n");
		return;
	}
	if (write(fd, name, strlen(name)) != (ssize_t)strlen(name))
		log_line("sf2000-power: cpufreq governor write failed\n");
	close(fd);
}

static void leave_clocked_standby(void)
{
	set_governor("performance");
	unlink(POWER_STATE_PATH);
	clocked_standby = 0;
	standby_released = 0;
	log_line("sf2000-power: normal 918 MHz, datetime retained\n");
}

static void maybe_power(uint32_t state)
{
	static int standby_armed = 1;
	static int shutdown_armed = 1;
	int requested = access(POWER_STATE_PATH, F_OK) == 0;

	if (!clocked_standby && requested) {
		set_governor("powersave");
		clocked_standby = 1;
		standby_released = state == 0;
		log_line("sf2000-power: external clocked standby request\n");
	}
	if (clocked_standby && !requested) {
		leave_clocked_standby();
		return;
	}

	if (clocked_standby) {
		if (!state)
			standby_released = 1;
		else if (standby_released)
			leave_clocked_standby();
		return;
	}
	if ((state & STANDBY_BUTTON_MASK) != STANDBY_BUTTON_MASK)
		standby_armed = 1;
	else if (standby_armed) {
		standby_armed = 0;
		if (write_text(POWER_STATE_PATH, "clocked-standby\n") == 0) {
			set_governor("powersave");
			clocked_standby = 1;
			standby_released = 0;
			log_line("sf2000-power: clocked standby 198 MHz; press any key to wake\n");
		}
	}
	if ((state & SHUTDOWN_BUTTON_MASK) != SHUTDOWN_BUTTON_MASK)
		shutdown_armed = 1;
	else if (shutdown_armed) {
		shutdown_armed = 0;
		(void)write_text(POWER_STATE_PATH, "shutdown\n");
		(void)write_text(POWER_REQUEST_PATH, "shutdown\n");
		log_line("sf2000-power: clean shutdown requested\n");
	}
}

static void delay_spins(unsigned count)
{
	volatile unsigned i;

	for (i = 0; i < count; i++)
		__asm__ volatile ("nop");
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

	log_line(line);
}

static void maybe_reboot(uint32_t state)
{
	static int armed = 1;
	int fd;
	unsigned wait;

	if ((state & REBOOT_BUTTON_MASK) != REBOOT_BUTTON_MASK) {
		armed = 1;
		return;
	}

	if (!armed)
		return;
	armed = 0;
	log_line("sf2000-pad: START+SELECT pressed, requesting clean restart\n");
	fd = open("/run/sf2000-reboot-request",
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd >= 0) {
		(void)write(fd, "restart\n", 8);
		close(fd);
	}
	for (wait = 0; wait < 60u; wait++)
		sleep_ms(100);
	sync();
	reboot(LINUX_REBOOT_CMD_RESTART);
	log_line("sf2000-pad: reboot syscall returned\n");
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
		maybe_reboot(state);
		maybe_power(state);

		sleep_ms(clocked_standby ? 200u : POLL_INTERVAL_MS);
	}

	(void)ioctl(uinput_fd, UI_DEV_DESTROY);
	close(uinput_fd);
	if (sysio_mapping && sysio_mapping != KSEG1ADDR(SYSIO_BASE_PHYS))
		munmap((void *)sysio_mapping, SYSIO_SIZE);
	log_line("sf2000-pad: stopped\n");
	return 0;
}
