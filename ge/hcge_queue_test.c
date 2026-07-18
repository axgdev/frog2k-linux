/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	hcge_context context;
	static const uint32_t addresses[] = {
		0x10000000, 0x10000004,
		0x10001ffc, 0x10002000, 0xffffffff,
	};
	static const uint32_t virtual_addresses[] = {
		0x71000000, 0x71000004, 0x71001ffc, 0x71002000, 0xffffffff,
	};
	unsigned int i;

	memset(&context, 0, sizeof(context));
	context.cmdq_buf_phyaddr = 0x10000000;
	context.cmdq_buf_vaddr = 0x71000000;
	context.cmdq_buf_size = 0x2000;
	for (i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i)
		printf("v %08x %08x\n", addresses[i],
		       hcge_cmdq_vaddr(&context, addresses[i]));
	for (i = 0; i < sizeof(virtual_addresses) / sizeof(virtual_addresses[0]); ++i)
		printf("p %08x %08x\n", virtual_addresses[i],
		       hcge_cmdq_paddr(&context, virtual_addresses[i]));
	return 0;
}
