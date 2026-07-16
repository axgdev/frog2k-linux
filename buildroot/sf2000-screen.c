// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
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
#include <time.h>
#include <unistd.h>

#include "ge_api.h"

extern char **environ;

static void progress_mark(const char *name, uint32_t kind, uint32_t value);

#define WIDTH 320u
#define HEIGHT 240u
#define PITCH (WIDTH * 2u)
#define FRAME_BYTES (PITCH * HEIGHT)
#define SCAN_WIDTH WIDTH
#define SCAN_HEIGHT HEIGHT
#define SCAN_PITCH (SCAN_WIDTH * 2u)
#define SCAN_BYTES (SCAN_PITCH * SCAN_HEIGHT)
#define LEGACY_SCAN_WIDTH 640u
#define LEGACY_SCAN_HEIGHT 480u
#define LEGACY_SCAN_PITCH (LEGACY_SCAN_WIDTH * 2u)
#define LEGACY_SCAN_BYTES (LEGACY_SCAN_PITCH * LEGACY_SCAN_HEIGHT)
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
#define GMA_DESC_STRIDE 0x00000280u
#define GMA_FRAME_PHYS (GMA_RAM_PHYS + 0x00010000u)
#define GMA_RENDER_PHYS (GMA_RAM_PHYS + 0x000a8000u)
#define GMA_DESC_OFF (GMA_DESC_PHYS - GMA_RAM_PHYS)
#define GMA_DESC_BYTES 640u
#define GMA_FRAME_OFF (GMA_FRAME_PHYS - GMA_RAM_PHYS)
#define GMA_RENDER_OFF (GMA_RENDER_PHYS - GMA_RAM_PHYS)

_Static_assert(GMA_DESC_OFF + GMA_DESC_STRIDE + GMA_DESC_BYTES <=
	GMA_RAM_SIZE, "GMA descriptors exceed reserved DMA arena");
_Static_assert(GMA_FRAME_OFF + LEGACY_SCAN_BYTES <= GMA_RAM_SIZE,
	"GMA scanout exceeds reserved DMA arena");
_Static_assert(GMA_RENDER_OFF + FRAME_BYTES <= GMA_RAM_SIZE,
	"GE render source exceeds reserved DMA arena");

#define GMA_MMIO_PHYS 0x18808000u
#define GMA_MMIO_SIZE 0x1000u
#define GMA_CTL 0x300u
#define GMA_CTL_HW 0xb00u
#define GMA_DMBA 0x304u
#define GMA_DMBA_HW 0xb04u
#define GMA_DMBA_ALT 0x384u
#define GMA_CTL_ALT 0x380u
#define GMA_K_ALT 0x388u
#define GMA_K 0x308u
#define GMA_MASK 0x350u
#define GMA_MASK_ALT 0x3d0u
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
#define SYS_RESET1_OFF 0x084u
#define SYS_LCD_SETUP_OFF 0x094u
#define SYS_IO_VOLTAGE_OFF 0x184u
#define SYS_LVDS_PHY_OFF 0x440u
#define SYS_VIDEO_SRC0_OFF 0x444u
#define SYS_VIDEO_SRC1_OFF 0x448u
#define SYS_VIDEO_SRC2_OFF 0x44cu
#define SYS_RGB_SOURCE_OFF 0x3c8u
#define VOU_HD_MODE 0x000u
#define VOU_HD_TIMING0 0x004u
#define VOU_HD_TIMING1 0x008u
#define VOU_HD_TIMING2 0x00cu
#define VOU_HD_TIMING3 0x080u
#define VOU_HD_CTRL 0x084u
#define VOU_HD_TIMING4 0x088u
#define VOU_HD_TIMING5 0x08cu
/* libviddrv's VPO accessors add 0x100 to their logical register offsets. */
#define VOU_VPO_CTRL 0x190u
#define VOU_VPO_PHASE 0x194u
#define VOU_VPO_COEF 0x198u
#define VOU_VPO_FORMAT 0x19cu
#define VOU_VPO_WIDTH 0x1b8u
#define VOU_VPO_AUX 0x1dcu
#define VOU_RGB_ENABLE 0x1ecu
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
/* GPIO-L's interrupt registers are in the SYSIO block, not in the
 * per-bank data register window. Linux polls the panel TE input from
 * userspace, so the inherited bootloader IRQ must be masked. */
#define GPIO_L_IER_OFF 0x044u
#define GPIO_L_RIS_IER_OFF 0x048u
#define GPIO_L_ISR_OFF 0x05cu
#define SYS_IRQ_ENABLE1_OFF 0x038u
#define SYS_IRQ_GPIO_BIT (1u << 0)
#define SYS_IRQ_IRC_BIT (1u << 19)
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
#define PINMUX_RGB 6u
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

#define CONSOLE_INPUT_FDS 16u
#define CONSOLE_POLL_MS 50u
#define CONSOLE_IDLE_REDRAW_TICKS 20u
#define CONSOLE_REPEAT_DELAY_TICKS 5u
#define CONSOLE_REPEAT_INTERVAL_TICKS 1u
#define CONSOLE_INPUT_REOPEN_TICKS 100u
#define CONSOLE_KMSG_READS_PER_FRAME 64u
#define CONDITIONING_FRAME_DWELL_MS 80u
#define CONDITIONING_TE_EDGES 4u
#define CONDITIONING_TIMEOUT_MS 1000u
#define CONSOLE_BTN_UP 0x01u
#define CONSOLE_BTN_DOWN 0x02u
#define CONSOLE_BTN_LEFT 0x04u
#define CONSOLE_BTN_RIGHT 0x08u
#define CONSOLE_BTN_SELECT 0x10u
#define CONSOLE_BTN_START 0x20u

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
static char boot_visual[16] = "console";
static uint16_t boot_color;
static unsigned boot_hold_ms = 750u;
static char console_lines[CONSOLE_SCROLLBACK_LINES][CONSOLE_LINE_LEN];
static unsigned console_line_count;
static unsigned console_line_start;
static unsigned console_view_offset;

static uint16_t *framebuffer(void);
static int ge_fill_render(uint16_t color);
static hcge_context *display_ge;
static unsigned display_ge_frames;
static unsigned display_ge_attempts;
static unsigned gma_desc_slot;
static uint32_t gma_desc_phys = GMA_DESC_PHYS;
static uint32_t gma_desc_off = GMA_DESC_OFF;
static uint32_t gma_frame_flush_bytes = SCAN_BYTES;
static int panel_te_streaming;
static int panel_te_busy;
static int panel_te_last;
static int panel_te_rising = 1;
static unsigned panel_te_rearms;
static int panel_rgb_vsync_enabled = 1;
static uint32_t panel_rgb_clock_word = 0xb6060606u;
static void panel_set_window(void);
static void panel_restart_frame(void);
static void panel_te_service_sample(void);
static void ge_copy_render_to_scanout(void);
static void panel_te_irq_disable(void);

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

struct gma_descriptor_profile {
	const char *name;
	uint32_t d0;
	uint16_t src_w;
	uint16_t src_h;
	uint16_t pitch;
	uint32_t d8;
	uint32_t d9;
};

struct panel_rgb_mode_profile {
	const char *name;
	uint8_t use_b0;
	uint8_t b0[2];
	uint8_t b1[3];
	uint8_t colmod;
};

struct hc15_panel_sync_profile {
	const char *name;
	uint8_t madctl;
	uint8_t final_ramwr;
	uint8_t te_mode;
	uint8_t vsync;
	uint8_t reset;
	uint32_t clock_word;
};

static void panel_apply_sync_profile(
	const struct hc15_panel_sync_profile *profile);

