typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_write 4004
#define SYS_open 4005
#define SYS_read 4003
#define SYS_close 4006
#define SYS_execve 4011
#define SYS_mkdirat 4289
#define SYS_mount 4021
#define SYS_clone 4120
#define SYS_pause 4029
#define SYS_dup2 4063
#define SYS_wait4 4114
#define SYS_mmap 4090
#define SYS_kill 4037
#define SYS_sync 4036
#define SYS_umount2 4052
#define SYS_reboot 4088
#define SYS_nanosleep 4166
#define SYS_mmap2 4210

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
/* MIPS o32 uses the architecture-specific values, not asm-generic's. */
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define AT_FDCWD -100
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define SIGCHLD 18
#define SIGTERM 15
#define SIGKILL 9
#define LINUX_REBOOT_MAGIC1 0xfee1deadUL
#define LINUX_REBOOT_MAGIC2 672274793UL
#define LINUX_REBOOT_CMD_RESTART 0x01234567UL
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321fedcUL
#define CLONE_VM 0x00000100UL
#define SERVICE_STACK_BYTES 4096u
#define SERVICE_STACK_WORDS (SERVICE_STACK_BYTES / sizeof(unsigned long))

#define SYSIO_BASE_PHYS 0x18800000UL
#define SYSIO_SIZE 0x1000UL
#define KSEG1ADDR(x) ((volatile unsigned char *)((unsigned long)(x) | 0xa0000000UL))
#define PINMUX_L_OFF 0x4a0UL
#define PINMUX_R_OFF 0x4e0UL
#define SYS_CLOCK_GATE0_OFF 0x060UL
#define SYS_SFCLK_OFF 0x07cUL
#define SYS_IO_VOLTAGE_OFF 0x184UL
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
#define PROGRESS_PHYS 0x07a00000UL
#define PROGRESS_MAGIC 0x52504653UL
#define PROGRESS_VERSION 1UL
#define PROGRESS_ENTRIES 1024UL
#define PROGRESS_NAME_LEN 32UL
#define INIT_TAG 0x0239UL

struct timespec {
	long tv_sec;
	long tv_nsec;
};

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

static char *const screen_argv[] = { "/usr/sbin/sf2000-screen", 0 };
static char *const pad_argv[] = { "/usr/sbin/sf2000-pad", "sf2000", 0 };
static char *const powerd_argv[] = { "/usr/sbin/sf2000-powerd", 0 };
static char *const audio_argv[] = { "/usr/sbin/sf2000-audio", 0 };
static char *const logd_argv[] = { "/usr/sbin/sf2000-logd", 0 };
static char *const panel_probe_argv[] = { "/usr/sbin/sf2000-panel-probe", 0 };
static char *const storage_argv[] = {
	"/usr/sbin/sf2000-mount", 0
};
static char *const fb_test_argv[] = { "/usr/bin/fb-test", "-p", "0", 0 };
static char *const init_envp[] = {
	"HOME=/",
	"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
	"TERM=linux",
	"SF2000_PAD_PROFILE=sf2000",
	"SF2000_SCREEN=0",
	"SF2000_HEARTBEAT=0",
	0
};
static unsigned long screen_stack[SERVICE_STACK_WORDS];
static unsigned long pad_stack[SERVICE_STACK_WORDS];
static unsigned long powerd_stack[SERVICE_STACK_WORDS];
static unsigned long audio_stack[SERVICE_STACK_WORDS];
static unsigned long logd_stack[SERVICE_STACK_WORDS];
static unsigned long storage_late_stack[SERVICE_STACK_WORDS];
static unsigned long fb_test_stack[SERVICE_STACK_WORDS];

static void log_message(const char *message);
extern long sf2000_clone_service(unsigned long child_stack, char *const argv[]);

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

void sf2000_init_entry_mark(void)
{
	progress_mark("init-entry-asm", 0x3eu, INIT_TAG);
}

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

