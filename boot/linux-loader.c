/* SPDX-License-Identifier: MIT */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned int usize;
typedef unsigned long uintptr;

#define EI_NIDENT 16
#define EI_CLASS 4
#define EI_DATA 5
#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define EM_MIPS 8
#define PT_LOAD 1

#define KSEG0_BASE 0x80000000u
#define KSEG1_BASE 0xa0000000u
#define RAM_TOP 0x88000000u
#define UART_BASE 0xb8818600u
#define UART_THR 0
#define UART_LSR 5
#define UART_LSR_THRE 0x20
/* HC15xx primary-cache contract (NuttX sf2000_sdio.c): 16-byte lines, two
 * ways, 0x4000-byte index span (32 KiB total).  Config1 is unreliable on
 * this core, so the I-cache invalidate sweep uses these fixed values: a
 * contiguous 0x8000-byte KSEG0 sweep at 16-byte steps visits every
 * (set, way) once (the second 0x4000 carries the bit-14 way select).  The
 * D-cache is not swept by index: the Index Writeback Invalidate op is
 * unreliable here, so freshly copied images are flushed with the vendor ROM
 * hit-writeback routine (0x810032f4) instead. */
#define CACHE_LINE 16u
#define CACHE_INDEX_SPAN 0x4000u
#define CACHE_WAY_SPAN 0x4000u
#define CACHE_TOTAL_SPAN (CACHE_INDEX_SPAN + CACHE_WAY_SPAN)
#define PINMUX_R05 0xb88004e5u
#define PINMUX_L25 0xb88004b9u
#define GPIO_L_OUT 0xb8800054u
#define GPIO_L_DIR 0xb8800058u
#define GPIO_R_OUT 0xb88000f4u
#define GPIO_R_DIR 0xb88000f8u
#define MAPPING_REG 0xb8800220u
#define SYSIO_CHIP_ID 0xb8800000u
#define SYSIO_SCPU_SELECT 0xb8800074u
#define SYSIO_SCPU_PLL_ENABLE 0xb880007cu
#define SYSIO_SCPU_PLL_CONTROL 0xb8800380u
#define HC1512_CHIP_ID 0x1512u
#define SCPU_TARGET_MHZ 918u
#define SCPU_PLL_MCTRL2 0x8153u
#define WDT0_COUNT 0xb8818500u
#define WDT0_CONF 0xb8818504u
#define WDT_BOOT_USEC 8000000u
#define WDT_BOOT_TICKS ((WDT_BOOT_USEC * 27u) / 128u)
#define WDT_BOOT_COUNT (0u - WDT_BOOT_TICKS)
#define WDT_BOOT_CONF 0x26u
#define BOOTROM_BASE 0xbfc00000u
#define ROM_F_MOUNT_ADDR 0x8101f044u
#define ROM_F_OPEN_ADDR 0x8101f0d4u
#define ROM_DISK_READ_ADDR 0x8101ffb4u
#define ROM_DISK_WRITE_ADDR 0x81020020u
#define ROM_CACHE_FLUSH_ADDR 0x810032f4u
#define BACKLIGHT_R05 (1u << 5)
#define STATUS_L25 (1u << 25)
#define BACKLIGHT_OFF_TICKS 0x00002000u
#define BACKLIGHT_ON_TICKS 0x00001000u
#define BACKLIGHT_STAGE_GAP_TICKS 0x00004000u
#define MAPPING_LINUX_BIT (1u << 24)
#define QEMU_DIRECT_DELAY_SHIFT 12
#define LOG_SECTOR_SIZE 512u
#define LOG_LIMIT 262144u
#define FA_READ 0x01u
#define PROGRESS_ADDR 0xa7a00000u
#define RAW_DIAG_ADDR 0xa7a10000u
#define LIVE_HANDOFF_ADDR 0xa7a2f000u
#define DIRECT_HANDOFF_TRACE_ADDR 0xa6820000u
#define PROGRESS_MAGIC 0x52504653u
#define PROGRESS_VERSION 1u
#define PROGRESS_ENTRIES 1024u
#define PROGRESS_NAME_LEN 32u
#define PROGRESS_DUMP_ENTRIES PROGRESS_ENTRIES
#define PROGRESS_LIVE_MAGIC 0x4c495645u
#define LOADER_BUILD_TAG "2026-08-06 rom-flush-handoff"
#define BOOTLOG_SD_WRITE 1

typedef unsigned long long u64;

struct fatfs {
	u8 fs_type;
	u8 pdrv;
	u8 n_fats;
	u8 wflag;
	u8 fsi_flag;
	u8 reserved0;
	u16 id;
	u16 n_rootdir;
	u16 csize;
	void *lfnbuf;
	u8 *dirbuf;
	u32 last_clst;
	u32 free_clst;
	u32 cdir;
	u32 cdc_scl;
	u32 cdc_size;
	u32 cdc_ofs;
	u32 n_fatent;
	u32 fsize;
	u32 volbase;
	u32 fatbase;
	u32 dirbase;
	u32 database;
	u32 bitbase;
	u32 winsect;
	u8 win[LOG_SECTOR_SIZE];
};

struct ffobjid {
	struct fatfs *fs;
	u16 id;
	u8 attr;
	u8 stat;
	u32 sclust;
	u8 reserved0[4];
	u64 objsize;
	u32 n_cont;
	u32 n_frag;
	u32 c_scl;
	u32 c_size;
	u32 c_ofs;
	u8 reserved1[4];
};

struct fil {
	struct ffobjid obj;
	u8 flag;
	u8 err;
	u8 reserved0[6];
	u64 fptr;
	u32 clust;
	u32 sect;
	u32 dir_sect;
	u8 *dir_ptr;
	u8 buf[LOG_SECTOR_SIZE];
};

struct progress_entry {
	u32 seq;
	u32 kind;
	u32 value;
	u32 name_ptr;
	char name[PROGRESS_NAME_LEN];
};

struct progress_log {
	u32 magic;
	u32 version;
	u32 seq;
	u32 write_index;
	u32 wrapped;
	u32 reserved[3];
	struct progress_entry entries[PROGRESS_ENTRIES];
};

