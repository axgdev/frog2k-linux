/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 Linus Torvalds
 * Copyright (C) 1995 Waldorf Electronics
 * Copyright (C) 1994, 95, 96, 97, 98, 99, 2000, 01, 02, 03  Ralf Baechle
 * Copyright (C) 1996 Stoned Elipot
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2000, 2001, 2002, 2007	 Maciej W. Rozycki
 */
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/export.h>
#include <linux/screen_info.h>
#include <linux/memblock.h>
#include <linux/initrd.h>
#include <linux/root_dev.h>
#include <linux/highmem.h>
#include <linux/console.h>
#include <linux/pfn.h>
#include <linux/debugfs.h>
#include <linux/kexec.h>
#include <linux/sizes.h>
#include <linux/device.h>
#include <linux/dma-map-ops.h>
#include <linux/decompress/generic.h>
#include <linux/of_fdt.h>
#include <linux/dmi.h>
#include <linux/crash_dump.h>
#include <linux/pm.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/bugs.h>
#include <asm/cache.h>
#include <asm/cdmm.h>
#include <asm/cpu.h>
#include <asm/debug.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/smp-ops.h>
#include <asm/reboot.h>
#include <asm/prom.h>

#ifdef CONFIG_MIPS_ELF_APPENDED_DTB
char __section(".appended_dtb") __appended_dtb[0x100000];
#endif /* CONFIG_MIPS_ELF_APPENDED_DTB */

struct cpuinfo_mips cpu_data[NR_CPUS] __read_mostly;

EXPORT_SYMBOL(cpu_data);

#ifdef CONFIG_VT
struct screen_info screen_info;
#endif

/*
 * Setup information
 *
 * These are initialized so they are in the .data section
 */
unsigned long mips_machtype __read_mostly = MACH_UNKNOWN;

EXPORT_SYMBOL(mips_machtype);

static char __initdata command_line[COMMAND_LINE_SIZE];
char __initdata arcs_cmdline[COMMAND_LINE_SIZE];

#ifdef CONFIG_CMDLINE_BOOL
static const char builtin_cmdline[] __initconst = CONFIG_CMDLINE;
#else
static const char builtin_cmdline[] __initconst = "";
#endif

#define SF2000_SYSIO_KSEG1 ((volatile unsigned char *)CKSEG1ADDR(0x18800000))
#define SF2000_ROM_F_MOUNT_KSEG0 ((volatile unsigned int *)0x8101f044)
#define SF2000_PINMUX_L_OFF 0x4a0
#define SF2000_PINMUX_R_OFF 0x4e0
#define SF2000_GPIO_L_OUT_OFF 0x54
#define SF2000_GPIO_L_DIR_OFF 0x58
#define SF2000_GPIO_R_OUT_OFF 0xf4
#define SF2000_GPIO_R_DIR_OFF 0xf8
#define SF2000_PIN_L25 25
#define SF2000_PIN_R05 5
#define SF2000_STATUS_L25 (1u << SF2000_PIN_L25)
#define SF2000_BACKLIGHT_R05 (1u << SF2000_PIN_R05)
#define SF2000_STAGE_OFF_TICKS 0x00002000u
#define SF2000_STAGE_ON_TICKS 0x00001000u
#define SF2000_STAGE_GAP_TICKS 0x00004000u
#define SF2000_TICK_OFF_TICKS 0x00001000u
#define SF2000_TICK_GAP_TICKS 0x00002000u
#define SF2000_QEMU_DIRECT_DELAY_SHIFT 12
#define SF2000_GMA_KSEG1 ((volatile unsigned char *)CKSEG1ADDR(0x18808000))
#define SF2000_GMA_DESC_KSEG1 ((volatile unsigned int *)CKSEG1ADDR(0x00f00000))
#define SF2000_GMA_FB_KSEG1 ((volatile unsigned short *)CKSEG1ADDR(0x00f10000))
#define SF2000_GMA_CTL 0x300
#define SF2000_GMA_DMBA 0x304
#define SF2000_GMA_K 0x308
#define SF2000_GMA_MASK 0x350
#define SF2000_GMA_LINEBUF 0x3b8
#define SF2000_GMA_DESC_PHYS 0x00f00000u
#define SF2000_GMA_FB_PHYS 0x00f10000u
#define SF2000_GMA_RESERVE_PHYS 0x00f00000u
#define SF2000_GMA_RESERVE_SIZE 0x00040000u
#define SF2000_SCREEN_W 320
#define SF2000_SCREEN_H 240
#define SF2000_SCREEN_PITCH (SF2000_SCREEN_W * 2)
#define SF2000_PROGRESS_PHYS 0x013f0000u
#define SF2000_PROGRESS_RESERVE_SIZE 0x00020000u
#define SF2000_PROGRESS_KSEG1 ((volatile struct sf2000_progress_log *)CKSEG1ADDR(0x013f0000))
#define SF2000_LIVE_HANDOFF_KSEG1 ((volatile unsigned int *)CKSEG1ADDR(0x0140f000))
#define SF2000_PROGRESS_MAGIC 0x52504653u
#define SF2000_PROGRESS_VERSION 1u
#define SF2000_PROGRESS_ENTRIES 1024u
#define SF2000_PROGRESS_NAME_LEN 32u
#define SF2000_PROGRESS_LIVE_MAGIC 0x4c495645u
#define SF2000_LIVE_SECTOR_SIZE 512u
#define SF2000_ROM_DISK_WRITE_KSEG0 0x81020020u
#define SF2000_ROM_CACHE_FLUSH_KSEG0 0x810032f4u
#define SF2000_WDT_COUNT_KSEG1 ((volatile u32 *)CKSEG1ADDR(0x18818500))
#define SF2000_WDT_CONF_KSEG1 ((volatile u8 *)CKSEG1ADDR(0x18818504))
#define SF2000_WDT_BOOT_USEC 8000000u
#define SF2000_WDT_BOOT_TICKS ((SF2000_WDT_BOOT_USEC * 27u) / 128u)
#define SF2000_WDT_BOOT_COUNT (0u - SF2000_WDT_BOOT_TICKS)
#define SF2000_WDT_BOOT_CONF 0x26u
#define SF2000_WDT_RESTART_COUNT 0xfffff000u
#define SF2000_WDT_RESTART_CONF 0x67u

static unsigned int sf2000_screen_seq;
static unsigned int sf2000_live_write_count;
void sf2000_progress_mark(const char *name, unsigned int kind,
	unsigned int value);
void sf2000_syscall_mark(unsigned int nr);

