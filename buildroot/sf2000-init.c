typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_execve 4011
#define SYS_pause 4029
#define SYS_dup2 4063
#define SYS_mmap 4090
#define SYS_nanosleep 4166
#define SYS_mmap2 4210

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1

#define SYSIO_BASE_PHYS 0x18800000UL
#define SYSIO_SIZE 0x1000UL
#define PINMUX_L_OFF 0x4a0UL
#define PINMUX_R_OFF 0x4e0UL
#define GPIO_L_OUT_OFF 0x54UL
#define GPIO_L_DIR_OFF 0x58UL
#define GPIO_R_OUT_OFF 0xf4UL
#define GPIO_R_DIR_OFF 0xf8UL
#define PIN_L25 25UL
#define PIN_R05 5UL
#define STATUS_L25 (1UL << PIN_L25)
#define BACKLIGHT_R05 (1UL << PIN_R05)

struct timespec {
	long tv_sec;
	long tv_nsec;
};

static char *const init_argv[] = { "/sbin/init", 0 };
static char *const init_envp[] = {
	"HOME=/",
	"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
	"TERM=linux",
	0
};

static void log_message(const char *message);

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

static long syscall2(long nr, long a0, long a1)
{
	register long r2 __asm__("$2") = nr;
	register long r4 __asm__("$4") = a0;
	register long r5 __asm__("$5") = a1;
	register long r7 __asm__("$7");

	__asm__ volatile (
		"syscall"
		: "+r"(r2), "=r"(r7)
		: "r"(r4), "r"(r5)
		: "$3", "$6", "$8", "$9", "$10", "$11", "$12",
		  "$13", "$14", "$15", "$24", "$25", "hi", "lo",
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

static long syscall6(long nr, long a0, long a1, long a2, long a3,
		long a4, long a5)
{
	register long r2 __asm__("$2") = nr;
	register long r4 __asm__("$4") = a0;
	register long r5 __asm__("$5") = a1;
	register long r6 __asm__("$6") = a2;
	register long r7 __asm__("$7") = a3;

	__asm__ volatile (
		"addiu $29, $29, -32\n\t"
		"sw %5, 16($29)\n\t"
		"sw %6, 20($29)\n\t"
		"syscall\n\t"
		"addiu $29, $29, 32"
		: "+r"(r2), "+r"(r7)
		: "r"(r4), "r"(r5), "r"(r6), "r"(a4), "r"(a5)
		: "$3", "$8", "$9", "$10", "$11", "$12", "$13",
		  "$14", "$15", "$24", "$25", "hi", "lo", "memory");
	if (r7)
		return -r2;
	return r2;
}

static void *sys_mmap2(void *addr, unsigned long length, unsigned long prot,
		unsigned long flags, long fd, unsigned long offset)
{
	long ret;

	ret = syscall6(SYS_mmap2, (long)addr, length, prot, flags, fd,
		offset >> 12);
	if (ret < 0) {
		unsigned long args[6];

		args[0] = (unsigned long)addr;
		args[1] = length;
		args[2] = prot;
		args[3] = flags;
		args[4] = (unsigned long)fd;
		args[5] = offset;
		ret = syscall1(SYS_mmap, (long)args);
	}
	return (void *)ret;
}

static void sleep_ms(unsigned int ms)
{
	struct timespec req;

	req.tv_sec = ms / 1000u;
	req.tv_nsec = (long)(ms % 1000u) * 1000000L;
	while (syscall2(SYS_nanosleep, (long)&req, (long)&req) < 0)
		;
}

static unsigned int mmio_read32(volatile unsigned char *base, unsigned long off)
{
	return *(volatile unsigned int *)(base + off);
}

static void mmio_write32(volatile unsigned char *base, unsigned long off,
		unsigned int value)
{
	*(volatile unsigned int *)(base + off) = value;
}

static void mmio_write8(volatile unsigned char *base, unsigned long off,
		unsigned char value)
{
	*(volatile unsigned char *)(base + off) = value;
}

static void userspace_backlight_set(volatile unsigned char *sysio, int on)
{
	unsigned int out;
	unsigned int dir;

	mmio_write8(sysio, PINMUX_R_OFF + PIN_R05, 0);
	dir = mmio_read32(sysio, GPIO_R_DIR_OFF);
	mmio_write32(sysio, GPIO_R_DIR_OFF, dir | BACKLIGHT_R05);

	out = mmio_read32(sysio, GPIO_R_OUT_OFF);
	if (on)
		out &= ~BACKLIGHT_R05;
	else
		out |= BACKLIGHT_R05;
	mmio_write32(sysio, GPIO_R_OUT_OFF, out);
}

static void userspace_status_led_set(volatile unsigned char *sysio, int on)
{
	unsigned int out;
	unsigned int dir;

	mmio_write8(sysio, PINMUX_L_OFF + PIN_L25, 0);
	dir = mmio_read32(sysio, GPIO_L_DIR_OFF);
	mmio_write32(sysio, GPIO_L_DIR_OFF, dir | STATUS_L25);

	out = mmio_read32(sysio, GPIO_L_OUT_OFF);
	if (on)
		out |= STATUS_L25;
	else
		out &= ~STATUS_L25;
	mmio_write32(sysio, GPIO_L_OUT_OFF, out);
}

static int visible_userspace_stage(void)
{
	volatile unsigned char *sysio;
	long fd;
	unsigned int i;

	fd = syscall3(SYS_open, (long)"/dev/mem", O_RDWR, 0);
	if (fd < 0)
		return -1;

	sysio = sys_mmap2(0, SYSIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
		fd, SYSIO_BASE_PHYS);
	syscall1(SYS_close, fd);
	if ((long)sysio < 0)
		return -2;

	userspace_backlight_set(sysio, 1);
	userspace_status_led_set(sysio, 0);
	sleep_ms(1200);
	for (i = 0; i < 10; i++) {
		userspace_backlight_set(sysio, 0);
		userspace_status_led_set(sysio, 1);
		sleep_ms(220);
		userspace_backlight_set(sysio, 1);
		userspace_status_led_set(sysio, 0);
		sleep_ms(120);
	}
	userspace_status_led_set(sysio, 0);
	sleep_ms(1200);
	return 0;
}

static void write_all(long fd, const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	syscall3(SYS_write, fd, (long)s, (long)(p - s));
}

static void setup_stdio(void)
{
	long null_fd;
	long log_fd;

	null_fd = syscall3(SYS_open, (long)"/dev/null", O_RDONLY, 0);
	if (null_fd >= 0) {
		syscall2(SYS_dup2, null_fd, 0);
		if (null_fd > 2)
			syscall1(SYS_close, null_fd);
	}

	log_fd = syscall3(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0);
	if (log_fd >= 0) {
		syscall2(SYS_dup2, log_fd, 1);
		syscall2(SYS_dup2, log_fd, 2);
		if (log_fd > 2)
			syscall1(SYS_close, log_fd);
	}
}

static void log_message(const char *message)
{
	long log_fd = syscall3(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0);
	long console_fd;

	if (log_fd >= 0) {
		write_all(log_fd, "<6>");
		write_all(log_fd, message);
		syscall1(SYS_close, log_fd);
		return;
	}

	console_fd = syscall3(SYS_open, (long)"/dev/console", O_WRONLY, 0);
	if (console_fd < 0)
		console_fd = 1;

	write_all(console_fd, message);
	if (console_fd > 2)
		syscall1(SYS_close, console_fd);
}

void _start(void)
{
	setup_stdio();
	log_message("sf2000_buildroot: /init visible userspace stage begin\n");
	if (visible_userspace_stage() == 0)
		log_message("sf2000_buildroot: /init visible userspace stage done\n");
	else
		log_message("sf2000_buildroot: /init visible userspace stage failed\n");
	log_message("sf2000_buildroot: userspace alive\n");

	syscall3(SYS_execve, (long)init_argv[0], (long)init_argv, (long)init_envp);

	log_message("sf2000_buildroot: /sbin/init exec failed\n");
	for (;;)
		syscall1(SYS_pause, 0);

	syscall1(SYS_exit, 1);
}