typedef int (*rom_f_mount_fn)(struct fatfs *fs, const char *path, u8 opt);
typedef int (*rom_f_open_fn)(struct fil *fp, const char *path, u8 mode);
typedef int (*rom_disk_read_fn)(u8 pdrv, u8 *buff, u32 sector, u32 count);
typedef int (*rom_disk_write_fn)(u8 pdrv, const u8 *buff, u32 sector,
		u32 count);
typedef void (*rom_cache_flush_fn)(void *addr, unsigned long len);

static rom_f_mount_fn const rom_f_mount = (rom_f_mount_fn)ROM_F_MOUNT_ADDR;
static rom_f_open_fn const rom_f_open = (rom_f_open_fn)ROM_F_OPEN_ADDR;
static rom_disk_read_fn const rom_disk_read =
	(rom_disk_read_fn)ROM_DISK_READ_ADDR;
static rom_disk_write_fn const rom_disk_write =
	(rom_disk_write_fn)ROM_DISK_WRITE_ADDR;
static rom_cache_flush_fn const rom_cache_flush =
	(rom_cache_flush_fn)ROM_CACHE_FLUSH_ADDR;

struct elf32_ehdr {
	u8 e_ident[EI_NIDENT];
	u16 e_type;
	u16 e_machine;
	u32 e_version;
	u32 e_entry;
	u32 e_phoff;
	u32 e_shoff;
	u32 e_flags;
	u16 e_ehsize;
	u16 e_phentsize;
	u16 e_phnum;
	u16 e_shentsize;
	u16 e_shnum;
	u16 e_shstrndx;
};

struct elf32_phdr {
	u32 p_type;
	u32 p_offset;
	u32 p_vaddr;
	u32 p_paddr;
	u32 p_filesz;
	u32 p_memsz;
	u32 p_flags;
	u32 p_align;
};

extern const u8 linux_vmlinux_start[];
extern const u8 linux_vmlinux_end[];
extern const u8 linux_dtb_start[];
extern const u8 linux_dtb_end[];
extern u8 __bss_start[];
extern u8 __bss_end[];

static void uart_putc(char ch)
{
	volatile u8 *uart = (volatile u8 *)UART_BASE;
	unsigned int timeout = 100000;

	if (ch == '\n')
		uart_putc('\r');

	while (timeout-- != 0 && (uart[UART_LSR] & UART_LSR_THRE) == 0)
		;
	uart[UART_THR] = (u8)ch;
}

static void uart_puts(const char *s)
{
	while (*s != '\0')
		uart_putc(*s++);
}

static void uart_hex(u32 value)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	uart_puts("0x");
	for (shift = 28; shift >= 0; shift -= 4)
		uart_putc(digits[(value >> shift) & 0xf]);
}

static u32 mmio_read32(uintptr addr)
{
	return *(volatile u32 *)addr;
}

static void mmio_write32(uintptr addr, u32 value)
{
	*(volatile u32 *)addr = value;
}

static void mmio_write8(uintptr addr, u8 value)
{
	*(volatile u8 *)addr = value;
}

static void progress_mark(const char *name, u32 kind, u32 value);

static u32 read_c0_count(void)
{
	u32 value;

	__asm__ volatile("mfc0 %0, $9" : "=r"(value));
	return value;
}

static void delay_clock_count_ticks(u32 ticks)
{
	u32 start = read_c0_count();

	while ((u32)(read_c0_count() - start) < ticks)
		;
}

static void set_scpu_clock_918(void)
{
	u32 reg074;
	u32 reg07c;
	u32 reg380;

	if ((mmio_read32(SYSIO_CHIP_ID) >> 16) != HC1512_CHIP_ID)
		return;

	reg074 = mmio_read32(SYSIO_SCPU_SELECT);
	reg07c = mmio_read32(SYSIO_SCPU_PLL_ENABLE);
	reg380 = mmio_read32(SYSIO_SCPU_PLL_CONTROL);
	progress_mark("loader-scpu-before-074", 0x42, reg074);
	progress_mark("loader-scpu-before-07c", 0x42, reg07c);
	progress_mark("loader-scpu-before-380", 0x42, reg380);

	/*
	 * This is the HC1512 sequence used by the vendor HCRTOS clock hook and
	 * mufrog-commandc.  Establish it before Linux starts so CP0 Count has a
	 * single, stable 459 MHz rate for the entire kernel lifetime.
	 */
	reg380 = (reg380 & 0x0000ffffu) | (SCPU_PLL_MCTRL2 << 16);
	mmio_write32(SYSIO_SCPU_PLL_CONTROL, reg380);
	/* This is at least 1 ms even if a warm boot already runs at 918 MHz. */
	delay_clock_count_ticks(500000u);

	reg074 = (reg074 & ~(7u << 8)) | (7u << 8);
	mmio_write32(SYSIO_SCPU_SELECT, reg074);
	reg07c |= 1u << 7;
	mmio_write32(SYSIO_SCPU_PLL_ENABLE, reg07c);
	reg074 |= 1u << 22;
	mmio_write32(SYSIO_SCPU_SELECT, reg074);
	/* The new 918 MHz clock makes CP0 Count run at 459 ticks/us. */
	delay_clock_count_ticks(459u * 5000u);

	progress_mark("loader-scpu-after-074", 0x42,
		mmio_read32(SYSIO_SCPU_SELECT));
	progress_mark("loader-scpu-after-07c", 0x42,
		mmio_read32(SYSIO_SCPU_PLL_ENABLE));
	progress_mark("loader-scpu-after-380", 0x42,
		mmio_read32(SYSIO_SCPU_PLL_CONTROL));
	uart_puts("linux-loader: SCPU 918MHz regs=");
	uart_hex(mmio_read32(SYSIO_SCPU_SELECT));
	uart_putc('/');
	uart_hex(mmio_read32(SYSIO_SCPU_PLL_ENABLE));
	uart_putc('/');
	uart_hex(mmio_read32(SYSIO_SCPU_PLL_CONTROL));
	uart_puts("\n");
}

