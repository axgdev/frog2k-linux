/* SPDX-License-Identifier: MIT */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

typedef unsigned int size_t;

extern long syscall(long number, ...);

#define PROGRESS_PHYS 0x013f0000u
#define PROGRESS_MAGIC 0x52504653u
#define PROGRESS_VERSION 1u
#define PROGRESS_ENTRIES 1024u
#define PROGRESS_NAME_LEN 32u

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
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_CLOEXEC 02000000
#define SEEK_SET 0

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

static volatile struct progress_log *progress_log_current(void)
{
	return (volatile struct progress_log *)(uintptr_t)(
		PROGRESS_PHYS | 0xa0000000u);
}

static void progress_copy_name(volatile char *dst, const char *src)
{
	unsigned i;

	for (i = 0; i < PROGRESS_NAME_LEN - 1u && src[i]; i++)
		dst[i] = src[i];
	for (; i < PROGRESS_NAME_LEN; i++)
		dst[i] = 0;
}

static void progress_mark(const char *name, unsigned int kind,
		unsigned int value)
{
	volatile struct progress_log *log = progress_log_current();
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
	entry->name_ptr = (unsigned int)(uintptr_t)name;
	progress_copy_name(entry->name, name);
	log->write_index = index + 1u;
	log->seq = seq;
}

void storage_probe_entry_mark(void)
{
	progress_mark("entry-start", 0x11u, 0x656e7472u);
}

static int path_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0;
}

static void log_message(const char *message);

static int mount_vfat_writeback(void)
{
	long fd;
	long ret;

	(void)mkdir("/mnt", 0755);
	(void)mkdir("/mnt/sd", 0755);
	ret = mount("/dev/mmcblk0", "/mnt/sd", "vfat", MS_NOATIME, "");
	if (ret != 0) {
		ret = mount("/dev/mmcblk0", "/mnt/sd", "msdos", MS_NOATIME, "");
	}
	if (ret != 0) {
		log_message("sf2000_storage_fastprobe: mount failed\n");
	return 1;
	}
	log_message("sf2000_storage_fastprobe: mount ok\n");
	fd = syscall3(SYS_open, (long)"/mnt/sd/sf2000-linux-rw-0227.txt",
		      O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0) {
		log_message("sf2000_storage_fastprobe: file open failed\n");
		(void)umount("/mnt/sd");
		return 1;
	}
	write_all(fd, "sf2000 linux sd write test 0227\n");
	if (fsync((int)fd) != 0) {
		log_message("sf2000_storage_fastprobe: fsync failed\n");
	}
	(void)syscall1(SYS_close, fd);
	log_message("sf2000_storage_fastprobe: write ok /sf2000-linux-rw-0227.txt\n");
	if (umount("/mnt/sd") != 0) {
		log_message("sf2000_storage_fastprobe: umount failed\n");
		return 1;
	}
	log_message("sf2000_storage_fastprobe: umount ok\n");
	return 0;
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
	log_message("sf2000_storage_fastprobe: probe begin\n");
	return 0;
}