static const struct panel_variant panel_variants[] = {
	{ "SF2000", st7789_sf2000_init, { 0x60, 0x00, 0x80, 0xc0 } },
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

/*
 * G1 is the recovered MuFrog native RGB565 descriptor and the only profile
 * used by production scanout.  G2-G8 deliberately vary one field at a time
 * for the opt-in SF2000_GE_DIAG differential test.
 */
static const struct gma_descriptor_profile gma_descriptor_profiles[] = {
	{ "G1 MUFROG NATIVE", 0xaa201b61u, WIDTH, HEIGHT, PITCH, 0, 0 },
	{ "G2 NATIVE NO CSC", 0xaa200b61u, WIDTH, HEIGHT, PITCH, 0, 0 },
	{ "G3 NATIVE MIN LAST", 0xaa200161u, WIDTH, HEIGHT, PITCH, 0, 0 },
	{ "G4 EARLY EXACT", 0xaa200160u, WIDTH, HEIGHT, PITCH, 0, 0 },
	{ "G5 SCALE ONE TO ONE", 0xaa201b65u, WIDTH, HEIGHT, PITCH,
		0, 0x10001000u },
	{ "G6 SCALE INC TWO", 0xaa201b65u, WIDTH, HEIGHT, PITCH,
		0, 0x20002000u },
	{ "G7 NATIVE PITCH1280", 0xaa201b61u, WIDTH, HEIGHT,
		LEGACY_SCAN_PITCH, 0, 0 },
	{ "G8 LEGACY 640X480", 0xaa200b65u, LEGACY_SCAN_WIDTH,
		LEGACY_SCAN_HEIGHT, LEGACY_SCAN_PITCH, 0, 0x20002000u },
};

static const struct panel_rgb_mode_profile panel_rgb_mode_profiles[] = {
	/*
	 * A hardware reset restores ST7789 RAMCTRL to 00:f0 (MCU RAM access
	 * and MCU display operation).  RM=1 plus DM=01 is required before the
	 * panel will consume the live RGB stream; B1 configures its timing but
	 * does not transfer that ownership.
	 */
	{ "SF2000-RGB-RAM-565", 1, { 0x11, 0xf0 },
		{ 0x40, 0x04, 0x14 }, 0x55 },
	{ "HCLINUX-RAMCTRL-666", 1, { 0x11, 0xf0 }, { 0x42, 0x08, 0x14 }, 0x66 },
	{ "RAMCTRL-SF2000-565", 1, { 0x11, 0xf0 }, { 0x40, 0x04, 0x14 }, 0x55 },
	{ "RAMCTRL-HCLINUX-565", 1, { 0x11, 0xf0 }, { 0x42, 0x08, 0x14 }, 0x55 },
};

/*
 * log48 eliminates the clock-gate/skew matrix and proves that the narrow L08
 * panel TE signal is electrically active.  Exercise the exact original and
 * recovered MuFrog SF2000 ownership transactions, both TE edges, both known
 * MADCTL values, VSYNC dependence, the recovered L07 pad configuration versus
 * its lossy selector-only form, and a complete reset/init.  Every phase is
 * labelled in its GMA framebuffer.
 */
static const struct hc15_panel_sync_profile hc15_panel_sync_profiles[] = {
	{ "H1 STOCK DISPON", 0x60, 0, 0, 1, 0, 0xb6060606u },
	{ "H2 MUFROG PRE-IRQ", 0x70, 1, 0, 1, 0, 0xb6060606u },
	{ "H3 TE RISE 70", 0x70, 1, 1, 1, 0, 0xb6060606u },
	{ "H4 TE FALL 70", 0x70, 1, 2, 1, 0, 0xb6060606u },
	{ "H5 TE RISE 60", 0x60, 1, 1, 1, 0, 0xb6060606u },
	{ "H6 TE RISE NO VS", 0x70, 1, 1, 0, 0, 0xb6060606u },
	{ "H7 TE RISE CLK06", 0x70, 1, 1, 1, 0, 0x06060606u },
	{ "H8 RESET SF TE RISE", 0x70, 1, 1, 1, 1, 0xb6060606u },
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
	struct timespec delay;
	volatile unsigned spin;
	unsigned i;

	/*
	 * Only the short RGB ownership interval needs cycle-level polling of the
	 * narrow L08 TE pulse.  The old diagnostic delay busy-spun permanently,
	 * consuming the entire CPU after handoff.  Use the kernel clock whenever
	 * TE sampling is not active.
	 */
	if (!panel_te_streaming) {
		delay.tv_sec = msec / 1000u;
		delay.tv_nsec = (long)(msec % 1000u) * 1000000L;
		watchdog_pet();
		while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
			;
		watchdog_pet();
		return;
	}

	for (i = 0; i < msec && !stopping; i++) {
		if ((i & 31u) == 0)
			watchdog_pet();
		for (spin = 0; spin < 18000u; spin++) {
			__asm__ volatile ("" ::: "memory");
			/* Four CPU spins remain far shorter than the panel TE pulse. */
			if (panel_te_streaming && !(spin & 3u))
				panel_te_service_sample();
		}
	}
	watchdog_pet();
}

static uint64_t monotonic_us(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		return 0;
	return (uint64_t)now.tv_sec * 1000000u +
		(uint64_t)now.tv_nsec / 1000u;
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

static void map_framebuffer_device(void)
{
	uint16_t black = 0;
	int fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		log_line("sf2000-screen: open /dev/fb0 failed, using GMA RAM\n");
		return;
	}
	if (pwrite(fd, &black, sizeof(black), 0) != (ssize_t)sizeof(black)) {
		char line[112];

		snprintf(line, sizeof(line),
			"sf2000-screen: write /dev/fb0 failed errno=%d, using GMA RAM\n",
			errno);
		log_line(line);
		close(fd);
		return;
	}
	close(fd);
	log_line("sf2000-screen: /dev/fb0 RGB565 write ready\n");
	progress_mark("screen-fb0-write-ok", 0x3fu, sizeof(black));
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

	/*
	 * These are HC15 clock enables, not HC16 active-low gates.  Repeated logs
	 * from the working MuFrog frontend show CLOCK_GATE1=0x00000600 after the
	 * vendor display stack starts.  The previous bits 0/2/18 assignment came
	 * from the different HC16 clock tree and left both HC15 RGB clocks stopped.
	 */
	gate1 = mmio_read32(sysio, SYS_CLOCK_GATE1_OFF);
	gate1 |= (1u << 9) | (1u << 10);
	mmio_write32(sysio, SYS_CLOCK_GATE1_OFF, gate1);
	progress_mark("screen-rgb-gate1", 0x3fu,
		mmio_read32(sysio, SYS_CLOCK_GATE1_OFF));
}

static void panel_rgb_output_mux_enable(void)
{
	uint32_t strap;
	uint32_t value;

	panel_rgb_clock_enable();

	strap = mmio_read32(sysio, SYS_LCD_SETUP_OFF);
	/*
	 * lcd_pinmux_rgb() in the vendor ST7789 driver does not rewrite the
	 * board's high strap selector.  That selector is latched by the board
	 * boot ROM (the stock SF2000 value is 0x000d0000); forcing a guessed
	 * value here changes the electrical mode on some revisions and leaves
	 * the panel showing only its last retained colour.  Keep every inherited
	 * strap bit and only assert the documented LCD setup enable.
	 */
	strap |= 1u << 16;
	mmio_write32(sysio, SYS_LCD_SETUP_OFF, strap);
	progress_mark("screen-rgb-strap", 0x3fu, strap);

	/*
	 * The source selectors are shared with PQ, HDMI, MIPI and LVDS.  The
	 * original RGB helper only updates the RGB fields.  Retain every other
	 * field: in particular, bits 16:27 of SRC2 are not RGB lane selectors in
	 * libviddrv and changing them was an unsupported inference.
	 */
	value = mmio_read32(sysio, SYS_VIDEO_SRC0_OFF);
	value &= ~((3u << 4) | (3u << 20));
	mmio_write32(sysio, SYS_VIDEO_SRC0_OFF, value);
	progress_mark("screen-rgb-src0", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC0_OFF));

	value = mmio_read32(sysio, SYS_VIDEO_SRC1_OFF);
	value &= ~((3u << 0) | (3u << 4) | (0xfu << 12) |
		(3u << 16) | (3u << 18));
	mmio_write32(sysio, SYS_VIDEO_SRC1_OFF, value);
	progress_mark("screen-rgb-src1", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC1_OFF));

	value = mmio_read32(sysio, SYS_VIDEO_SRC2_OFF);
	/*
	 * Only bits 0:2, 4:6 and 8:10 are the three documented video-source
	 * selectors.  Bits 28:30 are the corresponding direction controls.
	 */
	value &= ~0x70000777u;
	mmio_write32(sysio, SYS_VIDEO_SRC2_OFF, value);
	progress_mark("screen-rgb-src2", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC2_OFF));

	progress_mark("screen-rgb-output-mux-done", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC0_OFF) ^
		mmio_read32(sysio, SYS_VIDEO_SRC1_OFF) ^
		mmio_read32(sysio, SYS_VIDEO_SRC2_OFF));

	/*
	 * Do not touch the 0x18860000 PHY window here. Physical SF2000 logs
	 * 10 and 11 prove that an access to that clock-domain window wedges the
	 * CPU without raising a recoverable bus exception. The stock bootloader
	 * has already established the TTL electrical mode; Linux only needs to
	 * route FXDE and switch the external pads from GPIO to PRGB.
	 */
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
	static const uint32_t stock_vpo_commands[] = {
		0x00000010u, 0x01000011u, 0x00000012u, 0x00000013u,
		0x00000014u, 0x01000015u, 0x00000016u, 0x01000017u,
		0x00000018u, 0x01000019u,
	};
	static const uint32_t stock_vpo_coef_indexes[] = {
		0x00000800u, 0x00000900u, 0x00000a00u, 0x00000b00u,
		0x00000c00u, 0x00000d00u, 0x00000e00u, 0x00000f00u,
	};
	uint32_t inherited = mmio_read32(gma, VOU_VPO_CTRL);
	uint32_t clock;
	uint32_t source;
	unsigned i;

	progress_mark("screen-vpo-before-90", 0x3fu, inherited);
	progress_mark("screen-vpo-before-94", 0x3fu,
		mmio_read32(gma, VOU_VPO_PHASE));
	/* Reproduce libviddrv's complete RGB/VPO setup, including write ports. */
	mmio_write32(gma, VOU_RGB_ENABLE, 0x00010000u);
	mmio_write32(gma, 0x1e4u, 0x016c6800u);
	mmio_write32(gma, 0x1e4u, 0x016ca000u);
	mmio_write32(gma, 0x234u, 0xa13c7c00u);
	mmio_write32(gma, 0x234u, 0xa13c7b00u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00000001u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00000001u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800001u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800001u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800001u);
	mmio_write32(gma, VOU_VPO_FORMAT, 0x00000000u);
	mmio_write32(gma, VOU_VPO_FORMAT, 0x00000000u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800001u);
	for (i = 0; i < ARRAY_SIZE(stock_vpo_commands); i++)
		mmio_write32(gma, 0x14cu, stock_vpo_commands[i]);
	progress_mark("screen-vpo-command-last", 0x3fu,
		mmio_read32(gma, 0x14cu));
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800101u);
	mmio_write32(gma, VOU_VPO_WIDTH, WIDTH);
	mmio_write32(gma, 0x188u, 0x00108080u);
	mmio_write32(gma, 0x18cu, 0x00000000u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800101u);
	for (i = 0; i < ARRAY_SIZE(stock_vpo_coef_indexes); i++) {
		mmio_write32(gma, VOU_VPO_PHASE, stock_vpo_coef_indexes[i]);
		mmio_write32(gma, VOU_VPO_COEF, 0x0000000fu);
	}
	mmio_write32(gma, VOU_VPO_PHASE, 0x00000000u);
	mmio_write32(gma, VOU_VPO_PHASE, 0x00010000u);
	mmio_write32(gma, VOU_VPO_AUX, 0x00000000u);
	mmio_write32(gma, VOU_VPO_CTRL, 0x00800100u);

	/*
	 * These are not redundant writes.  libviddrv moves the HD/VPO block
	 * through reset, timing-load and output-enable states before loading the
	 * final RGB timings.  Writing only the final values leaves readable
	 * registers that look correct, but the compositor remains unlatched and
	 * the RGB port emits VPO's constant 0x00108080 background.
	 */
	mmio_write32(gma, VOU_HD_MODE, 0x00000011u);
	mmio_write32(gma, VOU_HD_MODE, 0x00000001u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00127002u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00127002u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00127102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00127102u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00020502u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00020102u);
	mmio_write32(gma, 0x07cu, 0x00000000u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00020102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00127102u);
	mmio_write32(gma, 0x07cu, 0x00010000u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00030102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, 0x07cu, 0x00010000u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00030102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00030102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_TIMING0, 0x00122914u);
	mmio_write32(gma, 0x030u, 0x00000038u);
	mmio_write32(gma, 0x030u, 0x000a0038u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_MODE, 0x00000011u);
	mmio_write32(gma, 0x064u, 0x0037012au);
	mmio_write32(gma, 0x068u, 0x021d0089u);
	mmio_write32(gma, 0x06cu, 0x000001cbu);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00030302u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00030702u);
	mmio_write32(gma, 0x094u, 0x00000140u);
	mmio_write32(gma, VOU_HD_TIMING3, 0x00030702u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	mmio_write32(gma, VOU_HD_CTRL, 0x00137102u);
	/*
	 * At precisely this arm-to-timing boundary libviddrv pulses SYS_CLK_CTR
	 * bit 19 (0x00083700 -> 0x00003700).  Linux inherits zero on a cold
	 * path, so merely clearing the bit produces no edge and the RGB timing
	 * generator never latches.  Preserve unrelated clock fields while
	 * reproducing both writes and the observed RGB divider/source bits.
	 */
	clock = mmio_read32(sysio, SYS_CLK_CTR_OFF);
	progress_mark("screen-rgb-sysclk-before", 0x3fu, clock);
	clock |= 0x00083700u;
	mmio_write32(sysio, SYS_CLK_CTR_OFF, clock);
	clock &= ~(1u << 19);
	mmio_write32(sysio, SYS_CLK_CTR_OFF, clock);
	progress_mark("screen-rgb-sysclk", 0x3fu,
		mmio_read32(sysio, SYS_CLK_CTR_OFF));

	/*
	 * The stock SF2000 trace writes SYSIO+0x3c8 at this exact boundary.
	 * Reverse engineering gEBtxmNvcpqDReQnckPKMYdltzlKuamg in libviddrv
	 * identifies bits 0:2, 4:6 and 8:10 as a replicated RGB-channel source
	 * selector.  Source zero is the VPO/FXDE stream.  Linux previously left
	 * the bootloader's inherited selection untouched, which can emit a valid
	 * raster filled with unrelated data even though every VOU readback is
	 * correct.
	 */
	source = mmio_read32(sysio, SYS_RGB_SOURCE_OFF);
	progress_mark("screen-rgb-source-before", 0x3fu, source);
	source &= ~0x00000777u;
	mmio_write32(sysio, SYS_RGB_SOURCE_OFF, source);
	progress_mark("screen-rgb-source", 0x3fu,
		mmio_read32(sysio, SYS_RGB_SOURCE_OFF));

	/* Exact final HC15 timing state captured immediately before GMA use. */
	mmio_write32(gma, VOU_HD_TIMING2, 0x01300378u);
	mmio_write32(gma, 0x010u, 0x00040378u);
	mmio_write32(gma, 0x040u, 0x00040378u);
	mmio_write32(gma, 0x014u, 0x028e000au);
	mmio_write32(gma, 0x044u, 0x028e000au);
	mmio_write32(gma, 0x018u, 0x00240130u);
	mmio_write32(gma, 0x048u, 0x00240130u);
	mmio_write32(gma, 0x01cu, 0x07ff07ffu);
	mmio_write32(gma, 0x04cu, 0x07ff07ffu);
	mmio_write32(gma, 0x020u, 0x00001fffu);
	mmio_write32(gma, 0x050u, 0x00001fffu);
	mmio_write32(gma, 0x024u, 0x011e002eu);
	mmio_write32(gma, 0x054u, 0x011e002eu);
	mmio_write32(gma, 0x028u, 0x07ff07ffu);
	mmio_write32(gma, 0x058u, 0x07ff07ffu);
	mmio_write32(gma, 0x02cu, 0x013007ffu);
	mmio_write32(gma, 0x05cu, 0x013007ffu);
	mmio_write32(gma, VOU_HD_MODE, 0x00000015u);
	mmio_write32(gma, VOU_HD_MODE, 0x00000015u);
	mmio_write32(gma, VOU_RGB_ENABLE, 0x00050000u);
	mmio_write32(gma, VOU_RGB_ENABLE, 0x00050000u);
	progress_mark("screen-vou-total", 0x3fu,
		mmio_read32(gma, VOU_HD_TIMING2));
	progress_mark("screen-vou-hactive", 0x3fu,
		mmio_read32(gma, 0x014u));
	progress_mark("screen-vou-vactive", 0x3fu,
		mmio_read32(gma, 0x024u));
	progress_mark("screen-vou-latch-done", 0x3fu,
		mmio_read32(gma, VOU_HD_MODE) ^
		mmio_read32(gma, VOU_HD_TIMING3) ^
		mmio_read32(gma, VOU_HD_CTRL));
	progress_mark("screen-vpo-after-90", 0x3fu,
		mmio_read32(gma, VOU_VPO_CTRL));
	progress_mark("screen-vpo-after-94", 0x3fu,
		mmio_read32(gma, VOU_VPO_PHASE));
	progress_mark("screen-vpo-after-98", 0x3fu,
		mmio_read32(gma, VOU_VPO_COEF));
	progress_mark("screen-vpo-after-9c", 0x3fu,
		mmio_read32(gma, VOU_VPO_FORMAT));
	progress_mark("screen-vpo-after-b8", 0x3fu,
		mmio_read32(gma, VOU_VPO_WIDTH));
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

static void panel_te_irq_disable(void)
{
	static int logged;
	uint32_t bit = gpio_bit_for_pad(PINPAD_L08);
	uint32_t ier = mmio_read32(sysio, GPIO_L_IER_OFF);
	uint32_t ris_ier = mmio_read32(sysio, GPIO_L_RIS_IER_OFF);
	uint32_t intc_enable;

	/* A stale bootloader GPIO-L08 IRQ otherwise starves Linux in EIRQ3. */
	mmio_write32(sysio, GPIO_L_IER_OFF, ier & ~bit);
	mmio_write32(sysio, GPIO_L_RIS_IER_OFF, ris_ier & ~bit);
	/* GPIO ISR is write-one-to-clear on HC15xx. */
	mmio_write32(sysio, GPIO_L_ISR_OFF, bit);
	/*
	 * The ROM also leaves the unsupported infrared source enabled in the
	 * cascaded system interrupt controller.  Linux has no IRC child driver;
	 * masking it (and the GPIO aggregate now owned by this poller) prevents a
	 * level IRQ from starving userspace during the first panel delay.
	 */
	intc_enable = mmio_read32(sysio, SYS_IRQ_ENABLE1_OFF);
	intc_enable &= ~(SYS_IRQ_IRC_BIT | SYS_IRQ_GPIO_BIT);
	mmio_write32(sysio, SYS_IRQ_ENABLE1_OFF, intc_enable);
	if (!logged) {
		progress_mark("screen-te-irq-disabled", 0x3fu,
			((mmio_read32(sysio, GPIO_L_IER_OFF) & bit) << 16) |
			(mmio_read32(sysio, GPIO_L_RIS_IER_OFF) & bit));
		progress_mark("screen-intc-sanitized", 0x3fu,
			mmio_read32(sysio, SYS_IRQ_ENABLE1_OFF));
		logged = 1;
	}
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
	log_line("sf2000-screen: taking backlight ownership\n");
	append_file_log("sf2000-screen: taking backlight ownership\n");
	progress_mark("screen-bl-owned", 0x3fu, SCREEN_TAG);
	/*
	 * The loader has already emitted the single health blink.  Keep inherited
	 * panel GRAM hidden until the complete, selected boot frame is committed.
	 */
	backlight_set(0);
	status_led_set(0);
}

static void panel_control_pinmux(void)
{
	unsigned i;

	panel_te_streaming = 0;
	panel_lcd_setup_enable();
	/*
	 * This is the non-RGB half of HCRTOS's lcd_pinmux_rgb(0).  The RGB
	 * signals overlap only part of the 8080 bus; clearing the remaining
	 * signal pads is still required when returning from scanout.
	 */
	mmio_write32(sysio, PINMUX_L_OFF + 0x04, 0x00000000u);
	mmio_write32(sysio, PINMUX_L_OFF + 0x08,
		mmio_read32(sysio, PINMUX_L_OFF + 0x08) & 0xff00ffffu);
	mmio_write32(sysio, PINMUX_L_OFF + 0x00,
		mmio_read32(sysio, PINMUX_L_OFF + 0x00) & 0x0000ffffu);
	mmio_write32(sysio, PINMUX_T_OFF + 0x08,
		mmio_read32(sysio, PINMUX_T_OFF + 0x08) & 0x000000ffu);
	mmio_write32(sysio, PINMUX_T_OFF + 0x0c,
		mmio_read32(sysio, PINMUX_T_OFF + 0x0c) & 0xff000000u);
	mmio_write32(sysio, PINMUX_T_OFF + 0x00,
		mmio_read32(sysio, PINMUX_T_OFF + 0x00) & 0x0000ffffu);
	mmio_write32(sysio, PINMUX_T_OFF + 0x04,
		mmio_read32(sysio, PINMUX_T_OFF + 0x04) & 0xff000000u);
	/*
	 * L09 belongs to the display controller's global RGB state, not to the
	 * LCD driver's shared 8080/RGB bus state.  Keep VSYNC connected while the
	 * other pins temporarily return to GPIO: ST7789 resets its RGB-mode RAM
	 * address counter on the falling VSYNC edge.
	 */
	pinmux_set_pad(PINPAD_L09,
		panel_rgb_vsync_enabled ? PINMUX_RGB : PINMUX_GPIO);
	for (i = 0; i < ARRAY_SIZE(panel_control_pads); i++)
		pinmux_set_pad(panel_control_pads[i], PINMUX_GPIO);
}

static void panel_rgb_controller_connect(void)
{
	uint32_t ctrl = mmio_read32(gma, VOU_HD_CTRL);
	uint32_t mode = mmio_read32(gma, VOU_HD_MODE);

	/*
	 * Exact first two writes from the proven HC15 lcd_pinmux_rgb(true):
	 *
	 *   *(u32 *)0xb8808084 &= ~0x100;
	 *   *(u32 *)0xb8808000 = (... & ~0xff) | 0x15;
	 *
	 * These addresses belong to VOU, not SYSIO.  The old replacement lost
	 * the 0x8000 bank displacement and cleared SYSIO+0x84 instead, leaving
	 * the VOU output at 0x00137102.  That state has a live frame counter but
	 * does not connect the completed HD raster to the parallel RGB pads.
	 */
	progress_mark("screen-rgb-vou-connect-before", 0x3fu, ctrl);
	ctrl &= ~(1u << 8);
	mode = (mode & ~0xffu) | 0x15u;
	mmio_write32(gma, VOU_HD_CTRL, ctrl);
	mmio_write32(gma, VOU_HD_MODE, mode);
	progress_mark("screen-rgb-vou-connect-ctrl", 0x3fu,
		mmio_read32(gma, VOU_HD_CTRL));
	progress_mark("screen-rgb-vou-connect-mode", 0x3fu,
		mmio_read32(gma, VOU_HD_MODE));
}

static void panel_rgb_pad_mux_only(void)
{
	static int logged;
	/*
	 * L07 is the PRGB pixel clock.  MuFrog's recovered HC15 implementation
	 * writes 0xb6060606: selector 6 plus the existing L07 electrical-pad bits
	 * in the upper nibble.  Writing 0x06060606 silently discarded those bits.
	 */
	mmio_write32(sysio, PINMUX_L_OFF + 0x04, panel_rgb_clock_word);
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
		(mmio_read32(sysio, PINMUX_L_OFF + 0x08) & 0xff0000ffu) |
		0x00060000u | (panel_rgb_vsync_enabled ? 0x00000600u : 0));
	if (!logged) {
		progress_mark("screen-rgb-pad-clock", 0x3fu,
			mmio_read32(sysio, PINMUX_L_OFF + 0x04));
		logged = 1;
	}
}