typedef int (*sf2000_rom_disk_write_fn)(unsigned char pdrv, const void *buff,
	unsigned int sector, unsigned int count);
typedef void (*sf2000_rom_cache_flush_fn)(const void *addr, unsigned long len);

struct sf2000_progress_entry {
	unsigned int seq;
	unsigned int kind;
	unsigned int value;
	unsigned int name_ptr;
	char name[SF2000_PROGRESS_NAME_LEN];
};

struct sf2000_progress_log {
	unsigned int magic;
	unsigned int version;
	unsigned int seq;
	unsigned int write_index;
	unsigned int wrapped;
	unsigned int reserved[3];
	struct sf2000_progress_entry entries[SF2000_PROGRESS_ENTRIES];
};

static unsigned int sf2000_cp0_count(void)
{
	unsigned int count;

	asm volatile("mfc0 %0, $9" : "=r"(count));
	return count;
}

static void sf2000_machine_restart(char *command)
{
	volatile u32 *wdt_count = (volatile u32 *)CKSEG1ADDR(0x18818500);
	volatile u8 *wdt_conf = (volatile u8 *)CKSEG1ADDR(0x18818504);
	volatile u32 *raw = (volatile u32 *)CKSEG1ADDR(0x01400000);

	(void)command;
	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;

	pr_emerg("sf2000: watchdog restart\n");
	sf2000_progress_mark("sf2000-restart-entry", 15, *wdt_conf);
	raw[48] = 0x51510600;
	raw[49] = *wdt_count;
	raw[50] = *wdt_conf;
	local_irq_disable();
	*wdt_conf = 0;
	*wdt_count = SF2000_WDT_RESTART_COUNT;
	*wdt_conf = SF2000_WDT_RESTART_CONF;
	raw[51] = 0x51510601;
	raw[52] = *wdt_count;
	raw[53] = *wdt_conf;
	sf2000_progress_mark("sf2000-restart-armed", 15, *wdt_conf);
	while (1)
		;
}

static void sf2000_machine_power_off(void)
{
	sf2000_machine_restart("poweroff");
}

static void __init sf2000_restart_init(void)
{
	if (IS_ENABLED(CONFIG_MIPS_SF2000)) {
		_machine_restart = sf2000_machine_restart;
		pm_power_off = sf2000_machine_power_off;
	}
}

int sf2000_rom_handoff_present(void)
{
	unsigned int word = *SF2000_ROM_F_MOUNT_KSEG0;

	return word != 0 && word != 0xffffffffu;
}

static void sf2000_stage_delay(unsigned int ticks)
{
	unsigned int start = sf2000_cp0_count();

	if (!sf2000_rom_handoff_present())
		ticks >>= SF2000_QEMU_DIRECT_DELAY_SHIFT;

	if (!ticks)
		return;

	while ((unsigned int)(sf2000_cp0_count() - start) < ticks)
		asm volatile("nop");
}

static unsigned int sf2000_read32(unsigned int off)
{
	return *(volatile unsigned int *)(SF2000_SYSIO_KSEG1 + off);
}

static void sf2000_write32(unsigned int off, unsigned int value)
{
	*(volatile unsigned int *)(SF2000_SYSIO_KSEG1 + off) = value;
}

static void sf2000_write8(unsigned int off, unsigned char value)
{
	*(volatile unsigned char *)(SF2000_SYSIO_KSEG1 + off) = value;
}

static void sf2000_gma_write32(unsigned int off, unsigned int value)
{
	*(volatile unsigned int *)(SF2000_GMA_KSEG1 + off) = value;
}

static unsigned int sf2000_gma_read32(unsigned int off)
{
	return *(volatile unsigned int *)(SF2000_GMA_KSEG1 + off);
}

static void sf2000_screen_fill(unsigned short color)
{
	volatile unsigned short *fb = SF2000_GMA_FB_KSEG1;
	unsigned int i;

	for (i = 0; i < SF2000_SCREEN_W * SF2000_SCREEN_H; i++)
		fb[i] = color;
}

static void sf2000_screen_rect(unsigned int x, unsigned int y,
	unsigned int w, unsigned int h, unsigned short color)
{
	volatile unsigned short *fb = SF2000_GMA_FB_KSEG1;
	unsigned int yy;

	if (x >= SF2000_SCREEN_W || y >= SF2000_SCREEN_H)
		return;
	if (x + w > SF2000_SCREEN_W)
		w = SF2000_SCREEN_W - x;
	if (y + h > SF2000_SCREEN_H)
		h = SF2000_SCREEN_H - y;

	for (yy = 0; yy < h; yy++) {
		unsigned int xx;
		volatile unsigned short *line = fb + (y + yy) * SF2000_SCREEN_W + x;

		for (xx = 0; xx < w; xx++)
			line[xx] = color;
	}
}

static void sf2000_screen_digit(unsigned int x, unsigned int y,
	unsigned int digit, unsigned short color, unsigned short dim,
	unsigned int scale)
{
	static const unsigned char segments[16] = {
		0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
		0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71,
	};
	unsigned char bits = segments[digit & 0xf];
	unsigned int t = scale;
	unsigned int l = 6 * scale;
	unsigned int v = 10 * scale;

	sf2000_screen_rect(x, y, l, t, (bits & 0x01) ? color : dim);
	sf2000_screen_rect(x + l, y, t, v, (bits & 0x02) ? color : dim);
	sf2000_screen_rect(x + l, y + v + t, t, v, (bits & 0x04) ? color : dim);
	sf2000_screen_rect(x, y + 2 * v + 2 * t, l, t,
		(bits & 0x08) ? color : dim);
	sf2000_screen_rect(x - t, y + v + t, t, v,
		(bits & 0x10) ? color : dim);
	sf2000_screen_rect(x - t, y, t, v, (bits & 0x20) ? color : dim);
	sf2000_screen_rect(x, y + v + t, l, t,
		(bits & 0x40) ? color : dim);
}

