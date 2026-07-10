// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/cachectl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

extern char **environ;

static void progress_mark(const char *name, uint32_t kind, uint32_t value);

#define WIDTH 320u
#define HEIGHT 240u
#define PITCH (WIDTH * 2u)
#define FRAME_BYTES (PITCH * HEIGHT)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define CONSOLE_COLS 52u
#define CONSOLE_ROWS 28u
#define CONSOLE_LINE_LEN (CONSOLE_COLS + 1u)
#define CONSOLE_SCROLLBACK_LINES 512u

#define PROGRESS_PHYS 0x013f0000u
#define PROGRESS_MAGIC 0x52504653u
#define PROGRESS_VERSION 1u
#define PROGRESS_ENTRIES 1024u
#define PROGRESS_NAME_LEN 32u
#define SCREEN_TAG 0x0239u

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
#define GMA_DMBA_ALT 0x384u
#define GMA_K 0x308u
#define GMA_MASK 0x350u
#define GMA_LINEBUF 0x3b8u
#define GMA_DOORBELL_PRIMARY 0x01u
#define GMA_DOORBELL_ALT 0x02u

#define SYSIO_PHYS 0x18800000u
#define SYSIO_SIZE 0x1000u
#define LVDS_RGB_PHYS 0x18860000u
#define KSEG1ADDR(x) ((volatile uint8_t *)((uintptr_t)(x) | 0xa0000000u))
#define SYS_CLOCK_GATE0_OFF 0x060u
#define SYS_CLOCK_GATE1_OFF 0x064u
#define SYS_CLK_CTR_OFF 0x078u
#define SYS_SFCLK_OFF 0x07cu
#define SYS_LCD_SETUP_OFF 0x094u
#define SYS_IO_VOLTAGE_OFF 0x184u
#define SYS_LVDS_PHY_OFF 0x440u
#define SYS_VIDEO_SRC0_OFF 0x444u
#define SYS_VIDEO_SRC1_OFF 0x448u
#define SYS_VIDEO_SRC2_OFF 0x44cu
#define VOU_HD_MODE 0x000u
#define VOU_HD_TIMING0 0x004u
#define VOU_HD_TIMING1 0x008u
#define VOU_HD_TIMING2 0x00cu
#define VOU_HD_TIMING3 0x080u
#define VOU_HD_CTRL 0x084u
#define VOU_HD_TIMING4 0x088u
#define VOU_HD_TIMING5 0x08cu
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
#define WDT_CONF_OFF 0x04u
#define WDT_PET_COUNT 0xffc61075u
#define WDT_RESTART_COUNT 0xfffff000u
#define WDT_RESTART_CONF 0x67u

#define MMC_PHYS 0x1884c000u
#define MMC_CTRL 0x000u
#define MMC_PWREN 0x004u
#define MMC_CLKDIV 0x008u
#define MMC_CLKSRC 0x00cu
#define MMC_CLKENA 0x010u
#define MMC_TMOUT 0x014u
#define MMC_CTYPE 0x018u
#define MMC_BLKSIZ 0x01cu
#define MMC_BYTCNT 0x020u
#define MMC_INTMASK 0x024u
#define MMC_CMDARG 0x028u
#define MMC_CMD 0x02cu
#define MMC_RESP0 0x030u
#define MMC_MINTSTS 0x040u
#define MMC_RINTSTS 0x044u
#define MMC_STATUS 0x048u
#define MMC_FIFOTH 0x04cu
#define MMC_CDETECT 0x050u
#define MMC_WRTPRT 0x054u
#define MMC_GPIO 0x058u
#define MMC_TCBCNT 0x05cu
#define MMC_TBBCNT 0x060u
#define MMC_DEBNCE 0x064u
#define MMC_USRID 0x068u
#define MMC_VERID 0x06cu
#define MMC_HCON 0x070u
#define MMC_UHS_REG 0x074u
#define MMC_RST_N 0x078u
#define MMC_BMOD 0x080u
#define MMC_PLDMND 0x084u
#define MMC_DBADDR 0x088u
#define MMC_IDSTS 0x08cu
#define MMC_IDINTEN 0x090u
#define MMC_DSCADDR 0x094u
#define MMC_BUFADDR 0x098u
#define MMC_CARDTHRCTL 0x100u
#define HC15_CMDCTL 0x00u
#define HC15_CMDSTS 0x01u
#define HC15_CMDIDX 0x02u
#define HC15_CLKDIV_LO 0x03u
#define HC15_CMDARG 0x04u
#define HC15_BLKSIZ 0x08u
#define HC15_BLKCNT_LO 0x0au
#define HC15_BUS 0x0bu
#define HC15_FIFO 0x0cu
#define HC15_PIO 0x0eu
#define HC15_RESP0 0x10u
#define HC15_IRQSTS 0x30u
#define HC15_CLKDIV_HI 0x34u
#define HC15_BLKCNT_HI 0x36u
#define HC15_TIMING 0x50u

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
#define ST7789_RAMCTRL 0xb0u
#define ST7789_RGBCTRL 0xb1u

#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 0x220
#define BTN_DPAD_DOWN 0x221
#define BTN_DPAD_LEFT 0x222
#define BTN_DPAD_RIGHT 0x223
#endif

#define CONSOLE_INPUT_FDS 4u
#define CONSOLE_POLL_MS 50u
#define CONSOLE_IDLE_REDRAW_TICKS 40u
#define CONSOLE_REPEAT_DELAY_TICKS 5u
#define CONSOLE_REPEAT_INTERVAL_TICKS 1u
#define CONSOLE_INPUT_REOPEN_TICKS 100u
#define CONSOLE_KMSG_READS_PER_FRAME 8u
#define CONSOLE_BTN_UP 0x01u
#define CONSOLE_BTN_DOWN 0x02u
#define CONSOLE_BTN_LEFT 0x04u
#define CONSOLE_BTN_RIGHT 0x08u
#define CONSOLE_BTN_SELECT 0x10u

#ifndef LINUX_REBOOT_CMD_RESTART
#define LINUX_REBOOT_CMD_RESTART 0x01234567
#endif

static volatile uint8_t *gma_ram_mapping;
static volatile uint8_t *gma_mapping;
static volatile uint8_t *sysio_mapping;
#define gma_ram (gma_ram_mapping ? gma_ram_mapping : KSEG1ADDR(GMA_RAM_PHYS))
#define gma (gma_mapping ? gma_mapping : KSEG1ADDR(GMA_MMIO_PHYS))
#define sysio (sysio_mapping ? sysio_mapping : KSEG1ADDR(SYSIO_PHYS))
static volatile int stopping;
static int panel_enabled = 1;
static int led_enabled = 1;
static int slow_panel_bus = 1;
static char console_lines[CONSOLE_SCROLLBACK_LINES][CONSOLE_LINE_LEN];
static unsigned console_line_count;
static unsigned console_line_start;
static unsigned console_view_offset;

static uint16_t *framebuffer(void);
static void panel_restart_frame(void);

struct glyph {
	char ch;
	uint8_t rows[7];
};

struct progress_entry {
	uint32_t seq;
	uint32_t kind;
	uint32_t value;
	uint32_t name_ptr;
	char name[PROGRESS_NAME_LEN];
};

