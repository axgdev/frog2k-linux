#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#define KSEG1ADDR(x) ((volatile void *)(uintptr_t)((uint32_t)(x) | 0xa0000000u))
#define PROGRESS_PHYS 0x013f0000u
#define PROGRESS_MAGIC 0x52504653u
#define PROGRESS_VERSION 1u
#define PROGRESS_ENTRIES 1024u
#define PROGRESS_NAME_LEN 32u
#define STORAGE_TAG 0x0234u
#define RAW_TEST_NAME "SF2L0234TXT"
#define RAW_FIXED_CLUSTER 5u
#define RAW_FIXED_DIR_SECTOR 6u
#define RAW_FIXED_DIR_SLOT 2u
#define WDT_BASE_PHYS 0x18818000u
#define WDT_REG_OFF 0x500u
#define WDT_COUNT_OFF 0x00u
#define WDT_CONF_OFF 0x04u
#define WDT_STORAGE_COUNT 0xffe64035u
#define WDT_STORAGE_CONF 0x26u
#define STORAGE_WDT_CLAIM "/run/sf2000-storage-watchdog-owned"
#ifndef O_DIRECT
#define O_DIRECT 00040000
#endif
#define MMC_BLOCK_MAJOR 179
#define MMC_IOC_CMD _IOWR(MMC_BLOCK_MAJOR, 0, struct mmc_ioc_cmd)
#define MMC_READ_SINGLE_BLOCK 17u
#define MMC_WRITE_BLOCK 24u
#define MMC_RSP_R1 0x15u
#define MMC_CMD_ADTC 0x20u

struct mmc_ioc_cmd {
	int write_flag;
	int is_acmd;
	uint32_t opcode;
	uint32_t arg;
	uint32_t response[4];
	unsigned int flags;
	unsigned int blksz;
	unsigned int blocks;
	unsigned int postsleep_min_us;
	unsigned int postsleep_max_us;
	unsigned int data_timeout_ns;
	unsigned int cmd_timeout_ms;
	uint32_t __pad;
	uint64_t data_ptr;
};

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

static void progress_reset(const char *name)
{
	volatile struct progress_log *log =
		(volatile struct progress_log *)KSEG1ADDR(PROGRESS_PHYS);

	memset((void *)log, 0, sizeof(*log));
	log->magic = PROGRESS_MAGIC;
	log->version = PROGRESS_VERSION;
	progress_mark(name, 0x3au, STORAGE_TAG);
}

static unsigned char direct_write_buf[512] __attribute__((aligned(512)));

static uint32_t hash_name(const char *text)
{
	uint32_t hash = 2166136261u;

	while (*text) {
		hash ^= (unsigned char)*text++;
		hash *= 16777619u;
	}
	return hash;
}

static uint16_t get_le16(const unsigned char *buf, unsigned off)
{
	return (uint16_t)buf[off] | ((uint16_t)buf[off + 1u] << 8);
}

static uint32_t get_le32(const unsigned char *buf, unsigned off)
{
	return (uint32_t)buf[off] |
		((uint32_t)buf[off + 1u] << 8) |
		((uint32_t)buf[off + 2u] << 16) |
		((uint32_t)buf[off + 3u] << 24);
}

static void put_le16(unsigned char *buf, unsigned off, uint16_t value)
{
	buf[off] = (unsigned char)value;
	buf[off + 1u] = (unsigned char)(value >> 8);
}

static void put_le32(unsigned char *buf, unsigned off, uint32_t value)
{
	buf[off] = (unsigned char)value;
	buf[off + 1u] = (unsigned char)(value >> 8);
	buf[off + 2u] = (unsigned char)(value >> 16);
	buf[off + 3u] = (unsigned char)(value >> 24);
}

static void log_line(const char *line)
{
	(void)line;
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

static void storage_watchdog_arm(const char *name)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);
	int fd;

	progress_mark(name, 0x3du, STORAGE_TAG);
	fd = open(STORAGE_WDT_CLAIM, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC,
		0644);
	if (fd >= 0)
		close(fd);
	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) = 0;
	*(volatile uint32_t *)(wdt + WDT_REG_OFF + WDT_COUNT_OFF) =
		WDT_STORAGE_COUNT;
	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) =
		WDT_STORAGE_CONF;
	progress_mark("stor-wdt-armed", 0x3du, WDT_STORAGE_CONF);
}

static void storage_watchdog_release(const char *name)
{
	volatile uint8_t *wdt = KSEG1ADDR(WDT_BASE_PHYS);

	progress_mark(name, 0x3du, STORAGE_TAG);
	unlink(STORAGE_WDT_CLAIM);
	*(volatile uint8_t *)(wdt + WDT_REG_OFF + WDT_CONF_OFF) = 0;
	progress_mark("stor-wdt-released", 0x3du, 0);
}

static void ensure_mounts(void)
{
	int ret;

	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	mkdir("/dev", 0755);
	mkdir("/dev/pts", 0755);
	mkdir("/run", 0755);
	mkdir("/mnt", 0755);
	mkdir("/mnt/sd", 0755);
	errno = 0;
	ret = mount("proc", "/proc", "proc", 0, "");
	progress_mark("stor-mount-proc-ret", 0x3au, (uint32_t)ret);
	progress_mark("stor-mount-proc-err", 0x3au, (uint32_t)errno);
	errno = 0;
	ret = mount("sysfs", "/sys", "sysfs", 0, "");
	progress_mark("stor-mount-sys-ret", 0x3au, (uint32_t)ret);
	progress_mark("stor-mount-sys-err", 0x3au, (uint32_t)errno);
	errno = 0;
	ret = mount("devtmpfs", "/dev", "devtmpfs", 0, "");
	progress_mark("stor-mount-dev-ret", 0x3au, (uint32_t)ret);
	progress_mark("stor-mount-dev-err", 0x3au, (uint32_t)errno);
	errno = 0;
	ret = mount("devpts", "/dev/pts", "devpts", 0, "");
	progress_mark("stor-mount-pts-ret", 0x3au, (uint32_t)ret);
	progress_mark("stor-mount-pts-err", 0x3au, (uint32_t)errno);
}