static struct fatfs log_fs;
static struct fil log_file;
static u8 log_sector[LOG_SECTOR_SIZE];
static u8 log_fat_sector[LOG_SECTOR_SIZE];
static u8 log_live_sector[LOG_SECTOR_SIZE];
static u32 log_fat_lba = 0xffffffffu;
static u32 log_sector_index = 0xffffffffu;
static u32 log_pos;
static u32 log_limit;
static int log_ready;
static int log_tried;
static volatile struct progress_log * const progress_log =
	(volatile struct progress_log *)PROGRESS_ADDR;
static volatile u32 * const live_handoff = (volatile u32 *)LIVE_HANDOFF_ADDR;
static volatile u32 * const direct_handoff_trace =
	(volatile u32 *)DIRECT_HANDOFF_TRACE_ADDR;

/*
 * The direct ASD handoff has no UART on the physical device.  Keep a small,
 * monotonic C-side breadcrumb chain beside the bootloader's retained handoff
 * record.  The address is the KSEG1 base of the handoff diagnostic area and
 * is outside the kernel/DTB load window, so these words survive both a failed
 * warm handoff and the quick-powercycle log recovery path.  Each marker is
 * written before its value; a torn pair is therefore visible as an incomplete
 * stage instead of a false success.
 */
static void direct_handoff_trace_invalidate_line(u32 offset)
{
	uintptr cached;

	/*
	 * The trace is written through KSEG1, but a warm handoff can leave a
	 * dirty KSEG0 line for the same physical address in the old image.  The
	 * HC15xx's full index sweep is not a sufficient ownership boundary for
	 * this alias: the first cache sweep performed by the incoming loader can
	 * write that old line back over a newly written retained breadcrumb.
	 * Invalidate the exact cached alias immediately before each KSEG1 write.
	 */
	cached = ((DIRECT_HANDOFF_TRACE_ADDR + offset) & 0x1fffffffu) |
		KSEG0_BASE;
	__asm__ volatile(
		".set push\n\t"
		".set mips32\n\t"
		"cache 0x11, 0(%0)\n\t"
		"sync\n\t"
		".set pop"
		:
		: "r"(cached)
		: "memory");
}

static void direct_handoff_trace_mark(u32 offset, u32 marker, u32 value)
{
	direct_handoff_trace_invalidate_line(offset);
	direct_handoff_trace[offset / sizeof(u32)] = marker;
	direct_handoff_trace[offset / sizeof(u32) + 1u] = value;
	__asm__ volatile("sync" ::: "memory");
}

static u32 le16(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8);
}

