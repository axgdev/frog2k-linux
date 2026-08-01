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
#define GEN_PATH "/run/sf2000-storage-generation"
#define REBOOT_MARKER "/run/sf2000-reboot-request"
#define SHUTDOWN_MARKER "/run/sf2000-shutdown-request"
#define STANDBY_MARKER "/run/sf2000-display-standby"
#define PERFORMANCE_MARKER "/run/sf2000-performance-active"
#define PERFORMANCE_READY_MARKER "/run/sf2000-performance-ready"
#define PERFORMANCE_METRICS_PATH "/run/sf2000-frontend-metrics"
#define LOG_FLUSH_REQUEST "/run/sf2000-log-flush-request"
#define LOG_FLUSH_DONE "/run/sf2000-log-flush-done"
#define LOG_PATH "/mnt/sd/loglinux.txt"
#define LOG_BUFFER_SIZE (512u * 1024u)
#define LOG_FLUSH_BYTES (128u * 1024u)
#define LOG_FLUSH_MS 2000u
#define FRONTEND_PROBE_MS 60000u
#define INPUT_FDS 16u
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static char log_buffer[LOG_BUFFER_SIZE];
static size_t log_used;
static size_t log_peak;
static uint64_t log_dropped;
static int log_fd = -1;
static int storage_deferred;
static unsigned performance_metric_records;
static size_t performance_metric_bytes;
static long ticks_per_second = 100;
static unsigned mount_generation;
static unsigned log_chord_keys;
static unsigned log_chord_latched;

static int stop_requested(void)
{
	return access(REBOOT_MARKER, F_OK) == 0 ||
		access(SHUTDOWN_MARKER, F_OK) == 0;
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
	if (storage_deferred || log_fd < 0 || !log_used)
		return 0;
	if (write_all(log_fd, log_buffer, log_used) != 0)
		return -1;
	log_used = 0;
	return 0;
}

static int sync_log(void)
{
	if (storage_deferred)
		return 0;
	if (flush_log() != 0)
		return -1;
	return log_fd < 0 ? 0 : fsync(log_fd);
}

/* A manual checkpoint is intentionally allowed during a performance session.
 * Normal playback keeps FAT writes deferred, but START+RIGHT and the browser
 * pre-launch boundary explicitly trade a small one-shot I/O cost for a log
 * that survives a core which never reaches clean shutdown. */
static int force_flush_log(void)
{
	if (log_fd < 0)
		return -1;
	if (log_used) {
		if (write_all(log_fd, log_buffer, log_used) != 0)
			return -1;
		log_used = 0;
	}
	return fsync(log_fd);
}

static void append_record(const char *source, const char *payload,
	size_t payload_length);

static int checkpoint_log(const char *reason)
{
	char message[160];

	if (!reason || !reason[0])
		reason = "unspecified";
	snprintf(message, sizeof(message), "log flush checkpoint reason=%s",
		reason);
	append_record("logd", message, strlen(message));
	return force_flush_log();
}

static int performance_active(void)
{
	return access(PERFORMANCE_MARKER, F_OK) == 0;
}

static void set_performance_ready(int ready)
{
	if (ready) {
		int fd = open(PERFORMANCE_READY_MARKER,
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

		if (fd >= 0)
			close(fd);
	} else {
		(void)unlink(PERFORMANCE_READY_MARKER);
	}
}

static void append_bytes(const char *text, size_t length)
{
	if (!length)
		return;
	if (length > sizeof(log_buffer)) {
		if (!storage_deferred && log_fd >= 0) {
			(void)flush_log();
			(void)write_all(log_fd, text, length);
		} else {
			log_dropped += length - sizeof(log_buffer);
			memcpy(log_buffer, text + length - sizeof(log_buffer),
				sizeof(log_buffer));
			log_used = sizeof(log_buffer);
			log_peak = log_used;
		}
		return;
	}
	if (log_used + length > sizeof(log_buffer)) {
		if (!storage_deferred && log_fd >= 0)
			(void)flush_log();
		if (log_used + length > sizeof(log_buffer)) {
			/* Keep the newest bounded RAM-journal window. */
			size_t discard = log_used + length - sizeof(log_buffer);

			memmove(log_buffer, log_buffer + discard, log_used - discard);
			log_used -= discard;
			log_dropped += discard;
		}
	}
	memcpy(log_buffer + log_used, text, length);
	log_used += length;
	if (log_used > log_peak)
		log_peak = log_used;
	if (!storage_deferred && log_used >= LOG_FLUSH_BYTES)
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
	char line[256];
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	int length;

	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line), "<6>sf2000-logd: %s", message);
	if (length > 0) {
		size_t bytes = (size_t)length;

		if (bytes >= sizeof(line))
			bytes = sizeof(line) - 1u;
		(void)write_all(fd, line, bytes);
	}
	close(fd);
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