struct progress_log {
	uint32_t magic;
	uint32_t version;
	uint32_t seq;
	uint32_t write_index;
	uint32_t wrapped;
	uint32_t reserved[3];
	struct progress_entry entries[PROGRESS_ENTRIES];
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
	0, 2, ST7789_TEON, 0x00,
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

struct gma_scanout_profile {
	const char *name;
	uint32_t linebuf;
	unsigned doorbells;
	int sdk_enhance;
};

struct panel_rgb_mode_profile {
	const char *name;
	uint8_t use_b0;
	uint8_t b0[2];
	uint8_t b1[3];
	uint8_t colmod;
};

static const struct panel_variant panel_variants[] = {
	{ "SF2000", st7789_sf2000_init, { 0x70, 0x00, 0x80, 0xc0 } },
	{ "X60 OLD", st7789_x60_old_init, { 0xa8, 0x68, 0x28, 0xe8 } },
	{ "X60 NEW", st7789_x60_new_init, { 0xe8, 0x88, 0x28, 0x68 } },
	{ "Q19", st7789_q19_init, { 0x28, 0x68, 0xa8, 0xe8 } },
	{ "DY12", st7789_dy12_init, { 0x68, 0x28, 0xa8, 0xe8 } },
	{ "009306", st7789_009306_init, { 0x28, 0x68, 0xa8, 0xe8 } },
};

static const struct gma_scanout_profile gma_scanout_profiles[] = {
	{ "LB0A PRIMARY BYPASS", 0x0au, GMA_DOORBELL_PRIMARY, 0 },
	{ "LB12 ALT BYPASS", 0x12u, GMA_DOORBELL_ALT, 0 },
	{ "LB02 BOTH BYPASS", 0x02u, GMA_DOORBELL_PRIMARY | GMA_DOORBELL_ALT, 0 },
	{ "LB0A PRIMARY SDK", 0x0au, GMA_DOORBELL_PRIMARY, 1 },
	{ "LB12 ALT SDK", 0x12u, GMA_DOORBELL_ALT, 1 },
	{ "LB02 BOTH SDK", 0x02u, GMA_DOORBELL_PRIMARY | GMA_DOORBELL_ALT, 1 },
};

static const struct panel_rgb_mode_profile panel_rgb_mode_profiles[] = {
	{ "SF2000-RTOS-565", 0, { 0x00, 0x00 }, { 0x40, 0x04, 0x14 }, 0x55 },
	{ "HCLINUX-RAMCTRL-666", 1, { 0x11, 0xf0 }, { 0x42, 0x08, 0x14 }, 0x66 },
	{ "RAMCTRL-SF2000-565", 1, { 0x11, 0xf0 }, { 0x40, 0x04, 0x14 }, 0x55 },
	{ "RAMCTRL-HCLINUX-565", 1, { 0x11, 0xf0 }, { 0x42, 0x08, 0x14 }, 0x55 },
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

static void backlight_set(int on);

static void watchdog_pet(void)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);

	*(volatile uint32_t *)(wdt + WDT_REG_OFF + WDT_COUNT_OFF) =
		WDT_PET_COUNT;
}

static void watchdog_restart_now(void)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);

	progress_mark("diag-watchdog-now", 0x30u, SCREEN_TAG);
	if (sysio)
		backlight_set(1);
	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) = 0;
	*(volatile uint32_t *)(wdt + WDT_REG_OFF + WDT_COUNT_OFF) =
		WDT_RESTART_COUNT;
	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) =
		WDT_RESTART_CONF;
	for (;;)
		__asm__ volatile ("" ::: "memory");
}

static void progress_copy_name(volatile char *dst, const char *src)
{
	unsigned i;

	for (i = 0; i < PROGRESS_NAME_LEN - 1u && src[i]; i++)
		dst[i] = src[i];
	for (; i < PROGRESS_NAME_LEN; i++)
		dst[i] = 0;
}

static void progress_mark(const char *name, uint32_t kind, uint32_t value)
{
	volatile struct progress_log *log =
		(volatile struct progress_log *)(uintptr_t)KSEG1ADDR(PROGRESS_PHYS);
	volatile struct progress_entry *entry;
	uint32_t index;
	uint32_t seq;

	if (log->magic != PROGRESS_MAGIC || log->version != PROGRESS_VERSION) {
		memset((void *)log, 0, sizeof(*log));
		log->magic = PROGRESS_MAGIC;
		log->version = PROGRESS_VERSION;
	}

	seq = log->seq + 1u;
	index = log->write_index;
	if (index >= PROGRESS_ENTRIES) {
		index = 0;
		log->wrapped = 1;
	}

	entry = &log->entries[index];
	entry->seq = seq;
	entry->kind = kind;
	entry->value = value;
	entry->name_ptr = (uint32_t)(uintptr_t)name;
	progress_copy_name(entry->name, name);

	log->write_index = index + 1u;
	log->seq = seq;
}

static void progress_mark_text(const char *prefix, const char *text)
{
	char name[PROGRESS_NAME_LEN];
	unsigned byte;
	unsigned word_index = 0;
	uint32_t word = 0;

	for (byte = 0; text[byte] && byte < 192u; byte++) {
		word |= (uint32_t)(uint8_t)text[byte] << ((byte & 3u) * 8u);
		if ((byte & 3u) == 3u) {
			snprintf(name, sizeof(name), "%s-w%u", prefix, word_index++);
			progress_mark(name, 0x20u, word);
			word = 0;
		}
	}
	if (byte & 3u) {
		snprintf(name, sizeof(name), "%s-w%u", prefix, word_index);
		progress_mark(name, 0x20u, word);
	}
}

static void progress_mark_file_head(const char *prefix, const char *path)
{
	char buf[192];
	ssize_t got;
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0) {
		progress_mark(prefix, 0x21u, (uint32_t)errno);
		return;
	}
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got < 0) {
		progress_mark(prefix, 0x22u, (uint32_t)errno);
		return;
	}
	buf[got] = 0;
	progress_mark(prefix, 0x23u, (uint32_t)got);
	progress_mark_text(prefix, buf);
}

static const char *text_find(const char *haystack, const char *needle)
{
	size_t len = strlen(needle);

	if (!len)
		return haystack;
	while (*haystack) {
		if (strncmp(haystack, needle, len) == 0)
			return haystack;
		haystack++;
	}
	return NULL;
}

static void progress_mark_file_section(const char *prefix, const char *path,
	const char *needle)
{
	char buf[1024];
	const char *section;
	ssize_t got;
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0) {
		progress_mark(prefix, 0x26u, (uint32_t)errno);
		return;
	}
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got < 0) {
		progress_mark(prefix, 0x27u, (uint32_t)errno);
		return;
	}
	buf[got] = 0;
	section = text_find(buf, needle);
	progress_mark(prefix, 0x28u, (uint32_t)got);
	progress_mark("stor-section-off", 0x28u,
		section ? (uint32_t)(section - buf) : 0xffffffffu);
	if (section)
		progress_mark_text(prefix, section);
}

static void progress_mark_dir_count(const char *name, const char *path)
{
	char text[192];
	DIR *dir = opendir(path);
	struct dirent *de;
	unsigned count = 0;
	unsigned pos = 0;
	uint32_t hash = 2166136261u;

	if (!dir) {
		progress_mark(name, 0x24u, (uint32_t)errno);
		return;
	}

	text[0] = 0;
	while ((de = readdir(dir)) != NULL) {
		const char *s = de->d_name;

		if (strcmp(s, ".") == 0 || strcmp(s, "..") == 0)
			continue;
		count++;
		while (*s) {
			hash ^= (uint8_t)*s;
			hash *= 16777619u;
			if (pos + 1u < sizeof(text))
				text[pos++] = *s;
			s++;
		}
		hash ^= (uint8_t)' ';
		hash *= 16777619u;
		if (pos + 1u < sizeof(text))
			text[pos++] = ' ';
	}
	closedir(dir);
	text[pos] = 0;
	progress_mark(name, 0x25u, count);
	progress_mark("diag-dir-hash", 0x25u, hash);
	progress_mark_text(name, text);
}

static uint32_t direct_read32(uint32_t phys)
{
	return *(volatile uint32_t *)KSEG1ADDR(phys);
}

static uint8_t direct_read8(uint32_t phys)
{
	return *(volatile uint8_t *)KSEG1ADDR(phys);
}

static void progress_mark_mmc_snapshot(const char *suffix)
{
	progress_mark("diag-hc15-cmdctl", 0x32u, direct_read8(MMC_PHYS + HC15_CMDCTL));
	progress_mark("diag-hc15-cmdsts", 0x32u, direct_read8(MMC_PHYS + HC15_CMDSTS));
	progress_mark("diag-hc15-cmdidx", 0x32u, direct_read8(MMC_PHYS + HC15_CMDIDX));
	progress_mark("diag-hc15-clkdiv-lo", 0x32u, direct_read8(MMC_PHYS + HC15_CLKDIV_LO));
	progress_mark("diag-hc15-clkdiv-hi", 0x32u, direct_read8(MMC_PHYS + HC15_CLKDIV_HI));
	progress_mark("diag-hc15-cmdarg", 0x32u, direct_read32(MMC_PHYS + HC15_CMDARG));
	progress_mark("diag-hc15-blksiz", 0x32u, direct_read8(MMC_PHYS + HC15_BLKSIZ));
	progress_mark("diag-hc15-blkcnt-lo", 0x32u, direct_read8(MMC_PHYS + HC15_BLKCNT_LO));
	progress_mark("diag-hc15-blkcnt-hi", 0x32u, direct_read8(MMC_PHYS + HC15_BLKCNT_HI));
	progress_mark("diag-hc15-bus", 0x32u, direct_read8(MMC_PHYS + HC15_BUS));
	progress_mark("diag-hc15-pio", 0x32u, direct_read32(MMC_PHYS + HC15_PIO));
	progress_mark("diag-hc15-resp0", 0x32u, direct_read32(MMC_PHYS + HC15_RESP0));
	progress_mark("diag-hc15-irqsts", 0x32u, direct_read8(MMC_PHYS + HC15_IRQSTS));
	progress_mark("diag-hc15-timing", 0x32u, direct_read8(MMC_PHYS + HC15_TIMING));
	progress_mark(suffix, 0x32u, SCREEN_TAG);
}

