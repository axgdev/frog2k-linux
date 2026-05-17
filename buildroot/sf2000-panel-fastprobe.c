/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef unsigned int size_t;

extern long syscall(long number, ...);

#define SYS_exit 4001
#define SYS_read 4003
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006

#define O_WRONLY 1
#define SYSIO_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define KSEG1ADDR(x) ((volatile uint8_t *)((uintptr_t)(x) | 0xa0000000u))
#define SYS_LCD_SETUP_OFF 0x094u
#define PINMUX_L_OFF 0x4a0u
#define PINMUX_B_OFF 0x4c0u
#define PINMUX_R_OFF 0x4e0u
#define PINMUX_T_OFF 0x500u
#define GPIOL_OFF 0x044u
#define GPIOB_OFF 0x0c4u
#define GPIOR_OFF 0x0e4u
#define GPIOT_OFF 0x344u
#define GPIO_INPUT_OFF 0x0cu
#define GPIO_OUTPUT_OFF 0x10u
#define GPIO_DIR_OFF 0x14u
#define PINPAD_L01 1u
#define PINPAD_L02 2u
#define PINPAD_L03 3u
#define PINPAD_L04 4u
#define PINPAD_L05 5u
#define PINPAD_L06 6u
#define PINPAD_L07 7u
#define PINPAD_L08 8u
#define PINPAD_L10 10u
#define PINPAD_T00 96u
#define PINPAD_T01 97u
#define PINPAD_T02 98u
#define PINPAD_T03 99u
#define PINPAD_T04 100u
#define PINPAD_T05 101u
#define PINPAD_T06 102u
#define PINPAD_T09 105u
#define PINPAD_T10 106u
#define PINPAD_T11 107u
#define PINPAD_T12 108u
#define PINPAD_T13 109u
#define PINPAD_T14 110u
#define PINMUX_GPIO 0u

static volatile uint8_t *sysio;

static const unsigned panel_control_pads[] = {
	PINPAD_L10, PINPAD_T01, PINPAD_L07, PINPAD_T00, PINPAD_L01,
	PINPAD_L02, PINPAD_L03, PINPAD_L04, PINPAD_L05, PINPAD_L06,
	PINPAD_T09, PINPAD_T10, PINPAD_T11, PINPAD_T12, PINPAD_T13,
	PINPAD_T14, PINPAD_T02, PINPAD_T03, PINPAD_T04, PINPAD_T05,
	PINPAD_T06
};

static long syscall1(long nr, long a0)
{
	register long r2 __asm__("$2") = nr;
	register long r4 __asm__("$4") = a0;
	register long r7 __asm__("$7");

	__asm__ volatile (
		"syscall"
		: "+r"(r2), "=r"(r7)
		: "r"(r4)
		: "$3", "$5", "$6", "$8", "$9", "$10", "$11",
		  "$12", "$13", "$14", "$15", "$24", "$25", "hi", "lo",
		  "memory");
	if (r7)
		return -r2;
	return r2;
}

static long syscall3(long nr, long a0, long a1, long a2)
{
	register long r2 __asm__("$2") = nr;
	register long r4 __asm__("$4") = a0;
	register long r5 __asm__("$5") = a1;
	register long r6 __asm__("$6") = a2;
	register long r7 __asm__("$7");

	__asm__ volatile (
		"syscall"
		: "+r"(r2), "=r"(r7)
		: "r"(r4), "r"(r5), "r"(r6)
		: "$3", "$8", "$9", "$10", "$11", "$12", "$13",
		  "$14", "$15", "$24", "$25", "hi", "lo", "memory");
	if (r7)
		return -r2;
	return r2;
}

static void write_all(long fd, const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	(void)syscall3(SYS_write, fd, (long)s, (long)(p - s));
}

static void log_message(const char *message)
{
	long log_fd = syscall3(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0);

	if (log_fd < 0)
		log_fd = syscall3(SYS_open, (long)"/dev/console", O_WRONLY, 0);
	if (log_fd < 0)
		log_fd = 1;
	write_all(log_fd, "<6>");
	write_all(log_fd, message);
	if (log_fd > 2)
		(void)syscall1(SYS_close, log_fd);
}

static void log_message_hex24(const char *prefix, uint32_t value)
{
	static const char digits[] = "0123456789abcdef";
	char buf[64];
	unsigned int i = 0;
	unsigned int p;

	for (p = 0; prefix[p] && i + 1 < sizeof(buf); p++)
		buf[i++] = prefix[p];
	if (i + 8 >= sizeof(buf)) {
		buf[sizeof(buf) - 2] = '\n';
		buf[sizeof(buf) - 1] = 0;
		log_message(buf);
		return;
	}
	buf[i++] = '0';
	buf[i++] = 'x';
	for (p = 0; p < 6; p++)
		buf[i++] = digits[(value >> ((5 - p) * 4)) & 0xf];
	buf[i++] = '\n';
	buf[i] = 0;
	log_message(buf);
}