static void log_dir(const char *label, const char *path)
{
	DIR *dir;
	struct dirent *de;
	unsigned count = 0;

	progress_mark("stor-dir-begin", 0x3bu, hash_name(path));
	dir = opendir(path);
	if (!dir) {
		log_msgf("sf2000_storage_probe: %s open failed errno=%d\n",
			label, errno);
		progress_mark(label, 0x3bu, (uint32_t)errno);
		return;
	}
	progress_mark("stor-dir-open", 0x3bu, hash_name(label));
	while ((de = readdir(dir)) != NULL && count < 24) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		log_msgf("sf2000_storage_probe: %s %s\n", label, de->d_name);
		progress_mark("stor-dir-entry", 0x3bu, hash_name(de->d_name));
		count++;
	}
	progress_mark("stor-dir-read-done", 0x3bu, count);
	closedir(dir);
	if (count == 0)
		log_msgf("sf2000_storage_probe: %s empty\n", label);
	progress_mark(label, 0x3bu, count);
}

static void log_file_head(const char *label, const char *path)
{
	char buf[256];
	ssize_t got;
	int fd;

	progress_mark("stor-file-begin", 0x3cu, hash_name(path));
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		log_msgf("sf2000_storage_probe: %s open failed errno=%d\n",
			label, errno);
		progress_mark(label, 0x3cu, (uint32_t)errno);
		return;
	}
	progress_mark("stor-file-open", 0x3cu, hash_name(label));
	got = read(fd, buf, sizeof(buf) - 1u);
	progress_mark("stor-file-read-ret", 0x3cu, (uint32_t)got);
	close(fd);
	if (got < 0) {
		progress_mark(label, 0x3cu, (uint32_t)errno);
		log_msgf("sf2000_storage_probe: %s read failed errno=%d\n",
			label, errno);
		return;
	}
	buf[got] = 0;
	progress_mark(label, 0x3cu, (uint32_t)got);
	if (got >= 4)
		progress_mark("stor-file-head0", 0x3cu,
			(uint32_t)(unsigned char)buf[0] |
			((uint32_t)(unsigned char)buf[1] << 8) |
			((uint32_t)(unsigned char)buf[2] << 16) |
			((uint32_t)(unsigned char)buf[3] << 24));
	log_msgf("sf2000_storage_probe: %s %.180s\n", label, buf);
}

static int read_devt(const char *path, unsigned *major_out,
	unsigned *minor_out)
{
	char buf[32];
	unsigned major = 0;
	unsigned minor = 0;
	ssize_t got;
	unsigned i = 0;
	int fd;

	progress_mark("stor-devt-path", 0x3du, hash_name(path));
	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		progress_mark("stor-devt-open-fail", 0x3du, (uint32_t)errno);
		return -1;
	}
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got <= 0) {
		progress_mark("stor-devt-read-fail", 0x3du,
			got < 0 ? (uint32_t)errno : 0);
		return -1;
	}
	buf[got] = 0;
	while (buf[i] >= '0' && buf[i] <= '9') {
		major = major * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	if (buf[i] != ':') {
		progress_mark("stor-devt-bad", 0x3du, hash_name(buf));
		return -1;
	}
	i++;
	while (buf[i] >= '0' && buf[i] <= '9') {
		minor = minor * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	*major_out = major;
	*minor_out = minor;
	progress_mark("stor-devt-major", 0x3du, major);
	progress_mark("stor-devt-minor", 0x3du, minor);
	return 0;
}

static void mknod_from_devt(const char *sysdev, const char *node)
{
	unsigned major;
	unsigned minor;
	int ret;

	if (read_devt(sysdev, &major, &minor) != 0)
		return;
	errno = 0;
	unlink(node);
	errno = 0;
	ret = mknod(node, S_IFBLK | 0660, makedev(major, minor));
	progress_mark("stor-devt-mknod-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-devt-mknod-err", 0x3du, (uint32_t)errno);
}

static void stat_node(const char *path)
{
	struct stat st;

	progress_mark("stor-stat-path", 0x3du, hash_name(path));
	errno = 0;
	if (stat(path, &st) != 0) {
		progress_mark("stor-stat-fail", 0x3du, (uint32_t)errno);
		return;
	}
	progress_mark("stor-stat-mode", 0x3du, (uint32_t)st.st_mode);
	progress_mark("stor-stat-major", 0x3du, (uint32_t)major(st.st_rdev));
	progress_mark("stor-stat-minor", 0x3du, (uint32_t)minor(st.st_rdev));
}

static void ensure_block_nodes(void)
{
	int ret;

	errno = 0;
	ret = mknod("/dev/mmcblk0", S_IFBLK | 0660, makedev(179, 0));
	progress_mark("stor-mknod-mmc0-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-mknod-mmc0-err", 0x3du, (uint32_t)errno);
	errno = 0;
	ret = mknod("/dev/mmcblk0p1", S_IFBLK | 0660, makedev(179, 1));
	progress_mark("stor-mknod-mmc0p1-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-mknod-mmc0p1-err", 0x3du, (uint32_t)errno);
	errno = 0;
	ret = mknod("/dev/mmcblk0p2", S_IFBLK | 0660, makedev(179, 2));
	progress_mark("stor-mknod-mmc0p2-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-mknod-mmc0p2-err", 0x3du, (uint32_t)errno);
	mknod_from_devt("/sys/block/mmcblk0/dev", "/dev/mmcblk0sys");
	mknod_from_devt("/sys/class/block/mmcblk0/dev", "/dev/mmcblk0class");
	mknod_from_devt("/sys/class/block/mmcblk0p1/dev", "/dev/mmcblk0p1sys");
	mknod_from_devt("/sys/class/block/mmcblk0p2/dev", "/dev/mmcblk0p2sys");
}

static void log_block_head(const char *dev)
{
	unsigned char buf[512];
	ssize_t got;
	int fd;

	progress_mark("stor-blk-open-begin", 0x3du, hash_name(dev));
	fd = open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		log_msgf("sf2000_storage_probe: block open failed %s errno=%d\n",
			dev, errno);
		progress_mark("stor-blk-open-fail", 0x3du, (uint32_t)errno);
		return;
	}
	progress_mark("stor-blk-open-ok", 0x3du, hash_name(dev));
	got = read(fd, buf, sizeof(buf));
	progress_mark("stor-blk-read-ret", 0x3du, (uint32_t)got);
	if (got >= 4)
		progress_mark("stor-blk-head0", 0x3du,
			(uint32_t)buf[0] |
			((uint32_t)buf[1] << 8) |
			((uint32_t)buf[2] << 16) |
			((uint32_t)buf[3] << 24));
	if (got >= 512)
		progress_mark("stor-blk-sig", 0x3du,
			(uint32_t)buf[510] | ((uint32_t)buf[511] << 8));
	close(fd);
}