static void progress_mark_reset_snapshot_full(void)
{
	uint32_t pins = 0;
	unsigned i;

	progress_mark("diag-reset-begin", 0x30u, SCREEN_TAG);
	progress_mark_mmc_snapshot("diag-mmc-early-done");

	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	mkdir("/dev", 0755);
	errno = 0;
	(void)mount("proc", "/proc", "proc", 0, "");
	progress_mark("diag-mount-proc", 0x30u, (uint32_t)errno);
	errno = 0;
	(void)mount("sysfs", "/sys", "sysfs", 0, "");
	progress_mark("diag-mount-sys", 0x30u, (uint32_t)errno);
	errno = 0;
	(void)mount("devtmpfs", "/dev", "devtmpfs", 0, "");
	progress_mark("diag-mount-dev", 0x30u, (uint32_t)errno);
	errno = 0;

	progress_mark_file_head("diag-proc-part", "/proc/partitions");
	progress_mark_file_head("diag-proc-dev", "/proc/devices");
	progress_mark_file_head("diag-proc-int", "/proc/interrupts");
	progress_mark_file_head("diag-proc-mount", "/proc/mounts");
	progress_mark_dir_count("diag-sys-block", "/sys/block");
	progress_mark_dir_count("diag-mmc-host", "/sys/class/mmc_host");
	progress_mark_dir_count("diag-platform", "/sys/bus/platform/devices");
	progress_mark_dir_count("diag-usb-dev", "/sys/bus/usb/devices");
	progress_mark_dir_count("diag-dev-input", "/dev/input");
	progress_mark_dir_count("diag-dev-root", "/dev");

	progress_mark("diag-wdt-count", 0x31u,
		direct_read32(WDT_BASE_PHYS + WDT_REG_OFF + WDT_COUNT_OFF));
	progress_mark("diag-wdt-conf", 0x31u,
		direct_read8(WDT_BASE_PHYS + WDT_REG_OFF + WDT_CONF_OFF));
	progress_mark("diag-sfclk", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_SFCLK_OFF));
	progress_mark("diag-gate0", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_CLOCK_GATE0_OFF));
	progress_mark("diag-gate1", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_CLOCK_GATE1_OFF));
	progress_mark("diag-iovolt", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_IO_VOLTAGE_OFF));
	for (i = 16u; i <= 22u; i++)
		pins |= (uint32_t)(direct_read8(SYSIO_PHYS + PINMUX_L_OFF + i) & 0xfu)
			<< ((i - 16u) * 4u);
	progress_mark("diag-pin-l16-22", 0x31u, pins);

	progress_mark_mmc_snapshot("diag-mmc-late-done");

	progress_mark("diag-reset-done", 0x30u, SCREEN_TAG);
}

static void progress_mark_reset_snapshot_fast(void)
{
	uint32_t pins = 0;
	unsigned i;

	progress_mark("diag-fast-reset-begin", 0x30u, SCREEN_TAG);
	progress_mark_mmc_snapshot("diag-fast-mmc-done");
	progress_mark("diag-fast-wdt-count", 0x31u,
		direct_read32(WDT_BASE_PHYS + WDT_REG_OFF + WDT_COUNT_OFF));
	progress_mark("diag-fast-wdt-conf", 0x31u,
		direct_read8(WDT_BASE_PHYS + WDT_REG_OFF + WDT_CONF_OFF));
	progress_mark("diag-fast-sfclk", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_SFCLK_OFF));
	progress_mark("diag-fast-gate0", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_CLOCK_GATE0_OFF));
	progress_mark("diag-fast-iovolt", 0x31u,
		direct_read32(SYSIO_PHYS + SYS_IO_VOLTAGE_OFF));
	for (i = 16u; i <= 22u; i++)
		pins |= (uint32_t)(direct_read8(SYSIO_PHYS + PINMUX_L_OFF + i) & 0xfu)
			<< ((i - 16u) * 4u);
	progress_mark("diag-fast-pin-l16-22", 0x31u, pins);
	progress_mark("diag-fast-reset-done", 0x30u, SCREEN_TAG);
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
	gma_ram_mapping = KSEG1ADDR(GMA_RAM_PHYS);
	gma_mapping = KSEG1ADDR(GMA_MMIO_PHYS);
	sysio_mapping = KSEG1ADDR(SYSIO_PHYS);
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

#if 0
static uint32_t storage_hash_name(const char *text)
{
	uint32_t hash = 2166136261u;

	while (*text) {
		hash ^= (uint8_t)*text++;
		hash *= 16777619u;
	}
	return hash;
}

static void storage_log_msgf(const char *fmt, ...)
{
	char line[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	log_line(line);
	append_file_log(line);
}

static void storage_mount_once(const char *src, const char *target,
	const char *type, const char *ret_name, const char *err_name)
{
	int ret;

	errno = 0;
	ret = mount(src, target, type, 0, "");
	progress_mark(ret_name, 0x3au, (uint32_t)ret);
	progress_mark(err_name, 0x3au, (uint32_t)errno);
}

static void storage_ensure_mounts(void)
{
	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	mkdir("/dev", 0755);
	mkdir("/dev/pts", 0755);
	mkdir("/run", 0755);
	mkdir("/mnt", 0755);
	mkdir("/mnt/sd", 0755);
	storage_mount_once("proc", "/proc", "proc",
		"stor-mount-proc-ret", "stor-mount-proc-err");
	storage_mount_once("sysfs", "/sys", "sysfs",
		"stor-mount-sys-ret", "stor-mount-sys-err");
	storage_mount_once("devtmpfs", "/dev", "devtmpfs",
		"stor-mount-dev-ret", "stor-mount-dev-err");
	storage_mount_once("devpts", "/dev/pts", "devpts",
		"stor-mount-pts-ret", "stor-mount-pts-err");
}

static void storage_mknod_block(const char *path, unsigned minor,
	const char *ret_name, const char *err_name)
{
	int ret;

	errno = 0;
	ret = mknod(path, S_IFBLK | 0660, makedev(179, minor));
	progress_mark(ret_name, 0x3du, (uint32_t)ret);
	progress_mark(err_name, 0x3du, (uint32_t)errno);
}

static void storage_ensure_block_nodes(void)
{
	storage_mknod_block("/dev/mmcblk0", 0,
		"stor-mknod-mmc0-ret", "stor-mknod-mmc0-err");
	storage_mknod_block("/dev/mmcblk0p1", 1,
		"stor-mknod-mmc0p1-ret", "stor-mknod-mmc0p1-err");
	storage_mknod_block("/dev/mmcblk0p2", 2,
		"stor-mknod-mmc0p2-ret", "stor-mknod-mmc0p2-err");
}

static int storage_read_devt(const char *path, unsigned *major_out,
	unsigned *minor_out)
{
	char buf[32];
	unsigned major = 0;
	unsigned minor = 0;
	ssize_t got;
	int fd;
	unsigned i = 0;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		progress_mark("stor-devt-open-fail", 0x3du, (uint32_t)errno);
		return -1;
	}
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got <= 0) {
		progress_mark("stor-devt-read-fail", 0x3du,
			got < 0 ? (uint32_t)errno : 0);
		return -1;
	}
	buf[got] = 0;
	while (buf[i] >= '0' && buf[i] <= '9') {
		major = major * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	if (buf[i] != ':') {
		progress_mark("stor-devt-bad", 0x3du, storage_hash_name(buf));
		return -1;
	}
	i++;
	while (buf[i] >= '0' && buf[i] <= '9') {
		minor = minor * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	*major_out = major;
	*minor_out = minor;
	progress_mark("stor-devt-major", 0x3du, major);
	progress_mark("stor-devt-minor", 0x3du, minor);
	return 0;
}

static void storage_mknod_devt(const char *sysdev, const char *node)
{
	unsigned major;
	unsigned minor;
	int ret;

	progress_mark("stor-devt-path", 0x3du, storage_hash_name(sysdev));
	if (storage_read_devt(sysdev, &major, &minor) != 0)
		return;
	errno = 0;
	unlink(node);
	ret = mknod(node, S_IFBLK | 0660, makedev(major, minor));
	progress_mark("stor-devt-mknod-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-devt-mknod-err", 0x3du, (uint32_t)errno);
}

static void storage_stat_node(const char *path)
{
	struct stat st;

	progress_mark("stor-stat-path", 0x3du, storage_hash_name(path));
	errno = 0;
	if (stat(path, &st) != 0) {
		progress_mark("stor-stat-fail", 0x3du, (uint32_t)errno);
		return;
	}
	progress_mark("stor-stat-mode", 0x3du, (uint32_t)st.st_mode);
	progress_mark("stor-stat-major", 0x3du, (uint32_t)major(st.st_rdev));
	progress_mark("stor-stat-minor", 0x3du, (uint32_t)minor(st.st_rdev));
}

static void storage_log_block_head(const char *dev)
{
	unsigned char buf[512];
	ssize_t got;
	int fd;

	progress_mark("stor-blk-open-begin", 0x3du, storage_hash_name(dev));
	fd = open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		storage_log_msgf("sf2000_storage_inline: block open failed %s errno=%d\n",
			dev, errno);
		progress_mark("stor-blk-open-fail", 0x3du, (uint32_t)errno);
		return;
	}
	progress_mark("stor-blk-open-ok", 0x3du, storage_hash_name(dev));
	got = read(fd, buf, sizeof(buf));
	progress_mark("stor-blk-read-ret", 0x3du, (uint32_t)got);
	if (got >= 4)
		progress_mark("stor-blk-head0", 0x3du,
			(uint32_t)buf[0] |
			((uint32_t)buf[1] << 8) |
			((uint32_t)buf[2] << 16) |
			((uint32_t)buf[3] << 24));
	if (got >= 512)
		progress_mark("stor-blk-sig", 0x3du,
			(uint32_t)buf[510] | ((uint32_t)buf[511] << 8));
	close(fd);
}

