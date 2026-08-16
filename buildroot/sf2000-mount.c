// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

/*
 * SF2000 SD layout:
 *   /mnt/sd          primary volume (logs, bios/, saves/, firmware/)
 *   /mnt/sd2..sdN    extra volumes (often ROM libraries on p2/p3/...)
 *
 * Host-side /dev/sda1 maps to the card's first partition; on the device that
 * is /dev/mmcblk0p1 (never /dev/sda*).  Superfloppy cards still use
 * /dev/mmcblk0 with no partition nodes.
 *
 * Filesystems match the ROM bootloader (XMC bugfix image strings and QEMU
 * stock full-chain smokes): FAT12/16 ("msdos"/"vfat"), FAT32 ("vfat"), and
 * exFAT ("exfat").  NTFS is intentionally omitted — the bootloader has no
 * NTFS path and cannot load bios/bisrv.asd from an NTFS volume.
 *
 * This process is a long-running mount supervisor: it remounts after card
 * removal/reinsertion or card swap (size/CID change).  The board DTS uses
 * broken-cd + MMC_CAP_NEEDS_POLL, so presence is polled via sysfs.
 */

#define PRIMARY_MOUNT "/mnt/sd"
#define MOUNT_MARKER "/run/sf2000-storage-mounted"
#define ROOTS_PATH "/run/sf2000-storage-roots"
#define MAP_PATH "/run/sf2000-storage-map"
#define GEN_PATH "/run/sf2000-storage-generation"
#define UNIFROG_MOUNT_POINT "/media/mmcblk0"
#define STATUS_PATH "/run/sf2000-storage.status"
#define VOL_BASE "/mnt"
#define MAX_CANDIDATES 16u
#define MAX_MOUNTS 8u
#define MMC_BLOCK_MAJOR 179u
#define POLL_MS_IDLE 500u
#define POLL_MS_MOUNTED 1000u
#define POLL_MS_RETRY 1000u

static const char *const fallback_devices[] = {
	"/dev/mmcblk0p1", "/dev/mmcblk0p2", "/dev/mmcblk0p3",
	"/dev/mmcblk0p4", "/dev/mmcblk0p5", "/dev/mmcblk0p6",
	"/dev/mmcblk0p7", "/dev/mmcblk0"
};

/* Bootloader-compatible consumer SD formats only. */
static const char *const fstypes[] = { "vfat", "msdos", "exfat" };

static const char *const public_mounts[] = {
	"/mnt/sd", "/mnt/sd2", "/mnt/sd3", "/mnt/sd4",
	"/mnt/sd5", "/mnt/sd6", "/mnt/sd7", "/mnt/sd8",
};

static unsigned storage_generation;

