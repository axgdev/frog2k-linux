// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#define MOUNT_MARKER "/run/sf2000-storage-mounted"
#define REBOOT_MARKER "/run/sf2000-reboot-request"
#define LOG_PATH "/mnt/sd/loglinux.txt"
#define TEST_TMP_PATH "/mnt/sd/.sf2000-storage-test.tmp"
#define TEST_PATH "/mnt/sd/sf2000-storage-test.bin"
#define LOG_BUFFER_SIZE (512u * 1024u)
#define LOG_FLUSH_BYTES 8192u
#define LOG_FLUSH_MS 2000u
#define STORAGE_TEST_BYTES (256u * 1024u)
#define INPUT_FDS 16u
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static char log_buffer[LOG_BUFFER_SIZE];
static size_t log_used;
static int log_fd = -1;
static long ticks_per_second = 100;

static int stop_requested(void)
{
	return access(REBOOT_MARKER, F_OK) == 0;
}

static uint64_t monotonic_us(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 0;
	return (uint64_t)now.tv_sec * 1000000u +
		(uint64_t)now.tv_nsec / 1000u;
}

static void sleep_ms(unsigned msec)
{
	struct timespec delay;

	delay.tv_sec = msec / 1000u;
	delay.tv_nsec = (long)(msec % 1000u) * 1000000L;
	while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
		;
}

static int write_all(int fd, const void *data, size_t size)
{
	const char *bytes = data;

	while (size) {
		ssize_t written = write(fd, bytes, size);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (!written)
			return -1;
		bytes += written;
		size -= (size_t)written;
	}
	return 0;
}

static int flush_log(void)
{
	if (log_fd < 0 || !log_used)
		return 0;
	if (write_all(log_fd, log_buffer, log_used) != 0)
		return -1;
	log_used = 0;
	return fsync(log_fd);
}

static void append_bytes(const char *text, size_t length)
{
	if (!length)
		return;
	if (length > sizeof(log_buffer)) {
		if (log_fd >= 0) {
			(void)flush_log();
			(void)write_all(log_fd, text, length);
		}
		return;
	}
	if (log_used + length > sizeof(log_buffer)) {
		if (log_fd >= 0)
			(void)flush_log();
		else {
			/* Keep the newest complete pre-mount profiling window. */
			size_t discard = log_used + length - sizeof(log_buffer);

			memmove(log_buffer, log_buffer + discard, log_used - discard);
			log_used -= discard;
		}
	}
	memcpy(log_buffer + log_used, text, length);
	log_used += length;
	if (log_used >= LOG_FLUSH_BYTES)
		(void)flush_log();
}

static void append_record(const char *source, const char *payload,
	size_t payload_length)
{
	struct tms cpu;
	clock_t process_ticks = times(&cpu);
	uint64_t usec = monotonic_us();
	uint64_t tick = usec * (uint64_t)ticks_per_second / 1000000u;
	char prefix[192];
	int prefix_length;

	prefix_length = snprintf(prefix, sizeof(prefix),
		"tick=%llu mono_us=%llu proc=%ld user=%ld sys=%ld source=%s ",
		(unsigned long long)tick, (unsigned long long)usec,
		(long)process_ticks, (long)cpu.tms_utime, (long)cpu.tms_stime,
		source);
	if (prefix_length > 0)
		append_bytes(prefix, (size_t)prefix_length);
	append_bytes(payload, payload_length);
	if (!payload_length || payload[payload_length - 1u] != '\n')
		append_bytes("\n", 1);
}

static void kmsg_line(const char *message)
{
	static const char prefix[] = "<6>sf2000-logd: ";
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd < 0)
		return;
	(void)write_all(fd, prefix, sizeof(prefix) - 1u);
	(void)write_all(fd, message, strlen(message));
	close(fd);
}

static uint32_t pattern_word(unsigned offset)
{
	uint32_t x = 0x2390f00du ^ offset;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return x;
}

static uint32_t hash_bytes(uint32_t hash, const uint8_t *data, size_t size)
{
	while (size--) {
		hash ^= *data++;
		hash *= 16777619u;
	}
	return hash;
}

