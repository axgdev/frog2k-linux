/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "hcge_node.h"

#include <stdio.h>

int main(void)
{
	struct hcge_node_context context = { { 0 } };
	uint32_t node[HCGE_NODE_MAX_WORDS];
	size_t words;
	unsigned int i;

	for (i = 0; i < HCGE_NODE_CONTEXT_WORDS; i++)
		context.word[i] = 0x10000000u + i;
	context.word[0] = 0x000fffffu;
	if (hcge_node_build(&context, node, HCGE_NODE_MAX_WORDS, &words))
		return 1;
	if (words != HCGE_NODE_MAX_WORDS || node[8] != 0x020fffffu)
		return 1;
	if (node[9] != context.word[1] || node[10] != context.word[2] ||
	    node[11] != context.word[4] || node[words - 1] != context.word[169])
		return 1;
	if (hcge_node_build(&context, node, words - 1, &words) == 0)
		return 1;
	puts("hcge node serializer: OK");
	return 0;
}