static void storage_set_readahead_zero(const char *dev)
{
	unsigned long value = 0;
	int fd;
	int ret;

	progress_mark("stor-ra-path", 0x3du, storage_hash_name(dev));
	fd = open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		progress_mark("stor-ra-open-fail", 0x3du, (uint32_t)errno);
		return;
	}
	errno = 0;
	ret = ioctl(fd, BLKRASET, value);
	progress_mark("stor-ra-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-ra-err", 0x3du, (uint32_t)errno);
	close(fd);
}

static int storage_try_mount_write_type(const char *dev, const char *fstype)
{
	ssize_t wrote = -1;
	int saved_errno = 0;
	int fd;

	progress_mark("stor-try", 0x3du, storage_hash_name(dev));
	storage_log_msgf("sf2000_storage_inline: mount try %s type=%s\n",
		dev, fstype);
	progress_mark("stor-mount-type", 0x3du, storage_hash_name(fstype));
	errno = 0;
	if (mount(dev, "/mnt/sd", fstype, MS_NOATIME, "") != 0) {
		storage_log_msgf("sf2000_storage_inline: mount failed %s type=%s errno=%d\n",
			dev, fstype, errno);
		progress_mark("stor-mount-fail", 0x3du, (uint32_t)errno);
		return -1;
	}
	storage_log_msgf("sf2000_storage_inline: mount ok %s type=%s\n",
		dev, fstype);
	progress_mark("stor-mount-ok", 0x3du, storage_hash_name(dev));
	fd = open("/mnt/sd/sf2000-linux-rw-0239.txt",
		O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		storage_log_msgf("sf2000_storage_inline: write open failed errno=%d\n",
			errno);
		progress_mark("stor-open-fail", 0x3du, (uint32_t)errno);
	} else {
		const char msg[] = "sf2000 linux sd write test 0239 inline\n";

		errno = 0;
		wrote = write(fd, msg, sizeof(msg) - 1u);
		saved_errno = errno;
		progress_mark("stor-before-fsync", 0x3du, (uint32_t)wrote);
		if (wrote == (ssize_t)(sizeof(msg) - 1u)) {
			errno = 0;
			progress_mark("stor-fsync-ret", 0x3du,
				(uint32_t)fsync(fd));
			progress_mark("stor-fsync-err", 0x3du, (uint32_t)errno);
		}
		close(fd);
		storage_log_msgf("sf2000_storage_inline: write ret=%d errno=%d\n",
			(int)wrote, saved_errno);
		progress_mark("stor-write-ret", 0x3du, (uint32_t)wrote);
		progress_mark("stor-write-errno", 0x3du, (uint32_t)saved_errno);
	}
	progress_mark("stor-before-sync", 0x3du, (uint32_t)wrote);
	sync();
	progress_mark("stor-after-sync", 0x3du, (uint32_t)wrote);
	if (umount("/mnt/sd") != 0) {
		storage_log_msgf("sf2000_storage_inline: umount failed errno=%d\n",
			errno);
		progress_mark("stor-umount-fail", 0x3du, (uint32_t)errno);
	} else {
		storage_log_msgf("sf2000_storage_inline: umount ok\n");
		progress_mark("stor-umount-ok", 0x3du, 0);
	}
	return wrote > 0 ? 0 : -1;
}

static int storage_try_mount_write(const char *dev)
{
	if (storage_try_mount_write_type(dev, "vfat") == 0)
		return 0;
	if (storage_try_mount_write_type(dev, "msdos") == 0)
		return 0;
	return -1;
}

static void run_inline_storage_probe_once(const char *source)
{
	static int storage_started;
	int mounted = -1;

	if (storage_started)
		return;
	storage_started = 1;
	progress_mark("stor-start", 0x3au, SCREEN_TAG);
	progress_mark_text("diag-storage-src", source);
	publish_marker("/run/sf2000-storage-started", source);
	storage_log_msgf("sf2000_storage_inline: start %s", source);
	storage_ensure_mounts();
	storage_ensure_block_nodes();
	storage_mknod_devt("/sys/block/mmcblk0/dev", "/dev/mmcblk0sys");
	storage_mknod_devt("/sys/class/block/mmcblk0/dev", "/dev/mmcblk0class");
	storage_mknod_devt("/sys/class/block/mmcblk0p1/dev", "/dev/mmcblk0p1sys");
	storage_mknod_devt("/sys/class/block/mmcblk0p2/dev", "/dev/mmcblk0p2sys");
	progress_mark_dir_count("stor-sys-block", "/sys/block");
	progress_mark_dir_count("stor-class-block", "/sys/class/block");
	progress_mark_dir_count("stor-dev-block", "/sys/dev/block");
	progress_mark_dir_count("stor-mmc-host", "/sys/class/mmc_host");
	progress_mark_dir_count("stor-platform", "/sys/bus/platform/devices");
	progress_mark_dir_count("stor-dev-root", "/dev");
	progress_mark_file_head("stor-sys-mmc0-dev", "/sys/block/mmcblk0/dev");
	progress_mark_file_head("stor-sys-mmc0-size", "/sys/block/mmcblk0/size");
	progress_mark_file_head("stor-cls-mmc0-dev", "/sys/class/block/mmcblk0/dev");
	progress_mark_file_head("stor-cls-p1-dev", "/sys/class/block/mmcblk0p1/dev");
	progress_mark_file_head("stor-cls-p2-dev", "/sys/class/block/mmcblk0p2/dev");
	progress_mark_file_head("stor-proc-part", "/proc/partitions");
	progress_mark_file_head("stor-proc-dev", "/proc/devices");
	progress_mark_file_section("stor-proc-blk", "/proc/devices",
		"Block devices:");
	progress_mark_file_head("stor-proc-int", "/proc/interrupts");
	storage_stat_node("/dev/mmcblk0");
	storage_stat_node("/dev/mmcblk0p1");
	storage_stat_node("/dev/mmcblk0p2");
	storage_stat_node("/dev/mmcblk0sys");
	storage_stat_node("/dev/mmcblk0class");
	storage_stat_node("/dev/mmcblk0p1sys");
	storage_stat_node("/dev/mmcblk0p2sys");
	storage_set_readahead_zero("/dev/mmcblk0");
	storage_set_readahead_zero("/dev/mmcblk0p1");
	storage_set_readahead_zero("/dev/mmcblk0p2");
	storage_set_readahead_zero("/dev/mmcblk0sys");
	storage_set_readahead_zero("/dev/mmcblk0class");
	if (storage_try_mount_write("/dev/mmcblk0") == 0 ||
	    storage_try_mount_write("/dev/mmcblk0p1") == 0 ||
	    storage_try_mount_write("/dev/mmcblk0p2") == 0 ||
	    storage_try_mount_write("/dev/mmcblk0sys") == 0 ||
	    storage_try_mount_write("/dev/mmcblk0class") == 0)
		mounted = 0;
	progress_mark("stor-fast-result", 0x3au, (uint32_t)mounted);
	if (mounted == 0) {
		storage_log_msgf("sf2000_storage_inline: fast storage path ok\n");
		progress_mark("stor-done", 0x3au, SCREEN_TAG);
		return;
	}
	storage_log_block_head("/dev/mmcblk0");
	storage_log_block_head("/dev/mmcblk0p1");
	storage_log_block_head("/dev/mmcblk0p2");
	storage_log_block_head("/dev/mmcblk0sys");
	storage_log_block_head("/dev/mmcblk0class");
	storage_log_msgf("sf2000_storage_inline: done\n");
	progress_mark("stor-done", 0x3au, SCREEN_TAG);
}
#endif

