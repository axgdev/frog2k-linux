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
#define STORAGE_TAG 0x0223u
#define WDT_BASE_PHYS 0x18818000u
#define WDT_REG_OFF 0x500u
#define WDT_COUNT_OFF 0x00u
#define WDT_CONF_OFF 0x04u
#define WDT_STORAGE_COUNT 0xffe64035u
#define WDT_STORAGE_CONF 0x26u
#define STORAGE_WDT_CLAIM "/run/sf2000-storage-watchdog-owned"

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
	uint32_t cluster;
	uint32_t next;
	unsigned bps;
	unsigned spc;
	unsigned reserved;
	unsigned fats;
	unsigned root_entries;
	unsigned fsinfo;
	unsigned i;

	if (read_sector_lba(dev, 0, "fat-bpb", buf) != 0)
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

	if (fsinfo)
		(void)read_sector_lba(dev, fsinfo, "fat-fsinfo-sec", buf);
	(void)read_sector_lba(dev, first_fat, "fat-first-fat-sec", buf);

	cluster = root_cluster;
	for (i = 0; i < 24u; i++) {
		progress_mark("fat-chain-cluster", 0x3cu, cluster);
		next = read_fat32_entry(dev, first_fat, cluster);
		progress_mark("fat-chain-next", 0x3cu, next);
		if (next >= 0x0ffffff8u || next < 2u)
			break;
		cluster = next;
	}

	for (i = 0; i < 8u; i++)
		(void)read_sector_lba(dev, root_lba + i, "fat-root-sec", buf);
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
	fd = open("/mnt/sd/sf2000-linux-rw-0223.txt",
		O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		progress_mark("stor-open-fail", 0x3du, (uint32_t)errno);
		log_msgf("sf2000_storage_probe: write open failed errno=%d\n",
			errno);
	} else {
		const char msg[] = "sf2000 linux sd write test 0223\n";

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
	log_block_head("/dev/mmcblk0");
	log_block_head("/dev/mmcblk0sys");
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