static void sf2000_screen_present(void)
{
	volatile unsigned int *desc = SF2000_GMA_DESC_KSEG1;
	unsigned int ctl;

	desc[0] = (6u << 4) | (1u << 8) | (32u << 16) | (170u << 24);
	desc[1] = 0;
	desc[2] = ((SF2000_SCREEN_W - 1u) << 16);
	desc[3] = ((SF2000_SCREEN_H - 1u) << 16);
	desc[4] = (SF2000_SCREEN_H << 16) | SF2000_SCREEN_W;
	desc[5] = 0xffu | (SF2000_SCREEN_PITCH << 16);
	desc[6] = 0;
	desc[7] = SF2000_GMA_FB_PHYS;

	sf2000_write32(0x094, sf2000_read32(0x094) | (1u << 16));
	sf2000_gma_write32(SF2000_GMA_MASK, 1);
	sf2000_gma_write32(SF2000_GMA_LINEBUF, 0x0a);
	sf2000_gma_write32(SF2000_GMA_K, 0xff);
	ctl = sf2000_gma_read32(SF2000_GMA_CTL) | 1u;
	sf2000_gma_write32(SF2000_GMA_CTL, ctl);
	sf2000_gma_write32(SF2000_GMA_DMBA, SF2000_GMA_DESC_PHYS);
	sf2000_gma_write32(SF2000_GMA_MASK, 0);
	sf2000_gma_write32(SF2000_GMA_MASK, 1);
	sf2000_gma_write32(SF2000_GMA_CTL, ctl & ~(1u << 19));
	sf2000_gma_write32(SF2000_GMA_MASK, 0);
	sf2000_gma_write32(SF2000_GMA_MASK, 1);
	sf2000_gma_write32(SF2000_GMA_CTL, (ctl & ~(1u << 19)) | (1u << 18));
	sf2000_gma_write32(SF2000_GMA_MASK, 0);
}

static void sf2000_screen_mark(unsigned int kind, unsigned int detail)
{
	unsigned int seq = ++sf2000_screen_seq;
	unsigned short bg = kind == 1 ? 0x0200 : 0x2808;
	unsigned short fg = kind == 1 ? 0xffe0 : 0x07ff;
	unsigned short dim = kind == 1 ? 0x03ef : 0x4924;
	unsigned int i;

	sf2000_screen_fill(bg);
	sf2000_screen_rect(0, 0, SF2000_SCREEN_W, 18, fg);
	sf2000_screen_rect(0, 222, SF2000_SCREEN_W, 18, fg);
	for (i = 0; i < 8; i++)
		sf2000_screen_rect(12 + i * 38, 196, 26, 16,
			(i & 1) ? 0xf800 : ((i & 2) ? 0x07e0 : 0x001f));

	sf2000_screen_digit(26, 36, kind, 0xffff, dim, 4);
	sf2000_screen_digit(104, 54, (seq / 100) % 10, fg, dim, 7);
	sf2000_screen_digit(168, 54, (seq / 10) % 10, fg, dim, 7);
	sf2000_screen_digit(232, 54, seq % 10, fg, dim, 7);
	sf2000_screen_digit(210, 154, (detail / 10) % 10, 0xffff, dim, 4);
	sf2000_screen_digit(252, 154, detail % 10, 0xffff, dim, 4);

	sf2000_screen_present();
}

static void __init sf2000_reserve_diag_mem(void)
{
	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;

	memblock_reserve(SF2000_GMA_RESERVE_PHYS, SF2000_GMA_RESERVE_SIZE);
	memblock_reserve(SF2000_PROGRESS_PHYS, SF2000_PROGRESS_RESERVE_SIZE);
	pr_info("sf2000: reserved diag memory gma=%#x+%#x progress=%#x+%#x\n",
		SF2000_GMA_RESERVE_PHYS, SF2000_GMA_RESERVE_SIZE,
		SF2000_PROGRESS_PHYS, SF2000_PROGRESS_RESERVE_SIZE);
}

static void sf2000_progress_copy_name(volatile char *dst, const char *src)
{
	unsigned int i;

	for (i = 0; i + 1u < SF2000_PROGRESS_NAME_LEN && src[i] != '\0'; i++)
		dst[i] = src[i];
	dst[i] = '\0';
}

static void sf2000_live_putc(char *buf, unsigned int *pos, char ch)
{
	if (*pos + 1u >= SF2000_LIVE_SECTOR_SIZE)
		return;
	buf[*pos] = ch;
	(*pos)++;
	buf[*pos] = '\0';
}

static void sf2000_live_puts(char *buf, unsigned int *pos, const char *s)
{
	while (*s != '\0')
		sf2000_live_putc(buf, pos, *s++);
}

static void sf2000_live_hex(char *buf, unsigned int *pos, unsigned int value)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	sf2000_live_puts(buf, pos, "0x");
	for (shift = 28; shift >= 0; shift -= 4)
		sf2000_live_putc(buf, pos, digits[(value >> shift) & 0xf]);
}

static void sf2000_live_write_sector(const char *sector)
{
	sf2000_rom_disk_write_fn disk_write =
		(sf2000_rom_disk_write_fn)SF2000_ROM_DISK_WRITE_KSEG0;
	sf2000_rom_cache_flush_fn cache_flush =
		(sf2000_rom_cache_flush_fn)SF2000_ROM_CACHE_FLUSH_KSEG0;
	volatile unsigned int *handoff = SF2000_LIVE_HANDOFF_KSEG1;

	return;

	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;
	if (!sf2000_rom_handoff_present())
		return;
	if (*SF2000_WDT_CONF_KSEG1 != 0)
		return;
	if (handoff[0] != SF2000_PROGRESS_LIVE_MAGIC ||
	    handoff[2] == 0 || handoff[2] == 0xffffffffu)
		return;

	cache_flush(sector, SF2000_LIVE_SECTOR_SIZE);
	disk_write((unsigned char)handoff[1], sector, handoff[2], 1);
}

static void __attribute__((unused)) sf2000_live_progress_write(const char *name, unsigned int kind,
	unsigned int value, unsigned int seq)
{
	static char sector[SF2000_LIVE_SECTOR_SIZE] __aligned(16);
	volatile unsigned int *handoff = SF2000_LIVE_HANDOFF_KSEG1;
	unsigned int pos = 0;
	unsigned int i;

	for (i = 0; i < SF2000_LIVE_SECTOR_SIZE; i++)
		sector[i] = 0;

	sf2000_live_puts(sector, &pos, "sf2000-linux live progress\n");
	sf2000_live_puts(sector, &pos, "write=");
	sf2000_live_hex(sector, &pos, ++sf2000_live_write_count);
	sf2000_live_puts(sector, &pos, " seq=");
	sf2000_live_hex(sector, &pos, seq);
	sf2000_live_puts(sector, &pos, " kind=");
	sf2000_live_hex(sector, &pos, kind);
	sf2000_live_puts(sector, &pos, " value=");
	sf2000_live_hex(sector, &pos, value);
	sf2000_live_puts(sector, &pos, " pdrv=");
	sf2000_live_hex(sector, &pos, handoff[1]);
	sf2000_live_puts(sector, &pos, " lba=");
	sf2000_live_hex(sector, &pos, handoff[2]);
	sf2000_live_puts(sector, &pos, "\nname=");
	sf2000_live_puts(sector, &pos, name);
	sf2000_live_puts(sector, &pos, "\nwdt_count=");
	sf2000_live_hex(sector, &pos, *SF2000_WDT_COUNT_KSEG1);
	sf2000_live_puts(sector, &pos, " wdt_conf=");
	sf2000_live_hex(sector, &pos, *SF2000_WDT_CONF_KSEG1);
	sf2000_live_puts(sector, &pos, "\n");

	sf2000_live_write_sector(sector);
}