static void publish_screen_ready_and_storage(const char *source)
{
	progress_mark("screen-publish-ready", 0x3fu, SCREEN_TAG);
	publish_marker("/run/sf2000-screen-ready", "ready\n");
	publish_marker("/run/sf2000-screen-source", source);
	progress_mark("screen-ready-done", 0x3fu, SCREEN_TAG);
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

static void panel_rgb_clock_enable(void)
{
	uint32_t gate1;

	gate1 = mmio_read32(sysio, SYS_CLOCK_GATE1_OFF);
	gate1 &= ~(1u << 0);  /* VOU_HD_CLK */
	gate1 &= ~(1u << 2);  /* DE_CLK */
	gate1 &= ~(1u << 18); /* VOU_HD_EXT_CLK */
	mmio_write32(sysio, SYS_CLOCK_GATE1_OFF, gate1);
}

static void panel_rgb_output_mux_enable(void)
{
	volatile uint8_t *lvds = KSEG1ADDR(LVDS_RGB_PHYS);
	uint32_t strap;
	uint32_t value;
	unsigned ch;

	panel_rgb_clock_enable();

	strap = mmio_read32(sysio, SYS_LCD_SETUP_OFF);
	strap &= 0x0fffffffu;
	strap |= 2u << 28; /* LVDS_IO_TTL_SEL_RGB565 */
	strap |= 1u << 16;
	mmio_write32(sysio, SYS_LCD_SETUP_OFF, strap);

	value = mmio_read32(sysio, SYS_LVDS_PHY_OFF);
	value |= 0x3u;  /* enable RGB/LVDS channels 0 and 1 */
	value &= ~(1u << 2); /* power on */
	mmio_write32(sysio, SYS_LVDS_PHY_OFF, value);

	value = mmio_read32(sysio, SYS_VIDEO_SRC0_OFF);
	value &= 0xffffffcfu; /* RGB source: FXDE */
	mmio_write32(sysio, SYS_VIDEO_SRC0_OFF, value);

	value = mmio_read32(sysio, SYS_VIDEO_SRC1_OFF);
	value &= 0xfff3ffffu; /* RGB source mirror: FXDE */
	mmio_write32(sysio, SYS_VIDEO_SRC1_OFF, value);

	value = mmio_read32(sysio, SYS_VIDEO_SRC2_OFF);
	value &= 0xf000f888u; /* RGB565 lanes from FXDE, rgb order */
	mmio_write32(sysio, SYS_VIDEO_SRC2_OFF, value);

	for (ch = 0; ch < 2; ch++) {
		uint32_t base = 0x100u + ch * 0x40u;

		mmio_write8(lvds, base + 0x00u, 0x7fu);
		mmio_write8(lvds, base + 0x01u, 0x00u);
		mmio_write8(lvds, base + 0x02u, 0x00u);
		mmio_write8(lvds, base + 0x04u, 0x3fu);
		mmio_write8(lvds, base + 0x05u, 0x3fu);
	}
	mmio_write32(lvds, 0x04u, 1u);
}

static void runtime_watchdog_arm(void)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);

	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) = 0;
	*(volatile uint32_t *)(wdt + WDT_REG_OFF + WDT_COUNT_OFF) = 0xffe64035u;
	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) = 0x26;
}

static void runtime_watchdog_disable(void)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);

	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) = 0;
}

static void panel_vou_rgb_enable(void)
{
	mmio_write32(gma, VOU_HD_MODE, 0x00000015u);
	mmio_write32(gma, VOU_HD_TIMING0, 0x00122914u);
	mmio_write32(gma, VOU_HD_TIMING1, 0x00650000u);
	mmio_write32(gma, VOU_HD_TIMING2, 0x01300378u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00020702u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00127002u);
	mmio_write32(gma, VOU_HD_TIMING4, 0x00108080u);
	mmio_write32(gma, VOU_HD_TIMING5, 0x00000004u);
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
	/*
	 * Leave L25 untouched while debugging keypad input. The SF2000 HCRToS
	 * frontend uses it as a status LED, but it is not needed for scrolling.
	 */
	(void)on;
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
	progress_mark("screen-bl-off-1", 0x3fu, SCREEN_TAG);
	backlight_set(0);
	sleep_ms(80);
	log_line("sf2000-screen: diag step 2 backlight on\n");
	progress_mark("screen-bl-on-1", 0x3fu, SCREEN_TAG);
	backlight_set(1);
	sleep_ms(80);
	log_line("sf2000-screen: diag step 3 backlight off\n");
	progress_mark("screen-bl-off-2", 0x3fu, SCREEN_TAG);
	backlight_set(0);
	sleep_ms(80);
	log_line("sf2000-screen: diag step 4 backlight on\n");
	progress_mark("screen-bl-on-2", 0x3fu, SCREEN_TAG);
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
	panel_lcd_setup_enable();
	panel_rgb_output_mux_enable();
	panel_vou_rgb_enable();

	mmio_write32(sysio, PINMUX_L_OFF + 0x04, 0xb6060606u);
	mmio_write32(sysio, PINMUX_L_OFF + 0x00,
		(mmio_read32(sysio, PINMUX_L_OFF + 0x00) & 0x0000ffffu) |
		0x06060000u);
	mmio_write32(sysio, PINMUX_T_OFF + 0x08,
		(mmio_read32(sysio, PINMUX_T_OFF + 0x08) & 0x000000ffu) |
		0x06060600u);
	mmio_write32(sysio, PINMUX_T_OFF + 0x0c,
		(mmio_read32(sysio, PINMUX_T_OFF + 0x0c) & 0xff000000u) |
		0x00060606u);
	mmio_write32(sysio, PINMUX_T_OFF + 0x00,
		(mmio_read32(sysio, PINMUX_T_OFF + 0x00) & 0x0000ffffu) |
		0x06060000u);
	mmio_write32(sysio, PINMUX_T_OFF + 0x04,
		(mmio_read32(sysio, PINMUX_T_OFF + 0x04) & 0xff000000u) |
		0x00060606u);
	mmio_write32(sysio, PINMUX_L_OFF + 0x08,
		(mmio_read32(sysio, PINMUX_L_OFF + 0x08) & 0xff00ffffu) |
		0x00060000u);
}