static void import_performance_metrics(void)
{
	char input[512];
	char line[512];
	size_t line_used = 0;
	int fd = open(PERFORMANCE_METRICS_PATH, O_RDONLY | O_CLOEXEC);
	ssize_t got;

	if (fd < 0)
		return;
	while ((got = read(fd, input, sizeof(input))) > 0) {
		ssize_t i;

		performance_metric_bytes += (size_t)got;
		for (i = 0; i < got; i++) {
			if (input[i] == '\n') {
				append_record("frontend-metric", line, line_used);
				performance_metric_records++;
				line_used = 0;
			} else if (line_used < sizeof(line) - 1u) {
				line[line_used++] = input[i];
			}
		}
	}
	if (line_used) {
		append_record("frontend-metric", line, line_used);
		performance_metric_records++;
	}
	close(fd);
	(void)unlink(PERFORMANCE_METRICS_PATH);
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
			if (event.type == EV_KEY &&
					(event.code == BTN_START ||
						event.code == BTN_DPAD_RIGHT)) {
				unsigned bit = event.code == BTN_START ? 1u : 2u;

				if (event.value)
					log_chord_keys |= bit;
				else {
					log_chord_keys &= ~bit;
					log_chord_latched = 0;
				}
				if (event.value && log_chord_keys == 3u &&
						!log_chord_latched) {
					log_chord_latched = 1;
					if (checkpoint_log("START+RIGHT") != 0)
						kmsg_line("START+RIGHT log flush failed\n");
					else
						kmsg_line("START+RIGHT log flush complete\n");
				}
			}
		}
		if (got < 0 && errno != EAGAIN && errno != EINTR) {
			close(fds[i]);
			fds[i] = -1;
		}
	}
}

static void service_flush_request(void)
{
	char reason[96];
	ssize_t got;
	int request;
	int done;
	int status = 0;

	request = open(LOG_FLUSH_REQUEST, O_RDONLY | O_CLOEXEC);
	if (request < 0)
		return;
	got = read(request, reason, sizeof(reason) - 1u);
	close(request);
	(void)unlink(LOG_FLUSH_REQUEST);
	if (got < 0)
		got = 0;
	reason[got] = 0;
	if (!got)
		strcpy(reason, "unspecified");
	if (checkpoint_log(reason) != 0)
		status = -1;
	done = open(LOG_FLUSH_DONE, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		0644);
	if (done >= 0) {
		const char *result = status ? "error\n" : "ok\n";

		(void)write_all(done, result, strlen(result));
		(void)fsync(done);
		close(done);
	}
	if (status)
		kmsg_line("log flush checkpoint failed\n");
	else
		kmsg_line("log flush checkpoint complete\n");
}