void sf2000_live_user_write(unsigned int fd, const void *buf,
	unsigned int count, int ret)
{
	static char sector[SF2000_LIVE_SECTOR_SIZE] __aligned(16);
	volatile struct sf2000_progress_log *log = SF2000_PROGRESS_KSEG1;
	const unsigned char *src = buf;
	unsigned int pos = 0;
	unsigned int limit;
	unsigned int i;
	unsigned int addr = (unsigned int)buf;

	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;
	if (ret <= 0)
		return;
	if ((addr & 0xf0000000u) != 0x80000000u)
		return;

	for (i = 0; i < SF2000_LIVE_SECTOR_SIZE; i++)
		sector[i] = 0;

	sf2000_live_puts(sector, &pos, "sf2000-linux live userspace write\n");
	sf2000_live_puts(sector, &pos, "write=");
	sf2000_live_hex(sector, &pos, ++sf2000_live_write_count);
	sf2000_live_puts(sector, &pos, " seq=");
	sf2000_live_hex(sector, &pos, log->seq);
	sf2000_live_puts(sector, &pos, " fd=");
	sf2000_live_hex(sector, &pos, fd);
	sf2000_live_puts(sector, &pos, " count=");
	sf2000_live_hex(sector, &pos, count);
	sf2000_live_puts(sector, &pos, " ret=");
	sf2000_live_hex(sector, &pos, (unsigned int)ret);
	sf2000_live_puts(sector, &pos, "\n");

	limit = (unsigned int)ret;
	if (limit > count)
		limit = count;
	if (limit > 360u)
		limit = 360u;
	for (i = 0; i < limit; i++) {
		unsigned char ch = src[i];

		if (ch == '\n' || ch == '\r' || ch == '\t' ||
		    (ch >= ' ' && ch <= '~'))
			sf2000_live_putc(sector, &pos, (char)ch);
		else
			sf2000_live_putc(sector, &pos, '.');
	}
	sf2000_live_puts(sector, &pos, "\n");

	sf2000_live_write_sector(sector);
}
EXPORT_SYMBOL(sf2000_live_user_write);

static void sf2000_watchdog_pet(void)
{
	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;
	if (!sf2000_rom_handoff_present())
		return;

	*SF2000_WDT_COUNT_KSEG1 = SF2000_WDT_BOOT_COUNT;
}

static void sf2000_watchdog_arm(const char *name)
{
	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;
	if (!sf2000_rom_handoff_present())
		return;

	*SF2000_WDT_CONF_KSEG1 = 0;
	*SF2000_WDT_COUNT_KSEG1 = SF2000_WDT_BOOT_COUNT;
	*SF2000_WDT_CONF_KSEG1 = SF2000_WDT_BOOT_CONF;
	pr_info("sf2000: early watchdog armed count=%#x conf=%#x timeout_us=%u\n",
		SF2000_WDT_BOOT_COUNT, SF2000_WDT_BOOT_CONF,
		SF2000_WDT_BOOT_USEC);
	sf2000_progress_mark(name, 3, SF2000_WDT_BOOT_COUNT);
}

static int __init sf2000_builtin_cmdline_has(const char *needle)
{
	unsigned int i;
	unsigned int j;

	for (i = 0; builtin_cmdline[i] != '\0'; i++) {
		for (j = 0; needle[j] != '\0' &&
		     builtin_cmdline[i + j] == needle[j]; j++)
			;
		if (needle[j] == '\0')
			return 1;
	}

	return 0;
}

static void __init sf2000_watchdog_test_hang(void)
{
	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;
	if (!sf2000_builtin_cmdline_has("sf2000_early_wdt_test=1"))
		return;

	pr_emerg("sf2000: early watchdog test hang\n");
	sf2000_progress_mark("early-watchdog-test-hang", 3,
		SF2000_WDT_BOOT_COUNT);
	while (1)
		asm volatile("nop");
}

void sf2000_progress_mark(const char *name, unsigned int kind,
	unsigned int value)
{
	volatile struct sf2000_progress_log *log = SF2000_PROGRESS_KSEG1;
	volatile struct sf2000_progress_entry *entry;
	unsigned int index;
	unsigned int seq;

	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		return;
	if (log->magic != SF2000_PROGRESS_MAGIC ||
	    log->version != SF2000_PROGRESS_VERSION)
		return;

	index = log->write_index;
	if (index >= SF2000_PROGRESS_ENTRIES)
		index = 0;

	seq = log->seq + 1u;
	entry = &log->entries[index];
	entry->seq = seq;
	entry->kind = kind;
	entry->value = value;
	entry->name_ptr = (unsigned int)name;
	sf2000_progress_copy_name(entry->name, name);

	index++;
	if (index >= SF2000_PROGRESS_ENTRIES) {
		index = 0;
		log->wrapped = 1;
	}
	log->write_index = index;
	log->seq = seq;
	sf2000_watchdog_pet();
}
EXPORT_SYMBOL(sf2000_progress_mark);

void sf2000_syscall_mark(unsigned int nr)
{
	(void)nr;
}

static void sf2000_status_led_set(int on)
{
	unsigned int out;
	unsigned int dir;

	sf2000_write8(SF2000_PINMUX_L_OFF + SF2000_PIN_L25, 0);
	dir = sf2000_read32(SF2000_GPIO_L_DIR_OFF);
	sf2000_write32(SF2000_GPIO_L_DIR_OFF, dir | SF2000_STATUS_L25);

	out = sf2000_read32(SF2000_GPIO_L_OUT_OFF);
	if (on)
		out |= SF2000_STATUS_L25;
	else
		out &= ~SF2000_STATUS_L25;
	sf2000_write32(SF2000_GPIO_L_OUT_OFF, out);
}