static void set_readahead_zero(const char *dev)
{
	unsigned long value = 0;
	int fd;
	int ret;

	progress_mark("stor-ra-path", 0x3du, hash_name(dev));
	fd = open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		progress_mark("stor-ra-open-fail", 0x3du, (uint32_t)errno);
		return;
	}
	errno = 0;
	ret = ioctl(fd, BLKRASET, value);
	progress_mark("stor-ra-ret", 0x3du, (uint32_t)ret);
	progress_mark("stor-ra-err", 0x3du, (uint32_t)errno);
	close(fd);
}

static int read_sector_lba(const char *dev, uint32_t lba, const char *label,
	unsigned char *buf)
{
	ssize_t got;
	off_t pos;
	int fd;

	progress_mark(label, 0x3cu, lba);
	fd = open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		progress_mark("stor-sec-open-fail", 0x3cu, (uint32_t)errno);
		return -1;
	}
	pos = lseek(fd, (off_t)lba * 512, SEEK_SET);
	progress_mark("stor-sec-seek", 0x3cu, (uint32_t)pos);
	if (pos < 0) {
		progress_mark("stor-sec-seek-fail", 0x3cu, (uint32_t)errno);
		close(fd);
		return -1;
	}
	got = read(fd, buf, 512);
	progress_mark("stor-sec-read-ret", 0x3cu, (uint32_t)got);
	close(fd);
	if (got != 512) {
		progress_mark("stor-sec-read-err", 0x3cu,
			got < 0 ? (uint32_t)errno : (uint32_t)got);
		return -1;
	}
	progress_mark("stor-sec-head0", 0x3cu, get_le32(buf, 0));
	progress_mark("stor-sec-sig", 0x3cu, get_le16(buf, 510));
	return 0;
}

static int write_sector_lba_flags(const char *dev, uint32_t lba,
	const char *label, const unsigned char *buf, int flags, int do_fsync)
{
	ssize_t wrote;
	int saved_errno = 0;
	int fsret = 0;
	int open_flags;
	off_t pos;
	int fd;

	progress_mark(label, 0x3cu, lba);
	memcpy(direct_write_buf, buf, sizeof(direct_write_buf));
	open_flags = O_RDWR | O_CLOEXEC | O_DIRECT | flags;
	progress_mark("stor-wsec-flags", 0x3cu, (uint32_t)open_flags);
	fd = open(dev, open_flags);
	if (fd < 0) {
		progress_mark("stor-wsec-open-fail", 0x3cu, (uint32_t)errno);
		return -1;
	}
	pos = lseek(fd, (off_t)lba * 512, SEEK_SET);
	progress_mark("stor-wsec-seek", 0x3cu, (uint32_t)pos);
	if (pos < 0) {
		progress_mark("stor-wsec-seek-fail", 0x3cu, (uint32_t)errno);
		close(fd);
		return -1;
	}
	storage_watchdog_arm("stor-wdt-raw-write");
	errno = 0;
	progress_mark("stor-wsec-direct", 0x3cu, lba);
	wrote = write(fd, direct_write_buf, sizeof(direct_write_buf));
	saved_errno = errno;
	progress_mark("stor-wsec-write-ret", 0x3cu, (uint32_t)wrote);
	progress_mark("stor-wsec-write-errn", 0x3cu, (uint32_t)saved_errno);
	if (do_fsync) {
		errno = 0;
		fsret = fsync(fd);
		progress_mark("stor-wsec-fsync-ret", 0x3cu,
			(uint32_t)fsret);
		progress_mark("stor-wsec-fsync-err", 0x3cu,
			(uint32_t)errno);
	}
	storage_watchdog_release("stor-wdt-raw-write-done");
	close(fd);
	if (wrote != 512) {
		progress_mark("stor-wsec-write-err", 0x3cu,
			wrote < 0 ? (uint32_t)saved_errno : (uint32_t)wrote);
		return -1;
	}
	return 0;
}

static int ioctl_sector_lba(const char *dev, uint32_t lba, const char *label,
	unsigned char *buf, int write_flag)
{
	struct mmc_ioc_cmd cmd;
	int saved_errno;
	int ret;
	int fd;

	progress_mark(label, 0x3bu, lba);
	if (write_flag)
		memcpy(direct_write_buf, buf, sizeof(direct_write_buf));
	memset(&cmd, 0, sizeof(cmd));
	cmd.write_flag = write_flag ? 1 : 0;
	cmd.opcode = write_flag ? MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;
	cmd.arg = lba;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.blksz = 512;
	cmd.blocks = 1;
	cmd.data_timeout_ns = 1000000000u;
	cmd.cmd_timeout_ms = 1000;
	cmd.data_ptr = (uint64_t)(uintptr_t)(write_flag ? direct_write_buf : buf);

	fd = open(dev, (write_flag ? O_RDWR : O_RDONLY) | O_CLOEXEC);
	if (fd < 0) {
		progress_mark("stor-ioctl-open-fail", 0x3bu, (uint32_t)errno);
		return -1;
	}

	progress_mark("stor-ioctl-op", 0x3bu, cmd.opcode);
	progress_mark("stor-ioctl-arg", 0x3bu, cmd.arg);
	if (write_flag)
		storage_watchdog_arm("stor-wdt-ioctl-write");
	errno = 0;
	ret = ioctl(fd, MMC_IOC_CMD, &cmd);
	saved_errno = errno;
	progress_mark("stor-ioctl-ret", 0x3bu, (uint32_t)ret);
	progress_mark("stor-ioctl-errno", 0x3bu, (uint32_t)saved_errno);
	progress_mark("stor-ioctl-r0", 0x3bu, cmd.response[0]);
	if (write_flag)
		storage_watchdog_release("stor-wdt-ioctl-write-done");
	close(fd);
	if (ret != 0)
		return -1;
	if (!write_flag) {
		progress_mark("stor-ioctl-head0", 0x3bu, get_le32(buf, 0));
		progress_mark("stor-ioctl-sig", 0x3bu, get_le16(buf, 510));
	}
	return 0;
}