static void panel_rgb_stream_begin(void)
{
	uint32_t mux = mmio_read32(sysio, PINMUX_L_OFF + 0x08) & 0x00ffffffu;

	progress_mark("screen-rgb-stream-on", 0x3fu, mux);
	progress_mark("screen-rgb-vsync", 0x3fu, mux);
}

static void panel_rgb_engine_prepare(void)
{
	panel_lcd_setup_enable();
	panel_rgb_output_mux_enable();
	panel_vou_rgb_enable();
}

static void panel_rgb_bus_connect(void)
{
	panel_rgb_controller_connect();
	panel_rgb_pad_mux_only();
	panel_rgb_stream_begin();
}

static int panel_wait_gma_raster(uint32_t expected_dmba)
{
	uint32_t ctl_hw = 0;
	uint32_t dmba_hw = 0;
	unsigned elapsed;

	/*
	 * GMA_CTL/DMBA are staging registers.  Their *_HW mirrors only change at
	 * a VOU frame boundary, so readback from the staging side is not evidence
	 * that a pixel raster exists.  In particular, selecting ST7789 RAMCTRL
	 * RGB mode before this transition leaves a cold panel holding the last MCU
	 * frame.  Require the exact descriptor transition observed through the HC15
	 * hardware mirrors before transferring ownership of the shared pins.
	 */
	progress_mark("screen-raster-wait-begin", 0x3fu,
		mmio_read32(gma, GMA_CTL_HW));
	progress_mark("screen-raster-expected", 0x3fu, expected_dmba);
	for (elapsed = 0; elapsed < 200u && !stopping; elapsed++) {
		ctl_hw = mmio_read32(gma, GMA_CTL_HW);
		dmba_hw = mmio_read32(gma, GMA_DMBA_HW);
		if ((ctl_hw & 1u) && dmba_hw == expected_dmba)
			break;
		sleep_ms(1);
	}
	progress_mark("screen-raster-wait-ms", 0x3fu, elapsed);
	progress_mark("screen-raster-ctl-hw", 0x3fu, ctl_hw);
	progress_mark("screen-raster-dmba-hw", 0x3fu, dmba_hw);
	progress_mark("screen-raster-vou-mode", 0x3fu,
		mmio_read32(gma, VOU_HD_MODE));
	progress_mark("screen-raster-vou-ctrl", 0x3fu,
		mmio_read32(gma, VOU_HD_CTRL));
	progress_mark("screen-raster-rgb-enable", 0x3fu,
		mmio_read32(gma, VOU_RGB_ENABLE));

	if (elapsed == 200u || stopping) {
		progress_mark("screen-raster-wait-fail", 0x3fu, dmba_hw);
		return -1;
	}
	progress_mark("screen-raster-wait-ok", 0x3fu, dmba_hw);
	return 0;
}