static void sf2000_backlight_set(int on)
{
	unsigned int out;
	unsigned int dir;

	sf2000_write8(SF2000_PINMUX_R_OFF + SF2000_PIN_R05, 0);
	dir = sf2000_read32(SF2000_GPIO_R_DIR_OFF);
	sf2000_write32(SF2000_GPIO_R_DIR_OFF, dir | SF2000_BACKLIGHT_R05);

	out = sf2000_read32(SF2000_GPIO_R_OUT_OFF);
	if (on)
		out &= ~SF2000_BACKLIGHT_R05;
	else
		out |= SF2000_BACKLIGHT_R05;
	sf2000_write32(SF2000_GPIO_R_OUT_OFF, out);
}

void sf2000_visible_stage(const char *name, unsigned int pulses)
{
	unsigned int i;

	pr_info("sf2000: visible stage %s pulses=%u\n", name, pulses);
	sf2000_progress_mark(name, 1, pulses);
	sf2000_screen_mark(1, pulses);
	sf2000_backlight_set(1);
	sf2000_status_led_set(0);
	sf2000_watchdog_pet();
	sf2000_stage_delay(SF2000_STAGE_GAP_TICKS);
	for (i = 0; i < pulses; i++) {
		sf2000_watchdog_pet();
		sf2000_backlight_set(0);
		sf2000_status_led_set(1);
		sf2000_stage_delay(SF2000_STAGE_OFF_TICKS);
		sf2000_watchdog_pet();
		sf2000_backlight_set(1);
		sf2000_status_led_set(0);
		sf2000_stage_delay(SF2000_STAGE_ON_TICKS);
	}
	sf2000_status_led_set(0);
	sf2000_watchdog_pet();
	sf2000_stage_delay(SF2000_STAGE_GAP_TICKS);
}

void sf2000_visible_stage_nolog(const char *name, unsigned int pulses)
{
	sf2000_visible_stage(name, pulses);
}

void sf2000_visible_tick(const char *name)
{
	pr_info("sf2000: visible tick %s\n", name);
	sf2000_progress_mark(name, 2, 0);
	sf2000_screen_mark(2, 0);
	sf2000_backlight_set(1);
	sf2000_status_led_set(0);
	sf2000_stage_delay(SF2000_TICK_GAP_TICKS);
	sf2000_watchdog_pet();
	sf2000_backlight_set(0);
	sf2000_status_led_set(1);
	sf2000_stage_delay(SF2000_TICK_OFF_TICKS);
	sf2000_watchdog_pet();
	sf2000_backlight_set(1);
	sf2000_status_led_set(0);
	sf2000_stage_delay(SF2000_TICK_GAP_TICKS);
}

/*
 * mips_io_port_base is the begin of the address space to which x86 style
 * I/O ports are mapped.
 */
unsigned long mips_io_port_base = -1;
EXPORT_SYMBOL(mips_io_port_base);

static struct resource code_resource = { .name = "Kernel code", };
static struct resource data_resource = { .name = "Kernel data", };
static struct resource bss_resource = { .name = "Kernel bss", };

unsigned long __kaslr_offset __ro_after_init;
EXPORT_SYMBOL(__kaslr_offset);

static void *detect_magic __initdata = detect_memory_region;

#ifdef CONFIG_MIPS_AUTO_PFN_OFFSET
unsigned long ARCH_PFN_OFFSET;
EXPORT_SYMBOL(ARCH_PFN_OFFSET);
#endif

void __init detect_memory_region(phys_addr_t start, phys_addr_t sz_min, phys_addr_t sz_max)
{
	void *dm = &detect_magic;
	phys_addr_t size;

	for (size = sz_min; size < sz_max; size <<= 1) {
		if (!memcmp(dm, dm + size, sizeof(detect_magic)))
			break;
	}

	pr_debug("Memory: %lluMB of RAM detected at 0x%llx (min: %lluMB, max: %lluMB)\n",
		((unsigned long long) size) / SZ_1M,
		(unsigned long long) start,
		((unsigned long long) sz_min) / SZ_1M,
		((unsigned long long) sz_max) / SZ_1M);

	memblock_add(start, size);
}

/*
 * Manage initrd
 */
#ifdef CONFIG_BLK_DEV_INITRD

static int __init rd_start_early(char *p)
{
	unsigned long start = memparse(p, &p);

#ifdef CONFIG_64BIT
	/* Guess if the sign extension was forgotten by bootloader */
	if (start < XKPHYS)
		start = (int)start;
#endif
	initrd_start = start;
	initrd_end += start;
	return 0;
}
early_param("rd_start", rd_start_early);

static int __init rd_size_early(char *p)
{
	initrd_end += memparse(p, &p);
	return 0;
}
early_param("rd_size", rd_size_early);

/* it returns the next free pfn after initrd */
static unsigned long __init init_initrd(void)
{
	unsigned long end;

	/*
	 * Board specific code or command line parser should have
	 * already set up initrd_start and initrd_end. In these cases
	 * perfom sanity checks and use them if all looks good.
	 */
	if (!initrd_start || initrd_end <= initrd_start)
		goto disable;

	if (initrd_start & ~PAGE_MASK) {
		pr_err("initrd start must be page aligned\n");
		goto disable;
	}
	if (initrd_start < PAGE_OFFSET) {
		pr_err("initrd start < PAGE_OFFSET\n");
		goto disable;
	}

	/*
	 * Sanitize initrd addresses. For example firmware
	 * can't guess if they need to pass them through
	 * 64-bits values if the kernel has been built in pure
	 * 32-bit. We need also to switch from KSEG0 to XKPHYS
	 * addresses now, so the code can now safely use __pa().
	 */
	end = __pa(initrd_end);
	initrd_end = (unsigned long)__va(end);
	initrd_start = (unsigned long)__va(__pa(initrd_start));

	ROOT_DEV = Root_RAM0;
	return PFN_UP(end);
disable:
	initrd_start = 0;
	initrd_end = 0;
	return 0;
}

/* In some conditions (e.g. big endian bootloader with a little endian
   kernel), the initrd might appear byte swapped.  Try to detect this and
   byte swap it if needed.  */
static void __init maybe_bswap_initrd(void)
{
#if defined(CONFIG_CPU_CAVIUM_OCTEON)
	u64 buf;

	/* Check for CPIO signature */
	if (!memcmp((void *)initrd_start, "070701", 6))
		return;

	/* Check for compressed initrd */
	if (decompress_method((unsigned char *)initrd_start, 8, NULL))
		return;

	/* Try again with a byte swapped header */
	buf = swab64p((u64 *)initrd_start);
	if (!memcmp(&buf, "070701", 6) ||
	    decompress_method((unsigned char *)(&buf), 8, NULL)) {
		unsigned long i;

		pr_info("Byteswapped initrd detected\n");
		for (i = initrd_start; i < ALIGN(initrd_end, 8); i += 8)
			swab64s((u64 *)i);
	}
#endif
}