static uint32_t mmio_read32(volatile uint8_t *base, uint32_t off)
{
	return *(volatile uint32_t *)(base + off);
}

static void mmio_write32(volatile uint8_t *base, uint32_t off, uint32_t value)
{
	*(volatile uint32_t *)(base + off) = value;
}

static void mmio_write8(volatile uint8_t *base, uint32_t off, uint8_t value)
{
	*(volatile uint8_t *)(base + off) = value;
}

static uint32_t gpio_base_for_pad(unsigned pad)
{
	if (pad < 32u)
		return GPIOL_OFF;
	if (pad < 64u)
		return GPIOB_OFF;
	if (pad < 96u)
		return GPIOR_OFF;
	return GPIOT_OFF;
}

static uint32_t pinmux_off_for_pad(unsigned pad)
{
	if (pad < 32u)
		return PINMUX_L_OFF + pad;
	if (pad < 64u)
		return PINMUX_B_OFF + pad - 32u;
	if (pad < 96u)
		return PINMUX_R_OFF + pad - 64u;
	return PINMUX_T_OFF + pad - 96u;
}

static uint32_t gpio_bit_for_pad(unsigned pad)
{
	return 1u << (pad & 31u);
}

static void pinmux_set_pad(unsigned pad, unsigned mux)
{
	uint32_t off = pinmux_off_for_pad(pad);
	uint32_t aligned = off & ~3u;
	uint32_t shift = (off & 3u) * 8u;
	uint32_t value = mmio_read32(sysio, aligned);

	value &= ~(0xffu << shift);
	value |= (uint32_t)(mux & 0xffu) << shift;
	mmio_write32(sysio, aligned, value);
}

static void panel_lcd_setup_enable(void)
{
	mmio_write32(sysio, SYS_LCD_SETUP_OFF,
		mmio_read32(sysio, SYS_LCD_SETUP_OFF) | (1u << 16));
}

static void gpio_set_pad(unsigned pad, int high)
{
	uint32_t base = gpio_base_for_pad(pad);
	uint32_t bit = gpio_bit_for_pad(pad);
	uint32_t out = mmio_read32(sysio, base + GPIO_OUTPUT_OFF);

	if (high)
		out |= bit;
	else
		out &= ~bit;
	mmio_write32(sysio, base + GPIO_OUTPUT_OFF, out);
}

static int gpio_get_pad(unsigned pad)
{
	uint32_t base = gpio_base_for_pad(pad);

	return !!(mmio_read32(sysio, base + GPIO_INPUT_OFF) &
		  gpio_bit_for_pad(pad));
}

static void gpio_config_output(unsigned pad)
{
	uint32_t base = gpio_base_for_pad(pad);
	uint32_t dir = mmio_read32(sysio, base + GPIO_DIR_OFF);

	pinmux_set_pad(pad, PINMUX_GPIO);
	mmio_write32(sysio, base + GPIO_DIR_OFF, dir | gpio_bit_for_pad(pad));
}

static void gpio_config_input(unsigned pad)
{
	uint32_t base = gpio_base_for_pad(pad);
	uint32_t dir = mmio_read32(sysio, base + GPIO_DIR_OFF);

	pinmux_set_pad(pad, PINMUX_GPIO);
	mmio_write32(sysio, base + GPIO_DIR_OFF, dir & ~gpio_bit_for_pad(pad));
}

static void panel_control_pinmux(void)
{
	unsigned i;

	panel_lcd_setup_enable();
	for (i = 0; i < sizeof(panel_control_pads) / sizeof(panel_control_pads[0]); i++)
		pinmux_set_pad(panel_control_pads[i], PINMUX_GPIO);
}

static void panel_config_outputs(void)
{
	unsigned i;

	panel_lcd_setup_enable();
	for (i = 0; i < sizeof(panel_control_pads) / sizeof(panel_control_pads[0]); i++)
		gpio_config_output(panel_control_pads[i]);
	gpio_config_input(PINPAD_L08);
}

static void panel_config_data_input(void)
{
	unsigned pad;

	for (pad = PINPAD_L02; pad <= PINPAD_L06; pad++)
		gpio_config_input(pad);
	for (pad = PINPAD_T09; pad <= PINPAD_T14; pad++)
		gpio_config_input(pad);
	for (pad = PINPAD_T02; pad <= PINPAD_T06; pad++)
		gpio_config_input(pad);
}