static u32 le32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) |
		((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void zero_sector(u8 *sector)
{
	u32 i;

	for (i = 0; i < LOG_SECTOR_SIZE; i++)
		sector[i] = 0;
}

static int rom_word_present(u32 value)
{
	return value != 0 && value != 0xffffffffu;
}

static int rom_call_present(u32 addr)
{
	u32 insn = mmio_read32(addr);

	return rom_word_present(insn);
}

static int rom_handoff_present(void)
{
	return rom_call_present(ROM_F_MOUNT_ADDR) ||
		rom_call_present(ROM_DISK_WRITE_ADDR) ||
		rom_word_present(mmio_read32(BOOTROM_BASE));
}

static int log_fat_next(u32 cluster, u32 *next)
{
	u32 fat_offset;
	u32 fat_lba;
	u32 offset;
	u32 value;

	if (cluster < 2 || cluster >= log_fs.n_fatent)
		return -1;

	if (log_fs.fs_type == 3) {
		fat_offset = cluster * 4u;
		fat_lba = log_fs.fatbase + fat_offset / LOG_SECTOR_SIZE;
		offset = fat_offset % LOG_SECTOR_SIZE;
		if (fat_lba != log_fat_lba) {
			if (rom_disk_read(log_fs.pdrv, log_fat_sector,
					fat_lba, 1) != 0)
				return -1;
			log_fat_lba = fat_lba;
		}
		value = le32(log_fat_sector + offset) & 0x0fffffffu;
		if (value >= 0x0ffffff8u)
			return 1;
		*next = value;
		return 0;
	}

	if (log_fs.fs_type == 2) {
		fat_offset = cluster * 2u;
		fat_lba = log_fs.fatbase + fat_offset / LOG_SECTOR_SIZE;
		offset = fat_offset % LOG_SECTOR_SIZE;
		if (fat_lba != log_fat_lba) {
			if (rom_disk_read(log_fs.pdrv, log_fat_sector,
					fat_lba, 1) != 0)
				return -1;
			log_fat_lba = fat_lba;
		}
		value = le16(log_fat_sector + offset);
		if (value >= 0xfff8u)
			return 1;
		*next = value;
		return 0;
	}

	return -1;
}

static int log_file_lba(u32 file_sector, u32 *lba)
{
	u32 cluster = log_file.obj.sclust;
	u32 cluster_index;
	u32 sector_in_cluster;

	if (log_fs.csize == 0 || cluster < 2)
		return -1;

	cluster_index = file_sector / log_fs.csize;
	sector_in_cluster = file_sector % log_fs.csize;
	while (cluster_index-- != 0) {
		u32 next = 0;
		int ret = log_fat_next(cluster, &next);

		if (ret != 0)
			return -1;
		cluster = next;
	}

	if (cluster < 2 || cluster >= log_fs.n_fatent)
		return -1;

	*lba = log_fs.database + (cluster - 2u) * log_fs.csize +
		sector_in_cluster;
	return 0;
}

static void bootlog_flush_current(void)
{
	u32 lba;

	if (!log_ready || log_sector_index == 0xffffffffu)
		return;
	if (log_file_lba(log_sector_index, &lba) != 0)
		return;

	rom_cache_flush(log_sector, LOG_SECTOR_SIZE);
	rom_disk_write(log_fs.pdrv, log_sector, lba, 1);
}

static int bootlog_open_sector(u32 sector_index)
{
	if (sector_index == log_sector_index)
		return 0;

	bootlog_flush_current();
	zero_sector(log_sector);
	log_sector_index = sector_index;
	return 0;
}

static void bootlog_putc(char ch)
{
	u32 off;

	if (!log_ready || log_pos >= log_limit)
		return;
	if (bootlog_open_sector(log_pos / LOG_SECTOR_SIZE) != 0)
		return;

	off = log_pos % LOG_SECTOR_SIZE;
	log_sector[off] = (u8)ch;
	log_pos++;

	if (log_pos % LOG_SECTOR_SIZE == 0) {
		bootlog_flush_current();
		log_sector_index = 0xffffffffu;
	} else if (off + 1u < LOG_SECTOR_SIZE) {
		log_sector[off + 1u] = 0;
	}
}

static void bootlog_flush(void)
{
	u32 off;

	if (!log_ready || log_sector_index == 0xffffffffu)
		return;

	off = log_pos % LOG_SECTOR_SIZE;
	if (off < LOG_SECTOR_SIZE)
		log_sector[off] = 0;
	bootlog_flush_current();
}

static void bootlog_puts(const char *s)
{
	while (*s != '\0')
		bootlog_putc(*s++);
}

static void bootlog_hex(u32 value)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	bootlog_puts("0x");
	for (shift = 28; shift >= 0; shift -= 4)
		bootlog_putc(digits[(value >> shift) & 0xf]);
}

static void progress_copy_name(volatile char *dst, const char *src)
{
	u32 i;

	for (i = 0; i + 1u < PROGRESS_NAME_LEN && src[i] != '\0'; i++)
		dst[i] = src[i];
	dst[i] = '\0';
}

static void progress_reset(void)
{
	u32 i;

	progress_log->magic = PROGRESS_MAGIC;
	progress_log->version = PROGRESS_VERSION;
	progress_log->seq = 0;
	progress_log->write_index = 0;
	progress_log->wrapped = 0;
	for (i = 0; i < PROGRESS_ENTRIES; i++) {
		progress_log->entries[i].seq = 0;
		progress_log->entries[i].kind = 0;
		progress_log->entries[i].value = 0;
		progress_log->entries[i].name_ptr = 0;
		progress_log->entries[i].name[0] = '\0';
	}
}

static void progress_set_live_log_sector(void)
{
	u32 sector_index;
	u32 lba = 0;

	if (!log_ready || log_limit < LOG_SECTOR_SIZE * 2u)
		return;

	sector_index = log_limit / LOG_SECTOR_SIZE;
	if (sector_index == 0)
		return;
	sector_index--;

	if (log_file_lba(sector_index, &lba) != 0)
		return;

	live_handoff[0] = PROGRESS_LIVE_MAGIC;
	live_handoff[1] = log_fs.pdrv;
	live_handoff[2] = lba;

	bootlog_puts("live progress sector index=");
	bootlog_hex(sector_index);
	bootlog_puts(" pdrv=");
	bootlog_hex(log_fs.pdrv);
	bootlog_puts(" lba=");
	bootlog_hex(lba);
	bootlog_puts(" handoff=");
	bootlog_hex(LIVE_HANDOFF_ADDR);
	bootlog_puts("\n");
	bootlog_flush();
}

static void progress_mark(const char *name, u32 kind, u32 value)
{
	u32 index;
	u32 seq;
	volatile struct progress_entry *entry;

	if (progress_log->magic != PROGRESS_MAGIC ||
	    progress_log->version != PROGRESS_VERSION)
		return;

	index = progress_log->write_index;
	if (index >= PROGRESS_ENTRIES)
		index = 0;
	seq = progress_log->seq + 1u;
	entry = &progress_log->entries[index];

	entry->seq = seq;
	entry->kind = kind;
	entry->value = value;
	entry->name_ptr = (u32)(uintptr)name;
	progress_copy_name(entry->name, name);

	index++;
	if (index >= PROGRESS_ENTRIES) {
		index = 0;
		progress_log->wrapped = 1;
	}
	progress_log->write_index = index;
	progress_log->seq = seq;
}

static void bootlog_progress_entry(volatile struct progress_entry *entry)
{
	u32 i;
	u32 value;

	if (entry->seq == 0)
		return;

	value = entry->value;
	bootlog_puts("progress seq=");
	bootlog_hex(entry->seq);
	bootlog_puts(" kind=");
	bootlog_hex(entry->kind);
	bootlog_puts(" value=");
	bootlog_hex(value);
	if (entry->kind == 0x20u) {
		bootlog_puts(" ascii='");
		for (i = 0; i < 4u; i++) {
			u8 ch = (u8)(value >> (i * 8u));

			if (ch >= 32u && ch < 127u)
				bootlog_putc((char)ch);
			else if (ch == '\n')
				bootlog_putc('|');
			else if (ch != 0)
				bootlog_putc('.');
		}
		bootlog_putc('\'');
	}
	bootlog_puts(" name=");
	for (i = 0; i < PROGRESS_NAME_LEN && entry->name[i] != '\0'; i++)
		bootlog_putc(entry->name[i]);
	bootlog_puts(" ptr=");
	bootlog_hex(entry->name_ptr);
	bootlog_puts("\n");
}

static void bootlog_dump_raw_diag(void)
{
	volatile u32 *diag = (volatile u32 *)RAW_DIAG_ADDR;
	u32 i;

	bootlog_puts("raw diag addr=");
	bootlog_hex(RAW_DIAG_ADDR);
	for (i = 0; i < 96; i++) {
		bootlog_puts(" w");
		bootlog_hex(i);
		bootlog_puts("=");
		bootlog_hex(diag[i]);
	}
	bootlog_puts("\n");
}

static void bootlog_dump_previous_progress(void)
{
	u32 i;
	u32 start;
	u32 count;
	int header_usable;

	bootlog_puts("progress raw addr=");
	bootlog_hex(PROGRESS_ADDR);
	bootlog_puts(" magic=");
	bootlog_hex(progress_log->magic);
	bootlog_puts(" version=");
	bootlog_hex(progress_log->version);
	bootlog_puts(" seq=");
	bootlog_hex(progress_log->seq);
	bootlog_puts(" write=");
	bootlog_hex(progress_log->write_index);
	bootlog_puts(" wrapped=");
	bootlog_hex(progress_log->wrapped);
	bootlog_puts(" reserved0=");
	bootlog_hex(progress_log->reserved[0]);
	bootlog_puts(" reserved1=");
	bootlog_hex(progress_log->reserved[1]);
	bootlog_puts(" reserved2=");
	bootlog_hex(progress_log->reserved[2]);
	bootlog_puts("\n");
	bootlog_dump_raw_diag();

	header_usable = progress_log->version == PROGRESS_VERSION &&
		progress_log->seq != 0 &&
		progress_log->write_index <= PROGRESS_ENTRIES &&
		progress_log->wrapped <= 1 &&
		(progress_log->magic == PROGRESS_MAGIC ||
		 progress_log->magic == 0);
	if (!header_usable)
		return;

	bootlog_puts(progress_log->magic == PROGRESS_MAGIC ?
		"previous progress addr=" : "previous progress damaged addr=");
	bootlog_hex(PROGRESS_ADDR);
	bootlog_puts(" seq=");
	bootlog_hex(progress_log->seq);
	bootlog_puts(" write=");
	bootlog_hex(progress_log->write_index);
	bootlog_puts(" wrapped=");
	bootlog_hex(progress_log->wrapped);
	bootlog_puts(" reserved0=");
	bootlog_hex(progress_log->reserved[0]);
	bootlog_puts(" reserved1=");
	bootlog_hex(progress_log->reserved[1]);
	bootlog_puts(" reserved2=");
	bootlog_hex(progress_log->reserved[2]);
	bootlog_puts("\n");

	count = progress_log->wrapped ? PROGRESS_ENTRIES : progress_log->write_index;
	if (count > PROGRESS_ENTRIES)
		count = PROGRESS_ENTRIES;
	start = progress_log->wrapped ? progress_log->write_index : 0;
	if (start >= PROGRESS_ENTRIES)
		start = 0;
	if (count > PROGRESS_DUMP_ENTRIES) {
		u32 skipped = count - PROGRESS_DUMP_ENTRIES;

		start = (start + skipped) % PROGRESS_ENTRIES;
		count = PROGRESS_DUMP_ENTRIES;
		bootlog_puts("previous progress skipped-oldest=");
		bootlog_hex(skipped);
		bootlog_puts(" retained=");
		bootlog_hex(count);
		bootlog_puts("\n");
	}

	for (i = 0; i < count; i++)
		bootlog_progress_entry(&progress_log->entries[
			(start + i) % PROGRESS_ENTRIES]);
}

static void bootlog_dump_previous_live_sector(void)
{
	u32 sector_index;
	u32 lba;
	u32 i;

	if (!log_ready || log_limit < LOG_SECTOR_SIZE * 2u)
		return;

	sector_index = log_limit / LOG_SECTOR_SIZE;
	if (sector_index == 0)
		return;
	sector_index--;

	if (log_file_lba(sector_index, &lba) != 0)
		return;
	if (rom_disk_read(log_fs.pdrv, log_live_sector, lba, 1) != 0)
		return;

	bootlog_puts("previous live sector index=");
	bootlog_hex(sector_index);
	bootlog_puts(" lba=");
	bootlog_hex(lba);
	bootlog_puts(" first=");
	bootlog_hex(le32(log_live_sector));
	bootlog_puts("\n");

	if (log_live_sector[0] == 0)
		return;

	bootlog_puts("previous live text begin\n");
	for (i = 0; i < LOG_SECTOR_SIZE && log_live_sector[i] != 0; i++) {
		char ch = (char)log_live_sector[i];

		if (ch == '\n' || ch == '\r' || ch == '\t' ||
		    (ch >= ' ' && ch <= '~'))
			bootlog_putc(ch);
		else
			bootlog_putc('.');
	}
	bootlog_puts("\nprevious live text end\n");
}

static void bootlog_init(void)
{
	int rc;

	if (log_tried)
		return;
	log_tried = 1;

	if (!BOOTLOG_SD_WRITE)
		return;

	if (!rom_call_present(ROM_F_MOUNT_ADDR) ||
	    !rom_call_present(ROM_F_OPEN_ADDR) ||
	    !rom_call_present(ROM_DISK_WRITE_ADDR))
		return;

	rc = rom_f_mount(&log_fs, "", 1);
	if (rc != 0)
		return;
	rc = rom_f_open(&log_file, "log.txt", FA_READ);
	if (rc != 0)
		return;
	if (log_file.obj.fs == (void *)0 || log_file.obj.sclust < 2 ||
	    log_file.obj.objsize < LOG_SECTOR_SIZE)
		return;

	log_limit = log_file.obj.objsize < LOG_LIMIT ?
		(u32)log_file.obj.objsize : LOG_LIMIT;
	log_ready = 1;
	log_pos = 0;
	log_fat_lba = 0xffffffffu;
	log_sector_index = 0xffffffffu;
	bootlog_puts("sf2000-linux loader bootlog\n");
	bootlog_puts("loader build=");
	bootlog_puts(LOADER_BUILD_TAG);
	bootlog_puts("\n");
	bootlog_dump_previous_live_sector();
	bootlog_dump_previous_progress();
	bootlog_flush();
}

static void bootlog_stage(const char *name, u32 pulses)
{
	progress_mark(name, 1, pulses);
	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("stage ");
	bootlog_hex(pulses);
	bootlog_puts(": ");
	bootlog_puts(name);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_loader_info(u32 kernel_size, u32 dtb_size, u32 entry,
		u32 dtb)
{
	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("kernel=");
	bootlog_hex(kernel_size);
	bootlog_puts(" dtb_size=");
	bootlog_hex(dtb_size);
	bootlog_puts(" entry=");
	bootlog_hex(entry);
	bootlog_puts(" dtb=");
	bootlog_hex(dtb);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_wdt_state(void)
{
	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("wdt count=");
	bootlog_hex(mmio_read32(WDT0_COUNT));
	bootlog_puts(" conf=");
	bootlog_hex(mmio_read32(WDT0_CONF) & 0xffu);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_wdt_arm(const char *name)
{
	if (!rom_handoff_present())
		return;

	bootlog_init();
	if (log_ready) {
		bootlog_puts("wdt arm before count=");
		bootlog_hex(mmio_read32(WDT0_COUNT));
		bootlog_puts(" conf=");
		bootlog_hex(mmio_read32(WDT0_CONF) & 0xffu);
		bootlog_puts("\n");
		bootlog_flush();
	}

	mmio_write8(WDT0_CONF, 0);
	mmio_write32(WDT0_COUNT, WDT_BOOT_COUNT);
	mmio_write8(WDT0_CONF, WDT_BOOT_CONF);
	progress_mark(name, 3, WDT_BOOT_COUNT);

	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("wdt armed count=");
	bootlog_hex(mmio_read32(WDT0_COUNT));
	bootlog_puts(" conf=");
	bootlog_hex(mmio_read32(WDT0_CONF) & 0xffu);
	bootlog_puts(" timeout_us=");
	bootlog_hex(WDT_BOOT_USEC);
	bootlog_puts(" name=");
	bootlog_puts(name);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_wdt_disarm(const char *name)
{
	bootlog_init();
	if (log_ready) {
		bootlog_puts("wdt disarm before count=");
		bootlog_hex(mmio_read32(WDT0_COUNT));
		bootlog_puts(" conf=");
		bootlog_hex(mmio_read32(WDT0_CONF) & 0xffu);
		bootlog_puts("\n");
		bootlog_flush();
	}

	/*
	 * Linux does not feed WDT0.  Leaving it armed here would reset the box
	 * ~8s into boot, long before it reaches the menu.  Disable it; the
	 * kernel re-affirms ownership via its late_initcall takeover once early
	 * boot has clearly succeeded.  A genuine early-boot hang therefore
	 * stalls instead of rebooting, which is the desired behaviour for linux.
	 */
	mmio_write8(WDT0_CONF, 0);
	progress_mark(name, 3, 0);

	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("wdt disarmed conf=");
	bootlog_hex(mmio_read32(WDT0_CONF) & 0xffu);
	bootlog_puts(" name=");
	bootlog_puts(name);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_ebase(u32 before, u32 after)
{
	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("ebase=");
	bootlog_hex(before);
	bootlog_puts(" -> ");
	bootlog_hex(after);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_handoff_state(u32 bootrom, u32 mount, u32 write)
{
	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("handoff bootrom=");
	bootlog_hex(bootrom);
	bootlog_puts(" rom_f_mount=");
	bootlog_hex(mount);
	bootlog_puts(" rom_disk_write=");
	bootlog_hex(write);
	bootlog_puts("\n");
	bootlog_flush();
}

static void bootlog_mapping(u32 before, u32 after)
{
	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("mapping=");
	bootlog_hex(before);
	bootlog_puts(" -> ");
	bootlog_hex(after);
	bootlog_puts("\n");
	bootlog_flush();
}

static u32 cp0_count(void)
{
	u32 count;

	__asm__ volatile("mfc0 %0, $9" : "=r"(count));
	return count;
}

static void delay_count_ticks(u32 ticks)
{
	u32 start = cp0_count();

	if (!rom_handoff_present())
		ticks >>= QEMU_DIRECT_DELAY_SHIFT;
	if (ticks == 0)
		return;

	while ((u32)(cp0_count() - start) < ticks)
		__asm__ volatile("nop");
}

static void backlight_set(int on)
{
	u32 out = mmio_read32(GPIO_R_OUT);
	u32 dir = mmio_read32(GPIO_R_DIR);

	if (on)
		out &= ~BACKLIGHT_R05;
	else
		out |= BACKLIGHT_R05;

	mmio_write8(PINMUX_R05, 0);
	mmio_write32(GPIO_R_DIR, dir | BACKLIGHT_R05);
	mmio_write32(GPIO_R_OUT, out);
}

static void status_led_set(int on)
{
	u32 out = mmio_read32(GPIO_L_OUT);
	u32 dir = mmio_read32(GPIO_L_DIR);

	if (on)
		out |= STATUS_L25;
	else
		out &= ~STATUS_L25;

	mmio_write8(PINMUX_L25, 0);
	mmio_write32(GPIO_L_DIR, dir | STATUS_L25);
	mmio_write32(GPIO_L_OUT, out);
}

static void backlight_stage_mark(const char *name, unsigned int pulses)
{
	static int health_blink_done;

	uart_puts("linux-loader: visible stage ");
	uart_puts(name);
	uart_puts(" pulses=");
	uart_hex(pulses);
	uart_puts("\n");

	if (health_blink_done)
		return;
	health_blink_done = 1;

	backlight_set(0);
	status_led_set(1);
	delay_count_ticks(BACKLIGHT_OFF_TICKS);
	backlight_set(1);
	status_led_set(0);
	/*
	 * The off pulse is the single physical health blink.  Leave the panel
	 * dark afterwards so inherited ST7789 GRAM is never exposed while Linux
	 * is still preparing its first controlled frame.
	 */
	backlight_set(0);
}

static void loader_panic(const char *message)
{
	uart_puts(message);
	uart_puts("\n");

	for (;;) {
		backlight_set(0);
		delay_count_ticks(BACKLIGHT_OFF_TICKS);
		backlight_set(1);
		delay_count_ticks(BACKLIGHT_OFF_TICKS);
	}
}

static void clear_bss(void)
{
	u8 *p;

	for (p = __bss_start; p < __bss_end; p++)
		*p = 0;
}

static void disable_interrupts(void)
{
	u32 status;

	__asm__ volatile(
		"mfc0 %0, $12\n\t"
		"li $8, -2\n\t"
		"and %0, %0, $8\n\t"
		"mtc0 %0, $12\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop"
		: "=&r"(status)
		:
		: "$8", "memory");
}

static u32 read_ebase(void)
{
	u32 ebase;

	__asm__ volatile(
		".set push\n\t"
		".set mips32r2\n\t"
		"mfc0 %0, $15, 1\n\t"
		".set pop"
		: "=r"(ebase));
	return ebase;
}

static u32 read_status(void)
{
	u32 status;

	__asm__ volatile("mfc0 %0, $12" : "=r"(status));
	return status;
}

static void write_status(u32 status)
{
	__asm__ volatile(
		"mtc0 %0, $12\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop"
		:
		: "r"(status)
		: "memory");
}

static void write_ebase(u32 ebase)
{
	__asm__ volatile(
		".set push\n\t"
		".set mips32r2\n\t"
		"mtc0 %0, $15, 1\n\t"
		"nop\n\t"
		"nop\n\t"
		".set pop"
		:
		: "r"(ebase)
		: "memory");
}

static void reset_exception_base_for_linux(void)
{
	u32 bootrom;
	u32 mount;
	u32 write;
	u32 before;
	u32 after;

	bootrom = mmio_read32(BOOTROM_BASE);
	mount = mmio_read32(ROM_F_MOUNT_ADDR);
	write = mmio_read32(ROM_DISK_WRITE_ADDR);
	bootlog_handoff_state(bootrom, mount, write);

	if (!rom_word_present(mount) && !rom_word_present(write) &&
	    !rom_word_present(bootrom))
		return;

	before = read_ebase();
	write_ebase(0);
	after = read_ebase();

	uart_puts("linux-loader: ebase ");
	uart_hex(before);
	uart_puts(" -> ");
	uart_hex(after);
	uart_puts("\n");
	bootlog_ebase(before, after);
}

static void setup_system_mapping_for_linux(void)
{
	u32 before = mmio_read32(MAPPING_REG);
	u32 after = before | MAPPING_LINUX_BIT;

	mmio_write32(MAPPING_REG, after);
	after = mmio_read32(MAPPING_REG);

	uart_puts("linux-loader: mapping ");
	uart_hex(before);
	uart_puts(" -> ");
	uart_hex(after);
	uart_puts("\n");
	bootlog_mapping(before, after);
}

static void log_status_handoff(void)
{
	u32 status = read_status();

	bootlog_init();
	if (!log_ready)
		return;

	bootlog_puts("status=");
	bootlog_hex(status);
	bootlog_puts(" -> 0x00000000\n");
	bootlog_flush();
}

static uintptr align_up(uintptr value, uintptr align)
{
	return (value + align - 1u) & ~(align - 1u);
}

static uintptr to_kseg0(u32 addr)
{
	if ((addr & 0xe0000000u) == KSEG0_BASE)
		return (uintptr)addr;
	if ((addr & 0xe0000000u) == KSEG1_BASE)
		return (uintptr)((addr & 0x1fffffffu) | KSEG0_BASE);
	return (uintptr)(addr | KSEG0_BASE);
}

static int blob_range_valid(const u8 *blob, usize blob_size, u32 off, u32 len)
{
	(void)blob;
	return off <= blob_size && len <= blob_size - off;
}

static void copy_forward(u8 *dst, const u8 *src, usize len)
{
	while (len-- != 0)
		*dst++ = *src++;
}

static void copy_overlap(u8 *dst, const u8 *src, usize len)
{
	if (dst <= src || dst >= src + len) {
		copy_forward(dst, src, len);
		return;
	}

	dst += len;
	src += len;
	while (len-- != 0)
		*--dst = *--src;
}

static void zero_bytes(u8 *dst, usize len)
{
	while (len-- != 0)
		*dst++ = 0;
}

static void cache_invalidate_icache_indices(void)
{
	uintptr p;

	/* Index Invalidate (0x00) over every (set, way) of the same fixed
	 * primary I-cache, so a warm-boot alias can never shadow freshly copied
	 * kernel code. */
	uart_puts("linux-loader: icache index sweep\n");
	for (p = KSEG0_BASE; p < KSEG0_BASE + CACHE_TOTAL_SPAN;
			p += CACHE_LINE) {
		__asm__ volatile(".set push\n\t.set mips32\n\t"
			"cache 0x00, 0(%0)\n\tcache 0x00, 0(%0)\n\t"
			".set pop" : : "r"(p) : "memory");
	}
	__asm__ volatile("sync" ::: "memory");
}

static int valid_elf(const struct elf32_ehdr *eh, usize blob_size)
{
	if (blob_size < sizeof(*eh))
		return 0;
	if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
			eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
		return 0;
	if (eh->e_ident[EI_CLASS] != ELFCLASS32 ||
			eh->e_ident[EI_DATA] != ELFDATA2LSB)
		return 0;
	if (eh->e_type != ET_EXEC || eh->e_machine != EM_MIPS)
		return 0;
	if (eh->e_phentsize != sizeof(struct elf32_phdr) || eh->e_phnum == 0)
		return 0;
	if (!blob_range_valid((const u8 *)eh, blob_size, eh->e_phoff,
			eh->e_phnum * sizeof(struct elf32_phdr)))
		return 0;
	return 1;
}

static int scan_linux_elf(const u8 *blob, usize blob_size,
		uintptr *load_min_out, uintptr *load_max_out)
{
	const struct elf32_ehdr *eh = (const struct elf32_ehdr *)blob;
	const struct elf32_phdr *ph;
	uintptr load_min = 0xffffffffu;
	uintptr load_max = 0;
	u16 i;
	int loads = 0;

	if (!valid_elf(eh, blob_size)) {
		uart_puts("linux-loader: invalid ELF\n");
		return -1;
	}

	ph = (const struct elf32_phdr *)(blob + eh->e_phoff);
	for (i = 0; i < eh->e_phnum; i++) {
		uintptr dst;
		uintptr end;

		if (ph[i].p_type != PT_LOAD)
			continue;
		if (ph[i].p_filesz > ph[i].p_memsz ||
				!blob_range_valid(blob, blob_size, ph[i].p_offset,
					ph[i].p_filesz)) {
			uart_puts("linux-loader: bad LOAD range\n");
			return -1;
		}

		dst = to_kseg0(ph[i].p_paddr != 0 ? ph[i].p_paddr : ph[i].p_vaddr);
		end = dst + ph[i].p_memsz;
		if (end < dst || end > RAM_TOP) {
			uart_puts("linux-loader: LOAD outside RAM\n");
			return -1;
		}
		if (dst < load_min)
			load_min = dst;
		if (end > load_max)
			load_max = end;
		loads++;
	}

	if (loads == 0)
		return -1;

	*load_min_out = load_min;
	*load_max_out = load_max;
	return 0;
}

static void copy_linux_elf(const u8 *blob)
{
	const struct elf32_ehdr *eh = (const struct elf32_ehdr *)blob;
	const struct elf32_phdr *ph;
	u16 i;

	ph = (const struct elf32_phdr *)(blob + eh->e_phoff);
	/*
	 * The embedded ELF starts below its load addresses.  Treat all LOAD
	 * records as one overlapping move: a low destination can cover source
	 * bytes belonging to a later program header even when that individual
	 * record is copied with memmove semantics.  Linux emits LOAD records in
	 * ascending address order, so walking them backwards preserves every
	 * unread source byte.
	 */
	for (i = eh->e_phnum; i-- != 0;) {
		uintptr dst;

		if (ph[i].p_type != PT_LOAD)
			continue;

		dst = to_kseg0(ph[i].p_paddr != 0 ? ph[i].p_paddr : ph[i].p_vaddr);
		copy_overlap((u8 *)dst, blob + ph[i].p_offset, ph[i].p_filesz);
		zero_bytes((u8 *)(dst + ph[i].p_filesz),
			ph[i].p_memsz - ph[i].p_filesz);
	}
}

static void print_kernel_jump(u32 entry, uintptr dtb)
{
	uart_puts("linux-loader: jump entry=");
	uart_hex(entry);
	uart_puts(" dtb=");
	uart_hex((u32)dtb);
	uart_puts("\n");
}

static void jump_to_kernel(u32 entry, uintptr dtb)
{
	__asm__ volatile(
		"move $4, %0\n\t"
		"move $5, %1\n\t"
		"move $6, $0\n\t"
		"move $7, $0\n\t"
		"jr %2\n\t"
		"nop"
		:
		: "r"((s32)-2), "r"(dtb), "r"(entry)
		: "$4", "$5", "$6", "$7", "memory");
}

void linux_loader_main_impl(void)
{
	const struct elf32_ehdr *eh;
	usize kernel_size;
	usize dtb_size;
	uintptr load_min;
	uintptr load_max;
	uintptr payload_end;
	uintptr dtb_dest;
	u32 entry;

	direct_handoff_trace_mark(0xf0u, 0x4c435254u, 1u); /* LCRT */
	clear_bss();
	disable_interrupts();
	/* Keep filesystem recovery from hiding proof that the loader started. */
	backlight_stage_mark("loader-entry", 1);
	bootlog_init();
	direct_handoff_trace_mark(0xf8u, 0x4c4c4f47u, log_ready ? 1u : 0u);
	progress_reset();
	progress_set_live_log_sector();
	bootlog_wdt_state();
	bootlog_stage("loader-entry", 1);

	kernel_size = (usize)(linux_vmlinux_end - linux_vmlinux_start);
	dtb_size = (usize)(linux_dtb_end - linux_dtb_start);
	eh = (const struct elf32_ehdr *)linux_vmlinux_start;

	uart_puts("\nlinux-loader: start kernel=");
	uart_hex((u32)kernel_size);
	uart_puts(" dtb=");
	uart_hex((u32)dtb_size);
	uart_puts("\n");

	if (!valid_elf(eh, kernel_size)) {
		loader_panic("linux-loader: invalid embedded kernel");
	}

	load_min = 0;
	load_max = 0;
	if (scan_linux_elf(linux_vmlinux_start, kernel_size,
			&load_min, &load_max) != 0) {
		loader_panic("linux-loader: cannot map embedded kernel");
	}
	direct_handoff_trace_mark(0x100u, 0x4c454c46u, (u32)load_min);

	payload_end = align_up((uintptr)linux_dtb_end, 0x10000u);
	dtb_dest = align_up(load_max, 0x10000u);
	if (dtb_dest < payload_end)
		dtb_dest = payload_end;
	if (dtb_dest + dtb_size > RAM_TOP) {
		loader_panic("linux-loader: no room for DTB");
	}
	entry = eh->e_entry;
	bootlog_loader_info((u32)kernel_size, (u32)dtb_size, eh->e_entry,
		(u32)dtb_dest);
	setup_system_mapping_for_linux();
	reset_exception_base_for_linux();
	direct_handoff_trace_mark(0x108u, 0x4c4d4150u,
		mmio_read32(MAPPING_REG)); /* LMAP */
	log_status_handoff();
	bootlog_stage("loader-jump", 2);
	copy_forward((u8 *)dtb_dest, linux_dtb_start, dtb_size);
	copy_linux_elf(linux_vmlinux_start);

	/* The cached ELF stores left the kernel image dirty in the D-cache.
	 * Flush the copied ranges with the vendor ROM hit-writeback routine
	 * (the same call used for the bootlog sector and by the NuttX
	 * bootloader's handoff): it is geometry-independent and proven on this
	 * core, where a full fixed-geometry index writeback sweep still left
	 * pre-copy RAM in place (run 110: ri-insn-data=0x00000001 at
	 * 0x80667d3c). */
	if (rom_handoff_present()) {
		rom_cache_flush((void *)load_min, (usize)(load_max - load_min));
		rom_cache_flush((void *)dtb_dest, (usize)dtb_size);
		__asm__ volatile("sync" ::: "memory");
	}
	/* Remove every possible warm-boot I-cache alias, not only hit aliases. */
	cache_invalidate_icache_indices();
	disable_interrupts();
	backlight_stage_mark("loader-jump", 2);
	print_kernel_jump(entry, dtb_dest);
	direct_handoff_trace_mark(0x110u, 0x4c4a4d50u, entry); /* LJMP */
	direct_handoff_trace_mark(0x118u, 0x4c445442u, (u32)dtb_dest); /* LDTB */
	bootlog_wdt_disarm("loader-watchdog-disarmed");
	set_scpu_clock_918();
	write_status(0);
	jump_to_kernel(entry, dtb_dest);

	for (;;)
		;
}