static void __init finalize_initrd(void)
{
	unsigned long size = initrd_end - initrd_start;

	if (size == 0) {
		printk(KERN_INFO "Initrd not found or empty");
		goto disable;
	}
	if (__pa(initrd_end) > PFN_PHYS(max_low_pfn)) {
		printk(KERN_ERR "Initrd extends beyond end of memory");
		goto disable;
	}

	maybe_bswap_initrd();

	memblock_reserve(__pa(initrd_start), size);
	initrd_below_start_ok = 1;

	pr_info("Initial ramdisk at: 0x%lx (%lu bytes)\n",
		initrd_start, size);
	return;
disable:
	printk(KERN_CONT " - disabling initrd\n");
	initrd_start = 0;
	initrd_end = 0;
}

#else  /* !CONFIG_BLK_DEV_INITRD */

static unsigned long __init init_initrd(void)
{
	return 0;
}

#define finalize_initrd()	do {} while (0)

#endif

/*
 * Initialize the bootmem allocator. It also setup initrd related data
 * if needed.
 */
#if defined(CONFIG_SGI_IP27) || (defined(CONFIG_CPU_LOONGSON64) && defined(CONFIG_NUMA))

static void __init bootmem_init(void)
{
	init_initrd();
	finalize_initrd();
}

#else  /* !CONFIG_SGI_IP27 */

static void __init bootmem_init(void)
{
	phys_addr_t ramstart, ramend;
	unsigned long start, end;
	int i;

	ramstart = memblock_start_of_DRAM();
	ramend = memblock_end_of_DRAM();

	/*
	 * Sanity check any INITRD first. We don't take it into account
	 * for bootmem setup initially, rely on the end-of-kernel-code
	 * as our memory range starting point. Once bootmem is inited we
	 * will reserve the area used for the initrd.
	 */
	init_initrd();

	/* Reserve memory occupied by kernel. */
	memblock_reserve(__pa_symbol(&_text),
			__pa_symbol(&_end) - __pa_symbol(&_text));

	/* max_low_pfn is not a number of pages but the end pfn of low mem */

#ifdef CONFIG_MIPS_AUTO_PFN_OFFSET
	ARCH_PFN_OFFSET = PFN_UP(ramstart);
#else
	/*
	 * Reserve any memory between the start of RAM and PHYS_OFFSET
	 */
	if (ramstart > PHYS_OFFSET)
		memblock_reserve(PHYS_OFFSET, ramstart - PHYS_OFFSET);

	if (PFN_UP(ramstart) > ARCH_PFN_OFFSET) {
		pr_info("Wasting %lu bytes for tracking %lu unused pages\n",
			(unsigned long)((PFN_UP(ramstart) - ARCH_PFN_OFFSET) * sizeof(struct page)),
			(unsigned long)(PFN_UP(ramstart) - ARCH_PFN_OFFSET));
	}
#endif

	min_low_pfn = ARCH_PFN_OFFSET;
	max_pfn = PFN_DOWN(ramend);
	for_each_mem_pfn_range(i, MAX_NUMNODES, &start, &end, NULL) {
		/*
		 * Skip highmem here so we get an accurate max_low_pfn if low
		 * memory stops short of high memory.
		 * If the region overlaps HIGHMEM_START, end is clipped so
		 * max_pfn excludes the highmem portion.
		 */
		if (start >= PFN_DOWN(HIGHMEM_START))
			continue;
		if (end > PFN_DOWN(HIGHMEM_START))
			end = PFN_DOWN(HIGHMEM_START);
		if (end > max_low_pfn)
			max_low_pfn = end;
	}

	if (min_low_pfn >= max_low_pfn)
		panic("Incorrect memory mapping !!!");

	if (max_pfn > PFN_DOWN(HIGHMEM_START)) {
#ifdef CONFIG_HIGHMEM
		highstart_pfn = PFN_DOWN(HIGHMEM_START);
		highend_pfn = max_pfn;
#else
		max_low_pfn = PFN_DOWN(HIGHMEM_START);
		max_pfn = max_low_pfn;
#endif
	}

	/*
	 * Reserve initrd memory if needed.
	 */
	finalize_initrd();
}

#endif	/* CONFIG_SGI_IP27 */

static int usermem __initdata;

static int __init early_parse_mem(char *p)
{
	phys_addr_t start, size;

	/*
	 * If a user specifies memory size, we
	 * blow away any automatically generated
	 * size.
	 */
	if (usermem == 0) {
		usermem = 1;
		memblock_remove(memblock_start_of_DRAM(),
			memblock_end_of_DRAM() - memblock_start_of_DRAM());
	}
	start = 0;
	size = memparse(p, &p);
	if (*p == '@')
		start = memparse(p + 1, &p);

	memblock_add(start, size);

	return 0;
}
early_param("mem", early_parse_mem);

static int __init early_parse_memmap(char *p)
{
	char *oldp;
	u64 start_at, mem_size;

	if (!p)
		return -EINVAL;

	if (!strncmp(p, "exactmap", 8)) {
		pr_err("\"memmap=exactmap\" invalid on MIPS\n");
		return 0;
	}

	oldp = p;
	mem_size = memparse(p, &p);
	if (p == oldp)
		return -EINVAL;

	if (*p == '@') {
		start_at = memparse(p+1, &p);
		memblock_add(start_at, mem_size);
	} else if (*p == '#') {
		pr_err("\"memmap=nn#ss\" (force ACPI data) invalid on MIPS\n");
		return -EINVAL;
	} else if (*p == '$') {
		start_at = memparse(p+1, &p);
		memblock_add(start_at, mem_size);
		memblock_reserve(start_at, mem_size);
	} else {
		pr_err("\"memmap\" invalid format!\n");
		return -EINVAL;
	}

	if (*p == '\0') {
		usermem = 1;
		return 0;
	} else
		return -EINVAL;
}
early_param("memmap", early_parse_memmap);