struct mounted_volume {
	char device[32];
	char mountpoint[32];
	char fstype[16];
	unsigned score;
	unsigned is_primary;
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

static int path_is_dir(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int path_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0;
}

static int read_devt(const char *path, unsigned *major_out, unsigned *minor_out)
{
	char buf[32];
	unsigned major = 0;
	unsigned minor = 0;
	ssize_t got;
	unsigned i = 0;
	int fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got <= 0)
		return -1;
	buf[got] = 0;
	while (buf[i] >= '0' && buf[i] <= '9') {
		major = major * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	if (buf[i] != ':')
		return -1;
	i++;
	while (buf[i] >= '0' && buf[i] <= '9') {
		minor = minor * 10u + (unsigned)(buf[i] - '0');
		i++;
	}
	*major_out = major;
	*minor_out = minor;
	return 0;
}

static void ensure_block_node(const char *node, const char *sys_dev,
	unsigned fallback_minor, int require_sys)
{
	struct stat st;
	unsigned major = MMC_BLOCK_MAJOR;
	unsigned minor = fallback_minor;

	if (stat(node, &st) == 0 && S_ISBLK(st.st_mode))
		return;
	if (sys_dev && read_devt(sys_dev, &major, &minor) == 0) {
		/* use sysfs major:minor */
	} else if (require_sys) {
		return;
	} else {
		major = MMC_BLOCK_MAJOR;
		minor = fallback_minor;
	}
	(void)unlink(node);
	(void)mknod(node, S_IFBLK | 0660, makedev(major, minor));
}

static void ensure_known_nodes(void)
{
	unsigned i;

	ensure_block_node("/dev/mmcblk0", "/sys/block/mmcblk0/dev", 0, 0);
	for (i = 1; i <= 7u; i++) {
		char node[24];
		char sys_dev[48];

		snprintf(node, sizeof(node), "/dev/mmcblk0p%u", i);
		snprintf(sys_dev, sizeof(sys_dev),
			"/sys/block/mmcblk0/mmcblk0p%u/dev", i);
		ensure_block_node(node, sys_dev, i, 1);
	}
}

static int is_partition_name(const char *name)
{
	const char *p;

	if (strncmp(name, "mmcblk0p", 8) != 0)
		return 0;
	p = name + 8;
	if (*p < '1' || *p > '9')
		return 0;
	for (; *p; p++) {
		if (*p < '0' || *p > '9')
			return 0;
	}
	return 1;
}

static int candidate_index(const char candidates[][32], unsigned count,
	const char *node)
{
	unsigned i;

	for (i = 0; i < count; i++) {
		if (strcmp(candidates[i], node) == 0)
			return (int)i;
	}
	return -1;
}

/*
 * Stock SF2000 cards are superfloppy: FAT lives at sector 0 and any MBR
 * partition nodes are ghost entries that fail every mount probe (~300ms
 * each).  Reading the boot sector is one cheap block transfer, so detect
 * the layout once and let the whole disk be tried before the partitions.
 * Cards with a real partition table keep the partition-first order the
 * QEMU smokes and MBR users rely on.
 */
static int whole_disk_is_superfloppy(void)
{
	unsigned char sector[512];
	ssize_t got;
	int fd;

	fd = open("/dev/mmcblk0", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	got = pread(fd, sector, sizeof(sector), 0);
	close(fd);
	if (got < (ssize_t)sizeof(sector))
		return 0;
	/* A FAT/exFAT boot sector always carries the 0x55AA boot signature. */
	if (sector[510] != 0x55u || sector[511] != 0xaau)
		return 0;
	/* BS_FilSysType: FAT12/16 at offset 54, FAT32 at offset 82. */
	if (sector[54] == 'F' && sector[55] == 'A' && sector[56] == 'T')
		return 1;
	if (sector[82] == 'F' && sector[83] == 'A' && sector[84] == 'T')
		return 1;
	/* exFAT boot sector: "EXFAT   " OEM name at offset 3. */
	if (sector[3] == 'E' && sector[4] == 'X' && sector[5] == 'F')
		return 1;
	return 0;
}

static unsigned collect_candidates(char candidates[][32], unsigned max)
{
	DIR *dir;
	struct dirent *de;
	unsigned count = 0;
	unsigned i;

	dir = opendir("/sys/block/mmcblk0");
	if (dir) {
		while ((de = readdir(dir)) != NULL && count < max) {
			char sys_dev[48];
			char node[24];

			if (!is_partition_name(de->d_name))
				continue;
			snprintf(node, sizeof(node), "/dev/%.16s", de->d_name);
			snprintf(sys_dev, sizeof(sys_dev),
				"/sys/block/mmcblk0/%.16s/dev", de->d_name);
			ensure_block_node(node, sys_dev, 0, 1);
			if (candidate_index(candidates, count, node) >= 0)
				continue;
			snprintf(candidates[count], sizeof(candidates[0]),
				"%s", node);
			count++;
		}
		closedir(dir);
		for (i = 0; i + 1u < count; i++) {
			unsigned j;

			for (j = i + 1u; j < count; j++) {
				if (strcmp(candidates[j], candidates[i]) < 0) {
					char tmp[32];

					memcpy(tmp, candidates[i], sizeof(tmp));
					memcpy(candidates[i], candidates[j],
						sizeof(tmp));
					memcpy(candidates[j], tmp, sizeof(tmp));
				}
			}
		}
	}

	for (i = 0; i < sizeof(fallback_devices) / sizeof(fallback_devices[0]) &&
			count < max; i++) {
		if (candidate_index(candidates, count, fallback_devices[i]) >= 0)
			continue;
		snprintf(candidates[count], sizeof(candidates[0]), "%s",
			fallback_devices[i]);
		count++;
	}
	return count;
}

static unsigned score_volume(const char *mountpoint)
{
	unsigned score = 1u;
	char path[96];

	/*
	 * Prefer the SF2000 system partition (bios/firmware/saves) as primary so
	 * logs and the frontend keep writing under /mnt/sd.  Extra partitions
	 * that only hold ROM trees still mount and remain browsable.
	 */
	/* The UI contract is stronger evidence than a generic ROM directory. */
	snprintf(path, sizeof(path), "%s/sf2000", mountpoint);
	if (path_is_dir(path))
		score += 120u;
	snprintf(path, sizeof(path), "%s/sf2000/ui.ttf", mountpoint);
	if (path_exists(path))
		score += 100u;
	snprintf(path, sizeof(path), "%s/sf2000.conf", mountpoint);
	if (path_exists(path))
		score += 80u;
	snprintf(path, sizeof(path), "%s/bios", mountpoint);
	if (path_is_dir(path))
		score += 100u;
	snprintf(path, sizeof(path), "%s/firmware", mountpoint);
	if (path_is_dir(path))
		score += 80u;
	snprintf(path, sizeof(path), "%s/saves", mountpoint);
	if (path_is_dir(path))
		score += 40u;
	snprintf(path, sizeof(path), "%s/loglinux.txt", mountpoint);
	if (path_exists(path))
		score += 20u;
	snprintf(path, sizeof(path), "%s/GB", mountpoint);
	if (path_is_dir(path))
		score += 30u;
	snprintf(path, sizeof(path), "%s/GBA", mountpoint);
	if (path_is_dir(path))
		score += 30u;
	snprintf(path, sizeof(path), "%s/NES", mountpoint);
	if (path_is_dir(path))
		score += 30u;
	snprintf(path, sizeof(path), "%s/ROMS", mountpoint);
	if (path_is_dir(path))
		score += 25u;
	snprintf(path, sizeof(path), "%s/roms", mountpoint);
	if (path_is_dir(path))
		score += 25u;
	return score;
}

static int try_mount_at(const char *device, const char *mountpoint,
	char *fstype_out, size_t fstype_len)
{
	struct stat st;
	unsigned t;
	char line[160];

	if (stat(device, &st) != 0 || !S_ISBLK(st.st_mode))
		return -1;
	(void)mkdir(mountpoint, 0755);

	for (t = 0; t < sizeof(fstypes) / sizeof(fstypes[0]); t++) {
		/*
		 * Upstream VFAT mounts request UTF-8 name conversion so ROM and
		 * save names with non-ASCII characters stay readable.
		 */
		const char *data = NULL;

		if (strcmp(fstypes[t], "vfat") == 0 ||
		    strcmp(fstypes[t], "msdos") == 0)
			data = "iocharset=utf8";
		if (mount(device, mountpoint, fstypes[t], MS_NOATIME, data) == 0) {
			if (fstype_out && fstype_len)
				snprintf(fstype_out, fstype_len, "%s", fstypes[t]);
			return 0;
		}
		snprintf(line, sizeof(line), "mount try %.24s -> %.24s type=%s errno=%d",
			device, mountpoint, fstypes[t], errno);
		log_status(line);
	}
	return -1;
}

static void append_line(char *buf, size_t *len, size_t cap, const char *line)
{
	int n;

	if (*len >= cap)
		return;
	n = snprintf(buf + *len, cap - *len, "%s", line);
	if (n > 0 && (size_t)n < cap - *len)
		*len += (size_t)n;
}

static void publish_results(const struct mounted_volume *vols, unsigned count,
	unsigned primary)
{
	char roots[512];
	char map[768];
	char line[192];
	unsigned i;
	size_t roots_len = 0;
	size_t map_len = 0;

	roots[0] = 0;
	map[0] = 0;

	/* Primary first so consumers can treat line 0 as the system volume. */
	snprintf(line, sizeof(line), "%.30s\n", vols[primary].mountpoint);
	append_line(roots, &roots_len, sizeof(roots), line);
	snprintf(line, sizeof(line), "%.30s %.30s primary %.14s score=%u\n",
		vols[primary].device, vols[primary].mountpoint,
		vols[primary].fstype, vols[primary].score);
	append_line(map, &map_len, sizeof(map), line);

	for (i = 0; i < count; i++) {
		if (i == primary)
			continue;
		snprintf(line, sizeof(line), "%.30s\n", vols[i].mountpoint);
		append_line(roots, &roots_len, sizeof(roots), line);
		snprintf(line, sizeof(line), "%.30s %.30s extra %.14s score=%u\n",
			vols[i].device, vols[i].mountpoint,
			vols[i].fstype, vols[i].score);
		append_line(map, &map_len, sizeof(map), line);
	}

	write_text(ROOTS_PATH, roots);
	write_text(MAP_PATH, map);
	snprintf(line, sizeof(line), "%s\n", vols[primary].device);
	write_text(MOUNT_MARKER, line);

	(void)unlink(UNIFROG_MOUNT_POINT);
	(void)symlink(PRIMARY_MOUNT, UNIFROG_MOUNT_POINT);

	snprintf(line, sizeof(line),
		"mount ok primary=%.24s extras=%u at %s",
		vols[primary].device, count > 1u ? count - 1u : 0u,
		PRIMARY_MOUNT);
	log_status(line);
}

static void unmount_all_volumes(void);

/*
 * Mount every usable candidate.  The highest-scoring volume becomes the
 * primary at /mnt/sd; remaining volumes land on /mnt/sd2, /mnt/sd3, ...
 * so the browser can list ROM trees that live on other partitions.
 */
static int mount_all_volumes(void)
{
	char candidates[MAX_CANDIDATES][32];
	struct mounted_volume vols[MAX_MOUNTS];
	char tmp_mounts[MAX_MOUNTS][32];
	unsigned count;
	unsigned mounted = 0;
	unsigned primary = 0;
	unsigned i;
	unsigned extra_slot = 2;
	int superfloppy;
	char line[160];

	/*
	 * Stock superfloppy cards carry no partition table, so every
	 * /dev/mmcblk0pN node is a ghost that fails each mount probe (~150 ms
	 * each on the physical card).  Mount the whole disk directly: one
	 * candidate, no sysfs scan, no ghost probes, and no score walk.
	 */
	superfloppy = whole_disk_is_superfloppy();
	if (superfloppy) {
		count = 1;
		snprintf(candidates[0], sizeof(candidates[0]), "/dev/mmcblk0");
		log_status("superfloppy: whole disk first");
	} else {
		count = collect_candidates(candidates, MAX_CANDIDATES);
	}
	snprintf(line, sizeof(line), "candidates=%u", count);
	log_status(line);

	/*
	 * Probe the candidates, then fall back to the partition scan when the
	 * whole-disk fast path mounted nothing (misidentified card, transient
	 * read, unsupported variant).  The previous reorder path kept the
	 * scanned list as a recovery; keep that behaviour.
	 */
	for (;;) {
		for (i = 0; i < count && mounted < MAX_MOUNTS; i++) {
			char tmp[32];
			char fstype[16];

			snprintf(tmp, sizeof(tmp), "/mnt/.sf2000-vol%u", mounted);
			(void)umount(tmp);
			if (try_mount_at(candidates[i], tmp, fstype,
					 sizeof(fstype)) != 0)
				continue;

			memset(&vols[mounted], 0, sizeof(vols[mounted]));
			snprintf(vols[mounted].device,
				sizeof(vols[mounted].device),
				"%.30s", candidates[i]);
			snprintf(tmp_mounts[mounted], sizeof(tmp_mounts[0]),
				"%.30s", tmp);
			snprintf(vols[mounted].fstype,
				sizeof(vols[mounted].fstype),
				"%.14s", fstype);
			/*
			 * Scoring only picks the primary among multiple volumes.
			 * A lone whole disk is primary by definition, and the
			 * walk costs ~13 FAT stats on the slow card, so skip it
			 * on single-candidate cards.
			 */
			if (count > 1)
				vols[mounted].score = score_volume(tmp);
			else
				vols[mounted].score = 1u;
			/* Slight preference for lower partition numbers when tied. */
			vols[mounted].score += (MAX_CANDIDATES - i);
			snprintf(line, sizeof(line),
				"volume %.24s type=%s score=%u",
				candidates[i], fstype, vols[mounted].score);
			log_status(line);
			mounted++;
		}
		if (mounted || !superfloppy)
			break;
		log_status("superfloppy: whole-disk probe empty; scanning partitions");
		superfloppy = 0;
		count = collect_candidates(candidates, MAX_CANDIDATES);
		if (!count)
			break;
	}

	if (!mounted)
		return -1;

	primary = 0;
	for (i = 1; i < mounted; i++) {
		if (vols[i].score > vols[primary].score)
			primary = i;
	}
	vols[primary].is_primary = 1;

	/* Relocate mounts onto stable public paths. */
	for (i = 0; i < mounted; i++) {
		const char *dest;
		char dest_buf[32];
		int ok = 0;

		if (i == primary) {
			dest = PRIMARY_MOUNT;
		} else {
			snprintf(dest_buf, sizeof(dest_buf), "%s/sd%u",
				VOL_BASE, extra_slot++);
			dest = dest_buf;
		}

		(void)umount(dest);
		(void)mkdir(dest, 0755);
		if (mount(tmp_mounts[i], dest, NULL, MS_MOVE, NULL) == 0) {
			(void)rmdir(tmp_mounts[i]);
			ok = 1;
		} else {
			/* MS_MOVE unavailable or failed: remount from device. */
			(void)umount(tmp_mounts[i]);
			(void)rmdir(tmp_mounts[i]);
			if (try_mount_at(vols[i].device, dest,
					vols[i].fstype,
					sizeof(vols[i].fstype)) == 0)
				ok = 1;
		}
		if (!ok) {
			snprintf(line, sizeof(line),
				"relocate fail %.24s errno=%d",
				vols[i].device, errno);
			log_status(line);
			if (i == primary) {
				/* Never publish an extra volume as /mnt/sd.  That would make
				 * the logger and browser appear alive while hiding the system
				 * partition (including its font and configuration). */
				log_status("primary relocation failed; retrying");
				unmount_all_volumes();
				return -1;
			}
			vols[i].mountpoint[0] = 0;
			continue;
		}
		snprintf(vols[i].mountpoint, sizeof(vols[i].mountpoint),
			"%s", dest);
	}

	/* Drop any volumes that failed relocation. */
	{
		struct mounted_volume kept[MAX_MOUNTS];
		unsigned kept_n = 0;
		unsigned new_primary = 0;
		int have_primary = 0;

		for (i = 0; i < mounted; i++) {
			if (!vols[i].mountpoint[0])
				continue;
			kept[kept_n] = vols[i];
			if (i == primary) {
				new_primary = kept_n;
				have_primary = 1;
			}
			kept_n++;
		}
		if (!kept_n)
			return -1;
		if (!have_primary)
			return -1;
		for (i = 0; i < kept_n; i++)
			kept[i].is_primary = (i == new_primary);
		storage_generation++;
		{
			char gen[32];

			snprintf(gen, sizeof(gen), "%u\n", storage_generation);
			write_text(GEN_PATH, gen);
		}
		publish_results(kept, kept_n, new_primary);
		return 0;
	}
}

static int card_present(void)
{
	struct stat st;

	if (stat("/sys/block/mmcblk0", &st) == 0 && S_ISDIR(st.st_mode))
		return 1;
	if (stat("/dev/mmcblk0", &st) == 0 && S_ISBLK(st.st_mode))
		return 1;
	return 0;
}

/* Identity used to detect card swap without a clean remove event. */
static int read_card_identity(char *out, size_t out_len)
{
	char size_buf[32];
	char cid_buf[64];
	ssize_t got;
	int fd;
	size_t n = 0;

	if (!out || !out_len)
		return -1;
	out[0] = 0;

	fd = open("/sys/block/mmcblk0/size", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		got = read(fd, size_buf, sizeof(size_buf) - 1u);
		close(fd);
		if (got > 0) {
			size_buf[got] = 0;
			n = (size_t)snprintf(out, out_len, "size=%s", size_buf);
		}
	}
	fd = open("/sys/block/mmcblk0/device/cid", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		got = read(fd, cid_buf, sizeof(cid_buf) - 1u);
		close(fd);
		if (got > 0) {
			cid_buf[got] = 0;
			if (n && n + 1u < out_len)
				out[n++] = ' ';
			if (n < out_len)
				snprintf(out + n, out_len - n, "cid=%.48s", cid_buf);
		}
	}
	return out[0] ? 0 : -1;
}

static int primary_mount_alive(void)
{
	struct stat st;
	char path[64];

	if (stat(PRIMARY_MOUNT, &st) != 0 || !S_ISDIR(st.st_mode))
		return 0;
	/* Prefer a real directory read over a bare mountpoint check. */
	snprintf(path, sizeof(path), "%s/.", PRIMARY_MOUNT);
	if (stat(path, &st) != 0)
		return 0;
	return access(MOUNT_MARKER, R_OK) == 0;
}

static void unmount_all_volumes(void)
{
	unsigned i;
	char line[96];

	sync();
	/* Extras first so nothing depends on them. */
	for (i = sizeof(public_mounts) / sizeof(public_mounts[0]); i > 0; i--) {
		const char *mp = public_mounts[i - 1u];

		if (umount(mp) == 0) {
			snprintf(line, sizeof(line), "umount ok %.32s", mp);
			log_status(line);
		}
	}
	/* Clear leftover probe mounts. */
	for (i = 0; i < MAX_MOUNTS; i++) {
		char tmp[32];

		snprintf(tmp, sizeof(tmp), "/mnt/.sf2000-vol%u", i);
		(void)umount(tmp);
		(void)rmdir(tmp);
	}
	(void)unlink(MOUNT_MARKER);
	(void)unlink(ROOTS_PATH);
	(void)unlink(MAP_PATH);
	(void)unlink(UNIFROG_MOUNT_POINT);
	write_text(STATUS_PATH, "card removed or unmounted");
	log_status("storage released");
	sync();
}

int main(void)
{
	char identity[160];
	char last_identity[160];
	int mounted = 0;

	(void)mkdir("/run", 0755);
	(void)mkdir("/mnt", 0755);
	(void)mkdir(PRIMARY_MOUNT, 0755);
	(void)mkdir("/media", 0755);
	last_identity[0] = 0;
	log_status("mount service start (hotplug)");

	for (;;) {
		ensure_known_nodes();

		if (!card_present()) {
			if (mounted) {
				log_status("card removed");
				unmount_all_volumes();
				mounted = 0;
				last_identity[0] = 0;
			}
			sleep_ms(POLL_MS_IDLE);
			continue;
		}

		identity[0] = 0;
		(void)read_card_identity(identity, sizeof(identity));

		if (mounted) {
			/*
			 * Card still enumerated: drop mounts if the filesystem
			 * disappeared or a different card was swapped in under us.
			 */
			if (!primary_mount_alive() ||
			    (identity[0] && last_identity[0] &&
			     strcmp(identity, last_identity) != 0)) {
				log_status("card changed or mount lost; remounting");
				unmount_all_volumes();
				mounted = 0;
				/* Fall through to mount on this iteration. */
			} else {
				sleep_ms(POLL_MS_MOUNTED);
				continue;
			}
		}

		if (mount_all_volumes() == 0) {
			mounted = 1;
			if (identity[0])
				snprintf(last_identity, sizeof(last_identity),
					"%.150s", identity);
			else
				last_identity[0] = 0;
			sleep_ms(POLL_MS_MOUNTED);
			continue;
		}

		log_status("mount retry pending");
		sleep_ms(POLL_MS_RETRY);
	}
}
