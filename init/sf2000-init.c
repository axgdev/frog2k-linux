/* SPDX-License-Identifier: MIT */

typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_reboot 4088

#define O_WRONLY 1

#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_MAGIC2 672274793
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321fedc

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

void _start(void)
{
	long console_fd;
	long log_fd;

	log_fd = syscall3(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0);
	if (log_fd >= 0) {
		write_all(log_fd, "<6>sf2000_linux: initramfs alive\n");
		write_all(log_fd, "<6>sf2000_linux: no userspace payload yet\n");
		syscall1(SYS_close, log_fd);
	} else {
		console_fd = syscall3(SYS_open, (long)"/dev/console", O_WRONLY, 0);
		if (console_fd < 0)
			console_fd = 1;

		write_all(console_fd, "sf2000_linux: initramfs alive\n");
		write_all(console_fd, "sf2000_linux: no userspace payload yet\n");
		if (console_fd > 2)
			syscall1(SYS_close, console_fd);
	}

	syscall3(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
		 LINUX_REBOOT_CMD_POWER_OFF);
	syscall1(SYS_exit, 0);
	for (;;)
		;
}