static int write_sector_lba_ioctl(const char *dev, uint32_t lba,
	const char *label, unsigned char *buf)
{
	return ioctl_sector_lba(dev, lba, label, buf, 1);
}

static int read_sector_lba_ioctl(const char *dev, uint32_t lba,
	const char *label, unsigned char *buf)
{
	return ioctl_sector_lba(dev, lba, label, buf, 0);
}

static int write_sector_lba(const char *dev, uint32_t lba, const char *label,
	const unsigned char *buf)
{
	return write_sector_lba_flags(dev, lba, label, buf, 0, 0);
}

static uint32_t read_fat32_entry(const char *dev, uint32_t fat_start,
	uint32_t cluster)
{
	unsigned char buf[512];
	uint32_t fat_off = cluster * 4u;
	uint32_t lba = fat_start + fat_off / 512u;
	unsigned off = fat_off & 511u;

	if (read_sector_lba(dev, lba, "fat-entry-sec", buf) != 0)
		return 0xffffffffu;
	return get_le32(buf, off) & 0x0fffffffu;
}

static int write_fat32_entry(const char *dev, uint32_t first_fat,
	uint32_t fat_size, uint32_t cluster, uint32_t value)
{
	unsigned char buf[512];
	uint32_t fat_off = cluster * 4u;
	uint32_t rel_lba = fat_off / 512u;
	unsigned off = fat_off & 511u;

	if (read_sector_lba(dev, first_fat + rel_lba, "fat-wr-sec1", buf) != 0)
		return -1;
	put_le32(buf, off, value);
	if (write_sector_lba(dev, first_fat + rel_lba, "fat-write1", buf) != 0)
		return -1;

	if (read_sector_lba(dev, first_fat + fat_size + rel_lba,
			"fat-wr-sec2", buf) != 0)
		return -1;
	put_le32(buf, off, value);
	return write_sector_lba(dev, first_fat + fat_size + rel_lba,
		"fat-write2", buf);
}

static uint32_t find_free_fat32_cluster(const char *dev, uint32_t first_fat,
	uint32_t max_cluster)
{
	unsigned char buf[512];
	uint32_t lba;
	uint32_t cluster;
	unsigned off;
	unsigned i;

	cluster = 2u;
	for (i = 0; i < 512u; i++) {
		lba = first_fat + (cluster * 4u) / 512u;
		if (read_sector_lba(dev, lba, "fat-scan-sec", buf) != 0)
			return 0;
		for (off = (cluster * 4u) & 511u; off < 512u; off += 4u) {
			if ((get_le32(buf, off) & 0x0fffffffu) == 0) {
				progress_mark("fat-free-cluster", 0x3cu,
					cluster);
				return cluster;
			}
			cluster++;
			if (cluster >= max_cluster || cluster >= 65536u)
				return 0;
		}
	}
	return 0;
}

static uint32_t find_free_fat32_cluster_ioctl(const char *dev,
	uint32_t first_fat, uint32_t max_cluster)
{
	unsigned char buf[512];
	uint32_t lba;
	uint32_t cluster;
	unsigned off;
	unsigned i;

	cluster = 2u;
	for (i = 0; i < 64u; i++) {
		lba = first_fat + (cluster * 4u) / 512u;
		if (read_sector_lba_ioctl(dev, lba, "fat-iscan-sec", buf) != 0)
			return 0;
		for (off = (cluster * 4u) & 511u; off < 512u; off += 4u) {
			if ((get_le32(buf, off) & 0x0fffffffu) == 0) {
				progress_mark("fat-ifree-cluster", 0x3bu,
					cluster);
				return cluster;
			}
			cluster++;
			if (cluster >= max_cluster || cluster >= 8192u)
				return 0;
		}
	}
	progress_mark("fat-iscan-limit", 0x3bu, cluster);
	return 0;
}

static int find_root_dir_slot(const char *dev, uint32_t root_lba,
	uint32_t root_sectors, uint32_t *slot_lba, unsigned *slot_off,
	uint32_t *slot_cluster)
{
	unsigned char buf[512];
	uint32_t i;
	unsigned off;

	for (i = 0; i < root_sectors && i < 128u; i++) {
		if (read_sector_lba(dev, root_lba + i, "fat-dir-scan", buf) != 0)
			return -1;
		for (off = 0; off < 512u; off += 32u) {
			if (memcmp(buf + off, RAW_TEST_NAME, 11) == 0) {
				progress_mark("fat-dir-existing", 0x3cu,
					root_lba + i);
				*slot_lba = root_lba + i;
				*slot_off = off;
				*slot_cluster =
					((get_le16(buf, off + 20u) << 16) |
					get_le16(buf, off + 26u));
				return 0;
			}
			if (buf[off] == 0x00 || buf[off] == 0xe5) {
				progress_mark("fat-dir-free", 0x3cu,
					root_lba + i);
				*slot_lba = root_lba + i;
				*slot_off = off;
				*slot_cluster = 0;
				return 0;
			}
		}
	}
	progress_mark("fat-dir-no-slot", 0x3cu, root_sectors);
	return -1;
}

