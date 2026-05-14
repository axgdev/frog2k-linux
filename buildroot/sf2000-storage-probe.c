#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
	(void)mount("sysfs", "/sys", "sysfs", 0, "");
	(void)mount("devtmpfs", "/dev", "devtmpfs", 0, "");
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
		return;
	}
	while ((de = readdir(dir)) != NULL && count < 24) {
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		log_msgf("sf2000_storage_probe: %s %s\n", label, de->d_name);
		count++;
	}
	closedir(dir);
	if (count == 0)
		log_msgf("sf2000_storage_probe: %s empty\n", label);
}

static void log_file_head(const char *label, const char *path)
{
	char buf[256];
	ssize_t got;
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0) {
		log_msgf("sf2000_storage_probe: %s open failed errno=%d\n",
			label, errno);
		return;
	}
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got < 0) {
		log_msgf("sf2000_storage_probe: %s read failed errno=%d\n",
			label, errno);
		return;
	}
	buf[got] = 0;
	log_msgf("sf2000_storage_probe: %s %.180s\n", label, buf);
}

static int try_mount_write(const char *dev)
{
	int fd;

	if (access(dev, R_OK) != 0)
		return -1;
	log_msgf("sf2000_storage_probe: mount try %s\n", dev);
	if (mount(dev, "/mnt/sd", "vfat", MS_SYNCHRONOUS, "") != 0) {
		log_msgf("sf2000_storage_probe: mount failed %s errno=%d\n",
			dev, errno);
		return -1;
	}
	log_msgf("sf2000_storage_probe: mount ok %s\n", dev);
	fd = open("/mnt/sd/sf2000-linux-rw-0164.txt",
		O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		log_msgf("sf2000_storage_probe: write open failed errno=%d\n",
			errno);
	} else {
		const char msg[] = "sf2000 linux sd write test 0164\n";
		ssize_t wrote = write(fd, msg, sizeof(msg) - 1u);

		close(fd);
		log_msgf("sf2000_storage_probe: write ret=%d errno=%d\n",
			(int)wrote, errno);
	}
	sync();
	if (umount("/mnt/sd") != 0)
		log_msgf("sf2000_storage_probe: umount failed errno=%d\n", errno);
	else
		log_msgf("sf2000_storage_probe: umount ok\n");
	return 0;
}

int main(void)
{
	log_line("sf2000_storage_probe: start\n");
	ensure_mounts();
	log_dir("sys-block", "/sys/block");
	log_dir("mmc-host", "/sys/class/mmc_host");
	log_dir("dev", "/dev");
	log_file_head("proc-partitions", "/proc/partitions");
	if (try_mount_write("/dev/mmcblk0p1") != 0)
		(void)try_mount_write("/dev/mmcblk0");
	log_line("sf2000_storage_probe: done\n");
	return 0;
}
