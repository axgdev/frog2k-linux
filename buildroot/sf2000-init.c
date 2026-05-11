typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_execve 4011
#define SYS_pause 4029
#define SYS_dup2 4063

#define O_RDONLY 0
#define O_WRONLY 1

static char *const init_argv[] = { "/sbin/init", 0 };
static char *const init_envp[] = {
	"HOME=/",
	"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
	"TERM=linux",
	0
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
	log_message("sf2000_buildroot: userspace alive\n");

	syscall3(SYS_execve, (long)init_argv[0], (long)init_argv, (long)init_envp);

	log_message("sf2000_buildroot: /sbin/init exec failed\n");
	for (;;)
		syscall1(SYS_pause, 0);

	syscall1(SYS_exit, 1);
}