static unsigned read_mount_generation(void)
{
	char buf[32];
	ssize_t got;
	unsigned value = 0;
	unsigned i = 0;
	int fd;

	fd = open(GEN_PATH, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got <= 0)
		return 0;
	buf[got] = 0;
	while (buf[i] >= '0' && buf[i] <= '9') {
		value = value * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	return value;
}

static int open_log_file(void)
{
	if (log_fd >= 0) {
		close(log_fd);
		log_fd = -1;
	}
	log_fd = open(LOG_PATH, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
	return log_fd >= 0 ? 0 : -1;
}

/*
 * Hotplug: when the card is removed the mount marker disappears; when a new
 * card is mounted the generation increments.  Drain or drop the open log FD
 * and reopen so writes go to the live volume.
 */
static void refresh_storage_binding(void)
{
	unsigned gen = read_mount_generation();
	int marker = access(MOUNT_MARKER, R_OK) == 0;

	if (!marker) {
		if (log_fd >= 0) {
			(void)flush_log();
			close(log_fd);
			log_fd = -1;
			kmsg_line("storage unmounted; log file closed\n");
		}
		return;
	}
	if (log_fd < 0 || (gen && gen != mount_generation)) {
		if (log_fd >= 0) {
			(void)flush_log();
			close(log_fd);
			log_fd = -1;
		}
		if (open_log_file() == 0) {
			mount_generation = gen;
			append_record("logd",
				"--- SF2000 Linux storage mounted ---",
				strlen("--- SF2000 Linux storage mounted ---"));
			(void)sync_log();
			kmsg_line("storage remounted; log file reopened\n");
		} else {
			kmsg_line("cannot open /mnt/sd/loglinux.txt after remount\n");
		}
	}
}

int main(void)
{
	char kmsg[2048];
	char topology[2048] = "";
	char previous_topology[2048] = "";
	uint64_t last_flush;
	uint64_t last_probe;
	uint64_t last_sync;
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
	if (open_log_file() != 0) {
		kmsg_line("cannot open /mnt/sd/loglinux.txt\n");
		return 1;
	}
	mount_generation = read_mount_generation();
	append_record("logd", "--- SF2000 Linux storage mounted ---",
		strlen("--- SF2000 Linux storage mounted ---"));
	(void)sync_log();

	last_flush = last_probe = last_sync = monotonic_us();

	while (!stop_requested()) {
		ssize_t got;
		uint64_t now;
		int active;

		refresh_storage_binding();
		if (access(STANDBY_MARKER, F_OK) == 0) {
			/* Finish outstanding FAT writes, then avoid periodic SD traffic. */
			(void)sync_log();
			sleep_ms(500);
			last_flush = last_probe = last_sync = monotonic_us();
			continue;
		}
		active = performance_active();
		if (active && !storage_deferred) {
			/*
			 * Establish a clean storage boundary before the browser becomes
			 * interactive.  This one transition sync prevents old dirty log
			 * pages from being written back later during emulation.
			 */
			if (sync_log() != 0)
				kmsg_line("pre-journal sync failed\n");
			storage_deferred = 1;
			log_peak = log_used;
			log_dropped = 0;
			performance_metric_records = 0;
			performance_metric_bytes = 0;
			(void)unlink(PERFORMANCE_METRICS_PATH);
			append_record("logd", "RAM journal begin: FAT writes deferred", 38);
			set_performance_ready(1);
			kmsg_line("RAM journal begin: FAT writes deferred\n");
		} else if (!active && storage_deferred) {
			char summary[160];
			int length;

			import_performance_metrics();
			length = snprintf(summary, sizeof(summary),
				"RAM journal end: bytes=%lu peak=%lu dropped=%llu metrics=%u metric_bytes=%lu\n",
				(unsigned long)log_used, (unsigned long)log_peak,
				(unsigned long long)log_dropped,
				performance_metric_records,
				(unsigned long)performance_metric_bytes);

			if (length > 0)
				append_record("logd", summary, (size_t)length);
			storage_deferred = 0;
			if (length > 0)
				kmsg_line(summary);
			if (flush_log() != 0)
				kmsg_line("RAM journal drain failed\n");
			else
				kmsg_line("RAM journal drained after frontend exit\n");
			set_performance_ready(0);
			last_flush = now = monotonic_us();
		}
		if (kmsg_fd >= 0) {
			while ((got = read(kmsg_fd, kmsg, sizeof(kmsg))) > 0)
				append_record("kmsg", kmsg, (size_t)got);
		}
		drain_input_fds(input_fds);
		service_flush_request();
		now = monotonic_us();
		if (active) {
			/*
			 * Emulator ROM reads and audio playback share the single MMC
			 * channel with this logger.  Detailed /proc snapshots followed by
			 * fsync used to collide with gpSP's fourth 1 MiB read and caused
			 * periodic audio starvation.  Frontends publish their own video
			 * and audio counters, so retain kmsg continuously and use only a
			 * low-rate heartbeat while latency-sensitive work is active.
			 */
			if (now - last_probe >= FRONTEND_PROBE_MS * 1000u) {
				append_record("heartbeat", "frontend-active", 15);
				last_probe = now;
			}
		} else if (now - last_probe >= LOG_FLUSH_MS * 1000u) {
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
		if (!active && (log_used >= LOG_FLUSH_BYTES ||
				now - last_flush >= LOG_FLUSH_MS * 1000u)) {
			if (flush_log() != 0)
				kmsg_line("persistent log flush failed\n");
			last_flush = now;
		}
		if (!active && now - last_sync >= LOG_FLUSH_MS * 1000u) {
			if (sync_log() != 0)
				kmsg_line("persistent log sync failed\n");
			last_sync = now;
		}
		/*
		 * During a performance session the kernel queues kmsg and evdev
		 * records for us.  A 250 ms batch cadence cuts 16 needless
		 * scheduler wakeups per second without losing diagnostics.
		 */
		sleep_ms(active ? 250u : 50u);
	}

	append_record("logd", "--- SF2000 Linux logger shutdown ---",
		strlen("--- SF2000 Linux logger shutdown ---"));
	/* A clean system shutdown is the final safe drain budget. */
	storage_deferred = 0;
	set_performance_ready(0);
	(void)sync_log();
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