static int raw_fat32_write_test(const char *dev, uint32_t first_fat,
	uint32_t fat_size, uint32_t first_data, uint32_t total_sectors,
	uint32_t root_lba, uint32_t root_sectors, unsigned spc)
{
	static const char msg[] =
		"sf2000 linux raw fat32 write test 0234\r\n";
	unsigned char buf[512];
	uint32_t max_cluster;
	uint32_t cluster;
	uint32_t data_lba;
	uint32_t slot_lba;
	uint32_t slot_cluster;
	unsigned slot_off;

	max_cluster = (total_sectors - first_data) / spc + 2u;
	progress_mark("fat-raw-max-cluster", 0x3cu, max_cluster);
	if (find_root_dir_slot(dev, root_lba, root_sectors,
			&slot_lba, &slot_off, &slot_cluster) != 0)
		return -1;
	if (slot_cluster >= 2u && slot_cluster < max_cluster) {
		cluster = slot_cluster;
		progress_mark("fat-raw-reuse-cluster", 0x3cu, cluster);
	} else {
		cluster = find_free_fat32_cluster(dev, first_fat, max_cluster);
		if (!cluster) {
			progress_mark("fat-raw-no-cluster", 0x3cu, 0);
			return -1;
		}
	}
	data_lba = first_data + (cluster - 2u) * spc;
	progress_mark("fat-raw-cluster", 0x3cu, cluster);
	progress_mark("fat-raw-data-lba", 0x3cu, data_lba);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, msg, sizeof(msg) - 1u);
	if (write_sector_lba(dev, data_lba, "fat-write-data", buf) != 0)
		return -1;
	if (write_fat32_entry(dev, first_fat, fat_size, cluster,
			0x0ffffff8u) != 0)
		return -1;
	if (read_sector_lba(dev, slot_lba, "fat-dir-read-slot", buf) != 0)
		return -1;
	memset(buf + slot_off, 0, 32);
	memcpy(buf + slot_off, RAW_TEST_NAME, 11);
	buf[slot_off + 11u] = 0x20;
	put_le16(buf, slot_off + 20u, (uint16_t)(cluster >> 16));
	put_le16(buf, slot_off + 26u, (uint16_t)cluster);
	put_le32(buf, slot_off + 28u, (uint32_t)(sizeof(msg) - 1u));
	if (write_sector_lba(dev, slot_lba, "fat-write-dir", buf) != 0)
		return -1;
	progress_mark("fat-raw-write-ok", 0x3cu, cluster);
	return 0;
}

static int raw_fat32_fixed_write_test(const char *dev, uint32_t first_fat,
	uint32_t fat_size, uint32_t first_data, uint32_t total_sectors,
	uint32_t root_lba, unsigned spc)
{
	static const char msg[] =
		"sf2000 linux fixed fat32 write test 0234\r\n";
	unsigned char buf[512];
	uint32_t max_cluster;
	uint32_t cluster;
	uint32_t data_lba;
	uint32_t slot_lba;
	unsigned slot_off = 0;

	max_cluster = (total_sectors - first_data) / spc + 2u;
	progress_mark("fat-fixed-max-cluster", 0x3cu, max_cluster);
	cluster = find_free_fat32_cluster(dev, first_fat, max_cluster);
	if (!cluster) {
		progress_mark("fat-fixed-no-cluster", 0x3cu, 0);
		return -1;
	}

	data_lba = first_data + (cluster - 2u) * spc;
	slot_lba = root_lba + 6u;
	progress_mark("fat-fixed-cluster", 0x3cu, cluster);
	progress_mark("fat-fixed-data-lba", 0x3cu, data_lba);
	progress_mark("fat-fixed-slot-lba", 0x3cu, slot_lba);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, msg, sizeof(msg) - 1u);
	if (write_sector_lba(dev, data_lba, "fat-fixed-data", buf) != 0)
		return -1;
	if (read_sector_lba(dev, data_lba, "fat-fixed-readback", buf) != 0)
		return -1;
	progress_mark("fat-fixed-rb-head", 0x3cu, get_le32(buf, 0));

	if (write_fat32_entry(dev, first_fat, fat_size, cluster,
			0x0ffffff8u) != 0)
		return -1;

	memset(buf, 0, sizeof(buf));
	memcpy(buf + slot_off, RAW_TEST_NAME, 11);
	buf[slot_off + 11u] = 0x20;
	put_le16(buf, slot_off + 20u, (uint16_t)(cluster >> 16));
	put_le16(buf, slot_off + 26u, (uint16_t)cluster);
	put_le32(buf, slot_off + 28u, (uint32_t)(sizeof(msg) - 1u));
	if (write_sector_lba(dev, slot_lba, "fat-fixed-dir", buf) != 0)
		return -1;
	if (read_sector_lba(dev, slot_lba, "fat-fixed-dir-rb", buf) != 0)
		return -1;
	progress_mark("fat-fixed-dir-head", 0x3cu, get_le32(buf, 0));

	storage_watchdog_arm("stor-wdt-raw-sync");
	progress_mark("fat-fixed-before-sync", 0x3cu, cluster);
	sync();
	progress_mark("fat-fixed-after-sync", 0x3cu, cluster);
	storage_watchdog_release("stor-wdt-raw-sync-done");
	progress_mark("fat-fixed-write-ok", 0x3cu, cluster);
	return 0;
}

