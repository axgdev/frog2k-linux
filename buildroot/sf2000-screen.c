// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/cachectl.h>
#include <sys/mman.h>
#include <unistd.h>

extern char **environ;

#define WIDTH 320u
#define HEIGHT 240u
#define PITCH (WIDTH * 2u)
#define FRAME_BYTES (PITCH * HEIGHT)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define GMA_RAM_PHYS 0x00f00000u
#define GMA_RAM_SIZE 0x00100000u
#define GMA_DESC_PHYS GMA_RAM_PHYS
#define GMA_FRAME_PHYS (GMA_RAM_PHYS + 0x00010000u)
#define GMA_DESC_OFF (GMA_DESC_PHYS - GMA_RAM_PHYS)
#define GMA_FRAME_OFF (GMA_FRAME_PHYS - GMA_RAM_PHYS)

#define GMA_MMIO_PHYS 0x18808000u
#define GMA_MMIO_SIZE 0x1000u
#define GMA_CTL 0x300u
#define GMA_DMBA 0x304u
#define GMA_K 0x308u
#define GMA_MASK 0x350u
#define GMA_LINEBUF 0x3b8u

#define SYSIO_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define KSEG1ADDR(x) ((volatile uint8_t *)((uintptr_t)(x) | 0xa0000000u))
#define SYS_CLK_CTR_OFF 0x078u
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
#define GPIO_R_OUT_OFF 0xf4u
#define GPIO_R_DIR_OFF 0xf8u
#define GPIO_OUTPUT_OFF 0x10u
#define GPIO_DIR_OFF 0x14u
#define PIN_R05 5u
#define BACKLIGHT_R05 (1u << PIN_R05)
#define WDT_BASE_PHYS 0x18818000u
#define WDT_REG_OFF 0x500u
#define WDT_COUNT_OFF 0x00u
#define WDT_PET_COUNT 0xffc61075u

#define PINPAD_L01 1u
#define PINPAD_L02 2u
#define PINPAD_L03 3u
#define PINPAD_L04 4u
#define PINPAD_L05 5u
#define PINPAD_L06 6u
#define PINPAD_L07 7u
#define PINPAD_L08 8u
#define PINPAD_L09 9u
#define PINPAD_L10 10u
#define PINPAD_L25 25u
#define PINPAD_R05 69u
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
#define PINMUX_PWM_2 1u
#define PINMUX_PRGB_R3 6u
#define PINMUX_PRGB_R4 6u
#define PINMUX_PRGB_R5 6u
#define PINMUX_PRGB_R6 6u
#define PINMUX_PRGB_R7 6u
#define PINMUX_PRGB_G2 6u
#define PINMUX_PRGB_G3 6u
#define PINMUX_PRGB_G4 6u
#define PINMUX_PRGB_G5 6u
#define PINMUX_PRGB_G6 6u
#define PINMUX_PRGB_G7 6u
#define PINMUX_PRGB_B3 6u
#define PINMUX_PRGB_B4 6u
#define PINMUX_PRGB_B5 6u
#define PINMUX_PRGB_B6 6u
#define PINMUX_PRGB_B7 6u
#define PINMUX_PRGB_CLK 6u
#define PINMUX_PRGB_VSYNC 6u
#define PINMUX_PRGB_DE 6u

#define ST7789_SLPOUT 0x11u
#define ST7789_NORON 0x13u
#define ST7789_INVON 0x21u
#define ST7789_DISPON 0x29u
#define ST7789_CASET 0x2au
#define ST7789_RASET 0x2bu
#define ST7789_RAMWR 0x2cu
#define ST7789_TEON 0x35u
#define ST7789_MADCTL 0x36u
#define ST7789_COLMOD 0x3au

static volatile uint8_t *gma_ram;
static volatile uint8_t *gma;
static volatile uint8_t *sysio;
static volatile int stopping;
static int panel_enabled = 1;
static int led_enabled = 1;
static int slow_panel_bus = 1;

static uint16_t *framebuffer(void);

struct glyph {
	char ch;
	uint8_t rows[7];
};