static int storage_integrity_test(char *result, size_t result_size)
{
	uint32_t words[1024];
	uint32_t write_hash = 2166136261u;
	uint32_t read_hash = 2166136261u;
	unsigned offset = 0;
	int fd;

	fd = open(TEST_TMP_PATH, O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0644);
	if (fd < 0)
		goto fail;
	for (offset = 0; offset < STORAGE_TEST_BYTES; offset += sizeof(words)) {
		unsigned i;

		for (i = 0; i < ARRAY_SIZE(words); i++)
			words[i] = pattern_word(offset + i * sizeof(words[i]));
		write_hash = hash_bytes(write_hash, (const uint8_t *)words,
			sizeof(words));
		if (write_all(fd, words, sizeof(words)) != 0)
			goto fail_close;
	}
	if (fsync(fd) != 0)
		goto fail_close;
	if (close(fd) != 0) {
		fd = -1;
		goto fail_unlink;
	}
	fd = open(TEST_TMP_PATH, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		goto fail_unlink;
	for (offset = 0; offset < STORAGE_TEST_BYTES; offset += sizeof(words)) {
		size_t received = 0;

		while (received < sizeof(words)) {
			ssize_t got = read(fd, (uint8_t *)words + received,
				sizeof(words) - received);

			if (got < 0 && errno == EINTR)
				continue;
			if (got <= 0)
				goto fail_close;
			received += (size_t)got;
		}
		read_hash = hash_bytes(read_hash, (const uint8_t *)words,
			sizeof(words));
	}
	close(fd);
	if (write_hash != read_hash) {
		errno = EIO;
		goto fail_unlink;
	}
	if (rename(TEST_TMP_PATH, TEST_PATH) != 0)
		goto fail_unlink;
	/* Persist the FAT directory entry as well as the already-fsynced data. */
	sync();
	snprintf(result, result_size,
		"storage-test=pass bytes=%u hash=%08x path=%s",
		STORAGE_TEST_BYTES, write_hash, TEST_PATH);
	return 0;

fail_close:
	{
		int saved_errno = errno;

		close(fd);
		errno = saved_errno;
	}
fail_unlink:
	(void)unlink(TEST_TMP_PATH);
fail:
	snprintf(result, result_size,
		"storage-test=fail errno=%d offset=%u write_hash=%08x read_hash=%08x",
		errno, offset, write_hash, read_hash);
	return -1;
}

static void append_file_text(char *snapshot, size_t snapshot_size,
	size_t *used, const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t got;

	if (fd < 0 || *used + 2u >= snapshot_size) {
		if (fd >= 0)
			close(fd);
		return;
	}
	got = read(fd, snapshot + *used, snapshot_size - *used - 2u);
	close(fd);
	if (got <= 0)
		return;
	*used += (size_t)got;
	while (*used && (snapshot[*used - 1u] == '\n' ||
			snapshot[*used - 1u] == '\r'))
		(*used)--;
}

static void append_profile_file(const char *source, const char *path)
{
	char data[4096];
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	ssize_t got;

	if (fd < 0)
		return;
	while ((got = read(fd, data, sizeof(data))) > 0)
		append_record(source, data, (size_t)got);
	close(fd);
}

static void append_system_profile(void)
{
	append_profile_file("proc-stat", "/proc/stat");
	append_profile_file("proc-meminfo", "/proc/meminfo");
	append_profile_file("proc-interrupts", "/proc/interrupts");
	append_profile_file("proc-uptime", "/proc/uptime");
	append_profile_file("proc-loadavg", "/proc/loadavg");
	append_profile_file("proc-vmstat", "/proc/vmstat");
}

static void topology_snapshot(char *snapshot, size_t snapshot_size)
{
	static const char *const roots[] = {
		"/sys/bus/usb/devices", "/sys/class/input"
	};
	size_t used = 0;
	unsigned root;

	for (root = 0; root < ARRAY_SIZE(roots); root++) {
		DIR *directory = opendir(roots[root]);
		struct dirent *entry;

		if (!directory)
			continue;
		while ((entry = readdir(directory)) != NULL) {
			char path[256];
			int length;

			if (entry->d_name[0] == '.')
				continue;
			length = snprintf(snapshot + used, snapshot_size - used,
				"%s%s", used ? "," : "", entry->d_name);
			if (length < 0 || (size_t)length >= snapshot_size - used)
				break;
			used += (size_t)length;
			if (root != 1 || strncmp(entry->d_name, "event", 5))
				continue;
			snprintf(path, sizeof(path), "%s/%s/device/name", roots[root],
				entry->d_name);
			if (used + 1u < snapshot_size)
				snapshot[used++] = '=';
			append_file_text(snapshot, snapshot_size, &used, path);
		}
		closedir(directory);
		if (root + 1u < ARRAY_SIZE(roots) && used + 3u < snapshot_size) {
			memcpy(snapshot + used, " | ", 3);
			used += 3;
		}
	}
	snapshot[used < snapshot_size ? used : snapshot_size - 1u] = 0;
}

static void reopen_input_fds(int fds[INPUT_FDS])
{
	unsigned i;

	for (i = 0; i < INPUT_FDS; i++) {
		char path[32];

		if (fds[i] >= 0)
			continue;
		snprintf(path, sizeof(path), "/dev/input/event%u", i);
		fds[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
}

static void drain_input_fds(int fds[INPUT_FDS])
{
	unsigned i;

	for (i = 0; i < INPUT_FDS; i++) {
		struct input_event event;
		ssize_t got;

		if (fds[i] < 0)
			continue;
		while ((got = read(fds[i], &event, sizeof(event))) == sizeof(event)) {
			char line[128];
			int length = snprintf(line, sizeof(line),
				"event=%u type=%u code=%u value=%d",
				i, event.type, event.code, event.value);

			if (length > 0)
				append_record("input", line, (size_t)length);
		}
		if (got < 0 && errno != EAGAIN && errno != EINTR) {
			close(fds[i]);
			fds[i] = -1;
		}
	}
}

int main(void)
{
	char kmsg[2048];
	char topology[2048] = "";
	char previous_topology[2048] = "";
	char result[256];
	uint64_t last_flush;
	uint64_t last_probe;
	int input_fds[INPUT_FDS];
	int kmsg_fd;
	unsigned i;

	ticks_per_second = sysconf(_SC_CLK_TCK);
	if (ticks_per_second <= 0)
		ticks_per_second = 100;
	for (i = 0; i < INPUT_FDS; i++)
		input_fds[i] = -1;
	kmsg_fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	append_record("logd", "--- SF2000 Linux pre-mount profile begin ---",
		strlen("--- SF2000 Linux pre-mount profile begin ---"));
	while (access(MOUNT_MARKER, R_OK) != 0) {
		ssize_t got;

		if (kmsg_fd >= 0)
			while ((got = read(kmsg_fd, kmsg, sizeof(kmsg))) > 0)
				append_record("kmsg", kmsg, (size_t)got);
		sleep_ms(50);
	}
	log_fd = open(LOG_PATH, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
	if (log_fd < 0) {
		kmsg_line("cannot open /mnt/sd/loglinux.txt\n");
		return 1;
	}
	append_record("logd", "--- SF2000 Linux storage mounted ---",
		strlen("--- SF2000 Linux storage mounted ---"));
	(void)storage_integrity_test(result, sizeof(result));
	append_record("storage", result, strlen(result));
	(void)flush_log();
	kmsg_line(result);
	kmsg_line("\n");

	last_flush = last_probe = monotonic_us();

	while (!stop_requested()) {
		ssize_t got;
		uint64_t now;

		if (kmsg_fd >= 0) {
			while ((got = read(kmsg_fd, kmsg, sizeof(kmsg))) > 0)
				append_record("kmsg", kmsg, (size_t)got);
		}
		drain_input_fds(input_fds);
		now = monotonic_us();
		if (now - last_probe >= LOG_FLUSH_MS * 1000u) {
			append_system_profile();
			topology_snapshot(topology, sizeof(topology));
			if (strcmp(topology, previous_topology)) {
				append_record("topology", topology, strlen(topology));
				strncpy(previous_topology, topology,
					sizeof(previous_topology) - 1u);
				previous_topology[sizeof(previous_topology) - 1u] = 0;
			}
			reopen_input_fds(input_fds);
			append_record("heartbeat", "alive", 5);
			last_probe = now;
		}
		if (log_used >= LOG_FLUSH_BYTES ||
				now - last_flush >= LOG_FLUSH_MS * 1000u) {
			if (flush_log() != 0)
				kmsg_line("persistent log flush failed\n");
			last_flush = now;
		}
		sleep_ms(50);
	}

	append_record("logd", "--- SF2000 Linux logger shutdown ---",
		strlen("--- SF2000 Linux logger shutdown ---"));
	(void)flush_log();
	for (i = 0; i < INPUT_FDS; i++)
		if (input_fds[i] >= 0)
			close(input_fds[i]);
	if (kmsg_fd >= 0)
		close(kmsg_fd);
	if (log_fd >= 0)
		close(log_fd);
	sync();
	return 0;
}
