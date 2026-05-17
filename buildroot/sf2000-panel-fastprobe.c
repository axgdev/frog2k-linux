/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef unsigned int size_t;

extern long syscall(long number, ...);

#define SYS_exit 4001
#define SYS_read 4003
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006

#define O_WRONLY 1

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
	(void)syscall3(SYS_write, fd, (long)s, (long)(p - s));
}

static void log_message(const char *message)
{
	long log_fd = syscall3(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0);

	if (log_fd < 0)
		log_fd = syscall3(SYS_open, (long)"/dev/console", O_WRONLY, 0);
	if (log_fd < 0)
		log_fd = 1;
	write_all(log_fd, "<6>");
	write_all(log_fd, message);
	if (log_fd > 2)
		(void)syscall1(SYS_close, log_fd);
}

int main(void)
{
	log_message("sf2000_panel_fastprobe: probe begin\n");
	log_message("sf2000_panel_fastprobe: probe done\n");
	return 0;
}
