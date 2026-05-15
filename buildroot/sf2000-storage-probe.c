#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define KSEG1ADDR(x) ((volatile void *)(uintptr_t)((uint32_t)(x) | 0xa0000000u))
#define PROGRESS_PHYS 0x013f0000u
#define PROGRESS_MAGIC 0x52504653u
#define PROGRESS_VERSION 1u
#define PROGRESS_ENTRIES 1024u
#define PROGRESS_NAME_LEN 32u
#define STORAGE_TAG 0x0200u

struct progress_entry {
	uint32_t seq;
	uint32_t kind;
	uint32_t value;
	uint32_t name_ptr;
	char name[PROGRESS_NAME_LEN];
};

struct progress_log {
	uint32_t magic;
	uint32_t version;
	uint32_t seq;
	uint32_t write_index;
	uint32_t wrapped;
	uint32_t reserved[3];
	struct progress_entry entries[PROGRESS_ENTRIES];
};

static void progress_copy_name(volatile char *dst, const char *src)
{
	unsigned i;

	for (i = 0; i < PROGRESS_NAME_LEN - 1u && src[i]; i++)
		dst[i] = src[i];
	for (; i < PROGRESS_NAME_LEN; i++)
		dst[i] = 0;
}

static void progress_mark(const char *name, uint32_t kind, uint32_t value)
{
	volatile struct progress_log *log =
		(volatile struct progress_log *)KSEG1ADDR(PROGRESS_PHYS);
	volatile struct progress_entry *entry;
	uint32_t index;
	uint32_t seq;

	if (log->magic != PROGRESS_MAGIC || log->version != PROGRESS_VERSION) {
		memset((void *)log, 0, sizeof(*log));
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
	entry->name_ptr = (uint32_t)(uintptr_t)name;
	progress_copy_name(entry->name, name);

	log->write_index = index + 1u;
	log->seq = seq;
}

static uint32_t hash_name(const char *text)
{
	uint32_t hash = 2166136261u;

	while (*text) {
		hash ^= (unsigned char)*text++;
		hash *= 16777619u;
	}
	return hash;
}

static void log_line(const char *line)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		(void)write(fd, "<6>", 3);
		(void)write(fd, line, strlen(line));
		close(fd);
		return;
	}
	(void)write(STDERR_FILENO, line, strlen(line));
}

static void log_msgf(const char *fmt, ...)
{
	char line[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	log_line(line);
}

static void ensure_mounts(void)
{
	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	mkdir("/dev", 0755);
	mkdir("/dev/pts", 0755);
	mkdir("/run", 0755);
	mkdir("/mnt", 0755);
	mkdir("/mnt/sd", 0755);
	(void)mount("proc", "/proc", "proc", 0, "");
	progress_mark("stor-mount-proc", 0x3au, (uint32_t)errno);
	(void)mount("sysfs", "/sys", "sysfs", 0, "");
	progress_mark("stor-mount-sys", 0x3au, (uint32_t)errno);
	(void)mount("devtmpfs", "/dev", "devtmpfs", 0, "");
	progress_mark("stor-mount-dev", 0x3au, (uint32_t)errno);
	(void)mount("devpts", "/dev/pts", "devpts", 0, "");
}

static void log_dir(const char *label, const char *path)
{
	DIR *dir = opendir(path);
	struct dirent *de;
	unsigned count = 0;

	if (!dir) {
		log_msgf("sf2000_storage_probe: %s open failed errno=%d\n",
			label, errno);
		progress_mark(label, 0x3bu, (uint32_t)errno);
		return;
	}
	while ((de = readdir(dir)) != NULL && count < 24) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		log_msgf("sf2000_storage_probe: %s %s\n", label, de->d_name);
		progress_mark("stor-dir-entry", 0x3bu, hash_name(de->d_name));
		count++;
	}
	closedir(dir);
	if (count == 0)
		log_msgf("sf2000_storage_probe: %s empty\n", label);
	progress_mark(label, 0x3bu, count);
}