static const struct glyph glyphs[] = {
	{ ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ '-', { 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 } },
	{ ':', { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00 } },
	{ '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c } },
	{ '/', { 0x01, 0x02, 0x04, 0x04, 0x08, 0x10, 0x10 } },
	{ '0', { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e } },
	{ '1', { 0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f } },
	{ '2', { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f } },
	{ '3', { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e } },
	{ '4', { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 } },
	{ '5', { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e } },
	{ '6', { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e } },
	{ '7', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
	{ '8', { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e } },
	{ '9', { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c } },
	{ 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
	{ 'B', { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e } },
	{ 'C', { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e } },
	{ 'D', { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e } },
	{ 'E', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } },
	{ 'F', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 } },
	{ 'G', { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f } },
	{ 'H', { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
	{ 'I', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f } },
	{ 'J', { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c } },
	{ 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
	{ 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f } },
	{ 'M', { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 } },
	{ 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
	{ 'O', { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
	{ 'P', { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 } },
	{ 'Q', { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d } },
	{ 'R', { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } },
	{ 'S', { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } },
	{ 'T', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
	{ 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
	{ 'V', { 0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04 } },
	{ 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11 } },
	{ 'X', { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 } },
	{ 'Y', { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 } },
	{ 'Z', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f } },
};

static const uint8_t st7789_x60_old_init[] = {
	0, 1, ST7789_SLPOUT,
	99, 3, 0xcf, 0x00, 0xa1,
	0, 3, 0xb1, 0x00, 0x1e,
	0, 2, 0xb4, 0x02,
	0, 2, 0xb6, 0x02,
	0, 3, 0xc0, 0x0f, 0x0d,
	0, 2, 0xc1, 0x00,
	0, 2, 0xc5, 0xe7,
	0, 16, 0xe0, 0x05, 0x08, 0x0d, 0x07, 0x10, 0x08, 0x33, 0x35,
		0x45, 0x04, 0x0b, 0x08, 0x1a, 0x1d, 0x0f,
	0, 16, 0xe1, 0x06, 0x23, 0x26, 0x00, 0x0c, 0x01, 0x39, 0x02,
		0x4a, 0x02, 0x0c, 0x07, 0x31, 0x36, 0x0f,
	0, 2, ST7789_COLMOD, 0x55,
	0, 2, ST7789_MADCTL, 0xa8,
	0
};

static const uint8_t st7789_x60_new_init[] = {
	1, 3, 0xf0, 0x5a, 0x5a,
	0, 6, 0xf3, 0x00, 0x00, 0x00, 0x00, 0x00,
	0, 1, ST7789_SLPOUT,
	16, 1, ST7789_NORON,
	128, 12, 0xf4, 0x07, 0x00, 0x00, 0x00, 0x21, 0x47, 0x01, 0x02,
		0x2a, 0x66, 0x05,
	0, 11, 0xf5, 0x00, 0x4d, 0x66, 0x00, 0x00, 0x12, 0x00, 0x00,
		0x0d, 0x01,
	0, 2, ST7789_TEON, 0x00,
	0, 2, ST7789_MADCTL, 0x88,
	0, 2, ST7789_COLMOD, 0x55,
	0, 6, 0xf3, 0x00, 0x03, 0x00, 0x00, 0x00,
	16, 4, 0xf3, 0x00, 0x0f, 0x01,
	16, 3, 0xf3, 0x00, 0x1f,
	16, 3, 0xf3, 0x00, 0x3f,
	16, 4, 0xf3, 0x00, 0x3f, 0x03,
	48, 3, 0xf3, 0x00, 0x7f,
	48, 3, 0xf3, 0x00, 0xff,
	32, 6, 0xf3, 0x00, 0xff, 0x1f, 0x00, 0x02,
	0, 12, 0xf4, 0x07, 0x00, 0x00, 0x00, 0x21, 0x47, 0x04, 0x02,
		0x2a, 0x66, 0x05,
	16, 2, 0xf3, 0x01,
	0, 18, 0xf2, 0x28, 0x65, 0x7f, 0x08, 0x08, 0x00, 0x00, 0x15,
		0x48, 0x00, 0x07, 0x01, 0x00, 0x00, 0x94, 0x08, 0x08,
	0, 2, ST7789_TEON, 0x00,
	0, 2, ST7789_MADCTL, 0xe8,
	0, 2, ST7789_COLMOD, 0x55,
	0, 1, ST7789_NORON,
	0, 3, 0xf0, 0xa5, 0xa5,
	0
};

static const uint8_t st7789_q19_init[] = {
	1, 3, 0xf0, 0x5a, 0x5a,
	0, 6, 0xf3, 0x00, 0x00, 0x00, 0x00, 0x00,
	0, 1, ST7789_SLPOUT,
	16, 1, ST7789_NORON,
	128, 12, 0xf4, 0x07, 0x00, 0x00, 0x00, 0x21, 0x47, 0x01, 0x02,
		0x2a, 0x66, 0x05,
	0, 11, 0xf5, 0x00, 0x4d, 0x66, 0x00, 0x00, 0x12, 0x00, 0x00,
		0x0d, 0x01,
	0, 2, ST7789_TEON, 0x00,
	0, 2, ST7789_MADCTL, 0x88,
	0, 2, ST7789_COLMOD, 0x55,
	0, 6, 0xf3, 0x00, 0x03, 0x00, 0x00, 0x00,
	16, 4, 0xf3, 0x00, 0x0f, 0x01,
	16, 3, 0xf3, 0x00, 0x1f,
	16, 3, 0xf3, 0x00, 0x3f,
	16, 4, 0xf3, 0x00, 0x3f, 0x03,
	48, 3, 0xf3, 0x00, 0x7f,
	48, 3, 0xf3, 0x00, 0xff,
	32, 6, 0xf3, 0x00, 0xff, 0x1f, 0x00, 0x02,
	0, 12, 0xf4, 0x07, 0x00, 0x00, 0x00, 0x21, 0x47, 0x04, 0x02,
		0x2a, 0x66, 0x05,
	16, 2, 0xf3, 0x01,
	0, 18, 0xf2, 0x28, 0x65, 0x7f, 0x08, 0x08, 0x00, 0x00, 0x15,
		0x48, 0x00, 0x07, 0x01, 0x00, 0x00, 0x94, 0x08, 0x08,
	0, 2, ST7789_TEON, 0x00,
	0, 2, ST7789_MADCTL, 0x28,
	0, 2, ST7789_COLMOD, 0x55,
	0, 1, ST7789_NORON,
	0, 3, 0xf0, 0xa5, 0xa5,
	0
};

static const uint8_t st7789_dy12_init[] = {
	1, 4, 0xb9, 0xff, 0x83, 0x47,
	0, 1, ST7789_SLPOUT,
	99, 2, 0xcc, 0x08,
	0, 5, 0xb3, 0x00, 0x00, 0x08, 0x04,
	0, 2, ST7789_MADCTL, 0x68,
	0, 2, ST7789_COLMOD, 0x55,
	0, 4, 0xb6, 0x88, 0x2f, 0x57,
	0, 2, 0xb0, 0x3b,
	5, 8, 0xb1, 0x00, 0x01, 0x31, 0x03, 0x44, 0x44, 0xd4,
	0, 8, 0xb4, 0x11, 0x8f, 0x00, 0x04, 0x04, 0x1d, 0x88,
	0, 5, 0xe3, 0x10, 0x10, 0x10, 0x10,
	0, 9, 0xbf, 0x00, 0x00, 0xc0, 0x70, 0x38, 0x3c, 0xc7, 0x00,
	0, 4, 0xb6, 0x8a, 0x67, 0x57,
	0, 28, 0xe0, 0x01, 0x07, 0x07, 0x1f, 0x1c, 0x3e, 0x1b, 0x6b,
		0x07, 0x13, 0x19, 0x19, 0x16, 0x01, 0x23, 0x20,
		0x38, 0x38, 0x3e, 0x14, 0x64, 0x09, 0x06, 0x06,
		0x0c, 0x18, 0xcc,
	0
};

static const uint8_t st7789_sf2000_init[] = {
	0, 1, ST7789_SLPOUT,
	99, 2, ST7789_MADCTL, 0x70,
	0, 2, ST7789_COLMOD, 0x55,
	0, 4, 0xb1, 0x40, 0x04, 0x14,
	0, 6, 0xb2, 0x0c, 0x0c, 0x00, 0x33, 0x33,
	0, 2, 0xb7, 0x71,
	0, 2, 0xbb, 0x3b,
	0, 2, 0xc0, 0x2c,
	0, 2, 0xc2, 0x01,
	0, 2, 0xc3, 0x13,
	0, 2, 0xc4, 0x20,
	0, 2, 0xc6, 0x0f,
	0, 3, 0xd0, 0xa4, 0xa1,
	0, 2, 0xd6, 0xa1,
	0, 15, 0xe0, 0xd0, 0x06, 0x06, 0x0e, 0x0d, 0x06, 0x2f, 0x3a,
		0x47, 0x08, 0x15, 0x14, 0x2c, 0x33,
	0, 15, 0xe1, 0xd0, 0x06, 0x06, 0x0e, 0x0d, 0x06, 0x2f, 0x3b,
		0x47, 0x08, 0x15, 0x14, 0x2c, 0x33,
	0, 1, ST7789_INVON,
	0
};

static const uint8_t st7789_009306_init[] = {
	0, 1, ST7789_SLPOUT, 0,
	1, 0xfe, 0,
	1, 0xef, 0,
	2, ST7789_MADCTL, 0x28, 0,
	2, ST7789_COLMOD, 0x05, 0,
	3, 0xa4, 0x44, 0x44, 0,
	3, 0xa5, 0x42, 0x42, 0,
	3, 0xaa, 0x88, 0x88, 0,
	3, 0xe8, 0x11, 0x0b, 0,
	3, 0xe3, 0x01, 0x10, 0,
	2, 0xff, 0x61, 0,
	2, 0xac, 0x00, 0,
	2, 0xad, 0x33, 0,
	2, 0xae, 0x2b, 0,
	2, 0xaf, 0x55, 0,
	3, 0xa6, 0x25, 0x25, 0,
	3, 0xa7, 0x24, 0x24, 0,
	3, 0xa8, 0x13, 0x13, 0,
	3, 0xa9, 0x25, 0x25, 0,
	5, ST7789_CASET, 0x00, 0x00, 0x00, 0xef, 0,
	5, ST7789_RASET, 0x00, 0x00, 0x01, 0x3f, 0,
	1, ST7789_RAMWR, 0,
	7, 0xf0, 0x02, 0x01, 0x00, 0x06, 0x09, 0x0c, 0,
	7, 0xf1, 0x01, 0x03, 0x00, 0x3a, 0x3e, 0x09, 0,
	7, 0xf2, 0x0c, 0x09, 0x26, 0x07, 0x07, 0x30, 0,
	7, 0xf3, 0x09, 0x06, 0x57, 0x03, 0x03, 0x6b, 0,
	7, 0xf4, 0x0d, 0x1d, 0x1c, 0x06, 0x08, 0x0f, 0,
	7, 0xf5, 0x0c, 0x05, 0x06, 0x33, 0x31, 0x0f, 0,
	1, ST7789_SLPOUT, 99,
	0
};

struct panel_variant {
	const char *name;
	const uint8_t *init;
	uint8_t madctl[4];
};

static const struct panel_variant panel_variants[] = {
	{ "SF2000", st7789_sf2000_init, { 0x70, 0x00, 0x80, 0xc0 } },
	{ "X60 OLD", st7789_x60_old_init, { 0xa8, 0x68, 0x28, 0xe8 } },
	{ "X60 NEW", st7789_x60_new_init, { 0xe8, 0x88, 0x28, 0x68 } },
	{ "Q19", st7789_q19_init, { 0x28, 0x68, 0xa8, 0xe8 } },
	{ "DY12", st7789_dy12_init, { 0x68, 0x28, 0xa8, 0xe8 } },
	{ "009306", st7789_009306_init, { 0x28, 0x68, 0xa8, 0xe8 } },
};

struct pinmux_setting {
	unsigned pad;
	unsigned mux;
};

static const unsigned panel_control_pads[] = {
	PINPAD_L10, PINPAD_T01, PINPAD_L07, PINPAD_T00, PINPAD_L01,
	PINPAD_L02, PINPAD_L03, PINPAD_L04, PINPAD_L05, PINPAD_L06,
	PINPAD_T09, PINPAD_T10, PINPAD_T11, PINPAD_T12, PINPAD_T13,
	PINPAD_T14, PINPAD_T02, PINPAD_T03, PINPAD_T04, PINPAD_T05,
	PINPAD_T06
};

static const char *const panel_control_names[] = {
	"L10", "T01", "L07", "T00", "L01", "L02", "L03", "L04", "L05",
	"L06", "T09", "T10", "T11", "T12", "T13", "T14", "T02", "T03",
	"T04", "T05", "T06"
};

static const struct pinmux_setting panel_rgb_pads[] = {
	{ PINPAD_T06, PINMUX_PRGB_R7 },
	{ PINPAD_T05, PINMUX_PRGB_R6 },
	{ PINPAD_T04, PINMUX_PRGB_R5 },
	{ PINPAD_T03, PINMUX_PRGB_R4 },
	{ PINPAD_T02, PINMUX_PRGB_R3 },
	{ PINPAD_T14, PINMUX_PRGB_G7 },
	{ PINPAD_T13, PINMUX_PRGB_G6 },
	{ PINPAD_T12, PINMUX_PRGB_G5 },
	{ PINPAD_T11, PINMUX_PRGB_G4 },
	{ PINPAD_T10, PINMUX_PRGB_G3 },
	{ PINPAD_T09, PINMUX_PRGB_G2 },
	{ PINPAD_L06, PINMUX_PRGB_B7 },
	{ PINPAD_L05, PINMUX_PRGB_B6 },
	{ PINPAD_L04, PINMUX_PRGB_B5 },
	{ PINPAD_L03, PINMUX_PRGB_B4 },
	{ PINPAD_L02, PINMUX_PRGB_B3 },
	{ PINPAD_L07, PINMUX_PRGB_CLK },
	{ PINPAD_L09, PINMUX_PRGB_VSYNC },
	{ PINPAD_L10, PINMUX_PRGB_DE },
};

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

static uint16_t rgb565(unsigned r, unsigned g, unsigned b)
{
	return (uint16_t)(((r & 0x1f) << 11) | ((g & 0x3f) << 5) | (b & 0x1f));
}

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

static void append_file_log(const char *line)
{
	int fd = open("/tmp/sf2000-boot.log",
		O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);

	if (fd < 0)
		return;
	(void)write(fd, line, strlen(line));
	close(fd);
}

static void watchdog_pet(void)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);

	*(volatile uint32_t *)(wdt + WDT_REG_OFF + WDT_COUNT_OFF) =
		WDT_PET_COUNT;
}

static void sleep_ms(unsigned msec)
{
	volatile unsigned spin;
	unsigned i;

	for (i = 0; i < msec && !stopping; i++) {
		if ((i & 31u) == 0)
			watchdog_pet();
		for (spin = 0; spin < 18000u; spin++)
			__asm__ volatile ("" ::: "memory");
	}
	watchdog_pet();
}

static int map_region(int fd, volatile uint8_t **out, uint32_t phys,
		uint32_t size, const char *name)
{
	void *map;

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys);
	if (map == MAP_FAILED) {
		char line[96];

		snprintf(line, sizeof(line), "sf2000-screen: mmap %s failed\n", name);
		log_line(line);
		return -1;
	}
	*out = map;
	return 0;
}

static void map_regions_direct(void)
{
	gma_ram = KSEG1ADDR(GMA_RAM_PHYS);
	gma = KSEG1ADDR(GMA_MMIO_PHYS);
	sysio = KSEG1ADDR(SYSIO_PHYS);
	log_line("sf2000-screen: using direct MMIO mappings\n");
}

static void backlight_set(int on)
{
	uint32_t bit = BACKLIGHT_R05;
	uint32_t out;
	uint32_t dir;

	mmio_write8(sysio, PINMUX_R_OFF + PIN_R05, PINMUX_GPIO);
	dir = mmio_read32(sysio, GPIO_R_DIR_OFF);
	mmio_write32(sysio, GPIO_R_DIR_OFF, dir | bit);

	out = mmio_read32(sysio, GPIO_R_OUT_OFF);
	if (on)
		out &= ~bit;
	else
		out |= bit;
	mmio_write32(sysio, GPIO_R_OUT_OFF, out);
}

static int publish_marker(const char *path, const char *text)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	if (fd < 0)
		return -1;
	(void)write(fd, text, strlen(text));
	close(fd);
	return 0;
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
	mmio_write8(sysio, pinmux_off_for_pad(pad), (uint8_t)mux);
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

	return !!(mmio_read32(sysio, base + GPIO_INPUT_OFF) & gpio_bit_for_pad(pad));
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

static void status_led_set(int on)
{
	if (!led_enabled)
		return;
	gpio_config_output(PINPAD_L25);
	gpio_set_pad(PINPAD_L25, on);
}

static void diagnostic_pulse(unsigned count, unsigned on_ms, unsigned off_ms)
{
	unsigned i;

	for (i = 0; i < count && !stopping; i++) {
		backlight_set(1);
		status_led_set(1);
		sleep_ms(on_ms);
		backlight_set(0);
		status_led_set(0);
		sleep_ms(off_ms);
	}
	backlight_set(1);
	status_led_set(0);
}

static void startup_backlight_diagnostic(void)
{
	log_line("sf2000-screen: startup backlight off/on diagnostic begin\n");
	append_file_log("sf2000-screen: startup backlight off/on diagnostic begin\n");

	log_line("sf2000-screen: diag step 1 backlight off\n");
	backlight_set(0);
	sleep_ms(80);
	log_line("sf2000-screen: diag step 2 backlight on\n");
	backlight_set(1);
	sleep_ms(80);
	log_line("sf2000-screen: diag step 3 backlight off\n");
	backlight_set(0);
	sleep_ms(80);
	log_line("sf2000-screen: diag step 4 backlight on\n");
	backlight_set(1);

	log_line("sf2000-screen: startup backlight off/on diagnostic end\n");
	append_file_log("sf2000-screen: startup backlight off/on diagnostic end\n");
}

static void panel_control_pinmux(void)
{
	unsigned i;

	panel_lcd_setup_enable();
	for (i = 0; i < ARRAY_SIZE(panel_control_pads); i++)
		pinmux_set_pad(panel_control_pads[i], PINMUX_GPIO);
}

static void panel_rgb_pinmux(void)
{
	unsigned i;

	panel_lcd_setup_enable();
	for (i = 0; i < ARRAY_SIZE(panel_rgb_pads); i++)
		pinmux_set_pad(panel_rgb_pads[i].pad, panel_rgb_pads[i].mux);
}

static void panel_config_outputs(void)
{
	unsigned i;

	panel_lcd_setup_enable();
	for (i = 0; i < ARRAY_SIZE(panel_control_pads); i++) {
		char line[96];
		unsigned pad = panel_control_pads[i];
		uint32_t base = gpio_base_for_pad(pad);

		snprintf(line, sizeof(line),
			"sf2000-screen: panel gpio out %u %s base=0x%03x dir=0x%08x\n",
			i, panel_control_names[i], base,
			mmio_read32(sysio, base + GPIO_DIR_OFF));
		log_line(line);
		gpio_config_output(panel_control_pads[i]);
		snprintf(line, sizeof(line),
			"sf2000-screen: panel gpio out done %u %s dir=0x%08x\n",
			i, panel_control_names[i],
			mmio_read32(sysio, base + GPIO_DIR_OFF));
		log_line(line);
	}
	gpio_config_input(PINPAD_L08);
	log_line("sf2000-screen: panel gpio vsync L08 input done\n");
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

static void panel_bus_delay(void)
{
	if (slow_panel_bus)
		(void)mmio_read32(sysio, GPIOL_OFF + GPIO_OUTPUT_OFF);
}

static void panel_write16(int rs, uint16_t value)
{
	watchdog_pet();
	gpio_set_pad(PINPAD_T01, rs);
	gpio_set_pad(PINPAD_L10, 0);
	panel_write_bus(value);
	panel_bus_delay();
	gpio_set_pad(PINPAD_L07, 0);
	panel_bus_delay();
	gpio_set_pad(PINPAD_L07, 1);
	panel_bus_delay();
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

static void panel_restart_frame(void)
{
	panel_cmd(ST7789_CASET);
	panel_data(0);
	panel_data(0);
	panel_data((WIDTH - 1u) >> 8);
	panel_data((WIDTH - 1u) & 0xffu);
	panel_cmd(ST7789_RASET);
	panel_data(0);
	panel_data(0);
	panel_data((HEIGHT - 1u) >> 8);
	panel_data((HEIGHT - 1u) & 0xffu);
	panel_cmd(ST7789_RAMWR);
}

static void panel_reset(void)
{
	panel_bus_idle();
	gpio_set_pad(PINPAD_L01, 1);
	sleep_ms(120);
	gpio_set_pad(PINPAD_L01, 0);
	sleep_ms(40);
	gpio_set_pad(PINPAD_L01, 1);
	sleep_ms(120);
}

static void panel_apply_init_sequence(const uint8_t *sequence)
{
	const uint8_t *p = sequence;
	unsigned clock_skewed = *p++;

	watchdog_pet();
	panel_lcd_setup_enable();
	if (clock_skewed)
		mmio_write32(sysio, SYS_CLK_CTR_OFF,
			mmio_read32(sysio, SYS_CLK_CTR_OFF) | (1u << 15));
	else
		mmio_write32(sysio, SYS_CLK_CTR_OFF,
			mmio_read32(sysio, SYS_CLK_CTR_OFF) & ~(1u << 15));

	while (*p) {
		unsigned count = *p++;
		unsigned delay_ms;

		watchdog_pet();
		panel_cmd(*p++);
		while (--count)
			panel_data(*p++);
		delay_ms = *p++;
		if (delay_ms)
			sleep_ms(delay_ms);
	}
	watchdog_pet();
}

static void panel_set_madctl(uint8_t madctl)
{
	if (!panel_enabled)
		return;
	panel_cmd(ST7789_MADCTL);
	panel_data(madctl);
}

static int panel_init_variant(const struct panel_variant *variant)
{
	uint32_t panel_id;
	char line[128];

	if (!panel_enabled)
		return 0;

	log_line("sf2000-screen: panel init pinmux\n");
	panel_control_pinmux();
	log_line("sf2000-screen: panel init gpio outputs\n");
	panel_config_outputs();
	log_line("sf2000-screen: panel init idle/reset\n");
	panel_bus_idle();
	panel_reset();
	sleep_ms(120);
	log_line("sf2000-screen: panel init read id\n");
	panel_id = panel_read_id();
	log_line("sf2000-screen: panel init sequence\n");
	panel_apply_init_sequence(variant->init);
	panel_cmd(ST7789_DISPON);
	panel_restart_frame();

	snprintf(line, sizeof(line),
		"sf2000-screen: panel init done id=0x%06x init=%s\n",
		panel_id & 0xffffffu, variant->name);
	log_line(line);
	append_file_log(line);
	return 0;
}

static void panel_push_frame(int switch_to_rgb)
{
	uint16_t *fb = framebuffer();
	unsigned i;

	if (!panel_enabled)
		return;

	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_restart_frame();
	for (i = 0; i < WIDTH * HEIGHT; i++) {
		if ((i & 0xffu) == 0)
			watchdog_pet();
		panel_data(fb[i]);
	}
	panel_bus_idle();
	if (switch_to_rgb)
		panel_rgb_pinmux();
	watchdog_pet();
}

static void panel_push_probe_pixels(void)
{
	unsigned i;

	if (!panel_enabled)
		return;
	watchdog_pet();
	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_restart_frame();
	for (i = 0; i < 16; i++)
		panel_data(rgb565(31, 63, 31));
	panel_bus_idle();
	watchdog_pet();
}

static void panel_prepare_rgb_frame(void)
{
	if (!panel_enabled)
		return;
	watchdog_pet();
	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_restart_frame();
	panel_rgb_pinmux();
	watchdog_pet();
}

static const uint8_t *glyph_for(char ch)
{
	unsigned i;

	if (ch >= 'a' && ch <= 'z')
		ch = (char)(ch - 'a' + 'A');

	for (i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
		if (glyphs[i].ch == ch)
			return glyphs[i].rows;
	}
	return glyphs[0].rows;
}

static uint16_t *framebuffer(void)
{
	return (uint16_t *)(gma_ram + GMA_FRAME_OFF);
}

static void put_pixel(unsigned x, unsigned y, uint16_t color)
{
	if (x < WIDTH && y < HEIGHT)
		framebuffer()[y * WIDTH + x] = color;
}

static void fill_rect(unsigned x, unsigned y, unsigned w, unsigned h,
	uint16_t color)
{
	unsigned yy;

	for (yy = y; yy < y + h && yy < HEIGHT; yy++) {
		unsigned xx;

		for (xx = x; xx < x + w && xx < WIDTH; xx++)
			put_pixel(xx, yy, color);
	}
}

static void draw_char(unsigned x, unsigned y, char ch, uint16_t fg,
	uint16_t bg, unsigned scale)
{
	const uint8_t *rows = glyph_for(ch);
	unsigned yy;

	for (yy = 0; yy < 7; yy++) {
		unsigned xx;

		for (xx = 0; xx < 5; xx++) {
			uint16_t color = (rows[yy] & (1u << (4 - xx))) ? fg : bg;
			fill_rect(x + xx * scale, y + yy * scale, scale, scale, color);
		}
	}
}

static void draw_text(unsigned x, unsigned y, const char *text, uint16_t fg,
	uint16_t bg, unsigned scale)
{
	while (*text) {
		draw_char(x, y, *text++, fg, bg, scale);
		x += 6u * scale;
	}
}

static void draw_background(void)
{
	unsigned y;

	for (y = 0; y < HEIGHT; y++) {
		unsigned x;

		for (x = 0; x < WIDTH; x++) {
			unsigned r = 1 + (x * 10u) / WIDTH;
			unsigned g = 4 + (y * 14u) / HEIGHT;
			unsigned b = 8 + (((x ^ y) & 0x7f) * 12u) / 0x7f;

			put_pixel(x, y, rgb565(r, g, b));
		}
	}

	fill_rect(0, 0, WIDTH, 31, rgb565(0, 10, 18));
	fill_rect(0, 209, WIDTH, 31, rgb565(1, 8, 5));
	fill_rect(0, 31, WIDTH, 2, rgb565(31, 46, 0));
	fill_rect(0, 207, WIDTH, 2, rgb565(31, 46, 0));
}

static void draw_diag_screen(const char *phase, const char *variant,
	uint8_t madctl, unsigned frame)
{
	char line[48];
	uint16_t white = rgb565(31, 63, 31);
	uint16_t yellow = rgb565(31, 54, 4);
	uint16_t green = rgb565(8, 60, 16);
	uint16_t blue = rgb565(5, 24, 31);
	uint16_t red = rgb565(31, 8, 6);
	uint16_t dark = rgb565(0, 4, 6);
	unsigned i;

	draw_background();
	draw_text(20, 8, "SF2000 LINUX", yellow, rgb565(0, 10, 18), 3);
	draw_text(18, 48, "BUILDROOT USERSPACE OK", white, dark, 2);
	draw_text(18, 72, phase, green, dark, 2);
	draw_text(18, 96, variant, blue, dark, 2);
	snprintf(line, sizeof(line), "MADCTL %02X", madctl);
	draw_text(18, 120, line, red, dark, 2);
	draw_text(18, 144, "KMSG AND TMP LOG READY", yellow, dark, 2);

	snprintf(line, sizeof(line), "FRAME %06u", frame);
	draw_text(18, 180, line, white, dark, 2);

	for (i = 0; i < 8; i++) {
		uint16_t color = rgb565((i & 1) ? 31 : 5,
			(i & 2) ? 63 : 10, (i & 4) ? 31 : 5);

		fill_rect(16 + i * 36, 216, 28, 12, color);
	}
}

static void write_desc32(unsigned idx, uint32_t value)
{
	((volatile uint32_t *)(gma_ram + GMA_DESC_OFF))[idx] = value;
}

static void build_gma_descriptor(void)
{
	unsigned i;

	for (i = 0; i < 16; i++)
		write_desc32(i, 0);

	write_desc32(0, (6u << 4) | (1u << 8) | (32u << 16) | (170u << 24));
	write_desc32(1, 0);
	write_desc32(2, ((WIDTH - 1u) << 16) | 0u);
	write_desc32(3, ((HEIGHT - 1u) << 16) | 0u);
	write_desc32(4, (HEIGHT << 16) | WIDTH);
	write_desc32(5, 0xffu | (PITCH << 16));
	write_desc32(6, 0);
	write_desc32(7, GMA_FRAME_PHYS);
}

static void flush_present_memory(void)
{
	(void)cacheflush((void *)(gma_ram + GMA_DESC_OFF), 64, BCACHE);
	(void)cacheflush((void *)(gma_ram + GMA_FRAME_OFF), FRAME_BYTES, BCACHE);
}

static void gma_set_bit(uint32_t off, uint32_t bit, int on)
{
	uint32_t value = mmio_read32(gma, off);

	if (on)
		value |= bit;
	else
		value &= ~bit;
	mmio_write32(gma, GMA_MASK, 1);
	mmio_write32(gma, off, value);
	mmio_write32(gma, GMA_MASK, 0);
}

static void present_frame(void)
{
	flush_present_memory();
	mmio_write32(gma, GMA_MASK, 1);
	mmio_write32(gma, GMA_LINEBUF, 0x0a);
	mmio_write32(gma, GMA_K, 0xff);
	mmio_write32(gma, GMA_CTL, mmio_read32(gma, GMA_CTL) | 1u);
	mmio_write32(gma, GMA_DMBA, GMA_DESC_PHYS);
	mmio_write32(gma, GMA_MASK, 0);
	gma_set_bit(GMA_CTL, 1u << 19, 0);
	gma_set_bit(GMA_CTL, 1u << 18, 1);
}

static int env_is(const char *const *envp, const char *name, const char *value)
{
	size_t name_len = strlen(name);

	if (!envp)
		return 0;
	while (*envp) {
		if (strncmp(*envp, name, name_len) == 0 &&
				(*envp)[name_len] == '=' &&
				strcmp(*envp + name_len + 1, value) == 0)
			return 1;
		envp++;
	}
	return 0;
}

static void log_gma_ready(void)
{
	char line[160];

	snprintf(line, sizeof(line),
		"sf2000-screen: gma console ready desc=0x%08x fb=0x%08x %ux%u pitch=%u\n",
		GMA_DESC_PHYS, GMA_FRAME_PHYS, WIDTH, HEIGHT, PITCH);
	log_line(line);
	append_file_log(line);
}

static void log_direct_diag(const struct panel_variant *variant,
	uint8_t madctl, unsigned frame)
{
	char line[160];

	snprintf(line, sizeof(line),
		"sf2000-screen: direct diag variant=%s madctl=0x%02x frame=%u\n",
		variant->name, madctl, frame);
	log_line(line);
	append_file_log(line);
}

static void show_direct_frame(const char *phase,
	const struct panel_variant *variant, uint8_t madctl, unsigned *frame,
	unsigned hold_ms)
{
	if (stopping)
		return;
	panel_set_madctl(madctl);
	(*frame)++;
	draw_diag_screen(phase, variant->name, madctl, *frame);
	panel_push_frame(0);
	log_direct_diag(variant, madctl, *frame);
	sleep_ms(hold_ms);
}

static void run_direct_diag(unsigned *frame)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(panel_variants) && !stopping; i++) {
		const struct panel_variant *variant = &panel_variants[i];
		unsigned madctl_count = i == 0 ? ARRAY_SIZE(variant->madctl) : 1;
		unsigned m;

		diagnostic_pulse((i % 3u) + 1u, 120, 120);
		panel_init_variant(variant);
		for (m = 0; m < madctl_count && !stopping; m++)
			show_direct_frame("DIRECT PANEL BUS", variant,
				variant->madctl[m], frame, 350);
	}
}

static void run_rgb_diag(unsigned *frame)
{
	const struct panel_variant *variant = &panel_variants[0];
	unsigned i;

	if (stopping)
		return;

	diagnostic_pulse(4, 120, 120);
	panel_init_variant(variant);
	panel_set_madctl(variant->madctl[0]);
	(*frame)++;
	draw_diag_screen("GMA RGB SCANOUT", variant->name,
		variant->madctl[0], *frame);
	panel_push_frame(0);
	sleep_ms(350);

	for (i = 0; i < 8 && !stopping; i++) {
		(*frame)++;
		draw_diag_screen("GMA RGB SCANOUT", variant->name,
			variant->madctl[0], *frame);
		panel_prepare_rgb_frame();
		present_frame();
		sleep_ms(350);
	}
}

static void run_rgb_only_diag(unsigned *frame)
{
	int ready_published = 0;

	log_line("sf2000-screen: rgb-only diag begin\n");
	append_file_log("sf2000-screen: rgb-only diag begin\n");
	panel_lcd_setup_enable();
	panel_rgb_pinmux();
	build_gma_descriptor();

	while (!stopping) {
		(*frame)++;
		draw_diag_screen("RGB ONLY NO PANEL BUS", "GMA PRGB", 0,
			*frame);
		panel_rgb_pinmux();
		present_frame();
		if (!ready_published) {
			publish_marker("/run/sf2000-screen-ready", "ready\n");
			log_gma_ready();
			ready_published = 1;
		}
		backlight_set(1);
		sleep_ms(900);
		backlight_set(0);
		sleep_ms(70);
		backlight_set(1);
		sleep_ms(900);
	}
}

int main(int argc, char **argv, char **envp)
{
	int fd;
	unsigned frame = 0;

	(void)argc;
	(void)argv;
	if (envp)
		environ = envp;
	const struct panel_variant *first_variant = &panel_variants[0];

	if (env_is((const char *const *)envp, "SF2000_PANEL", "0"))
		panel_enabled = 0;
	if (env_is((const char *const *)envp, "SF2000_LED", "0"))
		led_enabled = 0;
	if (env_is((const char *const *)envp, "SF2000_FAST_PANEL", "1"))
		slow_panel_bus = 0;

	fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (fd < 0) {
		map_regions_direct();
	} else {
		if (map_region(fd, &gma_ram, GMA_RAM_PHYS, GMA_RAM_SIZE, "gma-ram") != 0 ||
				map_region(fd, &gma, GMA_MMIO_PHYS, GMA_MMIO_SIZE, "gma") != 0 ||
				map_region(fd, &sysio, SYSIO_PHYS, SYSIO_SIZE, "sysio") != 0) {
			close(fd);
			map_regions_direct();
		} else {
			close(fd);
		}
	}

	publish_marker("/run/sf2000-screen-own-backlight", "owned\n");
	startup_backlight_diagnostic();

	build_gma_descriptor();
	if (!env_is((const char *const *)envp, "SF2000_DIRECT_PANEL", "1"))
		run_rgb_only_diag(&frame);

	log_line("sf2000-screen: before panel init\n");
	panel_init_variant(first_variant);
	log_line("sf2000-screen: after panel init\n");
	panel_push_probe_pixels();
	draw_diag_screen("GMA TRACE READY", first_variant->name,
		first_variant->madctl[0], frame);
	present_frame();
	publish_marker("/run/sf2000-screen-ready", "ready\n");
	log_gma_ready();

	draw_diag_screen("FIRST DIRECT BUS", first_variant->name,
		first_variant->madctl[0], frame);
	panel_push_frame(0);
	sleep_ms(500);

	while (!stopping) {
		run_direct_diag(&frame);
		run_rgb_diag(&frame);
	}

	backlight_set(1);
	status_led_set(0);
	unlink("/run/sf2000-screen-own-backlight");
	log_line("sf2000-screen: stopped\n");
	return 0;
}