static void panel_config_outputs(void)
{
	static int logged;
	unsigned i;

	panel_lcd_setup_enable();
	for (i = 0; i < ARRAY_SIZE(panel_control_pads); i++) {
		char line[96];
		unsigned pad = panel_control_pads[i];
		uint32_t base = gpio_base_for_pad(pad);

		if (!logged) {
			snprintf(line, sizeof(line),
				"sf2000-screen: panel gpio out %u %s base=0x%03x dir=0x%08x\n",
				i, panel_control_names[i], base,
				mmio_read32(sysio, base + GPIO_DIR_OFF));
			log_line(line);
		}
		gpio_config_output(panel_control_pads[i]);
		if (!logged) {
			snprintf(line, sizeof(line),
				"sf2000-screen: panel gpio out done %u %s dir=0x%08x\n",
				i, panel_control_names[i],
				mmio_read32(sysio, base + GPIO_DIR_OFF));
			log_line(line);
		}
	}
	gpio_config_input(PINPAD_L08);
	if (!logged)
		log_line("sf2000-screen: panel gpio vsync L08 input done\n");
	logged = 1;
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

static void panel_apply_rgb_mode_profile(
	const struct panel_rgb_mode_profile *profile)
{
	if (!panel_enabled)
		return;

	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	if (profile->use_b0) {
		panel_cmd(ST7789_RAMCTRL);
		panel_data(profile->b0[0]);
		panel_data(profile->b0[1]);
	}
	panel_cmd(ST7789_RGBCTRL);
	panel_data(profile->b1[0]);
	panel_data(profile->b1[1]);
	panel_data(profile->b1[2]);
	panel_cmd(ST7789_COLMOD);
	panel_data(profile->colmod);
	panel_restart_frame();
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

static int panel_init_sf2000_original_order(void)
{
	uint32_t panel_id;
	char line[128];
	const struct panel_variant *variant = &panel_variants[0];

	if (!panel_enabled)
		return 0;

	log_line("sf2000-screen: guarded panel init begin\n");
	append_file_log("sf2000-screen: guarded panel init begin\n");
	runtime_watchdog_arm();
	panel_control_pinmux();
	gpio_config_input(PINPAD_L08);
	panel_config_outputs();
	gpio_set_pad(PINPAD_L10, 1);
	gpio_set_pad(PINPAD_T01, 1);
	gpio_set_pad(PINPAD_L07, 1);
	gpio_set_pad(PINPAD_T00, 1);
	gpio_set_pad(PINPAD_L01, 1);
	sleep_ms(120);
	panel_apply_init_sequence(st7789_sf2000_init);
	panel_cmd(ST7789_DISPON);
	panel_restart_frame();
	panel_id = panel_read_id();
	if ((panel_id & 0xffffffu) == 0x009306u) {
		log_line("sf2000-screen: guarded panel 009306 reinit\n");
		panel_reset();
		panel_config_outputs();
		gpio_set_pad(PINPAD_L10, 1);
		gpio_set_pad(PINPAD_T01, 1);
		gpio_set_pad(PINPAD_L07, 1);
		gpio_set_pad(PINPAD_T00, 1);
		panel_apply_init_sequence(st7789_009306_init);
		panel_cmd(ST7789_DISPON);
		panel_restart_frame();
	}
	snprintf(line, sizeof(line),
		"sf2000-screen: guarded panel init done id=0x%06x init=%s\n",
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

static void panel_fill_solid_direct(uint16_t color)
{
	unsigned i;

	if (!panel_enabled)
		return;

	watchdog_pet();
	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_restart_frame();
	for (i = 0; i < WIDTH * HEIGHT; i++) {
		if ((i & 0xffu) == 0)
			watchdog_pet();
		panel_data(color);
	}
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

static void draw_background_variant(unsigned variant)
{
	unsigned y;
	uint16_t top;
	uint16_t bottom;

	if (variant == 1u) {
		top = rgb565(28, 3, 3);
		bottom = rgb565(10, 2, 2);
	} else if (variant == 2u) {
		top = rgb565(3, 48, 8);
		bottom = rgb565(1, 18, 4);
	} else if (variant == 3u) {
		top = rgb565(3, 12, 30);
		bottom = rgb565(1, 5, 12);
	} else {
		top = rgb565(0, 10, 18);
		bottom = rgb565(1, 8, 5);
	}

	for (y = 0; y < HEIGHT; y++) {
		unsigned x;

		for (x = 0; x < WIDTH; x++) {
			unsigned r = 1 + (x * 10u) / WIDTH;
			unsigned g = 4 + (y * 14u) / HEIGHT;
			unsigned b = 8 + (((x ^ y) & 0x7f) * 12u) / 0x7f;

			put_pixel(x, y, rgb565(r, g, b));
		}
	}

	fill_rect(0, 0, WIDTH, 31, top);
	fill_rect(0, 209, WIDTH, 31, bottom);
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

	draw_background_variant(madctl);
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

static uint16_t solid_rgb565_color(unsigned variant)
{
	if (variant == 0u)
		return rgb565(31, 0, 0);
	if (variant == 1u)
		return rgb565(0, 63, 0);
	return rgb565(0, 0, 31);
}

static void draw_solid_rgb565_screen(unsigned variant)
{
	volatile uint16_t *fb = framebuffer();
	uint16_t color = solid_rgb565_color(variant);
	unsigned i;

	for (i = 0; i < WIDTH * HEIGHT; i++)
		fb[i] = color;
}

static void console_clear(void)
{
	unsigned row;

	for (row = 0; row < CONSOLE_SCROLLBACK_LINES; row++)
		console_lines[row][0] = 0;
	console_line_count = 0;
	console_line_start = 0;
	console_view_offset = 0;
}

static unsigned console_visible_lines(void)
{
	return console_line_count < CONSOLE_ROWS ? console_line_count :
		CONSOLE_ROWS;
}

static unsigned console_max_view_offset(void)
{
	unsigned visible = console_visible_lines();

	return console_line_count > visible ? console_line_count - visible : 0;
}

static char *console_line_at(unsigned logical)
{
	return console_lines[(console_line_start + logical) %
		CONSOLE_SCROLLBACK_LINES];
}

static void console_clamp_view(void)
{
	unsigned max_offset = console_max_view_offset();

	if (console_view_offset > max_offset)
		console_view_offset = max_offset;
}

static void console_add_line(const char *line)
{
	char *dst;
	int preserve_view = console_view_offset != 0;
	unsigned len = 0;

	if (console_line_count >= CONSOLE_SCROLLBACK_LINES) {
		dst = console_line_at(0);
		console_line_start = (console_line_start + 1u) %
			CONSOLE_SCROLLBACK_LINES;
	} else {
		dst = console_line_at(console_line_count);
		console_line_count++;
	}

	while (line[len] && line[len] != '\n' && line[len] != '\r' &&
			len < CONSOLE_COLS) {
		char ch = line[len];

		dst[len] = (ch >= 32 && ch <= 126) ? ch : ' ';
		len++;
	}
	dst[len] = 0;
	if (preserve_view)
		console_view_offset++;
	console_clamp_view();
}

static void console_add_text(const char *text, size_t len)
{
	static char partial[CONSOLE_LINE_LEN];
	static unsigned partial_len;
	size_t i;

	for (i = 0; i < len; i++) {
		char ch = text[i];

		if (ch == '\r')
			continue;
		if (ch == '\n' || partial_len >= CONSOLE_COLS) {
			partial[partial_len] = 0;
			console_add_line(partial);
			partial_len = 0;
			if (ch == '\n')
				continue;
		}
		if (ch >= 32 && ch <= 126)
			partial[partial_len++] = ch;
		else if (ch == '\t')
			partial[partial_len++] = ' ';
	}
}

static void console_add_kmsg_record(const char *record, size_t len)
{
	size_t i = 0;

	while (i < len && record[i] != ';')
		i++;
	if (i < len && record[i] == ';')
		i++;
	console_add_text(record + i, len - i);
}

static void draw_console_screen(unsigned frame)
{
	uint16_t fg = rgb565(26, 58, 26);
	uint16_t dim = rgb565(12, 32, 22);
	uint16_t bg = rgb565(0, 2, 3);
	uint16_t bar = rgb565(0, 10, 14);
	char title[48];
	unsigned visible = console_visible_lines();
	unsigned start = console_line_count > visible ?
		console_line_count - visible : 0;
	unsigned row;

	console_clamp_view();
	if (console_view_offset < start)
		start -= console_view_offset;
	else
		start = 0;

	fill_rect(0, 0, WIDTH, HEIGHT, bg);
	fill_rect(0, 0, WIDTH, 12, bar);
	snprintf(title, sizeof(title), "SF2000 LOG %06u +%03u/%03u",
		frame, console_view_offset, console_max_view_offset());
	draw_text(2, 2, title, rgb565(31, 63, 20), bar, 1);
	for (row = 0; row < visible; row++) {
		unsigned y = 16u + row * 8u;
		const char *line = console_line_at(start + row);
		uint16_t color = row + 4u >= visible ? fg : dim;

		draw_text(2, y, line, color, bg, 1);
	}
}

static int console_scroll_delta(int delta)
{
	unsigned old_offset = console_view_offset;
	unsigned max_offset;

	console_clamp_view();
	max_offset = console_max_view_offset();

	if (delta > 0) {
		unsigned step = (unsigned)delta;

		if (step > max_offset - console_view_offset)
			console_view_offset = max_offset;
		else
			console_view_offset += step;
	} else if (delta < 0) {
		unsigned step = (unsigned)(-delta);

		if (step > console_view_offset)
			console_view_offset = 0;
		else
			console_view_offset -= step;
	}

	return console_view_offset != old_offset;
}

static int console_handle_buttons(uint32_t buttons)
{
	int changed = 0;
	int page = (int)CONSOLE_ROWS - 3;

	if (buttons & CONSOLE_BTN_SELECT) {
		console_add_line("SELECT pressed: restarting");
		log_line("sf2000-screen: SELECT pressed, restarting\n");
		runtime_watchdog_arm();
		progress_mark_reset_snapshot_fast();
		console_add_line("direct watchdog reset");
		log_line("sf2000-screen: direct watchdog reset\n");
		watchdog_restart_now();
		reboot(LINUX_REBOOT_CMD_RESTART);
	}
	if (buttons & CONSOLE_BTN_UP)
		changed |= console_scroll_delta(1);
	if (buttons & CONSOLE_BTN_DOWN)
		changed |= console_scroll_delta(-1);
	if (buttons & CONSOLE_BTN_LEFT)
		changed |= console_scroll_delta(page);
	if (buttons & CONSOLE_BTN_RIGHT)
		changed |= console_scroll_delta(-page);
	return changed;
}

static uint32_t console_button_for_key(uint16_t code)
{
	if (code == BTN_DPAD_UP || code == KEY_UP)
		return CONSOLE_BTN_UP;
	if (code == BTN_DPAD_DOWN || code == KEY_DOWN)
		return CONSOLE_BTN_DOWN;
	if (code == BTN_DPAD_LEFT || code == KEY_LEFT)
		return CONSOLE_BTN_LEFT;
	if (code == BTN_DPAD_RIGHT || code == KEY_RIGHT)
		return CONSOLE_BTN_RIGHT;
	if (code == BTN_SELECT || code == KEY_BACKSPACE)
		return CONSOLE_BTN_SELECT;
	return 0;
}

static void console_input_init_fds(int fds[CONSOLE_INPUT_FDS])
{
	unsigned i;

	for (i = 0; i < CONSOLE_INPUT_FDS; i++)
		fds[i] = -1;
}

static int console_input_has_fd(const int fds[CONSOLE_INPUT_FDS])
{
	unsigned i;

	for (i = 0; i < CONSOLE_INPUT_FDS; i++) {
		if (fds[i] >= 0)
			return 1;
	}
	return 0;
}

static void console_input_close_fds(int fds[CONSOLE_INPUT_FDS])
{
	unsigned i;

	for (i = 0; i < CONSOLE_INPUT_FDS; i++) {
		if (fds[i] >= 0)
			close(fds[i]);
		fds[i] = -1;
	}
}

static void console_input_open_fds(int fds[CONSOLE_INPUT_FDS])
{
	char path[] = "/dev/input/event0";
	unsigned i;

	console_input_close_fds(fds);
	for (i = 0; i < CONSOLE_INPUT_FDS; i++) {
		path[16] = (char)('0' + i);
		fds[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	if (console_input_has_fd(fds))
		console_add_line("dpad scroll: evdev input");
	else
		console_add_line("dpad scroll: waiting for evdev");
}

static void console_poll_evdev_buttons(int fds[CONSOLE_INPUT_FDS],
	uint32_t *held_buttons)
{
	struct input_event ev;
	unsigned i;

	for (i = 0; i < CONSOLE_INPUT_FDS; i++) {
		if (fds[i] < 0)
			continue;
		while (read(fds[i], &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
			uint32_t button;

			if (ev.type != EV_KEY)
				continue;
			button = console_button_for_key(ev.code);
			if (!button)
				continue;
			if (ev.value)
				*held_buttons |= button;
			else
				*held_buttons &= ~button;
		}
	}
}

static int console_handle_held_buttons(uint32_t held_buttons)
{
	static uint32_t previous_held;
	static unsigned hold_ticks;
	uint32_t pressed = held_buttons & ~previous_held;
	int changed = 0;

	if (pressed)
		changed |= console_handle_buttons(pressed);

	if (held_buttons) {
		if (held_buttons != previous_held)
			hold_ticks = 0;
		else
			hold_ticks++;
		if (hold_ticks >= CONSOLE_REPEAT_DELAY_TICKS &&
				((hold_ticks - CONSOLE_REPEAT_DELAY_TICKS) %
				 CONSOLE_REPEAT_INTERVAL_TICKS) == 0)
			changed |= console_handle_buttons(held_buttons);
	} else {
		hold_ticks = 0;
	}
	previous_held = held_buttons;
	return changed;
}

static void write_desc32(unsigned idx, uint32_t value)
{
	((volatile uint32_t *)(gma_ram + GMA_DESC_OFF))[idx] = value;
}

static uint32_t gma_descriptor_d0(unsigned variant, uint32_t mode)
{
	uint32_t d0 = (mode << 4) | (32u << 16) | (170u << 24);

	(void)variant;
	d0 |= 1u;
	if (mode == 0x06u)
		d0 |= 1u << 8;
	return d0;
}

static void build_gma_descriptor_profile(unsigned variant, uint32_t mode,
	uint32_t pitch)
{
	unsigned i;

	for (i = 0; i < 16; i++)
		write_desc32(i, 0);

	write_desc32(0, gma_descriptor_d0(variant, mode));
	write_desc32(1, 0);
	write_desc32(2, ((WIDTH - 1u) << 16) | 0u);
	write_desc32(3, ((HEIGHT - 1u) << 16) | 0u);
	write_desc32(4, (HEIGHT << 16) | WIDTH);
	write_desc32(5, 0xffu | (pitch << 16));
	write_desc32(6, 0);
	write_desc32(7, GMA_FRAME_PHYS);
}

static void build_gma_descriptor_variant(unsigned variant)
{
	build_gma_descriptor_profile(variant, 6u, PITCH);
}

static void build_gma_descriptor(void)
{
	build_gma_descriptor_variant(0);
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

static void present_frame_profile(const struct gma_scanout_profile *profile)
{
	flush_present_memory();
	mmio_write32(gma, GMA_MASK, 1);
	mmio_write32(gma, GMA_LINEBUF, profile->linebuf);
	mmio_write32(gma, GMA_K, 0xff);
	mmio_write32(gma, GMA_CTL, mmio_read32(gma, GMA_CTL) | 1u);
	if (profile->doorbells & GMA_DOORBELL_PRIMARY)
		mmio_write32(gma, GMA_DMBA, GMA_DESC_PHYS);
	if (profile->doorbells & GMA_DOORBELL_ALT)
		mmio_write32(gma, GMA_DMBA_ALT, GMA_DESC_PHYS);
	mmio_write32(gma, GMA_MASK, 0);
	gma_set_bit(GMA_CTL, 1u << 19, !profile->sdk_enhance);
	gma_set_bit(GMA_CTL, 1u << 18, profile->sdk_enhance);
}

static void present_frame(void)
{
	present_frame_profile(&gma_scanout_profiles[0]);
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

static int cmdline_contains(const char *needle)
{
	int fd = open("/proc/cmdline", O_RDONLY | O_CLOEXEC);
	char buf[512];
	ssize_t n;

	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	return strstr(buf, needle) != NULL;
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

static void log_gma_regs(const char *name, unsigned variant, uint32_t mode,
	uint32_t pitch, const struct gma_scanout_profile *profile,
	const struct panel_rgb_mode_profile *panel_profile)
{
	char line[300];

	snprintf(line, sizeof(line),
		"sf2000-screen: %s profile=%s panel=%s variant=%u mode=0x%02x pitch=%u d0=0x%08x door=0x%x enhance=%d vou=0x%08x ctrl=0x%08x gctl=0x%08x dmba=0x%08x alt=0x%08x line=0x%08x src0=0x%08x src2=0x%08x\n",
		name, profile->name, panel_profile->name, variant, mode, pitch,
		gma_descriptor_d0(variant, mode),
		profile->doorbells, profile->sdk_enhance,
		mmio_read32(gma, VOU_HD_MODE), mmio_read32(gma, VOU_HD_CTRL),
		mmio_read32(gma, GMA_CTL), mmio_read32(gma, GMA_DMBA),
		mmio_read32(gma, GMA_DMBA_ALT),
		mmio_read32(gma, GMA_LINEBUF),
		mmio_read32(sysio, SYS_VIDEO_SRC0_OFF),
		mmio_read32(sysio, SYS_VIDEO_SRC2_OFF));
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

static int argv0_is(const char *argv0, const char *name)
{
	const char *base;

	if (!argv0 || !name)
		return 0;
	base = strrchr(argv0, '/');
	base = base ? base + 1 : argv0;
	return strcmp(base, name) == 0;
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

static void run_direct_console(unsigned *frame)
{
	char buf[768];
	int input_fds[CONSOLE_INPUT_FDS];
	int fd;
	uint32_t evdev_held = 0;
	unsigned idle = 0;
	unsigned input_retry = 0;

	log_line("sf2000-screen: direct text console begin\n");
	append_file_log("sf2000-screen: direct text console begin\n");

	panel_lcd_setup_enable();
	if (!env_is((const char *const *)environ, "SF2000_SKIP_PANEL_INIT", "1"))
		panel_init_sf2000_original_order();

	console_clear();
	console_add_line("sf2000 linux direct lcd console");
	console_add_line("reading /dev/kmsg");
	console_input_init_fds(input_fds);
	console_input_open_fds(input_fds);
	draw_console_screen(++*frame);
	panel_push_frame(0);
	log_gma_ready();

	fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		console_add_line("open /dev/kmsg failed");
		draw_console_screen(++*frame);
		panel_push_frame(0);
	}
	publish_screen_ready_and_storage("direct-console\n");

	while (!stopping) {
		ssize_t got = -1;
		int changed = 0;
		unsigned kmsg_reads = 0;

		if (fd >= 0) {
			do {
				got = read(fd, buf, sizeof(buf) - 1u);
				if (got > 0) {
					buf[got] = 0;
					console_add_kmsg_record(buf, (size_t)got);
					changed = 1;
					watchdog_pet();
					kmsg_reads++;
				}
			} while (got > 0 && kmsg_reads < CONSOLE_KMSG_READS_PER_FRAME);
		}

		if (console_input_has_fd(input_fds)) {
			console_poll_evdev_buttons(input_fds, &evdev_held);
			changed |= console_handle_held_buttons(evdev_held);
		} else {
			evdev_held = 0;
			changed |= console_handle_held_buttons(0);
			if (++input_retry >= CONSOLE_INPUT_REOPEN_TICKS) {
				console_input_open_fds(input_fds);
				input_retry = 0;
				changed = 1;
			}
		}

		if (changed || idle >= CONSOLE_IDLE_REDRAW_TICKS) {
			draw_console_screen(++*frame);
			panel_push_frame(0);
			idle = 0;
		} else {
			idle++;
		}
		sleep_ms(CONSOLE_POLL_MS);
	}

	if (fd >= 0)
		close(fd);
	console_input_close_fds(input_fds);
	runtime_watchdog_disable();
}

static void run_rgb_only_diag(unsigned *frame)
{
	int ready_published = 0;
	unsigned last_phase = 99;
	unsigned direct;
	unsigned phase = 0;

	log_line("sf2000-screen: rgb-only diag begin\n");
	append_file_log("sf2000-screen: rgb-only diag begin\n");
	panel_lcd_setup_enable();
	if (!env_is((const char *const *)environ, "SF2000_SKIP_PANEL_INIT", "1"))
		panel_init_sf2000_original_order();

	for (direct = 0; direct < 3u && !stopping; direct++) {
		char line[128];
		uint16_t color = solid_rgb565_color(direct);

		snprintf(line, sizeof(line),
			"sf2000-screen: direct solid fill variant=%u color=0x%04x\n",
			direct, color);
		log_line(line);
		append_file_log(line);
		diagnostic_pulse(direct + 1u, 70, 70);
		panel_fill_solid_direct(color);
		backlight_set(1);
		sleep_ms(1200);
	}

	panel_rgb_pinmux();
	build_gma_descriptor();

	while (!stopping) {
		unsigned variant = phase % 3u;
		const struct gma_scanout_profile *profile =
			&gma_scanout_profiles[phase % ARRAY_SIZE(gma_scanout_profiles)];
		const struct panel_rgb_mode_profile *panel_profile =
			&panel_rgb_mode_profiles[phase % ARRAY_SIZE(panel_rgb_mode_profiles)];
		uint32_t mode = 6u;
		uint32_t pitch = PITCH;
		char variant_name[24];
		unsigned pulse;
		unsigned hold;

		backlight_set(0);
		sleep_ms(160);
		for (pulse = 0; pulse <= phase; pulse++) {
			backlight_set(1);
			sleep_ms(70);
			backlight_set(0);
			sleep_ms(70);
		}
		backlight_set(1);

		snprintf(variant_name, sizeof(variant_name), "RGB565 SOLID %u",
			variant);
		if (phase != last_phase) {
			log_gma_regs("rgb format cycle", variant, mode, pitch,
				profile, panel_profile);
			last_phase = phase;
		}

		for (hold = 0; hold < 3 && !stopping; hold++) {
			(*frame)++;
			build_gma_descriptor_profile(variant, mode, pitch);
			draw_solid_rgb565_screen(variant);
			panel_apply_rgb_mode_profile(panel_profile);
			panel_rgb_pinmux();
			present_frame_profile(profile);
			if (!ready_published) {
				publish_screen_ready_and_storage("rgb-diag\n");
				log_gma_ready();
				ready_published = 1;
			}
			sleep_ms(500);
			watchdog_pet();
		}
		phase = (phase + 1u) % ARRAY_SIZE(gma_scanout_profiles);
	}
	runtime_watchdog_disable();
}

#ifdef PANEL_PROBE_INIT
int main(void)
{
	int argc = 0;
	char **argv = 0;
	char **envp = 0;
#else
int main(int argc, char **argv, char **envp)
{
#endif
	int fd;
	unsigned frame = 0;
	const struct panel_variant *first_variant;

	(void)argc;
	(void)argv;
	log_line("sf2000-screen: main entry\n");
	progress_mark("screen-main", 0x3fu, SCREEN_TAG);
	first_variant = &panel_variants[0];
	if (envp)
		environ = envp;
	progress_mark("screen-after-env", 0x3fu, SCREEN_TAG);

	if (env_is((const char *const *)envp, "SF2000_PANEL", "0"))
		panel_enabled = 0;
	if (env_is((const char *const *)envp, "SF2000_LED", "0"))
		led_enabled = 0;
	if (env_is((const char *const *)envp, "SF2000_FAST_PANEL", "1"))
		slow_panel_bus = 0;
	progress_mark("screen-after-env-checks", 0x3fu, SCREEN_TAG);

	if (cmdline_contains("SF2000_RESET_SNAPSHOT=fast")) {
		log_line("sf2000-screen: reset snapshot fast\n");
		progress_mark("screen-reset-snapshot-fast", 0x3fu, SCREEN_TAG);
		progress_mark_reset_snapshot_fast();
		return 0;
	}
	if (cmdline_contains("SF2000_RESET_SNAPSHOT=full")) {
		log_line("sf2000-screen: reset snapshot full\n");
		progress_mark("screen-reset-snapshot-full", 0x3fu, SCREEN_TAG);
		progress_mark_reset_snapshot_full();
		return 0;
	}

	fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	progress_mark("screen-devmem-fd", 0x3fu, (uint32_t)fd);
	if (fd < 0) {
		map_regions_direct();
		progress_mark("screen-map-direct", 0x3fu, SCREEN_TAG);
	} else {
		if (map_region(fd, &gma_ram_mapping, GMA_RAM_PHYS,
				GMA_RAM_SIZE, "gma-ram") != 0 ||
				map_region(fd, &gma_mapping, GMA_MMIO_PHYS,
					GMA_MMIO_SIZE, "gma") != 0 ||
				map_region(fd, &sysio_mapping, SYSIO_PHYS,
					SYSIO_SIZE, "sysio") != 0) {
			close(fd);
			map_regions_direct();
			progress_mark("screen-map-fallback", 0x3fu, SCREEN_TAG);
		} else {
			close(fd);
			progress_mark("screen-map-devmem-ok", 0x3fu, SCREEN_TAG);
		}
	}

	{
#ifdef PANEL_PROBE_INIT
	progress_mark("screen-raw-main-entry", 0x3fu, SCREEN_TAG);
	log_line("sf2000-screen: main entry\n");
	log_line("sf2000-screen: panel probe begin\n");
	panel_init_variant(first_variant);
	log_line("sf2000-screen: panel probe done\n");
	return 0;
#else
	if (argv0_is(argc > 0 ? argv[0] : 0, "sf2000-panel-probe") ||
	    env_is((const char *const *)envp, "SF2000_PANEL_PROBE", "1") ||
	    cmdline_contains("SF2000_PANEL_PROBE=1")) {
		log_line("sf2000-screen: panel probe begin\n");
		panel_init_variant(first_variant);
		log_line("sf2000-screen: panel probe done\n");
		return 0;
	}
#endif
	}

	progress_mark("screen-before-owner", 0x3fu, SCREEN_TAG);
	publish_marker("/run/sf2000-screen-own-backlight", "owned\n");
	progress_mark("screen-before-backlight", 0x3fu, SCREEN_TAG);
	startup_backlight_diagnostic();
	progress_mark("screen-after-backlight", 0x3fu, SCREEN_TAG);

	build_gma_descriptor();
	progress_mark("screen-after-gma-desc", 0x3fu, SCREEN_TAG);
	if (env_is((const char *const *)envp, "SF2000_RGB_DIAG", "1"))
		run_rgb_only_diag(&frame);
	else
		run_direct_console(&frame);

	log_line("sf2000-screen: before panel init\n");
	panel_init_variant(first_variant);
	log_line("sf2000-screen: after panel init\n");
	panel_push_probe_pixels();
	draw_diag_screen("GMA TRACE READY", first_variant->name,
		first_variant->madctl[0], frame);
	present_frame();
	publish_screen_ready_and_storage("post-direct-diag\n");
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
