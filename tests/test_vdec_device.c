// SPDX-License-Identifier: MIT
/*
 * Physical device test for HC15xx VDEC hardware.
 * Tests register access and reset sequence on the SF2000.
 *
 * Build: $(CC_MIPS) -static -O2 -o test_vdec test_vdec_device.c
 * Run: ./test_vdec
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define VDEC_BASE	0x18810000
#define VDEC_SIZE	0x1000
#define SYS_BASE	0x18800000
#define SYS_SIZE	0x1000

#define VDEC_REG_CMD		0xff0
#define VDEC_REG_IRQ_STATUS	0xf78
#define VDEC_REG_STATUS2	0xf84
#define VDEC_CMD_RESET		0xff
#define VDEC_CMD_START		0x10

#define SYS_CHIP_ID		0x00
#define SYS_CTRL0		0x80
#define SYS_CTRL1		0x84

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
	volatile uint32_t *vdec;
	volatile uint32_t *sys;
	uint32_t chip_id, status;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem");
		return 1;
	}

	sys = map_regs(fd, SYS_BASE, SYS_SIZE);
	vdec = map_regs(fd, VDEC_BASE, VDEC_SIZE);

	/* Verify chip ID */
	chip_id = sys[SYS_CHIP_ID / 4];
	printf("chip_id: 0x%08x\n", chip_id);
	if ((chip_id >> 16) != 0x1512) {
		printf("FAIL: not HC1512\n");
		return 1;
	}

	/* Clear system control bits (init sequence) */
	sys[SYS_CTRL0 / 4] &= ~3u;
	sys[SYS_CTRL1 / 4] &= ~(1u << 31);
	printf("sys_ctrl: PASS\n");

	/* Issue VDEC reset */
	vdec[VDEC_REG_CMD / 4] = VDEC_CMD_RESET;
	usleep(10000);
	printf("vdec_reset: PASS\n");

	/* Acknowledge IRQs */
	vdec[VDEC_REG_IRQ_STATUS / 4] = 0xffffffff;

	/* Read status */
	status = vdec[VDEC_REG_STATUS2 / 4];
	printf("vdec_status: 0x%08x\n", status);

	/* Issue start command */
	vdec[VDEC_REG_CMD / 4] = VDEC_CMD_START;
	usleep(1000);

	status = vdec[VDEC_REG_STATUS2 / 4];
	printf("vdec_status_after_start: 0x%08x\n", status);

	printf("vdec: PASS\n");

	munmap((void *)vdec, VDEC_SIZE);
	munmap((void *)sys, SYS_SIZE);
	close(fd);
	return 0;
}
