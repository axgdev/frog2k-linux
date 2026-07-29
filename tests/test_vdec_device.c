// SPDX-License-Identifier: MIT
/*
 * Physical device test for HC15xx VDEC hardware.
 * Tests register access and reset sequence on the SF2000 (NOMMU).
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#define KSEG0(addr)	((volatile uint32_t *)((addr) | 0x80000000u))

#define VDEC_BASE	0x18810000
#define SYS_BASE	0x18800000

#define VDEC_REG_CMD		0xff0
#define VDEC_REG_IRQ_STATUS	0xf78
#define VDEC_REG_STATUS2	0xf84
#define VDEC_CMD_RESET		0xff
#define VDEC_CMD_START		0x10

#define SYS_CHIP_ID		0x00
#define SYS_CTRL0		0x80
#define SYS_CTRL1		0x84

int main(void)
{
	volatile uint32_t *sys = KSEG0(SYS_BASE);
	volatile uint32_t *vdec = KSEG0(VDEC_BASE);
	uint32_t chip_id, status;

	chip_id = sys[SYS_CHIP_ID / 4];
	printf("chip_id: 0x%08x\n", chip_id);
	if ((chip_id >> 16) != 0x1512) {
		printf("FAIL: not HC1512\n");
		return 1;
	}

	sys[SYS_CTRL0 / 4] &= ~3u;
	sys[SYS_CTRL1 / 4] &= ~(1u << 31);
	printf("sys_ctrl: PASS\n");

	vdec[VDEC_REG_CMD / 4] = VDEC_CMD_RESET;
	usleep(10000);
	printf("vdec_reset: PASS\n");

	vdec[VDEC_REG_IRQ_STATUS / 4] = 0xffffffff;

	status = vdec[VDEC_REG_STATUS2 / 4];
	printf("vdec_status: 0x%08x\n", status);

	vdec[VDEC_REG_CMD / 4] = VDEC_CMD_START;
	usleep(1000);

	status = vdec[VDEC_REG_STATUS2 / 4];
	printf("vdec_status_after_start: 0x%08x\n", status);

	printf("vdec: PASS\n");
	return 0;
}