static void mark_hc15_display_state(int post)
{
	/*
	 * Retain a safe MMIO-only display dump across the watchdog reboot.  These
	 * are the HC15 SYSIO, pinmux, VOU and GMA registers that can be compared
	 * directly with the original firmware/UniFrog; deliberately exclude the
	 * 0x18860000 PHY window which faults on the physical SF2000.
	 */
	progress_mark(post ? "screen-post-sysclk" : "screen-pre-sysclk", 0x3fu,
		mmio_read32(sysio, SYS_CLK_CTR_OFF));
	progress_mark(post ? "screen-post-gate0" : "screen-pre-gate0", 0x3fu,
		mmio_read32(sysio, SYS_CLOCK_GATE0_OFF));
	progress_mark(post ? "screen-post-gate1" : "screen-pre-gate1", 0x3fu,
		mmio_read32(sysio, SYS_CLOCK_GATE1_OFF));
	progress_mark(post ? "screen-post-reset1" : "screen-pre-reset1", 0x3fu,
		mmio_read32(sysio, SYS_RESET1_OFF));
	progress_mark(post ? "screen-post-lcdsetup" : "screen-pre-lcdsetup", 0x3fu,
		mmio_read32(sysio, SYS_LCD_SETUP_OFF));
	progress_mark(post ? "screen-post-rgbsource" : "screen-pre-rgbsource", 0x3fu,
		mmio_read32(sysio, SYS_RGB_SOURCE_OFF));
	progress_mark(post ? "screen-post-src0" : "screen-pre-src0", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC0_OFF));
	progress_mark(post ? "screen-post-src1" : "screen-pre-src1", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC1_OFF));
	progress_mark(post ? "screen-post-src2" : "screen-pre-src2", 0x3fu,
		mmio_read32(sysio, SYS_VIDEO_SRC2_OFF));
	progress_mark(post ? "screen-post-pin-l00" : "screen-pre-pin-l00", 0x3fu,
		mmio_read32(sysio, PINMUX_L_OFF + 0x00u));
	progress_mark(post ? "screen-post-pin-l04" : "screen-pre-pin-l04", 0x3fu,
		mmio_read32(sysio, PINMUX_L_OFF + 0x04u));
	progress_mark(post ? "screen-post-pin-l08" : "screen-pre-pin-l08", 0x3fu,
		mmio_read32(sysio, PINMUX_L_OFF + 0x08u));
	progress_mark(post ? "screen-post-pin-t00" : "screen-pre-pin-t00", 0x3fu,
		mmio_read32(sysio, PINMUX_T_OFF + 0x00u));
	progress_mark(post ? "screen-post-pin-t04" : "screen-pre-pin-t04", 0x3fu,
		mmio_read32(sysio, PINMUX_T_OFF + 0x04u));
	progress_mark(post ? "screen-post-pin-t08" : "screen-pre-pin-t08", 0x3fu,
		mmio_read32(sysio, PINMUX_T_OFF + 0x08u));
	progress_mark(post ? "screen-post-pin-t0c" : "screen-pre-pin-t0c", 0x3fu,
		mmio_read32(sysio, PINMUX_T_OFF + 0x0cu));
	progress_mark(post ? "screen-post-vou-mode" : "screen-pre-vou-mode", 0x3fu,
		mmio_read32(gma, VOU_HD_MODE));
	progress_mark(post ? "screen-post-vou-total" : "screen-pre-vou-total", 0x3fu,
		mmio_read32(gma, VOU_HD_TIMING2));
	progress_mark(post ? "screen-post-vou-ctrl" : "screen-pre-vou-ctrl", 0x3fu,
		mmio_read32(gma, VOU_HD_CTRL));
	progress_mark(post ? "screen-post-rgb-enable" : "screen-pre-rgb-enable", 0x3fu,
		mmio_read32(gma, VOU_RGB_ENABLE));
	progress_mark(post ? "screen-post-gma-ctl" : "screen-pre-gma-ctl", 0x3fu,
		mmio_read32(gma, GMA_CTL));
	progress_mark(post ? "screen-post-gma-ctl-hw" : "screen-pre-gma-ctl-hw", 0x3fu,
		mmio_read32(gma, GMA_CTL_HW));
	progress_mark(post ? "screen-post-gma-dmba" : "screen-pre-gma-dmba", 0x3fu,
		mmio_read32(gma, GMA_DMBA));
	progress_mark(post ? "screen-post-gma-dmba-hw" : "screen-pre-gma-dmba-hw", 0x3fu,
		mmio_read32(gma, GMA_DMBA_HW));
}

static void panel_rgb_pinmux(void)
{
	panel_rgb_engine_prepare();
	panel_rgb_bus_connect();
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
		progress_mark("screen-panel-ramctrl", 0x3fu,
			((uint32_t)profile->b0[0] << 8) | profile->b0[1]);
	}
	panel_cmd(ST7789_RGBCTRL);
	panel_data(profile->b1[0]);
	panel_data(profile->b1[1]);
	panel_data(profile->b1[2]);
	panel_cmd(ST7789_COLMOD);
	panel_data(profile->colmod);
	/*
	 * End in the original firmware's external-RGB command state.  RAMWR is
	 * the MCU/system-interface memory-write command; leaving it active while
	 * switching the shared pads to RGB was an inferred handoff that the stock
	 * binary never performs.  The stock sequence ends with CASET, RASET,
	 * INVON and DISPON, after which VSYNC starts the RGB address counter.
	 */
	panel_set_window();
	panel_cmd(ST7789_INVON);
	panel_cmd(ST7789_DISPON);
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

static void panel_read_register(uint8_t reg, uint8_t *data, unsigned count)
{
	unsigned i;

	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_cmd(reg);
	panel_config_data_input();
	for (i = 0; i < count; i++)
		data[i] = (uint8_t)panel_read_data();
	panel_config_data_output();
	panel_bus_idle();
}

static uint32_t panel_read_id(void)
{
	uint8_t id[4];

	panel_read_register(0x04, id, ARRAY_SIZE(id));
	return ((uint32_t)id[1] << 16) | ((uint32_t)id[2] << 8) | id[3];
}

static uint32_t panel_read_sf2000_aux_id(void)
{
	uint8_t b3[4];
	uint8_t f2[4];

	panel_read_register(0xb3, b3, ARRAY_SIZE(b3));
	panel_read_register(0xf2, f2, ARRAY_SIZE(f2));
	return ((uint32_t)b3[1] << 24) | ((uint32_t)b3[2] << 16) |
		((uint32_t)f2[1] << 8) | f2[2];
}

static void panel_set_window(void)
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
}

static void panel_restart_frame(void)
{
	panel_set_window();
	panel_cmd(ST7789_RAMWR);
}

static void panel_te_stream_start(int rising)
{
	panel_te_rising = rising;
	panel_te_rearms = 0;
	panel_te_last = gpio_get_pad(PINPAD_L08);
	panel_te_streaming = 1;
	progress_mark("screen-te-stream-start", 0x3fu,
		((uint32_t)rising << 31) | (uint32_t)panel_te_last);
}

static void panel_te_rearm(void)
{
	unsigned rearm;

	if (!panel_enabled || !panel_te_streaming || panel_te_busy)
		return;
	panel_te_busy = 1;
	/* Exact recovered MuFrog st7789v2:vsync_irq() transaction. */
	panel_control_pinmux();
	panel_config_outputs();
	panel_restart_frame();
	panel_rgb_pad_mux_only();
	panel_te_last = gpio_get_pad(PINPAD_L08);
	panel_te_streaming = 1;
	panel_te_busy = 0;
	rearm = ++panel_te_rearms;
	if (rearm <= 4u)
		progress_mark("screen-te-rearm-edge", 0x3fu,
			((uint32_t)panel_te_rising << 31) | (rearm << 16) |
			(uint32_t)panel_te_last);
}

static void panel_te_service_sample(void)
{
	int level;
	int edge;

	if (!panel_te_streaming || panel_te_busy)
		return;
	level = gpio_get_pad(PINPAD_L08);
	edge = panel_te_rising ? (level && !panel_te_last) :
		(!level && panel_te_last);
	panel_te_last = level;
	if (edge)
		panel_te_rearm();
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

static void panel_reset_sf2000(void)
{
	/* Exact reset widths used by the proven MuFrog SF2000 LCD driver. */
	panel_bus_idle();
	gpio_set_pad(PINPAD_L01, 1);
	sleep_ms(500);
	gpio_set_pad(PINPAD_L01, 0);
	sleep_ms(500);
	gpio_set_pad(PINPAD_L01, 1);
	sleep_ms(500);
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
	uint32_t panel_aux;
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
	panel_reset_sf2000();
	sleep_ms(120);
	panel_id = panel_read_id();
	panel_aux = panel_read_sf2000_aux_id();
	progress_mark("screen-panel-id", 0x3fu, panel_id);
	progress_mark("screen-panel-aux", 0x3fu, panel_aux);
	panel_apply_init_sequence(st7789_sf2000_init);
	panel_restart_frame();
	panel_cmd(ST7789_DISPON);
	snprintf(line, sizeof(line),
		"sf2000-screen: guarded panel init done id=0x%06x aux=0x%08x init=%s\n",
		panel_id & 0xffffffu, panel_aux, variant->name);
	log_line(line);
	append_file_log(line);
	return 0;
}

static void panel_push_pixels(const uint16_t *fb, int switch_to_rgb)
{
	unsigned i;

	if (!panel_enabled || !fb)
		return;

	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	panel_restart_frame();
	for (i = 0; i < WIDTH * HEIGHT; i++) {
		if ((i & 7u) == 0)
			watchdog_pet();
		panel_data(fb[i]);
	}
	panel_bus_idle();
	if (switch_to_rgb)
		panel_rgb_pinmux();
	watchdog_pet();
}

static void panel_push_frame(int switch_to_rgb)
{
	panel_push_pixels(framebuffer(), switch_to_rgb);
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
		if ((i & 7u) == 0)
			watchdog_pet();
		panel_data(color);
	}
	panel_bus_idle();
	watchdog_pet();
}

