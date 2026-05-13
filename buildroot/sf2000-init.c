typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_execve 4011
#define SYS_clone 4120
#define SYS_pause 4029
#define SYS_dup2 4063
#define SYS_wait4 4114
#define SYS_mmap 4090
#define SYS_nanosleep 4166
#define SYS_mmap2 4210

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define SIGCHLD 18
#define CLONE_VM 0x00000100UL
#define SERVICE_STACK_BYTES 4096u
#define SERVICE_STACK_WORDS (SERVICE_STACK_BYTES / sizeof(unsigned long))

#define SYSIO_BASE_PHYS 0x18800000UL
#define SYSIO_SIZE 0x1000UL
#define KSEG1ADDR(x) ((volatile unsigned char *)((unsigned long)(x) | 0xa0000000UL))
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
#define WDT_MAP_BASE_PHYS 0x18818000UL
#define WDT_MAP_SIZE 0x1000UL
#define WDT_REG_OFF 0x500UL
#define WDT_COUNT_OFF 0x00UL
#define WDT_CONF_OFF 0x04UL
#define WDT_DIAG_COUNT 0xffc61075UL
#define WDT_DIAG_CONF 0x26UL

struct timespec {
	long tv_sec;
	long tv_nsec;
};

static char *const screen_argv[] = { "/usr/sbin/sf2000-screen", 0 };
static char *const init_envp[] = {
	"HOME=/",
	"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
	"TERM=linux",
	"SF2000_PAD_PROFILE=sf2000",
	0
};
static unsigned long screen_stack[SERVICE_STACK_WORDS];

static void log_message(const char *message);
extern long sf2000_clone_service(unsigned long child_stack, char *const argv[]);

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

