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
#define CACHE_LINE 16u
#define PINMUX_R05 0xb88004e5u
#define PINMUX_L25 0xb88004b9u
#define GPIO_L_OUT 0xb8800054u
#define GPIO_L_DIR 0xb8800058u
#define GPIO_R_OUT 0xb88000f4u
#define GPIO_R_DIR 0xb88000f8u
#define MAPPING_REG 0xb8800220u
#define WDT0_COUNT 0xb8818500u
#define WDT0_CONF 0xb8818504u
#define BOOTROM_BASE 0xbfc00000u
#define ROM_F_MOUNT_ADDR 0x8101f044u
#define ROM_F_OPEN_ADDR 0x8101f0d4u
#define ROM_DISK_READ_ADDR 0x8101ffb4u
#define ROM_DISK_WRITE_ADDR 0x81020020u
#define ROM_CACHE_FLUSH_ADDR 0x810032f4u
#define BACKLIGHT_R05 (1u << 5)
#define STATUS_L25 (1u << 25)
#define BACKLIGHT_OFF_TICKS 0x00008000u
#define BACKLIGHT_ON_TICKS 0x00004000u
#define BACKLIGHT_STAGE_GAP_TICKS 0x00010000u
#define MAPPING_LINUX_BIT (1u << 24)
#define QEMU_DIRECT_DELAY_SHIFT 12
#define LOG_SECTOR_SIZE 512u
#define LOG_LIMIT 262144u
#define FA_READ 0x01u
#define PROGRESS_ADDR 0xa13f0000u
#define PROGRESS_MAGIC 0x52504653u
#define PROGRESS_VERSION 1u
#define PROGRESS_ENTRIES 1024u
#define PROGRESS_NAME_LEN 32u
#define LOADER_BUILD_TAG "2026-05-12 direct-stub-epc-branch"

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

static struct fatfs log_fs;
static struct fil log_file;
static u8 log_sector[LOG_SECTOR_SIZE];
static u8 log_fat_sector[LOG_SECTOR_SIZE];
static u32 log_fat_lba = 0xffffffffu;
static u32 log_sector_index = 0xffffffffu;
static u32 log_pos;
static u32 log_limit;
static int log_ready;
static int log_tried;
static volatile struct progress_log * const progress_log =
	(volatile struct progress_log *)PROGRESS_ADDR;

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

	if (entry->seq == 0)
		return;

	bootlog_puts("progress seq=");
	bootlog_hex(entry->seq);
	bootlog_puts(" kind=");
	bootlog_hex(entry->kind);
	bootlog_puts(" value=");
	bootlog_hex(entry->value);
	bootlog_puts(" name=");
	for (i = 0; i < PROGRESS_NAME_LEN && entry->name[i] != '\0'; i++)
		bootlog_putc(entry->name[i]);
	bootlog_puts(" ptr=");
	bootlog_hex(entry->name_ptr);
	bootlog_puts("\n");
}

static void bootlog_dump_previous_progress(void)
{
	u32 i;
	u32 start;
	u32 count;

	if (progress_log->magic != PROGRESS_MAGIC ||
	    progress_log->version != PROGRESS_VERSION ||
	    progress_log->seq == 0)
		return;

	bootlog_puts("previous progress addr=");
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

	for (i = 0; i < count; i++)
		bootlog_progress_entry(&progress_log->entries[
			(start + i) % PROGRESS_ENTRIES]);
}

static void bootlog_init(void)
{
	int rc;

	if (log_tried)
		return;
	log_tried = 1;

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
	unsigned int i;

	uart_puts("linux-loader: visible stage ");
	uart_puts(name);
	uart_puts(" pulses=");
	uart_hex(pulses);
	uart_puts("\n");

	backlight_set(1);
	status_led_set(0);
	delay_count_ticks(BACKLIGHT_STAGE_GAP_TICKS);
	for (i = 0; i < pulses; i++) {
		backlight_set(0);
		status_led_set(1);
		delay_count_ticks(BACKLIGHT_OFF_TICKS);
		backlight_set(1);
		status_led_set(0);
		delay_count_ticks(BACKLIGHT_ON_TICKS);
	}
	status_led_set(0);
	delay_count_ticks(BACKLIGHT_STAGE_GAP_TICKS);
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

static void cache_flush_line(uintptr addr)
{
	__asm__ volatile(
		".set push\n\t"
		".set mips32\n\t"
		"cache 0x15, 0(%0)\n\t"
		"cache 0x10, 0(%0)\n\t"
		".set pop"
		:
		: "r"(addr)
		: "memory");
}

static void cache_flush_range(uintptr addr, usize len)
{
	uintptr p;
	uintptr end;

	if (len == 0)
		return;

	p = addr & ~(uintptr)(CACHE_LINE - 1u);
	end = align_up(addr + len, CACHE_LINE);
	for (; p < end; p += CACHE_LINE)
		cache_flush_line(p);

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
	for (i = 0; i < eh->e_phnum; i++) {
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

void linux_loader_main(void)
{
	const struct elf32_ehdr *eh;
	usize kernel_size;
	usize dtb_size;
	uintptr load_min;
	uintptr load_max;
	uintptr payload_end;
	uintptr dtb_dest;
	u32 entry;

	clear_bss();
	disable_interrupts();
	bootlog_init();
	progress_reset();
	bootlog_wdt_state();
	backlight_stage_mark("loader-entry", 1);
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
	log_status_handoff();
	bootlog_stage("loader-jump", 2);
	copy_forward((u8 *)dtb_dest, linux_dtb_start, dtb_size);
	copy_linux_elf(linux_vmlinux_start);

	cache_flush_range(load_min, (usize)(load_max - load_min));
	cache_flush_range(dtb_dest, dtb_size);
	disable_interrupts();
	backlight_stage_mark("loader-jump", 2);
	print_kernel_jump(entry, dtb_dest);
	write_status(0);
	jump_to_kernel(entry, dtb_dest);

	for (;;)
		;
}