static void panel_config_data_output(void)
{
	unsigned pad;

	for (pad = PINPAD_L02; pad <= PINPAD_L06; pad++)
		gpio_config_output(pad);
	for (pad = PINPAD_T09; pad <= PINPAD_T14; pad++)
		gpio_config_output(pad);
	for (pad = PINPAD_T02; pad <= PINPAD_T06; pad++)
		gpio_config_output(pad);
}

static void panel_bus_idle(void)
{
	gpio_set_pad(PINPAD_L10, 1);
	gpio_set_pad(PINPAD_T01, 1);
	gpio_set_pad(PINPAD_L07, 1);
	gpio_set_pad(PINPAD_T00, 1);
}

static void panel_write_bus(uint16_t value)
{
	uint32_t lout = mmio_read32(sysio, GPIOL_OFF + GPIO_OUTPUT_OFF);
	uint32_t tout = mmio_read32(sysio, GPIOT_OFF + GPIO_OUTPUT_OFF);

	lout = (lout & ~0x0000007cu) | ((uint32_t)(value & 0x001fu) << 2);
	tout = (tout & ~0x00007e7cu) |
		((uint32_t)(value & 0x07e0u) << 4) |
		(((uint32_t)value >> 9) & 0x0000007cu);
	mmio_write32(sysio, GPIOL_OFF + GPIO_OUTPUT_OFF, lout);
	mmio_write32(sysio, GPIOT_OFF + GPIO_OUTPUT_OFF, tout);
}

static void panel_write16(int rs, uint16_t value)
{
	gpio_set_pad(PINPAD_T01, rs);
	gpio_set_pad(PINPAD_L10, 0);
	panel_write_bus(value);
	gpio_set_pad(PINPAD_L07, 0);
	gpio_set_pad(PINPAD_L07, 1);
	gpio_set_pad(PINPAD_L10, 1);
	gpio_set_pad(PINPAD_T01, 1);
}

static void panel_cmd(uint8_t cmd)
{
	panel_write16(0, cmd);
}

static void panel_data(uint16_t data)
{
	panel_write16(1, data);
}

static uint16_t panel_read_data(void)
{
	uint16_t data = 0;

	gpio_set_pad(PINPAD_T01, 1);
	gpio_set_pad(PINPAD_L10, 0);
	gpio_set_pad(PINPAD_T00, 0);
	data |= (uint16_t)gpio_get_pad(PINPAD_L02) << 0;
	data |= (uint16_t)gpio_get_pad(PINPAD_L03) << 1;
	data |= (uint16_t)gpio_get_pad(PINPAD_L04) << 2;
	data |= (uint16_t)gpio_get_pad(PINPAD_L05) << 3;
	data |= (uint16_t)gpio_get_pad(PINPAD_L06) << 4;
	data |= (uint16_t)gpio_get_pad(PINPAD_T09) << 5;
	data |= (uint16_t)gpio_get_pad(PINPAD_T10) << 6;
	data |= (uint16_t)gpio_get_pad(PINPAD_T11) << 7;
	data |= (uint16_t)gpio_get_pad(PINPAD_T12) << 8;
	data |= (uint16_t)gpio_get_pad(PINPAD_T13) << 9;
	data |= (uint16_t)gpio_get_pad(PINPAD_T14) << 10;
	data |= (uint16_t)gpio_get_pad(PINPAD_T02) << 11;
	data |= (uint16_t)gpio_get_pad(PINPAD_T03) << 12;
	data |= (uint16_t)gpio_get_pad(PINPAD_T04) << 13;
	data |= (uint16_t)gpio_get_pad(PINPAD_T05) << 14;
	data |= (uint16_t)gpio_get_pad(PINPAD_T06) << 15;
	gpio_set_pad(PINPAD_T00, 1);
	gpio_set_pad(PINPAD_L10, 1);
	return data;
}

static uint32_t panel_read_id(void)
{
	uint8_t id[4];
	unsigned i;

	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_cmd(0x04);
	panel_config_data_input();
	for (i = 0; i < sizeof(id); i++)
		id[i] = (uint8_t)panel_read_data();
	panel_config_data_output();
	return ((uint32_t)id[1] << 16) | ((uint32_t)id[2] << 8) | id[3];
}

int main(void)
{
	log_message("sf2000_panel_fastprobe: probe begin\n");
	log_message("sf2000_panel_fastprobe: probe done\n");
	return 0;
}
