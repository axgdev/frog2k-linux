// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

extern long syscall(long number, ...);

#define READY_MARKER "/run/sf2000-frontend-ready"
#define GAMBATTE_PATH "/usr/bin/sf2000-gambatte"
#define GPSP_PATH "/usr/bin/sf2000-gpsp"
#define PLAYER_PATH "/usr/bin/sf2000-player"
#define SD_ROOT "/mnt/sd"
#define MAX_ENTRIES 128
#define MAX_NAME 128
#define MAX_PATH 512
#define MAX_FRAME_PIXELS (320u * 240u)
#define LINUX_DT_DIR 4u
#define LINUX_DT_REG 8u

struct linux_dirent64 {
	uint64_t inode;
	int64_t offset;
	unsigned short record_length;
	unsigned char type;
	char name[];
};

static ssize_t kernel_getdents64(int fd, void *buffer, size_t bytes)
{
#ifdef __mips__
	register long v0 __asm__("$2") = __NR_getdents64;
	register long a0 __asm__("$4") = fd;
	register long a1 __asm__("$5") = (long)buffer;
	register long a2 __asm__("$6") = (long)bytes;
	register long a3 __asm__("$7") = 0;

	__asm__ volatile ("syscall"
		: "+r"(v0), "+r"(a3)
		: "r"(a0), "r"(a1), "r"(a2)
		: "memory");
	if (a3) {
		errno = (int)-v0;
		return -1;
	}
	return v0;
#else
	return syscall(__NR_getdents64, fd, buffer, bytes);
#endif
}

struct entry { char name[MAX_NAME]; unsigned directory; };
static struct entry entries[MAX_ENTRIES];
static unsigned entry_count, selected, first;
static uint16_t framebuffer[MAX_FRAME_PIXELS];
static int framebuffer_fd = -1;
static unsigned width, height, stride;
static char current[MAX_PATH] = SD_ROOT;

static void log_message(const char *message)
{
	char line[640];
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	int length;

	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line), "<6>sf2000-browser: %s\n", message);
	if (length > 0)
		(void)write(fd, line, (size_t)length);
	close(fd);
}

static uint32_t glyph(char c)
{
	if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
	switch (c) {
	case 'A': return 0x7e11117e; case 'B': return 0x7f494936;
	case 'C': return 0x3e414122; case 'D': return 0x7f41413e;
	case 'E': return 0x7f494941; case 'F': return 0x7f090901;
	case 'G': return 0x3e41497a; case 'H': return 0x7f08087f;
	case 'I': return 0x41417f41; case 'J': return 0x2040403f;
	case 'K': return 0x7f081463; case 'L': return 0x7f404040;
	case 'M': return 0x7f06187f; case 'N': return 0x7f0e707f;
	case 'O': return 0x3e41413e; case 'P': return 0x7f090906;
	case 'Q': return 0x3e41613e; case 'R': return 0x7f091966;
	case 'S': return 0x26494932; case 'T': return 0x01017f01;
	case 'U': return 0x3f40403f; case 'V': return 0x1f60401f;
	case 'W': return 0x7f30187f; case 'X': return 0x631c1c63;
	case 'Y': return 0x07087807; case 'Z': return 0x61514947;
	case '0': return 0x3e45493e; case '1': return 0x00427f40;
	case '2': return 0x62514946; case '3': return 0x22494936;
	case '4': return 0x0f087f08; case '5': return 0x2f494931;
	case '6': return 0x3e494932; case '7': return 0x01611907;
	case '8': return 0x36494936; case '9': return 0x2649493e;
	case '.': return 0x00006060; case '-': return 0x00080808;
	case '_': return 0x40404040; case '/': return 0x60180c03;
	case '<': return 0x08142241; case '>': return 0x41221408;
	case ':': return 0x00363600; case '?': return 0x02015906;
	default: return 0;
	}
}

static void text(int x, int y, const char *value, uint16_t color)
{
	for (; *value && x + 5 < (int)width; value++, x += 6) {
		uint32_t bits = glyph(*value);
		unsigned column, row;
		for (column = 0; column < 4; column++)
			for (row = 0; row < 7; row++)
				if ((bits >> ((3u - column) * 8u + row)) & 1u)
					framebuffer[(y + row) * stride + x + column] = color;
	}
}

static int compare_entries(const void *left, const void *right)
{
	const struct entry *a = left, *b = right;
	if (a->directory != b->directory)
		return a->directory ? -1 : 1;
	return strcasecmp(a->name, b->name);
}

