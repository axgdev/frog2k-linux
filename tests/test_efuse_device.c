// SPDX-License-Identifier: MIT
/*
 * Physical device test for HC15xx eFuse and chip ID.
 * Reads hardware registers via /dev/mem on the SF2000.
 *
 * Build: $(CC_MIPS) -static -O2 -o test_efuse test_efuse_device.c
 * Run: ./test_efuse
 *
 * Expected output on SF2000:
 *   chip_id: 0x1512a501
 *   efuse data: <32 bytes of OTP data>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define SYS_BASE	0x18800000
#define SYS_SIZE	0x1000
#define EFUSE_BASE	0x18818e00
#define EFUSE_SIZE	0x100

#define EFUSE_CTRL	0x00
#define EFUSE_STATUS	0x08
#define EFUSE_DATA0	0x10
#define EFUSE_TIMING	0x40

#define EFUSE_CTRL_READ		0x01
#define EFUSE_STATUS_BUSY	0x80000000u
#define EFUSE_TIMING_VAL	0x16004843u

static volatile uint32_t *map_regs(int fd, unsigned long base, size_t size)
{
	void *ptr;

	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	return (volatile uint32_t *)ptr;
}

int main(void)
{
	int fd;
	volatile uint32_t *sys;
	volatile uint32_t *efuse;
	uint32_t chip_id;
	unsigned int i;
	int timeout;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem");
		return 1;
	}

	sys = map_regs(fd, SYS_BASE, SYS_SIZE);
	efuse = map_regs(fd, EFUSE_BASE, EFUSE_SIZE);

	/* Read chip ID */
	chip_id = sys[0];
	printf("chip_id: 0x%08x\n", chip_id);
	if ((chip_id >> 16) != 0x1512) {
		printf("FAIL: expected HC1512 (0x1512), got 0x%04x\n",
		       chip_id >> 16);
		return 1;
	}
	printf("chip_id: PASS (HC1512)\n");

	/* Read eFuse */
	efuse[EFUSE_TIMING / 4] = EFUSE_TIMING_VAL;
	efuse[EFUSE_CTRL / 4] = EFUSE_CTRL_READ;

	timeout = 5000;
	while ((efuse[EFUSE_STATUS / 4] & EFUSE_STATUS_BUSY) && --timeout)
		usleep(1000);

	if (timeout == 0) {
		printf("FAIL: eFuse read timed out\n");
		return 1;
	}

	printf("efuse data:");
	for (i = 0; i < 8; i++)
		printf(" %08x", efuse[(EFUSE_DATA0 + i * 4) / 4]);
	printf("\n");

	printf("efuse: PASS\n");

	munmap((void *)efuse, EFUSE_SIZE);
	munmap((void *)sys, SYS_SIZE);
	close(fd);
	return 0;
}
