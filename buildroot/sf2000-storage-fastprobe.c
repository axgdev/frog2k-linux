/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>

typedef unsigned int size_t;

extern long syscall(long number, ...);

#define SYS_exit 4001
#define SYS_read 4003
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_lseek 4019
#define SYS_getpid 4020
#ifdef SYS_mount
#undef SYS_mount
#endif
#define SYS_mount 4021
#define SYS_fsync 4148

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_CLOEXEC 02000000
#define SEEK_SET 0

#define SECTOR_SIZE 512u
#define WRITE_LBA 16u
#define WRITE_OFFSET (WRITE_LBA * SECTOR_SIZE)
#define WRITE_SIGNATURE "sf2000 linux sd write test 0239"

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

static void write_all(long fd, const char *s)
{
	const char *p = s;

	while (*p)
		p++;
	(void)syscall3(SYS_write, fd, (long)s, (long)(p - s));
}

static void log_message(const char *message);

void storage_probe_entry_mark(void)
{
	log_message("sf2000_storage_fastprobe: entry-start\n");
}

static void log_message(const char *message)
{
	long log_fd = syscall3(SYS_open, (long)"/dev/kmsg", O_WRONLY, 0);

	if (log_fd < 0) {
		log_fd = syscall3(SYS_open, (long)"/dev/console", O_WRONLY, 0);
	}
	if (log_fd < 0) {
		log_fd = 1;
	}
	write_all(log_fd, "<6>");
	write_all(log_fd, message);
	if (log_fd > 2) {
		(void)syscall1(SYS_close, log_fd);
	}
}

static void log_message_long(const char *prefix, long value)
{
	static const char digits[] = "0123456789abcdef";
	char buf[64];
	unsigned int i = 0;
	unsigned int v = (unsigned int)value;
	unsigned int p;

	for (p = 0; prefix[p] && i + 1 < sizeof(buf); p++)
		buf[i++] = prefix[p];
	if (i + 10 >= sizeof(buf)) {
		buf[sizeof(buf) - 2] = '\n';
		buf[sizeof(buf) - 1] = 0;
		log_message(buf);
		return;
	}
	buf[i++] = '0';
	buf[i++] = 'x';
	for (p = 0; p < 8; p++)
		buf[i++] = digits[(v >> ((7 - p) * 4)) & 0xf];
	buf[i++] = '\n';
	buf[i] = 0;
	log_message(buf);
}

static __attribute__((noinline)) int open_mmcb_block(void)
{
	long fd;
	unsigned int i;

	for (i = 0; i < 80; i++) {
		fd = syscall3(SYS_open, (long)"/dev/mmcblk0",
			      O_RDWR | O_CLOEXEC, 0);
		if (fd >= 0) {
			return (int)fd;
		}
		if (i == 0) {
			(void)syscall6(SYS_mount, (long)"sysfs", (long)"/sys",
				       (long)"sysfs", 0, 0, 0);
			(void)syscall6(SYS_mount, (long)"devtmpfs", (long)"/dev",
				       (long)"devtmpfs", 0, 0, 0);
		}
		(void)syscall1(SYS_getpid, 0);
	}
	return -1;
}

static __attribute__((noinline)) int raw_writeback_probe(void)
{
	int fd;
	long ret;
	static char out[SECTOR_SIZE];
	static char in[SECTOR_SIZE];
	const char signature[] = WRITE_SIGNATURE;
	unsigned int i;

	for (i = 0; i < SECTOR_SIZE; i++) {
		out[i] = 0;
		in[i] = 0;
	}
	for (i = 0; signature[i] && i < SECTOR_SIZE; i++) {
		out[i] = signature[i];
	}

	fd = open_mmcb_block();
	if (fd < 0) {
		log_message("sf2000_storage_fastprobe: open mmcblk0 failed\n");
		return 1;
	}
	ret = syscall3(SYS_lseek, fd, WRITE_OFFSET, SEEK_SET);
	if (ret < 0) {
		log_message("sf2000_storage_fastprobe: lseek failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}
	ret = syscall3(SYS_write, fd, (long)out, (long)SECTOR_SIZE);
	if (ret != (long)SECTOR_SIZE) {
		log_message("sf2000_storage_fastprobe: write failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}
	if (fsync(fd) != 0) {
		log_message("sf2000_storage_fastprobe: fsync failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}
	ret = syscall3(SYS_lseek, fd, WRITE_OFFSET, SEEK_SET);
	if (ret < 0) {
		log_message("sf2000_storage_fastprobe: readback seek failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}
	ret = syscall3(SYS_read, fd, (long)in, (long)SECTOR_SIZE);
	(void)syscall1(SYS_close, fd);
	if (ret != (long)SECTOR_SIZE) {
		log_message("sf2000_storage_fastprobe: readback failed\n");
		return 1;
	}
	for (i = 0; i < SECTOR_SIZE; i++) {
		if (in[i] != out[i]) {
			log_message("sf2000_storage_fastprobe: verify mismatch\n");
			return 1;
		}
	}
	log_message("sf2000_storage_fastprobe: raw writeback ok 0239\n");
	return 0;
}

int main(void)
{
	storage_probe_entry_mark();
	log_message("sf2000_storage_fastprobe: probe begin\n");
	return raw_writeback_probe();
}