static void panel_commit_rgb_handoff(void)
{
	/* SF2000 production path: recovered MuFrog transaction plus L08 TE rearm. */
	panel_apply_sync_profile(&hc15_panel_sync_profiles[2]);
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
	return (uint16_t *)(gma_ram +
		(display_ge ? GMA_RENDER_OFF : GMA_FRAME_OFF));
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

static void draw_console_dynamic(unsigned frame, uint16_t bg, uint16_t bar)
{
	char title[48];

	fill_rect(0, 0, WIDTH, 12, bar);
	snprintf(title, sizeof(title), "SF2000 LOG %06u +%03u/%03u",
		frame, console_view_offset, console_max_view_offset());
	draw_text(2, 2, title, rgb565(31, 63, 20), bar, 1);
	/* Dynamic anchor proving that the GE copied this specific frame. */
	fill_rect(WIDTH - 2u, HEIGHT - 2u, 2u, 2u,
		(uint16_t)(0x8000u | (frame & 0x7fffu)));
	(void)bg;
}

static void draw_console_tick(unsigned frame)
{
	draw_console_dynamic(frame, rgb565(0, 2, 3), rgb565(0, 10, 14));
}

static void draw_console_screen(unsigned frame)
{
	uint16_t fg = rgb565(26, 58, 26);
	uint16_t dim = rgb565(12, 32, 22);
	uint16_t bg = rgb565(0, 2, 3);
	uint16_t bar = rgb565(0, 10, 14);
	unsigned visible = console_visible_lines();
	unsigned start = console_line_count > visible ?
		console_line_count - visible : 0;
	unsigned row;

	console_clamp_view();
	if (console_view_offset < start)
		start -= console_view_offset;
	else
		start = 0;

	if (ge_fill_render(bg) < 0)
		fill_rect(0, 0, WIDTH, HEIGHT, bg);
	draw_console_dynamic(frame, bg, bar);
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

	if ((buttons & (CONSOLE_BTN_SELECT | CONSOLE_BTN_START)) ==
	    (CONSOLE_BTN_SELECT | CONSOLE_BTN_START)) {
		unsigned wait;

		console_add_line("START+SELECT: clean restart");
		log_line("sf2000-screen: START+SELECT pressed, requesting clean restart\n");
		runtime_watchdog_arm();
		progress_mark_reset_snapshot_fast();
		publish_marker("/run/sf2000-reboot-request", "restart\n");
		progress_mark("screen-reboot-request", 0x3fu, SCREEN_TAG);
		for (wait = 0; wait < 60u; wait++) {
			watchdog_pet();
			sleep_ms(100);
		}
		sync();
		log_line("sf2000-screen: clean restart timed out, using watchdog\n");
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
	if (code == BTN_START || code == KEY_ENTER)
		return CONSOLE_BTN_START;
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

static int console_input_refresh_fds(int fds[CONSOLE_INPUT_FDS])
{
	char path[32];
	unsigned i;
	int changed = 0;

	for (i = 0; i < CONSOLE_INPUT_FDS; i++) {
		char name[80] = "unknown";
		char line[128];

		if (fds[i] >= 0)
			continue;
		snprintf(path, sizeof(path), "/dev/input/event%u", i);
		fds[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fds[i] < 0)
			continue;
		(void)ioctl(fds[i], EVIOCGNAME(sizeof(name)), name);
		snprintf(line, sizeof(line), "input event%u: %s", i, name);
		console_add_line(line);
		snprintf(line, sizeof(line),
			 "sf2000-screen: opened %s name=%s\n", path, name);
		log_line(line);
		changed = 1;
	}
	return changed;
}

static int console_poll_evdev_buttons(int fds[CONSOLE_INPUT_FDS],
	uint32_t *held_buttons)
{
	struct input_event ev;
	unsigned i;
	int changed = 0;

	for (i = 0; i < CONSOLE_INPUT_FDS; i++) {
		if (fds[i] < 0)
			continue;
		while (read(fds[i], &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
			uint32_t button;
			char line[64];

			if (ev.type != EV_KEY)
				continue;
			button = console_button_for_key(ev.code);
			if (!button) {
				if (ev.value == 1) {
					snprintf(line, sizeof(line),
						 "input event%u: key %u", i, ev.code);
					console_add_line(line);
					changed = 1;
				}
				continue;
			}
			if (ev.value)
				*held_buttons |= button;
			else
				*held_buttons &= ~button;
		}
	}
	return changed;
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
	((volatile uint32_t *)(gma_ram + gma_desc_off))[idx] = value;
}

static uint32_t gma_descriptor_d0(unsigned variant, uint32_t mode)
{
	(void)variant;
	(void)mode;
	return gma_descriptor_profiles[0].d0;
}

static void build_gma_descriptor_profile(
	const struct gma_descriptor_profile *profile)
{
	static const uint32_t scaler_coefficients[32][2] = {
		{ 0x0088003cu, 0x0000003cu }, { 0x00860036u, 0x00010043u },
		{ 0x0085002fu, 0x0002004au }, { 0x00840029u, 0x00030050u },
		{ 0x00810023u, 0x00050057u }, { 0x007d001eu, 0x0007005eu },
		{ 0x00790019u, 0x000a0064u }, { 0x00740015u, 0x000d006au },
		{ 0x00700010u, 0x00100070u }, { 0x006a000du, 0x00150074u },
		{ 0x0064000au, 0x00190079u }, { 0x005e0007u, 0x001e007du },
		{ 0x00570005u, 0x00230081u }, { 0x00500003u, 0x00290084u },
		{ 0x004a0002u, 0x002f0085u }, { 0x00430001u, 0x00360086u },
		{ 0x00800080u, 0x00000000u }, { 0x00880075u, 0x00000003u },
		{ 0x008e006bu, 0x00000007u }, { 0x00940060u, 0x0000000cu },
		{ 0x00990056u, 0x00000011u }, { 0x009c004cu, 0x00000018u },
		{ 0x009f0042u, 0x0000001fu }, { 0x00a20038u, 0x00000026u },
		{ 0x00a2002fu, 0x0000002fu }, { 0x00a10027u, 0x00000038u },
		{ 0x00a0001fu, 0x00000041u }, { 0x009d0018u, 0x0000004bu },
		{ 0x00990011u, 0x00000056u }, { 0x0094000cu, 0x00000060u },
		{ 0x008e0007u, 0x0000006bu }, { 0x00880003u, 0x00000075u },
	};
	static const int32_t bt709[12] = {
		93, 314, 32, 0, -52, -173, 225, 0, 225, -204, -21, 0
	};
	unsigned i;
	uint32_t source_bytes = (uint32_t)profile->pitch * profile->src_h;

	if (source_bytes > LEGACY_SCAN_BYTES)
		source_bytes = LEGACY_SCAN_BYTES;
	gma_frame_flush_bytes = source_bytes;

	/* Never rewrite the descriptor currently scanned by the HC15 GMA. */
	gma_desc_slot ^= 1u;
	gma_desc_off = GMA_DESC_OFF + gma_desc_slot * GMA_DESC_STRIDE;
	gma_desc_phys = GMA_DESC_PHYS + gma_desc_slot * GMA_DESC_STRIDE;

	for (i = 0; i < GMA_DESC_BYTES / sizeof(uint32_t); i++)
		write_desc32(i, 0);

	write_desc32(0, profile->d0);
	write_desc32(1, 0);
	write_desc32(2, ((WIDTH - 1u) << 16) | 0u);
	write_desc32(3, ((HEIGHT - 1u) << 16) | 0u);
	write_desc32(4, ((uint32_t)profile->src_h << 16) | profile->src_w);
	/* D5[7:0] is global alpha, not the panel's four-bit alpha range. */
	write_desc32(5, 0xffu | ((uint32_t)profile->pitch << 16));
	write_desc32(6, 0);
	write_desc32(7, GMA_FRAME_PHYS);
	write_desc32(8, profile->d8);
	write_desc32(9, profile->d9);
	/* HC15 fetches two scaler taps every four words for each phase. */
	for (i = 0; i < ARRAY_SIZE(scaler_coefficients); i++) {
		write_desc32(16u + i * 4u, scaler_coefficients[i][0]);
		write_desc32(17u + i * 4u, scaler_coefficients[i][1]);
	}
	for (i = 0; i < ARRAY_SIZE(bt709); i++)
		write_desc32(144u + i, (uint32_t)bt709[i]);
}

static void build_gma_descriptor_variant(unsigned variant)
{
	(void)variant;
	build_gma_descriptor_profile(&gma_descriptor_profiles[0]);
}

static void build_gma_descriptor(void)
{
	build_gma_descriptor_variant(0);
}

static void flush_present_memory(void)
{
	(void)cacheflush((void *)(gma_ram + gma_desc_off), GMA_DESC_BYTES, BCACHE);
	(void)cacheflush((void *)(gma_ram + GMA_FRAME_OFF),
		gma_frame_flush_bytes, BCACHE);
}

static void ge_display_open(hcge_context *storage)
{
	char line[96];
	int clock_ret;
	int ret;

	progress_mark("screen-ge-context-address", 0x3fu,
		(uint32_t)(uintptr_t)storage);
	ret = hcge_open_context(storage);
	if (ret == 0) {
		display_ge = storage;
		/*
		 * HC15 selector 0 is the measured fast 198 MHz GE profile used by
		 * MuFrog for fills and presentation.  The CPU remains untouched.
		 */
		clock_ret = hcge_set_clock(display_ge, 0u);
		log_line("sf2000-screen: GE RGB565 compositor ready\n");
		progress_mark("screen-ge-open-ok", 0x3fu, SCREEN_TAG);
		progress_mark(clock_ret == 0 ? "screen-ge-clock-fast" :
			"screen-ge-clock-fail", 0x3fu, (uint32_t)clock_ret);
	} else {
		snprintf(line, sizeof(line),
			"sf2000-screen: GE unavailable ret=%d errno=%d, using direct framebuffer\n",
			ret, errno);
		log_line(line);
		progress_mark("screen-ge-open-fail", 0x3fu, (uint32_t)ret);
	}
}

static int ge_fill_rgb565(uint32_t destination, uint16_t color)
{
	hcge_state *state;
	HCGERectangle rectangle = { 0, 0, WIDTH, HEIGHT };
	int sync_ret;

	if (!display_ge)
		return -1;
	/*
	 * Discard stale CPU lines before the GE becomes the buffer writer.  The
	 * original SF2000 firmware performs a direct GE clear of its eventual
	 * GMA bitmap before the first descriptor is installed; do the same for
	 * both the render and scanout surfaces.
	 */
	(void)cacheflush((void *)(gma_ram + destination - GMA_RAM_PHYS),
		FRAME_BYTES, BCACHE);
	state = &display_ge->state;
	memset(state, 0, sizeof(*state));
	state->render_options = HCGE_DSRO_NONE;
	state->drawingflags = HCGE_DSDRAW_NOFX;
	state->destination.config.format = HCGE_DSPF_RGB16;
	state->destination.config.size.w = WIDTH;
	state->destination.config.size.h = HEIGHT;
	state->dst.phys = destination;
	state->dst.pitch = PITCH;
	state->color.a = 0xff;
	state->color.r = (uint8_t)(((color >> 11) & 0x1fu) << 3);
	state->color.g = (uint8_t)(((color >> 5) & 0x3fu) << 2);
	state->color.b = (uint8_t)((color & 0x1fu) << 3);
	state->accel = HCGE_DFXL_FILLRECTANGLE;
	hcge_set_state(display_ge, state, state->accel);
	if (!hcge_fill_rect(display_ge, &rectangle))
		return -1;
	sync_ret = hcge_engine_sync(display_ge);
	if (sync_ret < 0)
		return -1;
	/* Invalidate cache lines written by the GE DMA master. */
	(void)cacheflush((void *)(gma_ram + destination - GMA_RAM_PHYS),
		FRAME_BYTES, BCACHE);
	return 0;
}

static int ge_fill_render(uint16_t color)
{
	static int logged;

	if (ge_fill_rgb565(GMA_RENDER_PHYS, color) < 0)
		return -1;
	if (!logged) {
		log_line("sf2000-screen: GE accelerated console clear active\n");
		progress_mark("screen-ge-console-fill-ok", 0x3fu, SCREEN_TAG);
		logged = 1;
	}
	return 0;
}

static int ge_prepare_scanout(void)
{
	progress_mark("screen-ge-scanout-init-begin", 0x3fu, GMA_FRAME_PHYS);
	if (ge_fill_rgb565(GMA_FRAME_PHYS, 0) < 0) {
		progress_mark("screen-ge-scanout-init-fail", 0x3fu,
			GMA_FRAME_PHYS);
		return -1;
	}
	progress_mark("screen-ge-scanout-clear-ok", 0x3fu, GMA_FRAME_PHYS);
	ge_copy_render_to_scanout();
	progress_mark("screen-ge-scanout-init-done", 0x3fu, GMA_FRAME_PHYS);
	return 0;
}

static void ge_copy_render_to_scanout(void)
{
	hcge_state *state;
	HCGERectangle source = { 0, 0, WIDTH, HEIGHT };
	bool submitted;
	int sync_ret = -1;
	int verified = 1;
	uint32_t verify_detail = 0;
	unsigned attempt = ++display_ge_attempts;
	int trace_attempt = attempt <= 4u;

	if (!display_ge) {
		uint16_t *dst = (uint16_t *)(gma_ram + GMA_FRAME_OFF);
		uint16_t *src = (uint16_t *)(gma_ram + GMA_RENDER_OFF);

		memcpy(dst, src, FRAME_BYTES);
		return;
	}
	(void)cacheflush((void *)(gma_ram + GMA_RENDER_OFF), FRAME_BYTES, BCACHE);
	state = &display_ge->state;
	memset(state, 0, sizeof(*state));
	state->render_options = HCGE_DSRO_NONE;
	state->drawingflags = HCGE_DSDRAW_NOFX;
	state->blittingflags = HCGE_DSBLIT_NOFX;
	state->destination.config.format = HCGE_DSPF_RGB16;
	state->destination.config.size.w = SCAN_WIDTH;
	state->destination.config.size.h = SCAN_HEIGHT;
	state->source.config.format = HCGE_DSPF_RGB16;
	state->source.config.size.w = WIDTH;
	state->source.config.size.h = HEIGHT;
	state->dst.phys = GMA_FRAME_PHYS;
	state->dst.pitch = SCAN_PITCH;
	state->src.phys = GMA_RENDER_PHYS;
	state->src.pitch = PITCH;
	state->accel = HCGE_DFXL_BLIT;
	hcge_set_state(display_ge, state, state->accel);
	if (trace_attempt)
		progress_mark("screen-ge-submit", 0x3fu, attempt);
	submitted = hcge_blit(display_ge, &source, 0, 0);
	if (trace_attempt)
		progress_mark(submitted ? "screen-ge-submit-ok" :
			"screen-ge-submit-fail", 0x3fu, attempt);
	if (submitted) {
		if (trace_attempt)
			progress_mark("screen-ge-sync", 0x3fu, attempt);
		sync_ret = hcge_engine_sync(display_ge);
		if (trace_attempt)
			progress_mark(sync_ret == 0 ? "screen-ge-sync-ok" :
				"screen-ge-sync-fail", 0x3fu, (uint32_t)sync_ret);
		if (sync_ret == 0 && trace_attempt) {
			uint16_t *dst = (uint16_t *)(gma_ram + GMA_FRAME_OFF);
			uint16_t *src = (uint16_t *)(gma_ram + GMA_RENDER_OFF);
			static const uint16_t samples[][2] = {
				{ 0, 0 }, { WIDTH / 2u, 0 }, { WIDTH - 1u, 0 },
				{ 0, HEIGHT / 2u }, { WIDTH / 2u, HEIGHT / 2u },
				{ WIDTH - 1u, HEIGHT / 2u }, { 0, HEIGHT - 1u },
				{ WIDTH / 2u, HEIGHT - 1u }, { WIDTH - 1u, HEIGHT - 1u },
				{ WIDTH / 4u, HEIGHT / 4u },
				{ (WIDTH * 3u) / 4u, HEIGHT / 4u },
				{ WIDTH / 4u, (HEIGHT * 3u) / 4u },
				{ (WIDTH * 3u) / 4u, (HEIGHT * 3u) / 4u },
				{ 32u, 32u }, { 160u, 80u }, { 288u, 208u },
			};
			unsigned i;

			/* The GE is a DMA master; invalidate any stale CPU lines first. */
			(void)cacheflush((void *)(gma_ram + GMA_FRAME_OFF),
				SCAN_BYTES, BCACHE);
			for (i = 0; i < ARRAY_SIZE(samples); i++) {
				unsigned sx = samples[i][0];
				unsigned sy = samples[i][1];
				unsigned si = sy * WIDTH + sx;
				unsigned di = si;
				uint16_t expected = src[si];

				if (dst[di] != expected) {
					verified = 0;
					verify_detail = ((uint32_t)i << 24) |
						((uint32_t)expected << 16) |
						dst[di];
					break;
				}
			}
			progress_mark(verified ? "screen-ge-verify-ok" :
				"screen-ge-verify-fail", 0x3fu,
				verified ? attempt : verify_detail);
		}
	}
	if (!submitted || sync_ret != 0 || !verified) {
		uint16_t *dst = (uint16_t *)(gma_ram + GMA_FRAME_OFF);
		uint16_t *src = (uint16_t *)(gma_ram + GMA_RENDER_OFF);

		memcpy(dst, src, FRAME_BYTES);
		log_line(verified ?
			"sf2000-screen: GE present failed, copied on CPU\n" :
			"sf2000-screen: GE copy mismatch, copied on CPU and retained GE\n");
		progress_mark("screen-ge-present-fail", 0x3fu, SCREEN_TAG);
	} else {
		display_ge_frames++;
		if (display_ge_frames == 1u) {
			log_line("sf2000-screen: first GE frame complete\n");
			progress_mark("screen-ge-frame-ok", 0x3fu, SCREEN_TAG);
		}
	}
}

static uint32_t ge_diag_frame_hash(void)
{
	const uint16_t *pixels = (const uint16_t *)(gma_ram + GMA_FRAME_OFF);
	uint32_t hash = 2166136261u;
	unsigned i;

	/* Sample every cache line; this is diagnostic identity, not integrity. */
	for (i = 0; i < WIDTH * HEIGHT; i += 16u) {
		hash ^= pixels[i];
		hash *= 16777619u;
	}
	return hash;
}

static int ge_diag_finish(unsigned stage, int submitted)
{
	int sync_ret = -1;

	progress_mark(submitted ? "screen-ge-mcu-submit-ok" :
		"screen-ge-mcu-submit-fail", 0x3fu, stage);
	if (submitted)
		sync_ret = hcge_engine_sync(display_ge);
	progress_mark(sync_ret == 0 ? "screen-ge-mcu-sync-ok" :
		"screen-ge-mcu-sync-fail", 0x3fu,
		(stage << 16) | ((uint32_t)sync_ret & 0xffffu));
	if (!submitted || sync_ret != 0)
		return -1;

	/* Invalidate CPU cache lines written by the GE DMA master. */
	(void)cacheflush((void *)(gma_ram + GMA_FRAME_OFF), FRAME_BYTES, BCACHE);
	progress_mark("screen-ge-mcu-hash", 0x3fu, ge_diag_frame_hash());
	progress_mark("screen-ge-mcu-visible", 0x3fu, stage);
	panel_push_pixels((const uint16_t *)(gma_ram + GMA_FRAME_OFF), 0);
	/* Synchronization above proves completion; this dwell is only visual. */
	sleep_ms(CONDITIONING_FRAME_DWELL_MS);
	return 0;
}

static void ge_diag_surface_state(hcge_state *state, int source)
{
	memset(state, 0, sizeof(*state));
	state->render_options = HCGE_DSRO_NONE;
	state->drawingflags = HCGE_DSDRAW_NOFX;
	state->blittingflags = HCGE_DSBLIT_NOFX;
	state->src_blend = HCGE_DSBF_SRCALPHA;
	state->dst_blend = HCGE_DSBF_ZERO;
	state->destination.config.format = HCGE_DSPF_RGB16;
	state->destination.config.size.w = WIDTH;
	state->destination.config.size.h = HEIGHT;
	state->dst.phys = GMA_FRAME_PHYS;
	state->dst.pitch = PITCH;
	if (source) {
		state->source.config.format = HCGE_DSPF_RGB16;
		state->source.config.size.w = WIDTH;
		state->source.config.size.h = HEIGHT;
		state->src.phys = GMA_RENDER_PHYS;
		state->src.pitch = PITCH;
	}
}

static int ge_diag_fill_quadrants(void)
{
	static const HCGERectangle rects[] = {
		{ 0, 0, WIDTH / 2, HEIGHT / 2 },
		{ WIDTH / 2, 0, WIDTH / 2, HEIGHT / 2 },
		{ 0, HEIGHT / 2, WIDTH / 2, HEIGHT / 2 },
		{ WIDTH / 2, HEIGHT / 2, WIDTH / 2, HEIGHT / 2 },
	};
	static const HCGEColor colors[] = {
		{ 0xff, 0xff, 0x00, 0x00 }, { 0xff, 0x00, 0xff, 0x00 },
		{ 0xff, 0x00, 0x00, 0xff }, { 0xff, 0xff, 0xff, 0xff },
	};
	hcge_state *state = &display_ge->state;
	unsigned i;
	int submitted = 1;

	ge_diag_surface_state(state, 0);
	state->accel = HCGE_DFXL_FILLRECTANGLE;
	for (i = 0; i < ARRAY_SIZE(rects); i++) {
		HCGERectangle rect = rects[i];

		state->color = colors[i];
		hcge_set_state(display_ge, state, state->accel);
		if (!hcge_fill_rect(display_ge, &rect))
			submitted = 0;
	}
	return ge_diag_finish(2u, submitted);
}

static int ge_diag_stretch(void)
{
	hcge_state *state = &display_ge->state;
	HCGERectangle source = { 0, 0, WIDTH / 2, HEIGHT / 2 };
	HCGERectangle destination = { 0, 0, WIDTH, HEIGHT };
	int submitted;

	draw_diag_screen("E3 GE STRETCH VIA MCU", "2X TOP LEFT", 0x70, 3u);
	(void)cacheflush((void *)(gma_ram + GMA_RENDER_OFF), FRAME_BYTES, BCACHE);
	ge_diag_surface_state(state, 1);
	state->accel = HCGE_DFXL_STRETCHBLIT;
	hcge_set_state(display_ge, state, state->accel);
	submitted = hcge_stretch_blit(display_ge, &source, &destination);
	return ge_diag_finish(3u, submitted);
}

static void run_ge_mcu_probe(unsigned frame)
{
	progress_mark("screen-ge-mcu-probe-begin", 0x3fu, 3u);
	if (!display_ge) {
		progress_mark("screen-ge-mcu-probe-skip", 0x3fu, SCREEN_TAG);
		return;
	}

	/*
	 * Keep the panel on its known-good 8080/MCU bus for all three screens.
	 * Thus the only hardware between each generated destination and the glass
	 * is the already-proven CPU pixel writer; VOU and GMA are not involved.
	 */
	draw_diag_screen("E1 GE BLIT VIA MCU", "NATIVE RGB565", 0x70, 1u);
	progress_mark("screen-ge-mcu-phase", 0x3fu, 1u);
	ge_copy_render_to_scanout();
	(void)cacheflush((void *)(gma_ram + GMA_FRAME_OFF), FRAME_BYTES, BCACHE);
	progress_mark("screen-ge-mcu-hash", 0x3fu, ge_diag_frame_hash());
	progress_mark("screen-ge-mcu-visible", 0x3fu, 1u);
	panel_push_pixels((const uint16_t *)(gma_ram + GMA_FRAME_OFF), 0);
	sleep_ms(CONDITIONING_FRAME_DWELL_MS);

	progress_mark("screen-ge-mcu-phase", 0x3fu, 2u);
	(void)ge_diag_fill_quadrants();
	progress_mark("screen-ge-mcu-phase", 0x3fu, 3u);
	(void)ge_diag_stretch();

	/* Restore the normal text console before beginning the RGB handoff. */
	draw_console_screen(frame);
	panel_push_frame(0);
	progress_mark("screen-ge-mcu-probe-done", 0x3fu, 3u);
}

static void gma_set_bit(uint32_t off, uint32_t bit, int on)
{
	uint32_t value = mmio_read32(gma, off);
	uint32_t mask0;
	uint32_t mask1;

	if (on)
		value |= bit;
	else
		value &= ~bit;
	mask0 = mmio_read32(gma, GMA_MASK);
	mask1 = mmio_read32(gma, GMA_MASK_ALT);
	mmio_write32(gma, GMA_MASK, mask0 | 1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
	mmio_write32(gma, off, value);
	mmio_write32(gma, GMA_MASK, mask0 & ~1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);
}

static void present_frame_profile(const struct gma_scanout_profile *profile)
{
	static int logged_hw_state;
	static int gma_taken_over;
	uint32_t mask0;
	uint32_t mask1;
	uint32_t linebuf;
	uint32_t ctl;

	ge_copy_render_to_scanout();
	flush_present_memory();
	/*
	 * The captured HC15 frontend masks both compositor banks around every
	 * primary DMBA update.  0x3d0 is not optional HC16-style layer state on
	 * this chip: leaving it unmasked produces readable primary shadows without
	 * proving that the complete HC15 compositor transaction was accepted.
	 */
	if (gma_taken_over) {
		if (profile->doorbells & GMA_DOORBELL_PRIMARY) {
			mask0 = mmio_read32(gma, GMA_MASK);
			mask1 = mmio_read32(gma, GMA_MASK_ALT);
			mmio_write32(gma, GMA_MASK, mask0 | 1u);
			mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
			mmio_write32(gma, GMA_DMBA, gma_desc_phys);
			mmio_write32(gma, GMA_MASK, mask0 & ~1u);
			mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);
		}
		if (profile->doorbells & GMA_DOORBELL_ALT) {
			mask0 = mmio_read32(gma, GMA_MASK);
			mask1 = mmio_read32(gma, GMA_MASK_ALT);
			mmio_write32(gma, GMA_MASK, mask0 | 1u);
			mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
			mmio_write32(gma, GMA_DMBA_ALT, gma_desc_phys);
			mmio_write32(gma, GMA_MASK, mask0 & ~1u);
			mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);
		}
		return;
	}
	linebuf = mmio_read32(gma, GMA_LINEBUF);
	linebuf = (linebuf & ~0x1fu) | (profile->linebuf & 0x1fu);
	linebuf |= 0x00020000u;
	mmio_write32(gma, GMA_LINEBUF, linebuf);
	/*
	 * The boot diagnostic leaves descriptor slot 0 active.  Do not modify
	 * that slot or change its base while the fetcher is running: the stock
	 * firmware prepares a new list with the layer disabled, rings DMBA, and
	 * enables the layer in a second masked transaction.  The descriptor
	 * builder alternates two 0x280-byte slots, so this first handoff and all
	 * subsequent updates always point the fetcher at a complete list.
	 */
	ctl = mmio_read32(gma, GMA_CTL);
	ctl &= ~((1u << 19) | 1u);
	if (profile->sdk_enhance)
		ctl |= 1u << 18;
	else {
		ctl &= ~(1u << 18);
		ctl |= 1u << 19;
	}
	progress_mark("screen-gma-takeover-off", 0x3fu, ctl);
	/* Initialize primary exactly as HC15 gma_open(): disabled, K=0xff. */
	mmio_write32(gma, GMA_CTL, ctl);
	mmio_write32(gma, GMA_K, 0xffu);
	mask0 = mmio_read32(gma, GMA_MASK);
	mask1 = mmio_read32(gma, GMA_MASK_ALT);
	mmio_write32(gma, GMA_MASK, mask0 | 1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
	mmio_write32(gma, GMA_CTL, ctl);
	mmio_write32(gma, GMA_MASK, mask0 & ~1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);

	/* HC15 initializes the otherwise unused alternate bank in lockstep. */
	mmio_write32(gma, GMA_CTL_ALT, ctl);
	mmio_write32(gma, GMA_K_ALT, 0xffu);
	mask0 = mmio_read32(gma, GMA_MASK);
	mask1 = mmio_read32(gma, GMA_MASK_ALT);
	mmio_write32(gma, GMA_MASK, mask0 | 1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
	mmio_write32(gma, GMA_CTL_ALT, ctl);
	mmio_write32(gma, GMA_MASK, mask0 & ~1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);

	/* Commit the first descriptor as a separate stock HC15 transaction. */
	mask0 = mmio_read32(gma, GMA_MASK);
	mask1 = mmio_read32(gma, GMA_MASK_ALT);
	mmio_write32(gma, GMA_MASK, mask0 | 1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
	if (profile->doorbells & GMA_DOORBELL_PRIMARY)
		mmio_write32(gma, GMA_DMBA, gma_desc_phys);
	mmio_write32(gma, GMA_MASK, mask0 & ~1u);
	mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);
	if (profile->doorbells & GMA_DOORBELL_ALT) {
		mask0 = mmio_read32(gma, GMA_MASK);
		mask1 = mmio_read32(gma, GMA_MASK_ALT);
		mmio_write32(gma, GMA_MASK, mask0 | 1u);
		mmio_write32(gma, GMA_MASK_ALT, mask1 | 1u);
		mmio_write32(gma, GMA_DMBA_ALT, gma_desc_phys);
		mmio_write32(gma, GMA_MASK, mask0 & ~1u);
		mmio_write32(gma, GMA_MASK_ALT, mask1 & ~1u);
	}
	gma_set_bit(GMA_CTL, 1u, 1);
	gma_taken_over = 1;
	progress_mark("screen-gma-takeover-on", 0x3fu,
		mmio_read32(gma, GMA_CTL));
	if (!logged_hw_state) {
		progress_mark("screen-gma-ctl", 0x3fu,
			mmio_read32(gma, GMA_CTL));
		progress_mark("screen-gma-ctl-hw", 0x3fu,
			mmio_read32(gma, GMA_CTL_HW));
		progress_mark("screen-gma-dmba", 0x3fu,
			mmio_read32(gma, GMA_DMBA));
		progress_mark("screen-gma-mask0", 0x3fu,
			mmio_read32(gma, GMA_MASK));
		progress_mark("screen-gma-mask1", 0x3fu,
			mmio_read32(gma, GMA_MASK_ALT));
		progress_mark("screen-gma-linebuf", 0x3fu,
			mmio_read32(gma, GMA_LINEBUF));
		progress_mark("screen-gma-desc0", 0x3fu,
			((volatile uint32_t *)(gma_ram + gma_desc_off))[0]);
		progress_mark("screen-gma-scale0", 0x3fu,
			((volatile uint32_t *)(gma_ram + gma_desc_off))[16]);
		progress_mark("screen-gma-scale31", 0x3fu,
			((volatile uint32_t *)(gma_ram + gma_desc_off))[140]);
		progress_mark("screen-gma-csc0", 0x3fu,
			((volatile uint32_t *)(gma_ram + gma_desc_off))[144]);
		logged_hw_state = 1;
	}
}

static void present_frame(void)
{
	static unsigned presents;

	/*
	 * Match gma_dmba_update() from the vendor framebuffer driver: build the
	 * inactive 0x280-byte block, ring DMBA, then alternate blocks on every
	 * update.  This keeps the active descriptor immutable while giving HC15 a
	 * real frame-boundary transaction for each completed GE frame.
	 */
	presents++;
	if (presents > 1u)
		build_gma_descriptor();
	if (presents <= 4u)
		progress_mark("screen-gma-present-desc", 0x3fu, gma_desc_phys);
	/* The captured HC15 RGB565 frontend uses CSC enhancement mode. */
	present_frame_profile(&gma_scanout_profiles[3]);
	if (presents <= 4u)
		progress_mark("screen-gma-present-dmba", 0x3fu,
			mmio_read32(gma, GMA_DMBA));
	if (presents <= 4u) {
		progress_mark("screen-gma-present-ctl-hw", 0x3fu,
			mmio_read32(gma, GMA_CTL_HW));
		progress_mark("screen-gma-present-dmba-hw", 0x3fu,
			mmio_read32(gma, GMA_DMBA_HW));
	}
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

static int cmdline_value(const char *name, char *value, size_t value_size)
{
	char cmdline[512];
	size_t name_len = strlen(name);
	char *token;
	ssize_t n;
	int fd;

	if (!name_len || value_size < 2u)
		return -1;
	fd = open("/proc/cmdline", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	n = read(fd, cmdline, sizeof(cmdline) - 1u);
	close(fd);
	if (n <= 0)
		return -1;
	cmdline[n] = 0;
	for (token = strtok(cmdline, " \t\n"); token;
	     token = strtok(NULL, " \t\n")) {
		size_t length;

		if (strncmp(token, name, name_len) || token[name_len] != '=')
			continue;
		token += name_len + 1u;
		length = strlen(token);
		if (length >= value_size)
			length = value_size - 1u;
		memcpy(value, token, length);
		value[length] = 0;
		return 0;
	}
	return -1;
}

static void configure_boot_visual(void)
{
	char value[32];
	const char *environment = getenv("SF2000_BOOT_VISUAL");
	char *end;
	unsigned long parsed;

	if (environment && *environment) {
		strncpy(boot_visual, environment, sizeof(boot_visual) - 1u);
		boot_visual[sizeof(boot_visual) - 1u] = 0;
	} else if (cmdline_value("SF2000_BOOT_VISUAL", value,
			sizeof(value)) == 0) {
		strncpy(boot_visual, value, sizeof(boot_visual) - 1u);
		boot_visual[sizeof(boot_visual) - 1u] = 0;
	}
	if (strcmp(boot_visual, "console") && strcmp(boot_visual, "color") &&
	    strcmp(boot_visual, "logo"))
		strcpy(boot_visual, "console");

	environment = getenv("SF2000_BOOT_COLOR");
	if (environment && *environment)
		strncpy(value, environment, sizeof(value) - 1u);
	else if (cmdline_value("SF2000_BOOT_COLOR", value, sizeof(value)) != 0)
		strcpy(value, "0");
	value[sizeof(value) - 1u] = 0;
	errno = 0;
	parsed = strtoul(value, &end, 0);
	if (!errno && end != value && !*end && parsed <= 0xffffu)
		boot_color = (uint16_t)parsed;

	environment = getenv("SF2000_BOOT_HOLD_MS");
	if (environment && *environment)
		strncpy(value, environment, sizeof(value) - 1u);
	else if (cmdline_value("SF2000_BOOT_HOLD_MS", value,
			sizeof(value)) != 0)
		strcpy(value, "750");
	value[sizeof(value) - 1u] = 0;
	errno = 0;
	parsed = strtoul(value, &end, 0);
	if (!errno && end != value && !*end && parsed <= 5000u)
		boot_hold_ms = (unsigned)parsed;
}

static void draw_boot_logo(void)
{
	uint16_t bg = boot_color;
	uint16_t panel = rgb565(0, 8, 13);
	uint16_t accent = rgb565(2, 40, 31);
	uint16_t text = rgb565(27, 63, 28);

	if (ge_fill_render(bg) < 0)
		fill_rect(0, 0, WIDTH, HEIGHT, bg);
	fill_rect(46, 64, WIDTH - 92u, HEIGHT - 128u, panel);
	fill_rect(46, 64, WIDTH - 92u, 4, accent);
	fill_rect(46, HEIGHT - 68u, WIDTH - 92u, 4, accent);
	draw_text(76, 91, "SF2000", text, panel, 3);
	draw_text(106, 126, "LINUX", text, panel, 2);
}

static int draw_boot_visual(unsigned frame)
{
	if (!strcmp(boot_visual, "logo")) {
		draw_boot_logo();
		progress_mark("screen-boot-visual", 0x3fu, 2u);
		return 1;
	}
	if (!strcmp(boot_visual, "color")) {
		if (ge_fill_render(boot_color) < 0)
			fill_rect(0, 0, WIDTH, HEIGHT, boot_color);
		progress_mark("screen-boot-visual", 0x3fu, 1u);
		return 1;
	}
	draw_console_screen(frame);
	progress_mark("screen-boot-visual", 0x3fu, 0u);
	return 0;
}

static void log_gma_ready(void)
{
	char line[160];

	snprintf(line, sizeof(line),
		"sf2000-screen: gma console ready desc=0x%08x fb=0x%08x %ux%u pitch=%u\n",
		gma_desc_phys, GMA_FRAME_PHYS, WIDTH, HEIGHT, PITCH);
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

static int argv0_is(const char *argv0, const char *name)
{
	const char *base;

	if (!argv0 || !name)
		return 0;
	base = strrchr(argv0, '/');
	base = base ? base + 1 : argv0;
	return strcmp(base, name) == 0;
}

static void panel_apply_sync_profile(
	const struct hc15_panel_sync_profile *profile)
{
	progress_mark("screen-rgb-control-mux", 0x3fu, SCREEN_TAG);
	panel_te_streaming = 0;
	panel_rgb_vsync_enabled = profile->vsync;
	panel_rgb_clock_word = profile->clock_word;
	panel_control_pinmux();
	panel_config_outputs();
	panel_bus_idle();
	if (profile->reset) {
		panel_reset_sf2000();
		sleep_ms(120);
		panel_apply_init_sequence(st7789_sf2000_init);
	} else {
		panel_cmd(ST7789_MADCTL);
		panel_data(profile->madctl);
		panel_cmd(ST7789_TEON);
		panel_data(0x00);
		panel_cmd(ST7789_COLMOD);
		panel_data(0x55);
		panel_cmd(ST7789_RGBCTRL);
		panel_data(0x40);
		panel_data(0x04);
		panel_data(0x14);
	}
	if (profile->final_ramwr) {
		/* MuFrog arms RAMWR before DISPON; its first TE IRQ re-arms RAMWR. */
		panel_restart_frame();
		panel_cmd(ST7789_DISPON);
	} else {
		/* Exact original SF2000 trace: CASET, RASET, INVON, DISPON. */
		panel_set_window();
		panel_cmd(ST7789_INVON);
		panel_cmd(ST7789_DISPON);
	}
	progress_mark("screen-panel-command-final", 0x3fu, ST7789_DISPON);
	progress_mark("screen-rgb-pinmux", 0x3fu, SCREEN_TAG);
	panel_rgb_bus_connect();
	progress_mark("screen-rgb-pinmux-done", 0x3fu, SCREEN_TAG);
	if (profile->te_mode)
		panel_te_stream_start(profile->te_mode == 1);
}

static int run_gma_descriptor_probe(void)
{
	unsigned i;

	progress_mark("screen-gma-probe-begin", 0x3fu,
		ARRAY_SIZE(gma_descriptor_profiles));
	for (i = 0; i < ARRAY_SIZE(gma_descriptor_profiles) && !stopping; i++) {
		const struct gma_descriptor_profile *profile =
			&gma_descriptor_profiles[i];
		uint32_t detail = (i << 24) |
			((uint32_t)profile->src_w << 12) | profile->src_h;
		unsigned before_rearms = panel_te_rearms;

		draw_diag_screen("GMA GEOMETRY PROBE", profile->name,
			0x70, 0x400u + i);
		build_gma_descriptor_profile(profile);
		progress_mark("screen-probe-phase-render", 0x3fu, detail);
		progress_mark("screen-probe-d0", 0x3fu, profile->d0);
		progress_mark("screen-probe-d4", 0x3fu,
			((uint32_t)profile->src_h << 16) | profile->src_w);
		progress_mark("screen-probe-d5", 0x3fu,
			((uint32_t)profile->pitch << 16) | 0xffu);
		progress_mark("screen-probe-d9", 0x3fu, profile->d9);
		present_frame_profile(&gma_scanout_profiles[3]);
		if (panel_wait_gma_raster(gma_desc_phys) < 0) {
			progress_mark("screen-probe-raster-fail", 0x3fu, detail);
			return -1;
		}

		progress_mark("screen-probe-phase-begin", 0x3fu, detail);
		sleep_ms(1500u);
		progress_mark("screen-probe-phase-rearms", 0x3fu,
			(i << 24) |
			((panel_te_rearms - before_rearms) & 0x00ffffffu));
		progress_mark("screen-probe-phase-vou", 0x3fu,
			mmio_read32(gma, VOU_HD_TIMING2));
		progress_mark("screen-probe-phase-gma", 0x3fu,
			mmio_read32(gma, GMA_CTL_HW));
		progress_mark("screen-probe-phase-dmba", 0x3fu,
			mmio_read32(gma, GMA_DMBA_HW));
		progress_mark("screen-probe-phase-done", 0x3fu, detail);
	}
	progress_mark("screen-gma-probe-done", 0x3fu,
		ARRAY_SIZE(gma_descriptor_profiles));
	panel_te_streaming = 0;
	progress_mark("screen-te-conditioning-done", 0x3fu, panel_te_rearms);
	return stopping ? -1 : 0;
}

static int condition_native_scanout(void)
{
	static const unsigned milestones[] = { 1u, 2u, 3u, 4u };
	uint64_t begin_us = monotonic_us();
	unsigned start_rearms = panel_te_rearms;
	unsigned milestone = 0;
	unsigned elapsed_ms = 0;
	unsigned rearmed;

	/*
	 * G1 is the first sharp frame in every successful physical run.  Its
	 * defining operation is a native descriptor update after RAMCTRL and the
	 * shared pads have changed to RGB ownership.  Submit that frame once, wait
	 * for its hardware mirror, then allow the first four TE/RAMWR boundaries
	 * to complete before leaving the panel in continuous RGB mode.
	 */
	progress_mark("screen-native-hold-begin", 0x3fu,
		CONDITIONING_TE_EDGES);
	draw_console_screen(0);
	build_gma_descriptor_profile(&gma_descriptor_profiles[0]);
	progress_mark("screen-native-present", 0x3fu, gma_desc_phys);
	present_frame_profile(&gma_scanout_profiles[3]);
	if (panel_wait_gma_raster(gma_desc_phys) < 0) {
		progress_mark("screen-native-present-fail", 0x3fu,
			gma_desc_phys);
		return -1;
	}
	while (!stopping) {
		uint64_t now_us;

		rearmed = panel_te_rearms - start_rearms;
		while (milestone < ARRAY_SIZE(milestones) &&
		       rearmed >= milestones[milestone]) {
			progress_mark("screen-native-hold-edge", 0x3fu,
				milestones[milestone]);
			milestone++;
		}
		if (rearmed >= CONDITIONING_TE_EDGES)
			break;
		now_us = monotonic_us();
		if (begin_us && now_us >= begin_us) {
			elapsed_ms = (unsigned)((now_us - begin_us) / 1000u);
			if (elapsed_ms >= CONDITIONING_TIMEOUT_MS)
				break;
		}
		sleep_ms(1);
	}

	rearmed = panel_te_rearms - start_rearms;
	if (begin_us) {
		uint64_t end_us = monotonic_us();

		if (end_us >= begin_us)
			elapsed_ms = (unsigned)((end_us - begin_us) / 1000u);
	}
	progress_mark("screen-native-hold-count", 0x3fu, rearmed);
	progress_mark("screen-native-hold-ms", 0x3fu, elapsed_ms);
	progress_mark("screen-native-hold-vou", 0x3fu,
		mmio_read32(gma, VOU_HD_TIMING2));
	progress_mark("screen-native-hold-gma", 0x3fu,
		mmio_read32(gma, GMA_CTL_HW));
	progress_mark("screen-native-hold-dmba", 0x3fu,
		mmio_read32(gma, GMA_DMBA_HW));
	if (stopping || rearmed < CONDITIONING_TE_EDGES) {
		progress_mark("screen-native-hold-fail", 0x3fu, rearmed);
		return -1;
	}

	/*
	 * The physical log77 interrupt count was about twice the panel frame rate,
	 * proving that the experimental kernel handler was retriggering on the
	 * GPIO level rather than servicing one edge.  That repeatedly disconnected
	 * the RGB pads and recreated the moving static.  The ownership transaction
	 * is complete here; steady RGB scanout needs no further MCU commands.
	 */
	panel_te_streaming = 0;
	progress_mark("screen-native-hold-done", 0x3fu, rearmed);
	progress_mark("screen-te-conditioning-done", 0x3fu, panel_te_rearms);
	return 0;
}

static void run_direct_console(unsigned *frame)
{
	char buf[768];
	int input_fds[CONSOLE_INPUT_FDS];
	int fd = -1;
	uint32_t evdev_held = 0;
	unsigned idle = 0;
	unsigned input_retry = 0;
	int rgb_active = 0;
	int ge_diagnostics;

	log_line("sf2000-screen: direct text console begin\n");
	append_file_log("sf2000-screen: direct text console begin\n");

	panel_lcd_setup_enable();
	if (!env_is((const char *const *)environ, "SF2000_SKIP_PANEL_INIT", "1"))
		panel_init_sf2000_original_order();

	console_clear();
	console_add_line("sf2000 linux direct lcd console");
	console_add_line("reading /dev/kmsg");
	console_input_init_fds(input_fds);
	if (!console_input_refresh_fds(input_fds))
		console_add_line("input: waiting for evdev devices");
	++*frame;
	ge_diagnostics =
		env_is((const char *const *)environ, "SF2000_GE_DIAG", "1") ||
		cmdline_contains("SF2000_GE_DIAG=1");
	if (ge_diagnostics)
		draw_console_screen(*frame);
	else
		(void)draw_boot_visual(*frame);
	/*
	 * Complete a real GRAM transaction before changing the ST7789 ownership
	 * from its 8080/MCU port to RGB.  This cannot be reduced to register
	 * readback: the first visible frame must be valid before the backlight is
	 * enabled, and the RGB handoff later depends on a completed address-window
	 * transaction rather than inherited panel state.
	 */
	progress_mark("screen-panel-push-begin", 0x3fu, SCREEN_TAG);
	panel_push_frame(0);
	progress_mark("screen-panel-push-done", 0x3fu, SCREEN_TAG);
	backlight_set(1);
	progress_mark("screen-boot-backlight-on", 0x3fu, SCREEN_TAG);
	if (!ge_diagnostics && strcmp(boot_visual, "console") && boot_hold_ms)
		sleep_ms(boot_hold_ms);
	/*
	 * Keep the broad GE/MCU cards as an explicit hardware diagnostic.  Normal
	 * boot already exercises the same fill and blit engines and no longer
	 * depends on displaying test patterns before the console.
	 */
	if (ge_diagnostics)
		run_ge_mcu_probe(*frame);
	/*
	 * The closed firmware clears its future GMA bitmap with GE before its
	 * first doorbell.  E2 accidentally supplied this initialization in all
	 * sharp test runs; make it an explicit, non-visible production step.
	 */
	if (ge_prepare_scanout() < 0)
		progress_mark("screen-ge-scanout-init-degraded", 0x3fu,
			GMA_FRAME_PHYS);
	/*
	 * Start the VOU timing generator while the panel still owns the GPIO bus,
	 * then submit and observe a real GMA hardware latch.  The former ordering
	 * submitted before VOU existed; log43 proves its CTL_HW and DMBA_HW were
	 * both zero when RAMCTRL transferred the display to RGB.
	 */
	progress_mark("screen-rgb-handoff-begin", 0x3fu, SCREEN_TAG);
	mark_hc15_display_state(0);
	progress_mark("screen-rgb-engine-prepare", 0x3fu, SCREEN_TAG);
	panel_rgb_engine_prepare();
	progress_mark("screen-rgb-engine-ready", 0x3fu, SCREEN_TAG);
	progress_mark("screen-rgb-prime-begin", 0x3fu, SCREEN_TAG);
	present_frame();
	progress_mark("screen-rgb-prime-done", 0x3fu, SCREEN_TAG);
	if (panel_wait_gma_raster(gma_desc_phys) < 0) {
		/* Preserve the working direct frame and retained failure snapshot. */
		progress_mark("screen-rgb-handoff-abort", 0x3fu, SCREEN_TAG);
		goto handoff_complete;
	}
	/*
	 * One matching shadow can still be inherited from a previous owner.  Make
	 * HC15 consume the alternate descriptor as a second independent proof that
	 * its live fetcher is responding to Linux before the panel changes mode.
	 */
	progress_mark("screen-rgb-prime2-begin", 0x3fu, SCREEN_TAG);
	present_frame();
	progress_mark("screen-rgb-prime2-done", 0x3fu, SCREEN_TAG);
	if (panel_wait_gma_raster(gma_desc_phys) < 0) {
		progress_mark("screen-rgb-handoff-abort", 0x3fu, SCREEN_TAG);
		goto handoff_complete;
	}
	/*
	 * Hold the proven MuFrog panel ownership transaction constant while the
	 * descriptor matrix varies only GMA source geometry and header semantics.
	 */
	panel_commit_rgb_handoff();
	rgb_active = 1;
	mark_hc15_display_state(1);
	/* Finish optional descriptor variations before the bounded TE handoff. */
	if (ge_diagnostics) {
		if (run_gma_descriptor_probe() < 0) {
			progress_mark("screen-rgb-handoff-abort", 0x3fu,
				SCREEN_TAG);
			goto handoff_complete;
		}
		draw_console_screen(*frame);
		build_gma_descriptor_profile(&gma_descriptor_profiles[0]);
		progress_mark("screen-probe-restore-present", 0x3fu, *frame);
		present_frame_profile(&gma_scanout_profiles[3]);
		if (panel_wait_gma_raster(gma_desc_phys) < 0) {
			progress_mark("screen-rgb-handoff-abort", 0x3fu,
				SCREEN_TAG);
			goto handoff_complete;
		}
		panel_te_streaming = 1;
	}
	/* Complete the recovered ownership transition with one post-switch frame. */
	if (condition_native_scanout() < 0) {
		progress_mark("screen-rgb-handoff-abort", 0x3fu,
			SCREEN_TAG);
		goto handoff_complete;
	}
	progress_mark("screen-rgb-handoff-done", 0x3fu, SCREEN_TAG);
	progress_mark("screen-first-present-done", 0x3fu, SCREEN_TAG);
	log_gma_ready();

handoff_complete:
	fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		console_add_line("open /dev/kmsg failed");
		draw_console_screen(++*frame);
		if (rgb_active)
			present_frame();
		else
			panel_push_frame(0);
	}

	publish_screen_ready_and_storage("direct-console\n");
	progress_mark("screen-loop-enter", 0x3fu, *frame);

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
			changed |= console_poll_evdev_buttons(input_fds, &evdev_held);
			changed |= console_handle_held_buttons(evdev_held);
		} else {
			evdev_held = 0;
			changed |= console_handle_held_buttons(0);
		}
		if (++input_retry >= CONSOLE_INPUT_REOPEN_TICKS) {
			changed |= console_input_refresh_fds(input_fds);
			input_retry = 0;
		}

		if (changed || idle >= CONSOLE_IDLE_REDRAW_TICKS) {
			if (*frame < 4u)
				progress_mark("screen-loop-draw", 0x3fu, *frame + 1u);
			++*frame;
			if (changed)
				draw_console_screen(*frame);
			else
				draw_console_tick(*frame);
			if (*frame <= 4u)
				progress_mark("screen-loop-present", 0x3fu, *frame);
			if (rgb_active)
				present_frame();
			else
				panel_push_frame(0);
			if (*frame <= 4u)
				progress_mark("screen-loop-present-done", 0x3fu, *frame);
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
			build_gma_descriptor_profile(&gma_descriptor_profiles[0]);
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
	hcge_context display_ge_storage;
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
	configure_boot_visual();
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
	map_framebuffer_device();
	/*
	 * Keep the GE context in main's persistent stack frame.  FLAT NOMMU
	 * executables must not depend on an absolute address-of-BSS relocation:
	 * the MIPS elf2flt path can otherwise leave that pointer in the low,
	 * unmapped link-time address range.  The process stack already carries
	 * the correct KSEG0 execution alias.
	 */
	ge_display_open(&display_ge_storage);
	/* Userspace polls L08 for TE; prevent the inherited IRQ from starving
	 * Linux before the first control-bus handoff. */
	panel_te_irq_disable();

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

	backlight_set(1);
	status_led_set(0);
	unlink("/run/sf2000-screen-own-backlight");
	log_line("sf2000-screen: stopped\n");
	return 0;
}
