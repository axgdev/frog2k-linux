// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MOUNT_POINT "/mnt/sd"
#define MOUNT_MARKER "/run/sf2000-storage-mounted"
#define STATUS_PATH "/run/sf2000-storage.status"
#define ATTEMPTS 30u

static const char *const devices[] = {
	"/dev/mmcblk0p1", "/dev/mmcblk0p2", "/dev/mmcblk0"
};

static void sleep_ms(unsigned msec)
{
	struct timespec delay = {
		.tv_sec = msec / 1000u,
		.tv_nsec = (long)(msec % 1000u) * 1000000L,
	};

	while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
		;
}

static void write_text(const char *path, const char *text)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	if (fd < 0)
		return;
	(void)write(fd, text, strlen(text));
	close(fd);
}

static void log_status(const char *message)
{
	char line[192];
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	int length;

	length = snprintf(line, sizeof(line), "<6>sf2000-mount: %s\n", message);
	if (fd >= 0) {
		if (length > 0) {
			size_t bytes = (size_t)length;

			if (bytes >= sizeof(line))
				bytes = sizeof(line) - 1u;
			(void)write(fd, line, bytes);
		}
		close(fd);
	}
	write_text(STATUS_PATH, message);
}

int main(void)
{
	unsigned attempt;

	(void)mkdir(MOUNT_POINT, 0755);
	log_status("mount service start");
	for (attempt = 0; attempt < ATTEMPTS; attempt++) {
		unsigned i;

		for (i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
			struct stat st;
			char line[128];

			if (stat(devices[i], &st) != 0 || !S_ISBLK(st.st_mode))
				continue;
			if (mount(devices[i], MOUNT_POINT, "vfat",
					MS_NOATIME, NULL) != 0)
				continue;
			snprintf(line, sizeof(line), "%s\n", devices[i]);
			write_text(MOUNT_MARKER, line);
			snprintf(line, sizeof(line), "mount ok %s at %s",
				devices[i], MOUNT_POINT);
			log_status(line);
			return 0;
		}
		sleep_ms(1000);
	}
	log_status("mount failed after 30 attempts");
	return 1;
}