static int raw_fat32_ioctl_fixed_write_test(const char *dev, uint32_t first_fat,
	uint32_t fat_size, uint32_t first_data, uint32_t total_sectors,
	uint32_t root_lba, unsigned spc)
{
	static const char msg[] =
		"sf2000 linux ioctl fat32 write test 0234\r\n";
	unsigned char buf[512];
	uint32_t cluster;
	uint32_t data_lba;
	uint32_t fat_off;
	uint32_t fat_lba;
	uint32_t fat_lba2;
	uint32_t slot_lba;
	unsigned fat_ent_off;
	unsigned slot_off = 32;

	cluster = find_free_fat32_cluster_ioctl(dev, first_fat,
		(total_sectors - first_data) / spc + 2u);
	if (!cluster) {
		progress_mark("fat-ioctl-no-cluster", 0x3bu, 0);
		return -1;
	}

	data_lba = first_data + (cluster - 2u) * spc;
	slot_lba = root_lba + 6u;
	progress_mark("fat-ioctl-cluster", 0x3bu, cluster);
	progress_mark("fat-ioctl-data-lba", 0x3bu, data_lba);
	progress_mark("fat-ioctl-slot-lba", 0x3bu, slot_lba);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, msg, sizeof(msg) - 1u);
	if (write_sector_lba_ioctl(dev, data_lba, "fat-ioctl-data", buf) != 0)
		return -1;
	if (read_sector_lba_ioctl(dev, data_lba, "fat-ioctl-rb", buf) == 0)
		progress_mark("fat-ioctl-rb-head", 0x3bu, get_le32(buf, 0));
	else
		progress_mark("fat-ioctl-rb-skip", 0x3bu, data_lba);

	fat_off = cluster * 4u;
	fat_lba = first_fat + fat_off / 512u;
	fat_lba2 = first_fat + fat_size + fat_off / 512u;
	fat_ent_off = fat_off & 511u;
	if (read_sector_lba_ioctl(dev, fat_lba, "fat-ioctl-fat-rd1", buf) != 0)
		return -1;
	put_le32(buf, fat_ent_off, 0x0ffffff8u);
	if (write_sector_lba_ioctl(dev, fat_lba, "fat-ioctl-fat1", buf) != 0)
		return -1;
	if (read_sector_lba_ioctl(dev, fat_lba2, "fat-ioctl-fat-rd2", buf) != 0)
		return -1;
	put_le32(buf, fat_ent_off, 0x0ffffff8u);
	if (write_sector_lba_ioctl(dev, fat_lba2, "fat-ioctl-fat2", buf) != 0)
		return -1;

	if (read_sector_lba_ioctl(dev, slot_lba, "fat-ioctl-dir-rd", buf) != 0)
		return -1;
	memset(buf + slot_off, 0, 32);
	memcpy(buf + slot_off, RAW_TEST_NAME, 11);
	buf[slot_off + 11u] = 0x20;
	put_le16(buf, slot_off + 20u, (uint16_t)(cluster >> 16));
	put_le16(buf, slot_off + 26u, (uint16_t)cluster);
	put_le32(buf, slot_off + 28u, (uint32_t)(sizeof(msg) - 1u));
	if (write_sector_lba_ioctl(dev, slot_lba, "fat-ioctl-dir", buf) != 0)
		return -1;
	progress_mark("fat-ioctl-write-ok", 0x3bu, cluster);
	return 0;
}

static int raw_fat32_ioctl_known_write_test(const char *dev, uint32_t first_fat,
	uint32_t fat_size, uint32_t first_data, uint32_t total_sectors,
	uint32_t root_lba, unsigned spc)
{
	static const char msg[] =
		"sf2000 linux known cluster ioctl fat32 write test 0234\r\n";
	unsigned char buf[512];
	uint32_t cluster = RAW_FIXED_CLUSTER;
	uint32_t data_lba;
	uint32_t fat_off;
	uint32_t fat_lba;
	uint32_t fat_lba2;
	uint32_t slot_lba;
	unsigned fat_ent_off;
	unsigned slot_off = RAW_FIXED_DIR_SLOT * 32u;

	progress_mark("fat-known-entry", 0x3bu, STORAGE_TAG);
	if (spc == 0 || cluster < 2u ||
	    cluster >= (total_sectors - first_data) / spc + 2u) {
		progress_mark("fat-known-bad-geom", 0x3bu, cluster);
		return -1;
	}

	data_lba = first_data + (cluster - 2u) * spc;
	slot_lba = root_lba + RAW_FIXED_DIR_SECTOR;
	progress_mark("fat-known-cluster", 0x3bu, cluster);
	progress_mark("fat-known-data-lba", 0x3bu, data_lba);
	progress_mark("fat-known-slot-lba", 0x3bu, slot_lba);
	progress_mark("fat-known-slot-off", 0x3bu, slot_off);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, msg, sizeof(msg) - 1u);
	if (write_sector_lba_ioctl(dev, data_lba, "fat-known-data", buf) != 0)
		return -1;
	if (read_sector_lba_ioctl(dev, data_lba, "fat-known-rb", buf) == 0)
		progress_mark("fat-known-rb-head", 0x3bu, get_le32(buf, 0));
	else
		progress_mark("fat-known-rb-skip", 0x3bu, data_lba);

	fat_off = cluster * 4u;
	fat_lba = first_fat + fat_off / 512u;
	fat_lba2 = first_fat + fat_size + fat_off / 512u;
	fat_ent_off = fat_off & 511u;
	progress_mark("fat-known-fat-lba", 0x3bu, fat_lba);
	progress_mark("fat-known-fat2-lba", 0x3bu, fat_lba2);
	if (read_sector_lba_ioctl(dev, fat_lba, "fat-known-fat-rd1", buf) != 0)
		return -1;
	progress_mark("fat-known-fat-old1", 0x3bu, get_le32(buf, fat_ent_off));
	put_le32(buf, fat_ent_off, 0x0ffffff8u);
	if (write_sector_lba_ioctl(dev, fat_lba, "fat-known-fat1", buf) != 0)
		return -1;

	if (read_sector_lba_ioctl(dev, fat_lba2, "fat-known-fat-rd2", buf) == 0) {
		progress_mark("fat-known-fat-old2", 0x3bu,
			get_le32(buf, fat_ent_off));
		put_le32(buf, fat_ent_off, 0x0ffffff8u);
		(void)write_sector_lba_ioctl(dev, fat_lba2,
			"fat-known-fat2", buf);
	} else {
		progress_mark("fat-known-fat2-skip", 0x3bu, fat_lba2);
	}

	if (read_sector_lba_ioctl(dev, slot_lba, "fat-known-dir-rd", buf) != 0)
		return -1;
	progress_mark("fat-known-dir-old", 0x3bu, get_le32(buf, slot_off));
	memset(buf + slot_off, 0, 32);
	memcpy(buf + slot_off, RAW_TEST_NAME, 11);
	buf[slot_off + 11u] = 0x20;
	put_le16(buf, slot_off + 20u, (uint16_t)(cluster >> 16));
	put_le16(buf, slot_off + 26u, (uint16_t)cluster);
	put_le32(buf, slot_off + 28u, (uint32_t)(sizeof(msg) - 1u));
	if (write_sector_lba_ioctl(dev, slot_lba, "fat-known-dir", buf) != 0)
		return -1;
	progress_mark("fat-known-write-ok", 0x3bu, cluster);
	return 0;
}

