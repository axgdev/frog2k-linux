/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

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

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CLOEXEC 02000000
#define SEEK_SET 0

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

static int path_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0;
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

static void create_block_node_from_sysfs(const char *sys_path,
					 const char *dev_path)
{
	FILE *f;
	unsigned major;
	unsigned minor;

	f = fopen(sys_path, "r");
	if (!f) {
		log_message("sf2000_storage_fastprobe: sysfs dev open failed\n");
		return;
	}
	if (fscanf(f, "%u:%u", &major, &minor) == 2) {
		(void)mknod(dev_path, S_IFBLK | 0660, makedev(major, minor));
		log_message("sf2000_storage_fastprobe: sysfs dev node created\n");
	} else {
		log_message("sf2000_storage_fastprobe: sysfs dev parse failed\n");
	}
	fclose(f);
}

int main(void)
{
	unsigned char write_buf[512] = { 0 };
	unsigned char read_buf[512];
	long fd;
	long ret;
	unsigned i;
	unsigned waited;

	log_message("sf2000_storage_fastprobe: probe begin\n");
	(void)mkdir("/dev", 0755);
	ret = syscall6(SYS_mount, (long)"devtmpfs", (long)"/dev",
		       (long)"devtmpfs", 0, (long)"mode=0755", 0);
	if (ret < 0) {
		log_message_long("sf2000_storage_fastprobe: devtmpfs mount failed ret=", ret);
	}
	(void)mkdir("/proc", 0755);
	ret = syscall6(SYS_mount, (long)"proc", (long)"/proc",
		       (long)"proc", 0, 0, 0);
	if (ret < 0) {
		log_message_long("sf2000_storage_fastprobe: proc mount failed ret=", ret);
	}
	(void)mkdir("/sys", 0755);
	ret = syscall6(SYS_mount, (long)"sysfs", (long)"/sys", (long)"sysfs", 0, 0, 0);
	if (ret < 0) {
		log_message_long("sf2000_storage_fastprobe: sysfs mount failed ret=", ret);
	}
	for (waited = 0; waited < 30; waited++) {
		if (path_exists("/dev/mmcblk0")) {
			break;
		}
		if (path_exists("/sys/block/mmcblk0/dev")) {
			create_block_node_from_sysfs("/sys/block/mmcblk0/dev",
						    "/dev/mmcblk0");
			if (path_exists("/dev/mmcblk0")) {
				break;
			}
		}
		log_message("sf2000_storage_fastprobe: waiting mmc device\n");
		sleep(1);
	}
	if (!path_exists("/dev/mmcblk0")) {
		create_block_node_from_sysfs("/sys/block/mmcblk0/dev", "/dev/mmcblk0");
	}
	if (path_exists("/dev/mmcblk0")) {
		log_message("sf2000_storage_fastprobe: mmc device ready\n");
	}
	fd = syscall3(SYS_open, (long)"/dev/mmcblk0", O_RDWR | O_CLOEXEC, 0);
	if (fd < 0) {
		log_message("sf2000_storage_fastprobe: open failed\n");
		return 1;
	}

	ret = syscall3(SYS_write, fd, (long)write_buf, sizeof(write_buf));
	if (ret != (long)sizeof(write_buf)) {
		log_message("sf2000_storage_fastprobe: write failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}

	ret = syscall3(SYS_lseek, fd, 0, SEEK_SET);
	if (ret < 0) {
		log_message("sf2000_storage_fastprobe: seek failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}

	ret = syscall3(SYS_read, fd, (long)read_buf, sizeof(read_buf));
	if (ret != (long)sizeof(read_buf)) {
		log_message("sf2000_storage_fastprobe: read failed\n");
		(void)syscall1(SYS_close, fd);
		return 1;
	}

	(void)syscall1(SYS_close, fd);

	for (i = 0; i < sizeof(write_buf); i++) {
		if (read_buf[i] != write_buf[i]) {
			log_message("sf2000_storage_fastprobe: readback mismatch\n");
			return 1;
		}
	}

	log_message("sf2000_storage_fastprobe: readback ok\n");
	return 0;
}