static void log_file_head(const char *label, const char *path)
{
	char buf[256];
	ssize_t got;
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0) {
		log_msgf("sf2000_storage_probe: %s open failed errno=%d\n",
			label, errno);
		progress_mark(label, 0x3cu, (uint32_t)errno);
		return;
	}
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got < 0) {
		log_msgf("sf2000_storage_probe: %s read failed errno=%d\n",
			label, errno);
		progress_mark(label, 0x3cu, (uint32_t)errno);
		return;
	}
	buf[got] = 0;
	log_msgf("sf2000_storage_probe: %s %.180s\n", label, buf);
	progress_mark(label, 0x3cu, (uint32_t)got);
	if (got >= 4)
		progress_mark("stor-file-head0", 0x3cu,
			(uint32_t)(unsigned char)buf[0] |
			((uint32_t)(unsigned char)buf[1] << 8) |
			((uint32_t)(unsigned char)buf[2] << 16) |
			((uint32_t)(unsigned char)buf[3] << 24));
}

static int try_mount_write(const char *dev)
{
	int fd;

	progress_mark("stor-try", 0x3du, hash_name(dev));
	if (access(dev, R_OK) != 0) {
		log_msgf("sf2000_storage_probe: missing %s errno=%d\n",
			dev, errno);
		progress_mark("stor-missing", 0x3du, (uint32_t)errno);
		return -1;
	}
	log_msgf("sf2000_storage_probe: mount try %s\n", dev);
	if (mount(dev, "/mnt/sd", "vfat", MS_SYNCHRONOUS, "") != 0) {
		log_msgf("sf2000_storage_probe: mount failed %s errno=%d\n",
			dev, errno);
		progress_mark("stor-mount-fail", 0x3du, (uint32_t)errno);
		return -1;
	}
	log_msgf("sf2000_storage_probe: mount ok %s\n", dev);
	progress_mark("stor-mount-ok", 0x3du, hash_name(dev));
	fd = open("/mnt/sd/sf2000-linux-rw-0200.txt",
		O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		log_msgf("sf2000_storage_probe: write open failed errno=%d\n",
			errno);
		progress_mark("stor-open-fail", 0x3du, (uint32_t)errno);
	} else {
		const char msg[] = "sf2000 linux sd write test 0200\n";
		ssize_t wrote = write(fd, msg, sizeof(msg) - 1u);

		close(fd);
		log_msgf("sf2000_storage_probe: write ret=%d errno=%d\n",
			(int)wrote, errno);
		progress_mark("stor-write-ret", 0x3du, (uint32_t)wrote);
		progress_mark("stor-write-errno", 0x3du, (uint32_t)errno);
	}
	sync();
	if (umount("/mnt/sd") != 0) {
		log_msgf("sf2000_storage_probe: umount failed errno=%d\n", errno);
		progress_mark("stor-umount-fail", 0x3du, (uint32_t)errno);
	} else {
		log_msgf("sf2000_storage_probe: umount ok\n");
		progress_mark("stor-umount-ok", 0x3du, 0);
	}
	return 0;
}

int main(void)
{
	progress_mark("stor-start", 0x3au, STORAGE_TAG);
	log_line("sf2000_storage_probe: start\n");
	ensure_mounts();
	log_dir("sys-block", "/sys/block");
	log_dir("mmc-host", "/sys/class/mmc_host");
	log_dir("platform-devices", "/sys/bus/platform/devices");
	log_dir("platform-drivers", "/sys/bus/platform/drivers");
	log_dir("dev", "/dev");
	log_file_head("proc-partitions", "/proc/partitions");
	log_file_head("proc-devices", "/proc/devices");
	log_file_head("proc-interrupts", "/proc/interrupts");
	if (try_mount_write("/dev/mmcblk0p1") != 0)
		(void)try_mount_write("/dev/mmcblk0");
	log_line("sf2000_storage_probe: done\n");
	progress_mark("stor-done", 0x3au, STORAGE_TAG);
	return 0;
}