static void log_fat_geometry(const char *dev)
{
	unsigned char buf[512];
	uint32_t total_sectors;
	uint32_t fat_size;
	uint32_t root_dir_sectors;
	uint32_t first_fat;
	uint32_t first_data;
	uint32_t root_cluster;
	uint32_t root_lba;
	uint32_t root_sectors;
	uint32_t cluster;
	uint32_t next;
	unsigned bps;
	unsigned spc;
	unsigned reserved;
	unsigned fats;
	unsigned root_entries;
	unsigned fsinfo;
	unsigned i;

	if (read_sector_lba_ioctl(dev, 0, "fat-bpb-ioctl", buf) != 0)
		return;

	bps = get_le16(buf, 11);
	spc = buf[13];
	reserved = get_le16(buf, 14);
	fats = buf[16];
	root_entries = get_le16(buf, 17);
	total_sectors = get_le16(buf, 19);
	if (!total_sectors)
		total_sectors = get_le32(buf, 32);
	fat_size = get_le16(buf, 22);
	if (!fat_size)
		fat_size = get_le32(buf, 36);
	root_cluster = get_le32(buf, 44);
	fsinfo = get_le16(buf, 48);
	root_dir_sectors = ((uint32_t)root_entries * 32u + bps - 1u) / bps;
	first_fat = reserved;
	first_data = reserved + (uint32_t)fats * fat_size + root_dir_sectors;
	root_lba = first_data + (root_cluster - 2u) * spc;
	root_sectors = spc;

	progress_mark("fat-bps", 0x3cu, bps);
	progress_mark("fat-spc", 0x3cu, spc);
	progress_mark("fat-reserved", 0x3cu, reserved);
	progress_mark("fat-count", 0x3cu, fats);
	progress_mark("fat-size", 0x3cu, fat_size);
	progress_mark("fat-total", 0x3cu, total_sectors);
	progress_mark("fat-root-cluster", 0x3cu, root_cluster);
	progress_mark("fat-fsinfo", 0x3cu, fsinfo);
	progress_mark("fat-first-fat", 0x3cu, first_fat);
	progress_mark("fat-first-data", 0x3cu, first_data);
	progress_mark("fat-root-lba", 0x3cu, root_lba);
	progress_mark("fat-root-sectors", 0x3cu, root_sectors);

	(void)raw_fat32_ioctl_known_write_test(dev, first_fat, fat_size,
		first_data, total_sectors, root_lba, spc);
	if (fsinfo)
		(void)read_sector_lba_ioctl(dev, fsinfo, "fat-fsinfo-ioc", buf);
	(void)read_sector_lba_ioctl(dev, first_fat, "fat-first-fat-ioc", buf);
	(void)raw_fat32_ioctl_fixed_write_test(dev, first_fat, fat_size,
		first_data, total_sectors, root_lba, spc);
	(void)raw_fat32_fixed_write_test(dev, first_fat, fat_size, first_data,
		total_sectors, root_lba, spc);

	cluster = root_cluster;
	for (i = 0; i < 24u; i++) {
		progress_mark("fat-chain-cluster", 0x3cu, cluster);
		next = read_fat32_entry(dev, first_fat, cluster);
		progress_mark("fat-chain-next", 0x3cu, next);
		if (next >= 0x0ffffff8u || next < 2u)
			break;
		cluster = next;
	}

	(void)raw_fat32_write_test(dev, first_fat, fat_size, first_data,
		total_sectors, root_lba, root_sectors, spc);
}

static int try_mount_read_type(const char *dev, const char *fstype)
{
	int saved_errno;
	DIR *dir;

	progress_mark("stor-ro-try", 0x3du, hash_name(dev));
	progress_mark("stor-ro-type", 0x3du, hash_name(fstype));
	storage_watchdog_arm("stor-wdt-ro-mount");
	errno = 0;
	if (mount(dev, "/mnt/sd", fstype, MS_RDONLY | MS_NOATIME, "") != 0) {
		saved_errno = errno;
		storage_watchdog_release("stor-wdt-ro-fail");
		progress_mark("stor-ro-mount-fail", 0x3du,
			(uint32_t)saved_errno);
		return -1;
	}
	progress_mark("stor-ro-mount-ok", 0x3du, hash_name(dev));
	errno = 0;
	dir = opendir("/mnt/sd");
	if (dir) {
		progress_mark("stor-ro-opendir-ok", 0x3du, 0);
		closedir(dir);
	} else {
		progress_mark("stor-ro-opendir-fail", 0x3du, (uint32_t)errno);
	}
	errno = 0;
	if (umount("/mnt/sd") != 0) {
		saved_errno = errno;
		storage_watchdog_release("stor-wdt-ro-umount-fail");
		progress_mark("stor-ro-umount-fail", 0x3du,
			(uint32_t)saved_errno);
		return -1;
	}
	progress_mark("stor-ro-umount-ok", 0x3du, 0);
	storage_watchdog_release("stor-wdt-ro-ok");
	return 0;
}