static void __init mips_reserve_vmcore(void)
{
#ifdef CONFIG_PROC_VMCORE
	phys_addr_t start, end;
	u64 i;

	if (!elfcorehdr_size) {
		for_each_mem_range(i, &start, &end) {
			if (elfcorehdr_addr >= start && elfcorehdr_addr < end) {
				/*
				 * Reserve from the elf core header to the end of
				 * the memory segment, that should all be kdump
				 * reserved memory.
				 */
				elfcorehdr_size = end - elfcorehdr_addr;
				break;
			}
		}
	}

	pr_info("Reserving %ldKB of memory at %ldKB for kdump\n",
		(unsigned long)elfcorehdr_size >> 10, (unsigned long)elfcorehdr_addr >> 10);

	memblock_reserve(elfcorehdr_addr, elfcorehdr_size);
#endif
}

#ifdef CONFIG_KEXEC

/* 64M alignment for crash kernel regions */
#define CRASH_ALIGN	SZ_64M
#define CRASH_ADDR_MAX	SZ_512M

static void __init mips_parse_crashkernel(void)
{
	unsigned long long total_mem;
	unsigned long long crash_size, crash_base;
	int ret;

	total_mem = memblock_phys_mem_size();
	ret = parse_crashkernel(boot_command_line, total_mem,
				&crash_size, &crash_base);
	if (ret != 0 || crash_size <= 0)
		return;

	if (crash_base <= 0) {
		crash_base = memblock_find_in_range(CRASH_ALIGN, CRASH_ADDR_MAX,
							crash_size, CRASH_ALIGN);
		if (!crash_base) {
			pr_warn("crashkernel reservation failed - No suitable area found.\n");
			return;
		}
	} else {
		unsigned long long start;

		start = memblock_find_in_range(crash_base, crash_base + crash_size,
						crash_size, 1);
		if (start != crash_base) {
			pr_warn("Invalid memory region reserved for crash kernel\n");
			return;
		}
	}

	crashk_res.start = crash_base;
	crashk_res.end	 = crash_base + crash_size - 1;
}

static void __init request_crashkernel(struct resource *res)
{
	int ret;

	if (crashk_res.start == crashk_res.end)
		return;

	ret = request_resource(res, &crashk_res);
	if (!ret)
		pr_info("Reserving %ldMB of memory at %ldMB for crashkernel\n",
			(unsigned long)(resource_size(&crashk_res) >> 20),
			(unsigned long)(crashk_res.start  >> 20));
}
#else /* !defined(CONFIG_KEXEC)		*/
static void __init mips_parse_crashkernel(void)
{
}

static void __init request_crashkernel(struct resource *res)
{
}
#endif /* !defined(CONFIG_KEXEC)  */

static void __init check_kernel_sections_mem(void)
{
	phys_addr_t start = __pa_symbol(&_text);
	phys_addr_t size = __pa_symbol(&_end) - start;

	if (!memblock_is_region_memory(start, size)) {
		pr_info("Kernel sections are not in the memory maps\n");
		memblock_add(start, size);
	}
}

static void __init bootcmdline_append(const char *s, size_t max)
{
	if (!s[0] || !max)
		return;

	if (boot_command_line[0])
		strlcat(boot_command_line, " ", COMMAND_LINE_SIZE);

	strlcat(boot_command_line, s, max);
}

#ifdef CONFIG_OF_EARLY_FLATTREE

