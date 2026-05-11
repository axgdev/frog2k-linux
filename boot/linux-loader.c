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
#define GPIO_R_OUT 0xb88000f4u
#define GPIO_R_DIR 0xb88000f8u
#define BACKLIGHT_R05 (1u << 5)
#define BACKLIGHT_PULSE_DELAY 50000000u

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

static void delay_cycles(u32 count)
{
	while (count-- != 0)
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

	mmio_write32(GPIO_R_OUT, out);
	mmio_write32(GPIO_R_DIR, dir | BACKLIGHT_R05);
	mmio_write8(PINMUX_R05, 0);
}

static void backlight_loader_mark(void)
{
	backlight_set(0);
	delay_cycles(BACKLIGHT_PULSE_DELAY);
	backlight_set(1);
	delay_cycles(BACKLIGHT_PULSE_DELAY);
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

static void jump_to_kernel(u32 entry, uintptr dtb)
{
	uart_puts("linux-loader: jump entry=");
	uart_hex(entry);
	uart_puts(" dtb=");
	uart_hex((u32)dtb);
	uart_puts("\n");

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

	clear_bss();
	disable_interrupts();
	backlight_loader_mark();

	kernel_size = (usize)(linux_vmlinux_end - linux_vmlinux_start);
	dtb_size = (usize)(linux_dtb_end - linux_dtb_start);
	eh = (const struct elf32_ehdr *)linux_vmlinux_start;

	uart_puts("\nlinux-loader: start kernel=");
	uart_hex((u32)kernel_size);
	uart_puts(" dtb=");
	uart_hex((u32)dtb_size);
	uart_puts("\n");

	if (!valid_elf(eh, kernel_size)) {
		uart_puts("linux-loader: invalid embedded kernel\n");
		for (;;)
			;
	}

	load_min = 0;
	load_max = 0;
	if (scan_linux_elf(linux_vmlinux_start, kernel_size,
			&load_min, &load_max) != 0) {
		for (;;)
			;
	}

	payload_end = align_up((uintptr)linux_dtb_end, 0x10000u);
	dtb_dest = align_up(load_max, 0x10000u);
	if (dtb_dest < payload_end)
		dtb_dest = payload_end;
	if (dtb_dest + dtb_size > RAM_TOP) {
		uart_puts("linux-loader: no room for DTB\n");
		for (;;)
			;
	}
	copy_forward((u8 *)dtb_dest, linux_dtb_start, dtb_size);
	copy_linux_elf(linux_vmlinux_start);

	cache_flush_range(load_min, (usize)(load_max - load_min));
	cache_flush_range(dtb_dest, dtb_size);
	disable_interrupts();
	backlight_set(1);
	jump_to_kernel(eh->e_entry, dtb_dest);

	for (;;)
		;
}
