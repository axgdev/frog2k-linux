/* SPDX-License-Identifier: MIT */

#include <stdint.h>

typedef unsigned int size_t;

#define SYS_exit 4001
#define SYS_read 4003
#define SYS_write 4004
#define SYS_open 4005
#define SYS_close 4006
#define SYS_lseek 4019
#define SYS_sched_yield 4162
#define SYS_fsync 4118

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_NONBLOCK 04000
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
	write_all(1, message);
}

static __attribute__((noinline)) int open_mmcb_block(void)
{
	long fd;
	unsigned int i;

	for (i = 0; i < 100000; i++) {
		fd = syscall3(SYS_open, (long)"/dev/mmcblk0",
			      O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
		if (fd >= 0) {
			return (int)fd;
		}
		(void)syscall1(SYS_sched_yield, 0);
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
		return 11;
	}
	ret = syscall3(SYS_lseek, fd, WRITE_OFFSET, SEEK_SET);
	if (ret < 0) {
		log_message("sf2000_storage_fastprobe: lseek failed\n");
		(void)syscall1(SYS_close, fd);
		return 12;
	}
	ret = syscall3(SYS_write, fd, (long)out, (long)SECTOR_SIZE);
	if (ret != (long)SECTOR_SIZE) {
		log_message("sf2000_storage_fastprobe: write failed\n");
		(void)syscall1(SYS_close, fd);
		return 13;
	}
	if (syscall1(SYS_fsync, fd) != 0) {
		log_message("sf2000_storage_fastprobe: fsync failed\n");
		(void)syscall1(SYS_close, fd);
		return 14;
	}
	ret = syscall3(SYS_lseek, fd, WRITE_OFFSET, SEEK_SET);
	if (ret < 0) {
		log_message("sf2000_storage_fastprobe: readback seek failed\n");
		(void)syscall1(SYS_close, fd);
		return 15;
	}
	ret = syscall3(SYS_read, fd, (long)in, (long)SECTOR_SIZE);
	(void)syscall1(SYS_close, fd);
	if (ret != (long)SECTOR_SIZE) {
		log_message("sf2000_storage_fastprobe: readback failed\n");
		return 16;
	}
	for (i = 0; i < SECTOR_SIZE; i++) {
		if (in[i] != out[i]) {
			log_message("sf2000_storage_fastprobe: verify mismatch\n");
			return 17;
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
