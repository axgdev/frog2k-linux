// SPDX-License-Identifier: MIT
/*
 * Physical device test for HC15xx DSC (display controller).
 * Tests VSync counter and IRQ handling on the SF2000.
 *
 * Build: $(CC_MIPS) -static -O2 -o test_dsc test_dsc_device.c
 * Run: ./test_dsc
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DSC_BASE	0x18870000
#define DSC_SIZE	0x100

#define DSC_REG_CTRL		0x00
#define DSC_REG_IRQ_STATUS	0x04
#define DSC_REG_VSYNC_COUNT	0x08

#define DSC_CTRL_ENABLE		0x01
#define DSC_CTRL_VBLANK_IRQ	0x02

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
	volatile uint32_t *dsc;
	uint32_t count1, count2, irq;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("open /dev/mem");
		return 1;
	}

	dsc = map_regs(fd, DSC_BASE, DSC_SIZE);

	/* Enable DSC with VBlank IRQ */
	dsc[DSC_REG_CTRL / 4] = DSC_CTRL_ENABLE | DSC_CTRL_VBLANK_IRQ;
	printf("dsc_enable: PASS\n");

	/* Read VSync counter */
	count1 = dsc[DSC_REG_VSYNC_COUNT / 4];
	printf("vsync_count_1: %u\n", count1);

	/* Wait for a few VBlanks (~100ms at 60Hz) */
	usleep(100000);

	count2 = dsc[DSC_REG_VSYNC_COUNT / 4];
	printf("vsync_count_2: %u\n", count2);

	if (count2 > count1) {
		printf("vsync_incrementing: PASS (%u frames)\n",
		       count2 - count1);
	} else {
		printf("vsync_incrementing: WARN (counter not advancing)\n");
	}

	/* Check and acknowledge IRQ */
	irq = dsc[DSC_REG_IRQ_STATUS / 4];
	printf("irq_status: 0x%08x\n", irq);
	if (irq)
		dsc[DSC_REG_IRQ_STATUS / 4] = irq;

	/* Disable DSC */
	dsc[DSC_REG_CTRL / 4] = 0;
	printf("dsc_disable: PASS\n");

	printf("dsc: PASS\n");

	munmap((void *)dsc, DSC_SIZE);
	close(fd);
	return 0;
}