static long syscall0(long nr)
{
	register long r2 __asm__("$2") = nr;
	register long r7 __asm__("$7");

	__asm__ volatile (
		"syscall"
		: "+r"(r2), "=r"(r7)
		:
		: "$3", "$4", "$5", "$6", "$8", "$9", "$10", "$11",
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

static int mkdir_path(const char *path, unsigned int mode)
{
	return (int)syscall3(SYS_mkdirat, AT_FDCWD, (long)path, (long)mode);
}

static int publish_request(const char *path)
{
	static const char request[] = "requested\n";
	long fd;

	fd = syscall3(SYS_open, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		return (int)fd;
	(void)syscall3(SYS_write, fd, (long)request, sizeof(request) - 1u);
	(void)syscall1(SYS_close, fd);
	return 0;
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

static int diagnostic_watchdog_disable(void)
{
	return early_watchdog_disable();
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

static int path_exists(const char *path)
{
	long fd = syscall3(SYS_open, (long)path, O_RDONLY, 0);

	if (fd < 0)
		return 0;
	syscall1(SYS_close, fd);
	return 1;
}

static int mount_procfs(void)
{
	(void)mkdir_path("/proc", 0755);
	if (syscall6(SYS_mount, (long)"proc", (long)"/proc", (long)"proc",
			0, (long)"", 0) == 0)
		return 0;
	return -1;
}

static int cmdline_contains(const char *needle)
{
	static char buf[512];
	long fd;
	long got;
	unsigned int i;
	unsigned int j;

	fd = syscall3(SYS_open, (long)"/proc/cmdline", O_RDONLY, 0);
	if (fd < 0)
		return 0;
	got = syscall3(SYS_read, fd, (long)buf, (long)(sizeof(buf) - 1u));
	syscall1(SYS_close, fd);
	if (got <= 0)
		return 0;
	buf[got] = 0;

	for (i = 0; buf[i]; i++) {
		for (j = 0; needle[j]; j++) {
			if (buf[i + j] != needle[j])
				break;
		}
		if (!needle[j])
			return 1;
	}
	return 0;
}

static unsigned int direct_read32(unsigned long phys)
{
	return *(volatile unsigned int *)KSEG1ADDR(phys);
}

static unsigned int direct_read8(unsigned long phys)
{
	return *(volatile unsigned char *)KSEG1ADDR(phys);
}

static void reset_snapshot_fast(void)
{
	unsigned int pins = 0;
	unsigned int i;

	log_message("sf2000_buildroot: reset snapshot fast\n");
	progress_mark("init-reset-snapshot-fast", 0x3eu, INIT_TAG);
	progress_mark("diag-fast-reset-begin", 0x30u, INIT_TAG);
	progress_mark("diag-fast-wdt-count", 0x31u,
		direct_read32(WDT_MAP_BASE_PHYS + WDT_REG_OFF + WDT_COUNT_OFF));
	progress_mark("diag-fast-wdt-conf", 0x31u,
		direct_read8(WDT_MAP_BASE_PHYS + WDT_REG_OFF + WDT_CONF_OFF));
	progress_mark("diag-fast-sfclk", 0x31u,
		direct_read32(SYSIO_BASE_PHYS + SYS_SFCLK_OFF));
	progress_mark("diag-fast-gate0", 0x31u,
		direct_read32(SYSIO_BASE_PHYS + SYS_CLOCK_GATE0_OFF));
	progress_mark("diag-fast-iovolt", 0x31u,
		direct_read32(SYSIO_BASE_PHYS + SYS_IO_VOLTAGE_OFF));
	for (i = 16u; i <= 22u; i++)
		pins |= (unsigned int)(direct_read8(
			SYSIO_BASE_PHYS + PINMUX_L_OFF + i) & 0xfu)
			<< ((i - 16u) * 4u);
	progress_mark("diag-fast-pin-l16-22", 0x31u, pins);
	progress_mark("diag-fast-reset-done", 0x30u, INIT_TAG);
}

static unsigned long service_stack_top(unsigned long *stack)
{
	return ((unsigned long)(stack + SERVICE_STACK_WORDS)) & ~7UL;
}

void service_child_exec(char *const *argv)
{
	long ret;

	progress_mark("init-child-exec", 0x3eu, INIT_TAG);
	ret = syscall3(SYS_execve, (long)argv[0], (long)argv,
		(long)init_envp);
	progress_mark("init-child-exec-fail", 0x3eu, (unsigned int)ret);
	log_message("sf2000_buildroot: service exec failed\n");
	log_message("sf2000_buildroot: service path ");
	log_message(argv[0]);
	log_message("\n");
	log_message("sf2000_buildroot: service exec ret ");
	log_hex_word((unsigned int)ret);
	syscall1(SYS_exit, 127);
}

typedef long (*clone_service_fn)(unsigned long child_stack, char *const argv[]);

static long spawn_service_with(const char *name, char *const argv[],
		unsigned long *stack, clone_service_fn clone_service)
{
	long pid;
	unsigned long child_stack = service_stack_top(stack);

	log_message(name);
	progress_mark("init-spawn-begin", 0x3eu, (unsigned int)child_stack);
	diagnostic_watchdog_pet();
	pid = clone_service(child_stack, argv);
	progress_mark("init-spawn-ret", 0x3eu, (unsigned int)pid);
	if (pid < 0) {
		log_message("sf2000_buildroot: service clone failed ");
		log_hex_word((unsigned int)pid);
		return pid;
	}
	if (pid == 0)
		service_child_exec(argv);
	diagnostic_watchdog_pet();
	return pid;
}

static long spawn_service(const char *name, char *const argv[],
		unsigned long *stack)
{
	return spawn_service_with(name, argv, stack, sf2000_clone_service);
}

static long reap_child(int *status)
{
	return syscall4(SYS_wait4, -1, (long)status, 1, 0);
}

static int stop_service(long pid)
{
	int status = 0;
	long ret;
	unsigned int elapsed;

	if (pid <= 0)
		return -1;
	/*
	 * The display diagnostic owns the hardware watchdog while it performs
	 * panel transactions.  Disarm it before and after signalling so neither
	 * a pending watchdog tick nor a final pet can survive the handoff.
	 * Process exit closes /dev/ge and releases its IRQ through the kernel.
	 */
	(void)diagnostic_watchdog_disable();
	progress_mark("init-screen-stop-signal", 0x3eu, (unsigned int)pid);
	ret = syscall2(SYS_kill, pid, SIGTERM);
	if (ret < 0)
		return (int)ret;
	(void)diagnostic_watchdog_disable();
	for (elapsed = 0; elapsed < 50u; elapsed++) {
		ret = syscall4(SYS_wait4, pid, (long)&status, 1, 0);
		if (ret == pid)
			break;
		sleep_ms(10);
	}
	if (ret != pid) {
		/*
		 * A wedged MMIO transaction cannot be unwound cooperatively.
		 * Never let it prevent an otherwise synchronized shutdown.
		 */
		progress_mark("init-screen-stop-timeout", 0x3eu, elapsed);
		(void)syscall2(SYS_kill, pid, SIGKILL);
		ret = syscall4(SYS_wait4, pid, (long)&status, 0, 0);
	}
	progress_mark("init-screen-stop-wait", 0x3eu, (unsigned int)ret);
	/* Keep the invariant true even if a future screen binary exits abruptly. */
	(void)diagnostic_watchdog_disable();
	return ret == pid ? 0 : -1;
}

static int stop_logger(long pid)
{
	int status = 0;
	long ret;

	if (pid <= 0)
		return 0;
	/*
	 * The input service has already published the reboot marker.  The
	 * logger polls that same marker, drains its buffer, fsyncs the log and
	 * exits.  A shared marker keeps shutdown ordered with ordinary log writes
	 * and avoids an asynchronous signal handler in the storage process.
	 */
	progress_mark("init-logd-stop-request", 0x3eu, (unsigned int)pid);
	ret = syscall4(SYS_wait4, pid, (long)&status, 0, 0);
	progress_mark("init-logd-stop-wait", 0x3eu, (unsigned int)ret);
	return ret == pid ? 0 : -1;
}

static void graceful_restart(long logd_pid)
{
	long ret;

	log_message("sf2000_buildroot: clean restart requested\n");
	progress_mark("init-reboot-request", 0x3eu, INIT_TAG);
	(void)stop_logger(logd_pid);
	(void)syscall0(SYS_sync);
	ret = syscall2(SYS_umount2, (long)"/mnt/sd", 0);
	progress_mark("init-reboot-umount", 0x3eu, (unsigned int)ret);
	(void)syscall0(SYS_sync);
	log_message("sf2000_buildroot: storage synchronized, restarting\n");
	progress_mark("init-reboot-synced", 0x3eu, INIT_TAG);
	(void)syscall4(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		LINUX_REBOOT_CMD_RESTART, 0);
	log_message("sf2000_buildroot: reboot syscall returned\n");
}

static void graceful_shutdown(long logd_pid, long screen_pid)
{
	long ret;

	log_message("sf2000_buildroot: safe shutdown requested\n");
	progress_mark("init-shutdown-request", 0x3eu, INIT_TAG);
	ret = publish_request("/run/sf2000-screen-stop-request");
	progress_mark("init-screen-stop-request", 0x3eu, (unsigned int)ret);
	(void)stop_logger(logd_pid);
	(void)syscall0(SYS_sync);
	ret = syscall2(SYS_umount2, (long)"/mnt/sd", 0);
	progress_mark("init-shutdown-umount", 0x3eu, (unsigned int)ret);
	(void)syscall0(SYS_sync);
	if (screen_pid > 0)
		(void)stop_service(screen_pid);
	log_message("sf2000_buildroot: storage safe; powering off\n");
	progress_mark("init-shutdown-synced", 0x3eu, INIT_TAG);
	(void)syscall4(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		LINUX_REBOOT_CMD_POWER_OFF, 0);
	log_message("sf2000_buildroot: power-off returned; safe to switch off\n");
	for (;;)
		(void)syscall0(SYS_pause);
}

int main(void)
{
	unsigned int storage_started = 0;
	unsigned int screen_wait_ticks = 0;
	unsigned int spawn_storage = 0;
	unsigned int panel_probe = 0;
	unsigned int fb_test_started = 0;
	unsigned int fb_test_enabled = 0;
	long screen_pid = -1;
	long fb_test_pid = -1;
	long logd_pid = -1;
	log_message("sf2000_buildroot: init main entry\n");
	progress_mark("init-main", 0x3eu, INIT_TAG);
	setup_stdio();
	progress_mark("init-stdio", 0x3eu, INIT_TAG);
	(void)mount_procfs();
	panel_probe = cmdline_contains("SF2000_PANEL_PROBE=1");
	fb_test_enabled = cmdline_contains("SF2000_FB_TEST=1");
#ifdef PANEL_PROBE_INIT
	panel_probe = 1;
#endif
	if (panel_probe)
		log_message("sf2000_buildroot: panel probe requested\n");
	if (early_watchdog_disable() == 0)
		log_message("sf2000_buildroot: early watchdog disabled\n");
	else
		log_message("sf2000_buildroot: early watchdog disable failed\n");
	if (diagnostic_watchdog_disable() == 0)
		log_message("sf2000_buildroot: runtime watchdog disabled\n");
	else
		log_message("sf2000_buildroot: runtime watchdog disable failed\n");
	log_message("sf2000_buildroot: /init visible userspace stage begin\n");
	if (visible_userspace_stage() == 0)
		log_message("sf2000_buildroot: /init visible userspace stage done\n");
	else
		log_message("sf2000_buildroot: /init visible userspace stage failed\n");
	log_message("sf2000_buildroot: userspace alive\n");
	progress_mark("init-userspace-alive", 0x3eu, INIT_TAG);
	if (path_exists("/dev/fb0")) {
		log_message("sf2000_buildroot: framebuffer ready /dev/fb0\n");
		progress_mark("init-framebuffer-ready", 0x3eu, INIT_TAG);
	} else {
		log_message("sf2000_buildroot: framebuffer missing /dev/fb0\n");
	}
	if (path_exists("/dev/ge")) {
		log_message("sf2000_buildroot: graphics engine ready /dev/ge\n");
		progress_mark("init-ge-ready", 0x3eu, INIT_TAG);
	} else {
		log_message("sf2000_buildroot: graphics engine missing /dev/ge\n");
	}
	if (cmdline_contains("SF2000_RESET_SNAPSHOT=fast"))
		reset_snapshot_fast();

	if (panel_probe) {
		log_message("sf2000_buildroot: starting panel probe\n");
		spawn_service("sf2000_buildroot: starting panel probe\n",
			panel_probe_argv, screen_stack);
		log_message("sf2000_buildroot: panel probe started\n");
		progress_mark("init-panel-probe", 0x3eu, INIT_TAG);
		storage_started = 1;
	}
	/* Buffer the complete boot profile in RAM while display and MMC settle. */
	logd_pid = spawn_service(
		"sf2000_buildroot: starting persistent logger\n", logd_argv,
		logd_stack);
	diagnostic_watchdog_pet();
	if (!panel_probe) {
		screen_pid = spawn_service("sf2000_buildroot: starting screen\n",
			screen_argv, screen_stack);
		/*
		 * The display handoff and the retained progress ring are shared board
		 * resources.  Finish that dependency before starting helpers which
		 * also publish retained records; this avoids both CPU starvation and
		 * unsynchronised multi-process writes during the RGB/GMA transition.
		 */
		while (screen_wait_ticks < 300u &&
		       !path_exists("/run/sf2000-screen-ready")) {
			diagnostic_watchdog_pet();
			sleep_ms(100);
			screen_wait_ticks++;
		}
	}
	spawn_service("sf2000_buildroot: starting input bridge\n", pad_argv,
		pad_stack);
	spawn_service("sf2000_buildroot: starting power coordinator\n", powerd_argv,
		powerd_stack);
	if (cmdline_contains("SF2000_AUDIO_TEST=1"))
		spawn_service("sf2000_buildroot: starting audio DMA test\n",
			audio_argv, audio_stack);
	diagnostic_watchdog_pet();

	log_message("sf2000_buildroot: direct init supervisor running\n");
	progress_mark("init-supervisor", 0x3eu, INIT_TAG);
	for (;;) {
		long child;
		int child_status;

		while ((child = reap_child(&child_status)) > 0) {
			if (child == logd_pid)
				logd_pid = -1;
			if (child == fb_test_pid) {
				progress_mark("init-fb-test-exit", 0x3eu,
					(unsigned int)child_status);
				log_message("sf2000_buildroot: framebuffer test complete\n");
				fb_test_pid = -1;
			}
		}
		if (path_exists("/run/sf2000-reboot-request"))
			graceful_restart(logd_pid);
		if (path_exists("/run/sf2000-shutdown-request"))
			graceful_shutdown(logd_pid, screen_pid);
		if (!panel_probe && fb_test_enabled && !fb_test_started &&
		    screen_pid > 0 && path_exists("/run/sf2000-screen-ready")) {
			log_message("sf2000_buildroot: stopping screen for framebuffer test\n");
			progress_mark("init-fb-handoff-begin", 0x3eu, INIT_TAG);
			if (stop_service(screen_pid) == 0) {
				screen_pid = -1;
				log_message("sf2000_buildroot: starting framebuffer test directly\n");
				fb_test_pid = spawn_service(
					"sf2000_buildroot: exec /usr/bin/fb-test -p 0\n",
					fb_test_argv, fb_test_stack);
				progress_mark("init-fb-test-started", 0x3eu,
					(unsigned int)fb_test_pid);
			} else {
				log_message("sf2000_buildroot: screen stop failed\n");
				progress_mark("init-fb-screen-stop-fail", 0x3eu,
					(unsigned int)screen_pid);
			}
			fb_test_started = 1;
		}
		if (!storage_started) {
			spawn_storage = 0;
			if (path_exists("/run/sf2000-screen-ready")) {
				log_message("sf2000_buildroot: screen ready\n");
				progress_mark("init-screen-ready", 0x3eu,
					screen_wait_ticks);
				storage_started = 1;
				spawn_storage = 1;
			} else if (screen_wait_ticks >= 10u) {
				log_message("sf2000_buildroot: screen ready timeout\n");
				progress_mark("init-screen-timeout", 0x3eu,
					screen_wait_ticks);
				storage_started = 1;
				spawn_storage = 1;
			} else {
				screen_wait_ticks++;
			}
			if (spawn_storage) {
				progress_mark("init-storage-spawn", 0x3eu,
					INIT_TAG);
				spawn_service("sf2000_buildroot: starting storage service after screen\n",
					storage_argv, storage_late_stack);
				log_message("sf2000_buildroot: storage service started\n");
				progress_mark("init-helpers-started", 0x3eu,
					INIT_TAG);
			}
		}
		if (path_exists("/run/sf2000-storage-watchdog-owned"))
			progress_mark("init-storage-wdt-owned", 0x3eu,
				INIT_TAG);
		else
			diagnostic_watchdog_pet();
		sleep_ms(100);
	}

	syscall1(SYS_exit, 1);
	return 0;
}