static int __init bootcmdline_scan_chosen(unsigned long node, const char *uname,
					  int depth, void *data)
{
	bool *dt_bootargs = data;
	const char *p;
	int l;

	if (depth != 1 || !data ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	p = of_get_flat_dt_prop(node, "bootargs", &l);
	if (p != NULL && l > 0) {
		bootcmdline_append(p, min(l, COMMAND_LINE_SIZE));
		*dt_bootargs = true;
	}

	return 1;
}

#endif /* CONFIG_OF_EARLY_FLATTREE */

static void __init bootcmdline_init(void)
{
	bool dt_bootargs = false;

	/*
	 * If CMDLINE_OVERRIDE is enabled then initializing the command line is
	 * trivial - we simply use the built-in command line unconditionally &
	 * unmodified.
	 */
	if (IS_ENABLED(CONFIG_CMDLINE_OVERRIDE)) {
		strlcpy(boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
		return;
	}

	/*
	 * If the user specified a built-in command line &
	 * MIPS_CMDLINE_BUILTIN_EXTEND, then the built-in command line is
	 * prepended to arguments from the bootloader or DT so we'll copy them
	 * to the start of boot_command_line here. Otherwise, empty
	 * boot_command_line to undo anything early_init_dt_scan_chosen() did.
	 */
	if (IS_ENABLED(CONFIG_MIPS_CMDLINE_BUILTIN_EXTEND))
		strlcpy(boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
	else
		boot_command_line[0] = 0;

#ifdef CONFIG_OF_EARLY_FLATTREE
	/*
	 * If we're configured to take boot arguments from DT, look for those
	 * now.
	 */
	if (IS_ENABLED(CONFIG_MIPS_CMDLINE_FROM_DTB) ||
	    IS_ENABLED(CONFIG_MIPS_CMDLINE_DTB_EXTEND))
		of_scan_flat_dt(bootcmdline_scan_chosen, &dt_bootargs);
#endif

	/*
	 * If we didn't get any arguments from DT (regardless of whether that's
	 * because we weren't configured to look for them, or because we looked
	 * & found none) then we'll take arguments from the bootloader.
	 * plat_mem_setup() should have filled arcs_cmdline with arguments from
	 * the bootloader.
	 */
	if (IS_ENABLED(CONFIG_MIPS_CMDLINE_DTB_EXTEND) || !dt_bootargs)
		bootcmdline_append(arcs_cmdline, COMMAND_LINE_SIZE);

	/*
	 * If the user specified a built-in command line & we didn't already
	 * prepend it, we append it to boot_command_line here.
	 */
	if (IS_ENABLED(CONFIG_CMDLINE_BOOL) &&
	    !IS_ENABLED(CONFIG_MIPS_CMDLINE_BUILTIN_EXTEND))
		bootcmdline_append(builtin_cmdline, COMMAND_LINE_SIZE);
}

/*
 * arch_mem_init - initialize memory management subsystem
 *
 *  o plat_mem_setup() detects the memory configuration and will record detected
 *    memory areas using memblock_add.
 *
 * At this stage the memory configuration of the system is known to the
 * kernel but generic memory management system is still entirely uninitialized.
 *
 *  o bootmem_init()
 *  o sparse_init()
 *  o paging_init()
 *  o dma_contiguous_reserve()
 *
 * At this stage the bootmem allocator is ready to use.
 *
 * NOTE: historically plat_mem_setup did the entire platform initialization.
 *	 This was rather impractical because it meant plat_mem_setup had to
 * get away without any kind of memory allocator.  To keep old code from
 * breaking plat_setup was just renamed to plat_mem_setup and a second platform
 * initialization hook for anything else was introduced.
 */
static void __init arch_mem_init(char **cmdline_p)
{
	/* call board setup routine */
	plat_mem_setup();
	memblock_set_bottom_up(true);

	bootcmdline_init();
	strlcpy(command_line, boot_command_line, COMMAND_LINE_SIZE);
	*cmdline_p = command_line;

	parse_early_param();

	if (usermem)
		pr_info("User-defined physical RAM map overwrite\n");

	check_kernel_sections_mem();

	early_init_fdt_reserve_self();
	early_init_fdt_scan_reserved_mem();
	sf2000_reserve_diag_mem();

#ifndef CONFIG_NUMA
	memblock_set_node(0, PHYS_ADDR_MAX, &memblock.memory, 0);
#endif
	bootmem_init();

	/*
	 * Prevent memblock from allocating high memory.
	 * This cannot be done before max_low_pfn is detected, so up
	 * to this point is possible to only reserve physical memory
	 * with memblock_reserve; memblock_alloc* can be used
	 * only after this point
	 */
	memblock_set_current_limit(PFN_PHYS(max_low_pfn));

	mips_reserve_vmcore();

	mips_parse_crashkernel();
#ifdef CONFIG_KEXEC
	if (crashk_res.start != crashk_res.end)
		memblock_reserve(crashk_res.start, resource_size(&crashk_res));
#endif
	device_tree_init();

	/*
	 * In order to reduce the possibility of kernel panic when failed to
	 * get IO TLB memory under CONFIG_SWIOTLB, it is better to allocate
	 * low memory as small as possible before plat_swiotlb_setup(), so
	 * make sparse_init() using top-down allocation.
	 */
	memblock_set_bottom_up(false);
	sparse_init();
	memblock_set_bottom_up(true);

	plat_swiotlb_setup();

	dma_contiguous_reserve(PFN_PHYS(max_low_pfn));

	/* Reserve for hibernation. */
	memblock_reserve(__pa_symbol(&__nosave_begin),
		__pa_symbol(&__nosave_end) - __pa_symbol(&__nosave_begin));

	early_memtest(PFN_PHYS(ARCH_PFN_OFFSET), PFN_PHYS(max_low_pfn));
}

static void __init resource_init(void)
{
	phys_addr_t start, end;
	u64 i;

	if (UNCAC_BASE != IO_BASE)
		return;

	code_resource.start = __pa_symbol(&_text);
	code_resource.end = __pa_symbol(&_etext) - 1;
	data_resource.start = __pa_symbol(&_etext);
	data_resource.end = __pa_symbol(&_edata) - 1;
	bss_resource.start = __pa_symbol(&__bss_start);
	bss_resource.end = __pa_symbol(&__bss_stop) - 1;

	for_each_mem_range(i, &start, &end) {
		struct resource *res;

		res = memblock_alloc(sizeof(struct resource), SMP_CACHE_BYTES);
		if (!res)
			panic("%s: Failed to allocate %zu bytes\n", __func__,
			      sizeof(struct resource));

		res->start = start;
		/*
		 * In memblock, end points to the first byte after the
		 * range while in resourses, end points to the last byte in
		 * the range.
		 */
		res->end = end - 1;
		res->flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;
		res->name = "System RAM";

		request_resource(&iomem_resource, res);

		/*
		 *  We don't know which RAM region contains kernel data,
		 *  so we try it repeatedly and let the resource manager
		 *  test it.
		 */
		request_resource(res, &code_resource);
		request_resource(res, &data_resource);
		request_resource(res, &bss_resource);
		request_crashkernel(res);
	}
}

#ifdef CONFIG_SMP
static void __init prefill_possible_map(void)
{
	int i, possible = num_possible_cpus();

	if (possible > nr_cpu_ids)
		possible = nr_cpu_ids;

	for (i = 0; i < possible; i++)
		set_cpu_possible(i, true);
	for (; i < NR_CPUS; i++)
		set_cpu_possible(i, false);

	nr_cpu_ids = possible;
}
#else
static inline void prefill_possible_map(void) {}
#endif

void __init setup_arch(char **cmdline_p)
{
	sf2000_restart_init();
	sf2000_watchdog_arm("early-watchdog-armed");
	sf2000_watchdog_test_hang();
	sf2000_visible_stage("mips-setup-entry", 3);
	cpu_probe();
	sf2000_visible_tick("mips-after-cpu-probe");

	if (!IS_ENABLED(CONFIG_MIPS_SF2000))
		mips_cm_probe();
	sf2000_visible_tick("mips-after-cm-probe");

	prom_init();
	sf2000_visible_tick("mips-after-prom-init");

	setup_early_fdc_console();
#ifdef CONFIG_EARLY_PRINTK
	setup_early_printk();
#endif
	sf2000_visible_tick("mips-after-early-console");

	cpu_report();
	check_bugs_early();
	sf2000_visible_tick("mips-after-cpu-report");

#if defined(CONFIG_VT)
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#endif
#endif

	arch_mem_init(cmdline_p);
	sf2000_visible_tick("mips-after-arch-mem-init");

	dmi_setup();

	resource_init();
	plat_smp_setup();
	prefill_possible_map();
	sf2000_visible_tick("mips-after-resource-init");

	cpu_cache_init();
	sf2000_visible_tick("mips-after-cpu-cache-init");

	paging_init();
	sf2000_visible_tick("mips-after-paging-init");

	memblock_dump_all();
	sf2000_visible_tick("mips-after-memblock-dump");

	sf2000_visible_stage("mips-setup-done", 4);
}

unsigned long kernelsp[NR_CPUS];
unsigned long fw_arg0, fw_arg1, fw_arg2, fw_arg3;

#ifdef CONFIG_DEBUG_FS
struct dentry *mips_debugfs_dir;
static int __init debugfs_mips(void)
{
	mips_debugfs_dir = debugfs_create_dir("mips", NULL);
	return 0;
}
arch_initcall(debugfs_mips);
#endif

#ifdef CONFIG_DMA_NONCOHERENT
static int __init setcoherentio(char *str)
{
	dma_default_coherent = true;
	pr_info("Hardware DMA cache coherency (command line)\n");
	return 0;
}
early_param("coherentio", setcoherentio);

static int __init setnocoherentio(char *str)
{
	dma_default_coherent = true;
	pr_info("Software DMA cache coherency (command line)\n");
	return 0;
}
early_param("nocoherentio", setnocoherentio);
#endif