static void scan_directory(void)
{
	char buffer[4096];
	int directory = open(current, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	int saved_errno = errno;

	entry_count = selected = first = 0;
	if (directory < 0) {
		char message[640];
		snprintf(message, sizeof(message),
			"cannot open directory path=%s errno=%d", current, saved_errno);
		log_message(message);
		return;
	}
	while (entry_count < MAX_ENTRIES) {
		ssize_t bytes = kernel_getdents64(directory, buffer, sizeof(buffer));
		long position = 0;

		if (bytes < 0) {
			char message[160];
			snprintf(message, sizeof(message),
				"directory read failed errno=%d", errno);
			log_message(message);
			break;
		}
		if (bytes == 0)
			break;
		while (position < bytes && entry_count < MAX_ENTRIES) {
			struct linux_dirent64 *item =
				(struct linux_dirent64 *)(void *)(buffer + position);
			char path[MAX_PATH];
			int is_directory = item->type == LINUX_DT_DIR;
			int is_regular = item->type == LINUX_DT_REG;

			if (item->record_length <
					offsetof(struct linux_dirent64, name) + 1u ||
					position + item->record_length > bytes)
				break;
			position += item->record_length;
			if (!strcmp(item->name, ".") || !strcmp(item->name, ".."))
				continue;
			if (snprintf(path, sizeof(path), "%s/%s", current, item->name) >=
					(int)sizeof(path))
				continue;
			if (!is_directory && !is_regular) {
				int item_fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
				if (item_fd >= 0)
					is_directory = 1;
				else
					item_fd = open(path, O_RDONLY | O_CLOEXEC);
				if (item_fd >= 0) {
					if (!is_directory)
						is_regular = 1;
					close(item_fd);
				}
				if (!is_directory && !is_regular)
					continue;
			}
			strncpy(entries[entry_count].name, item->name, MAX_NAME - 1u);
			entries[entry_count].name[MAX_NAME - 1u] = 0;
			entries[entry_count].directory = (unsigned)is_directory;
			entry_count++;
		}
	}
	close(directory);
	qsort(entries, entry_count, sizeof(entries[0]), compare_entries);
	{
		char message[640];
		snprintf(message, sizeof(message), "directory path=%s entries=%u",
			current, entry_count);
		log_message(message);
	}
}

static void draw(void)
{
	unsigned row, visible = height > 42 ? (height - 42) / 10u : 1u;
	char line[MAX_NAME + 4];

	memset(framebuffer, 0, (size_t)height * stride * sizeof(*framebuffer));
	text(6, 5, "SF2000 LIBRETRO", 0xffff);
	text(6, 16, current, 0x07ff);
	if (!entry_count)
		text(12, 42, "NO FILES", 0xf800);
	for (row = 0; row < visible && first + row < entry_count; row++) {
		unsigned index = first + row;
		uint16_t color = index == selected ? 0xffe0 : 0xffff;
		snprintf(line, sizeof(line), "%c %s", entries[index].directory ? '>' : ' ',
			entries[index].name);
		text(8, 32 + (int)row * 10, line, color);
	}
	text(6, height - 9, "A OPEN  B BACK  START+L EXIT", 0x07e0);
	if (pwrite(framebuffer_fd, framebuffer,
			(size_t)height * stride * sizeof(*framebuffer), 0) < 0)
		log_message("framebuffer write failed");
}

static int gameboy_path(const char *path)
{
	const char *component = path, *extension = strrchr(path, '.');
	int in_gameboy_directory = 0;

	while ((component = strchr(component, '/'))) {
		const char *end = strchr(++component, '/');
		size_t length = end ? (size_t)(end - component) : strlen(component);
		if ((length == 2 && !strncasecmp(component, "GB", 2)) ||
				(length == 3 && !strncasecmp(component, "GBC", 3)))
			in_gameboy_directory = 1;
		if (!end) break;
		component = end;
	}
	return in_gameboy_directory && extension &&
		(!strcasecmp(extension, ".gb") || !strcasecmp(extension, ".gbc"));
}

static int gba_path(const char *path)
{
	const char *extension = strrchr(path, '.');
	const char *component = path;

	while ((component = strchr(component, '/'))) {
		const char *end = strchr(++component, '/');
		size_t length = end ? (size_t)(end - component) : strlen(component);
		if (length == 3 && !strncasecmp(component, "GBA", 3))
			return extension && !strcasecmp(extension, ".gba");
		if (!end) break;
		component = end;
	}
	return 0;
}

static int media_path(const char *path)
{
	const char *ext = strrchr(path, '.');

	if (!ext)
		return 0;
	if (!strcasecmp(ext, ".mp3") || !strcasecmp(ext, ".aac") ||
			!strcasecmp(ext, ".flac") || !strcasecmp(ext, ".wav") ||
			!strcasecmp(ext, ".ogg") || !strcasecmp(ext, ".mp4") ||
			!strcasecmp(ext, ".mkv") || !strcasecmp(ext, ".avi") ||
			!strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg") ||
			!strcasecmp(ext, ".jpeg") || !strcasecmp(ext, ".bmp") ||
			!strcasecmp(ext, ".gif"))
		return 1;
	return 0;
}

static void launch_selected(void)
{
	char path[MAX_PATH], message[640];

	if (snprintf(path, sizeof(path), "%s/%s", current, entries[selected].name) >=
			(int)sizeof(path))
		return;
	if (entries[selected].directory) {
		strcpy(current, path);
		scan_directory();
		return;
	}
	if (!gameboy_path(path) && !gba_path(path) && !media_path(path)) {
		log_message("unsupported file; use GB, GBC, GBA, or media files");
		return;
	}
	if (media_path(path)) {
		snprintf(message, sizeof(message), "launch Player %s", path);
		log_message(message);
		memset(framebuffer, 0, (size_t)height * stride * sizeof(*framebuffer));
		text(72, 92, "OPENING MEDIA", 0xffff);
		text(48, 132, "PLEASE WAIT", 0x07e0);
		if (pwrite(framebuffer_fd, framebuffer,
				(size_t)height * stride * sizeof(*framebuffer), 0) < 0)
			log_message("loading framebuffer write failed");
		{
			char *const argv[] = { (char *)PLAYER_PATH, path, NULL };
			char *const envp[] = { NULL };

			execve(PLAYER_PATH, argv, envp);
		}
		snprintf(message, sizeof(message), "player exec failed errno=%d", errno);
		log_message(message);
		return;
	}
	{
		const char *core = gba_path(path) ? GPSP_PATH : GAMBATTE_PATH;
		const char *name = gba_path(path) ? "gpSP" : "Gambatte";

		snprintf(message, sizeof(message), "launch %s %s", name, path);
		log_message(message);
		/* exec of a large bFLT includes relocation and BSS setup on the weak
		 * CPU.  Replace the browser before entering the kernel loader so this
		 * unavoidable work never looks like a frozen selection screen. */
		memset(framebuffer, 0, (size_t)height * stride * sizeof(*framebuffer));
		text(72, 92, "LOADING EMULATOR", 0xffff);
		text(102, 108, name, 0x07ff);
		text(48, 132, "PLEASE WAIT - SYSTEM IS ACTIVE", 0x07e0);
		if (pwrite(framebuffer_fd, framebuffer,
				(size_t)height * stride * sizeof(*framebuffer), 0) < 0)
			log_message("loading framebuffer write failed");
		{
			char *const argv[] = { (char *)core, path, NULL };
			char *const envp[] = { NULL };

			execve(core, argv, envp);
		}
		snprintf(message, sizeof(message), "%s exec failed errno=%d", name, errno);
	}
	log_message(message);
}

static void parent_directory(void)
{
	char *slash;
	if (!strcmp(current, SD_ROOT)) return;
	slash = strrchr(current, '/');
	if (slash && slash > current + strlen(SD_ROOT) - 1u) *slash = 0;
	else strcpy(current, SD_ROOT);
	scan_directory();
}

int main(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct input_event event;
	int fb, input, start = 0, l = 0, exit_latched = 0;

	strcpy(current, SD_ROOT);
	fb = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (fb < 0 || ioctl(fb, FBIOGET_FSCREENINFO, &fix) < 0 ||
			ioctl(fb, FBIOGET_VSCREENINFO, &var) < 0 || var.bits_per_pixel != 16)
		return 1;
	width = var.xres; height = var.yres; stride = fix.line_length / 2u;
	if ((size_t)height * stride > MAX_FRAME_PIXELS)
		return 1;
	framebuffer_fd = fb;
	input = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (input < 0) return 1;
	scan_directory(); draw();
	{ int ready = open(READY_MARKER, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (ready >= 0) close(ready); }
	{
		char message[160];
		snprintf(message, sizeof(message),
			"ready: DPAD select A open B back START+L exit fb=%ux%u stride=%u",
			width, height, stride * 2u);
		log_message(message);
	}
	for (;;) {
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			unsigned visible = height > 42 ? (height - 42) / 10u : 1u;
			if (event.type != EV_KEY) continue;
			if (event.code == BTN_START || event.code == BTN_TL) {
				if (event.code == BTN_START) start = event.value != 0;
				else l = event.value != 0;
				if (!start && !l) exit_latched = 0;
				if (!exit_latched && start && l) {
					exit_latched = 1;
					goto done;
				}
				continue;
			}
			if (event.value != 1) continue;
			if (event.code == BTN_DPAD_UP && selected) selected--;
			else if (event.code == BTN_DPAD_DOWN && selected + 1 < entry_count) selected++;
			else if (event.code == BTN_EAST && entry_count) {
				launch_selected();
				/* Drop the chord which quit a core while waitpid() ran. */
				while (read(input, &event, sizeof(event)) == sizeof(event)) { }
				start = l = exit_latched = 0;
			}
			else if (event.code == BTN_SOUTH) parent_directory();
			if (selected < first) first = selected;
			if (selected >= first + visible) first = selected - visible + 1u;
			draw();
		}
		{ struct timespec delay = { 0, 10000000L }; nanosleep(&delay, NULL); }
	}
done:
	log_message("returned cleanly");
	close(input); close(fb);
	_exit(0);
}
