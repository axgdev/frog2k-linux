typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006

#define SYSIO_BASE_PHYS 0x18800000UL
#define KSEG1ADDR(x) ((volatile unsigned char *)((unsigned long)(x) | 0xa0000000UL))
#define PINMUX_L_OFF 0x4a0UL
#define SYS_CLOCK_GATE0_OFF 0x060UL
#define SYS_SFCLK_OFF 0x07cUL
#define SYS_IO_VOLTAGE_OFF 0x184UL
#define WDT_MAP_BASE_PHYS 0x18818000UL
#define WDT_REG_OFF 0x500UL
#define WDT_COUNT_OFF 0x00UL
#define WDT_CONF_OFF 0x04UL
#define PROGRESS_PHYS 0x07a00000UL
#define PROGRESS_MAGIC 0x52504653UL
#define PROGRESS_VERSION 1UL
#define PROGRESS_ENTRIES 1024UL
#define PROGRESS_NAME_LEN 32UL
#define RESET_TAG 0x0255UL

struct progress_entry {
	unsigned int seq;
	unsigned int kind;
	unsigned int value;
	unsigned int name_ptr;
	char name[PROGRESS_NAME_LEN];
};

struct progress_log {
	unsigned int magic;
	unsigned int version;
	unsigned int seq;
	unsigned int write_index;
	unsigned int wrapped;
	unsigned int reserved[3];
	struct progress_entry entries[PROGRESS_ENTRIES];
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
	syscall3(SYS_write, fd, (long)s, (long)(p - s));
}

static void log_message(const char *message)
{
	long log_fd = syscall3(SYS_open, (long)"/dev/kmsg", 1, 0);
	long console_fd;

	if (log_fd >= 0) {
		write_all(log_fd, "<6>");
		write_all(log_fd, message);
		syscall1(SYS_close, log_fd);
		return;
	}

	console_fd = syscall3(SYS_open, (long)"/dev/console", 1, 0);
	if (console_fd < 0)
		console_fd = 1;

	write_all(console_fd, message);
	if (console_fd > 2)
		syscall1(SYS_close, console_fd);
}

static void log_hex_word(unsigned int value)
{
	static const char digits[] = "0123456789abcdef";
	char buf[11];
	unsigned int i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < 8; i++)
		buf[2 + i] = digits[(value >> (28 - i * 4)) & 0xfu];
	buf[10] = 0;
	log_message(buf);
}

static void progress_copy_name(volatile char *dst, const char *src)
{
	unsigned int i;

	for (i = 0; i < PROGRESS_NAME_LEN - 1u && src[i]; i++)
		dst[i] = src[i];
	for (; i < PROGRESS_NAME_LEN; i++)
		dst[i] = 0;
}

static void progress_mark(const char *name, unsigned int kind,
		unsigned int value)
{
	volatile struct progress_log *log =
		(volatile struct progress_log *)KSEG1ADDR(PROGRESS_PHYS);
	volatile struct progress_entry *entry;
	unsigned int index;
	unsigned int seq;
	unsigned int i;

	if (log->magic != PROGRESS_MAGIC || log->version != PROGRESS_VERSION) {
		volatile unsigned int *p = (volatile unsigned int *)log;

		for (i = 0; i < sizeof(*log) / sizeof(*p); i++)
			p[i] = 0;
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
	entry->name_ptr = (unsigned int)(unsigned long)name;
	progress_copy_name(entry->name, name);
	log->write_index = index + 1u;
	log->seq = seq;
}

static unsigned int direct_read32(unsigned long phys)
{
	return *(volatile unsigned int *)KSEG1ADDR(phys);
}

static unsigned int direct_read8(unsigned long phys)
{
	return *(volatile unsigned char *)KSEG1ADDR(phys);
}

int main(void)
{
	unsigned int pins = 0;
	unsigned int i;

	log_message("sf2000_reset_fastprobe: begin\n");
	progress_mark("reset-fastprobe-begin", 0x3eu, RESET_TAG);
	log_message("sf2000_reset_fastprobe: wdt-count=");
	log_hex_word(direct_read32(WDT_MAP_BASE_PHYS + WDT_REG_OFF + WDT_COUNT_OFF));
	log_message("\n");
	log_message("sf2000_reset_fastprobe: wdt-conf=");
	log_hex_word(direct_read8(WDT_MAP_BASE_PHYS + WDT_REG_OFF + WDT_CONF_OFF));
	log_message("\n");
	log_message("sf2000_reset_fastprobe: sfclk=");
	log_hex_word(direct_read32(SYSIO_BASE_PHYS + SYS_SFCLK_OFF));
	log_message("\n");
	log_message("sf2000_reset_fastprobe: gate0=");
	log_hex_word(direct_read32(SYSIO_BASE_PHYS + SYS_CLOCK_GATE0_OFF));
	log_message("\n");
	log_message("sf2000_reset_fastprobe: iovolt=");
	log_hex_word(direct_read32(SYSIO_BASE_PHYS + SYS_IO_VOLTAGE_OFF));
	log_message("\n");
	for (i = 16u; i <= 22u; i++)
		pins |= (unsigned int)(direct_read8(
			SYSIO_BASE_PHYS + PINMUX_L_OFF + i) & 0xfu)
			<< ((i - 16u) * 4u);
	log_message("sf2000_reset_fastprobe: pin-l16-22=");
	log_hex_word(pins);
	log_message("\n");
	progress_mark("reset-fastprobe-pin-l16-22", 0x31u, pins);
	log_message("sf2000_reset_fastprobe: done\n");
	progress_mark("reset-fastprobe-done", 0x30u, RESET_TAG);
	syscall1(SYS_exit, 0);
	for (;;)
		;
}