static long syscall4(long nr, long a0, long a1, long a2, long a3)
{
	register long r2 __asm__("$2") = nr;
	register long r4 __asm__("$4") = a0;
	register long r5 __asm__("$5") = a1;
	register long r6 __asm__("$6") = a2;
	register long r7 __asm__("$7") = a3;

	__asm__ volatile (
		"syscall"
		: "+r"(r2), "+r"(r7)
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

static void diagnostic_watchdog_pet(void)
{
	volatile unsigned char *wdt = KSEG1ADDR(WDT_MAP_BASE_PHYS);

	*(volatile unsigned int *)(wdt + WDT_REG_OFF + WDT_COUNT_OFF) =
		WDT_DIAG_COUNT;
}

static void sleep_ms(unsigned int ms)
{
	struct timespec req;
	volatile unsigned int spin;
	unsigned int i;

	req.tv_sec = ms / 1000u;
	req.tv_nsec = (long)(ms % 1000u) * 1000000L;
	diagnostic_watchdog_pet();
	if (syscall2(SYS_nanosleep, (long)&req, (long)&req) >= 0) {
		diagnostic_watchdog_pet();
		return;
	}

	for (i = 0; i < ms; i++) {
		if ((i & 63u) == 0)
			diagnostic_watchdog_pet();
		for (spin = 0; spin < 18000u; spin++)
			__asm__ volatile ("" ::: "memory");
	}
	diagnostic_watchdog_pet();
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

static int early_watchdog_disable(void)
{
	volatile unsigned char *wdt;
	long fd;

	fd = syscall3(SYS_open, (long)"/dev/mem", O_RDWR, 0);
	if (fd < 0) {
		wdt = KSEG1ADDR(WDT_MAP_BASE_PHYS);
	} else {
		wdt = sys_mmap2(0, WDT_MAP_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, WDT_MAP_BASE_PHYS);
		syscall1(SYS_close, fd);
		if ((long)wdt < 0)
			wdt = KSEG1ADDR(WDT_MAP_BASE_PHYS);
	}

	mmio_write32(wdt, WDT_REG_OFF + WDT_COUNT_OFF, 0);
	mmio_write8(wdt, WDT_REG_OFF + WDT_CONF_OFF, 0);
	return 0;
}

static int diagnostic_watchdog_arm(void)
{
	volatile unsigned char *wdt;
	long fd;

	fd = syscall3(SYS_open, (long)"/dev/mem", O_RDWR, 0);
	if (fd < 0) {
		wdt = KSEG1ADDR(WDT_MAP_BASE_PHYS);
	} else {
		wdt = sys_mmap2(0, WDT_MAP_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, WDT_MAP_BASE_PHYS);
		syscall1(SYS_close, fd);
		if ((long)wdt < 0)
			wdt = KSEG1ADDR(WDT_MAP_BASE_PHYS);
	}

	mmio_write8(wdt, WDT_REG_OFF + WDT_CONF_OFF, 0);
	mmio_write32(wdt, WDT_REG_OFF + WDT_COUNT_OFF, WDT_DIAG_COUNT);
	mmio_write8(wdt, WDT_REG_OFF + WDT_CONF_OFF, WDT_DIAG_CONF);
	return 0;
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
	if (fd < 0) {
		sysio = KSEG1ADDR(SYSIO_BASE_PHYS);
	} else {
		sysio = sys_mmap2(0, SYSIO_SIZE, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, SYSIO_BASE_PHYS);
		syscall1(SYS_close, fd);
		if ((long)sysio < 0)
			sysio = KSEG1ADDR(SYSIO_BASE_PHYS);
	}

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

static void log_hex_word(unsigned int value)
{
	static const char digits[] = "0123456789abcdef";
	char buf[11];
	unsigned int i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < 8; i++)
		buf[2 + i] = digits[(value >> ((7 - i) * 4)) & 0xf];
	buf[10] = '\n';
	write_all(1, buf);
}

static unsigned long service_stack_top(unsigned long *stack)
{
	return ((unsigned long)(stack + SERVICE_STACK_WORDS)) & ~7UL;
}

void service_child_exec(char *const *argv)
{
	long ret;

	ret = syscall3(SYS_execve, (long)argv[0], (long)argv,
		(long)init_envp);
	log_message("sf2000_buildroot: service exec failed\n");
	log_message("sf2000_buildroot: service path ");
	log_message(argv[0]);
	log_message("\n");
	log_message("sf2000_buildroot: service exec ret ");
	log_hex_word((unsigned int)ret);
	syscall1(SYS_exit, 127);
}

static void spawn_service(const char *name, char *const argv[],
		unsigned long *stack)
{
	long pid;
	unsigned long child_stack = service_stack_top(stack);

	log_message(name);
	diagnostic_watchdog_pet();
	pid = sf2000_clone_service(child_stack, argv);
	if (pid < 0) {
		log_message("sf2000_buildroot: service clone failed ");
		log_hex_word((unsigned int)pid);
		return;
	}
	if (pid == 0)
		service_child_exec(argv);
	diagnostic_watchdog_pet();
}

static void reap_children(void)
{
	while (syscall4(SYS_wait4, -1, 0, 1, 0) > 0)
		;
}

void sf2000_init_main(void)
{
	setup_stdio();
	if (early_watchdog_disable() == 0)
		log_message("sf2000_buildroot: early watchdog disabled\n");
	else
		log_message("sf2000_buildroot: early watchdog disable failed\n");
	diagnostic_watchdog_arm();
	log_message("sf2000_buildroot: diagnostic watchdog armed\n");
	diagnostic_watchdog_pet();
	log_message("sf2000_buildroot: /init visible userspace stage begin\n");
	if (visible_userspace_stage() == 0)
		log_message("sf2000_buildroot: /init visible userspace stage done\n");
	else
		log_message("sf2000_buildroot: /init visible userspace stage failed\n");
	log_message("sf2000_buildroot: userspace alive\n");

	spawn_service("sf2000_buildroot: starting screen\n", screen_argv,
		screen_stack);
	diagnostic_watchdog_pet();
	log_message("sf2000_buildroot: libc helpers deferred\n");

	log_message("sf2000_buildroot: direct init supervisor running\n");
	for (;;) {
		reap_children();
		diagnostic_watchdog_pet();
		sleep_ms(250);
	}

	syscall1(SYS_exit, 1);
}