static int try_mount_write_type(const char *dev, const char *fstype)
{
	ssize_t wrote = -1;
	int saved_errno = 0;
	int fd;

	progress_mark("stor-try", 0x3du, hash_name(dev));
	progress_mark("stor-mount-type", 0x3du, hash_name(fstype));
	progress_mark("stor-before-mount-log", 0x3du, hash_name(dev));
	log_msgf("sf2000_storage_probe: mount try %s type=%s\n", dev, fstype);
	progress_mark("stor-before-mount-call", 0x3du, hash_name(dev));
	storage_watchdog_arm("stor-wdt-mount");
	errno = 0;
	if (mount(dev, "/mnt/sd", fstype, MS_NOATIME, "") != 0) {
		saved_errno = errno;
		storage_watchdog_release("stor-wdt-mount-fail");
		progress_mark("stor-mount-fail", 0x3du, (uint32_t)saved_errno);
		log_msgf("sf2000_storage_probe: mount failed %s type=%s errno=%d\n",
			dev, fstype, saved_errno);
		return -1;
	}
	storage_watchdog_release("stor-wdt-mount-ok");
	progress_mark("stor-mount-ok", 0x3du, hash_name(dev));
	log_msgf("sf2000_storage_probe: mount ok %s type=%s\n", dev, fstype);
	fd = open("/mnt/sd/sf2000-linux-rw-0234.txt",
		O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		progress_mark("stor-open-fail", 0x3du, (uint32_t)errno);
		log_msgf("sf2000_storage_probe: write open failed errno=%d\n",
			errno);
	} else {
		const char msg[] = "sf2000 linux sd write test 0234\n";

		errno = 0;
		wrote = write(fd, msg, sizeof(msg) - 1u);
		saved_errno = errno;
		progress_mark("stor-before-fsync", 0x3du, (uint32_t)wrote);
		if (wrote == (ssize_t)(sizeof(msg) - 1u)) {
			errno = 0;
			storage_watchdog_arm("stor-wdt-fsync");
			progress_mark("stor-fsync-ret", 0x3du,
				(uint32_t)fsync(fd));
			progress_mark("stor-fsync-err", 0x3du, (uint32_t)errno);
			storage_watchdog_release("stor-wdt-fsync-done");
		}
		close(fd);
		progress_mark("stor-write-ret", 0x3du, (uint32_t)wrote);
		progress_mark("stor-write-errno", 0x3du, (uint32_t)saved_errno);
		log_msgf("sf2000_storage_probe: write ret=%d errno=%d\n",
			(int)wrote, saved_errno);
	}
	progress_mark("stor-before-sync", 0x3du, (uint32_t)wrote);
	storage_watchdog_arm("stor-wdt-sync");
	sync();
	storage_watchdog_release("stor-wdt-sync-done");
	progress_mark("stor-after-sync", 0x3du, (uint32_t)wrote);
	storage_watchdog_arm("stor-wdt-umount");
	if (umount("/mnt/sd") != 0) {
		saved_errno = errno;
		storage_watchdog_release("stor-wdt-umount-fail");
		progress_mark("stor-umount-fail", 0x3du,
			(uint32_t)saved_errno);
		log_msgf("sf2000_storage_probe: umount failed errno=%d\n",
			saved_errno);
	} else {
		storage_watchdog_release("stor-wdt-umount-ok");
		progress_mark("stor-umount-ok", 0x3du, 0);
		log_msgf("sf2000_storage_probe: umount ok\n");
	}
	return wrote > 0 ? 0 : -1;
}

static int try_mount_write(const char *dev)
{
	(void)try_mount_read_type(dev, "vfat");
	if (try_mount_write_type(dev, "vfat") == 0)
		return 0;
	(void)try_mount_read_type(dev, "msdos");
	if (try_mount_write_type(dev, "msdos") == 0)
		return 0;
	return -1;
}

int main(void)
{
	int mounted = -1;

	progress_reset("stor-ring-reset");
	progress_mark("stor-start", 0x3au, STORAGE_TAG);
	log_line("sf2000_storage_probe: start\n");
	ensure_mounts();
	ensure_block_nodes();
	stat_node("/dev/mmcblk0");
	stat_node("/dev/mmcblk0p1");
	stat_node("/dev/mmcblk0p2");
	stat_node("/dev/mmcblk0sys");
	stat_node("/dev/mmcblk0class");
	stat_node("/dev/mmcblk0p1sys");
	stat_node("/dev/mmcblk0p2sys");
	set_readahead_zero("/dev/mmcblk0");
	set_readahead_zero("/dev/mmcblk0p1");
	set_readahead_zero("/dev/mmcblk0p2");
	set_readahead_zero("/dev/mmcblk0sys");
	set_readahead_zero("/dev/mmcblk0class");
	log_fat_geometry("/dev/mmcblk0");
	progress_mark("stor-before-mounts", 0x3au, STORAGE_TAG);
	if (try_mount_write("/dev/mmcblk0") == 0 ||
	    try_mount_write("/dev/mmcblk0p1") == 0 ||
	    try_mount_write("/dev/mmcblk0p2") == 0 ||
	    try_mount_write("/dev/mmcblk0sys") == 0 ||
	    try_mount_write("/dev/mmcblk0class") == 0)
		mounted = 0;
	progress_mark("stor-fast-result", 0x3au, (uint32_t)mounted);
	progress_mark("stor-fast-done", 0x3au, STORAGE_TAG);
	if (mounted == 0) {
		log_line("sf2000_storage_probe: fast storage path ok\n");
		progress_mark("stor-done", 0x3au, STORAGE_TAG);
		return 0;
	}
	log_block_head("/dev/mmcblk0");
	log_block_head("/dev/mmcblk0p1");
	log_block_head("/dev/mmcblk0p2");
	log_block_head("/dev/mmcblk0sys");
	log_block_head("/dev/mmcblk0class");
	log_file_head("sys-mmc0-dev", "/sys/block/mmcblk0/dev");
	log_file_head("sys-mmc0-size", "/sys/block/mmcblk0/size");
	log_file_head("cls-mmc0-dev", "/sys/class/block/mmcblk0/dev");
	log_file_head("cls-mmc0p1-dev", "/sys/class/block/mmcblk0p1/dev");
	log_file_head("cls-mmc0p2-dev", "/sys/class/block/mmcblk0p2/dev");
	log_dir("sys-block", "/sys/block");
	log_dir("class-block", "/sys/class/block");
	log_dir("dev-block", "/sys/dev/block");
	log_dir("mmc-host", "/sys/class/mmc_host");
	log_dir("platform-devices", "/sys/bus/platform/devices");
	log_dir("platform-drivers", "/sys/bus/platform/drivers");
	log_dir("dev", "/dev");
	log_file_head("proc-partitions", "/proc/partitions");
	log_file_head("proc-devices", "/proc/devices");
	log_file_head("proc-interrupts", "/proc/interrupts");
	log_line("sf2000_storage_probe: done\n");
	progress_mark("stor-done", 0x3au, STORAGE_TAG);
	return 0;
}
