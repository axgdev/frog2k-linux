/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static unsigned int ioctl_calls;

int __wrap_ioctl(int fd, unsigned long request, ...)
{
	(void)fd;
	(void)request;
	ioctl_calls++;
	return 0;
}

int main(void)
{
	hcge_context context;
	hcge_batch batch;
	uint32_t storage[16];
	uint32_t first[] = { 1, 2, 3 };
	uint32_t second[] = { 4, 5, 6, 7 };
	int ret;

	memset(&context, 0, sizeof(context));
	context.ge_fd = 3;
	context.cmdq_buf_size = sizeof(storage);
	ret = hcge_batch_begin(&context, &batch, storage, 16);
	ret |= hcge_linux_submit(&context, first, 3);
	ret |= hcge_linux_submit(&context, second, 4);
	ret |= hcge_batch_end(&batch, 1);
	printf("ret=%d calls=%u words=%u data=%u,%u,%u,%u,%u,%u,%u\n",
	       ret, ioctl_calls, batch.words, storage[0], storage[1], storage[2],
	       storage[3], storage[4], storage[5], storage[6]);
	return ret != 0 || ioctl_calls != 2 || batch.words != 7;
}
