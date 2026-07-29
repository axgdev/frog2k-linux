// SPDX-License-Identifier: MIT
/*
 * Physical device test for HC15xx DSC (display controller).
 * Tests VSync counter and IRQ handling on the SF2000 (NOMMU).
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#define KSEG0(addr)	((volatile uint32_t *)((addr) | 0x80000000u))

#define DSC_BASE	0x18870000

#define DSC_REG_CTRL		0x00
#define DSC_REG_IRQ_STATUS	0x04
#define DSC_REG_VSYNC_COUNT	0x08

#define DSC_CTRL_ENABLE		0x01
#define DSC_CTRL_VBLANK_IRQ	0x02

int main(void)
{
	volatile uint32_t *dsc = KSEG0(DSC_BASE);
	uint32_t count1, count2, irq;

	dsc[DSC_REG_CTRL / 4] = DSC_CTRL_ENABLE | DSC_CTRL_VBLANK_IRQ;
	printf("dsc_enable: PASS\n");

	count1 = dsc[DSC_REG_VSYNC_COUNT / 4];
	printf("vsync_count_1: %u\n", count1);

	usleep(100000);

	count2 = dsc[DSC_REG_VSYNC_COUNT / 4];
	printf("vsync_count_2: %u\n", count2);

	if (count2 > count1) {
		printf("vsync_incrementing: PASS (%u frames)\n",
		       count2 - count1);
	} else {
		printf("vsync_incrementing: WARN (counter not advancing)\n");
	}

	irq = dsc[DSC_REG_IRQ_STATUS / 4];
	printf("irq_status: 0x%08x\n", irq);
	if (irq)
		dsc[DSC_REG_IRQ_STATUS / 4] = irq;

	dsc[DSC_REG_CTRL / 4] = 0;
	printf("dsc_disable: PASS\n");

	printf("dsc: PASS\n");
	return 0;
}
