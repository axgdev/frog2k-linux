/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "hcge_node.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void hcge_construct_nodes(void *context, uint32_t **output);

int main(void)
{
	struct hcge_node_context node_context;
	unsigned char vendor_context[0x420];
	uint32_t vendor_node[HCGE_NODE_MAX_WORDS];
	uint32_t source_node[HCGE_NODE_MAX_WORDS];
	uint32_t *vendor_output = vendor_node;
	size_t source_words;
	size_t vendor_words;
	unsigned int i;

	memset(vendor_context, 0, sizeof(vendor_context));
	for (i = 0; i < HCGE_NODE_CONTEXT_WORDS; i++)
		node_context.word[i] = 0x10000000u + i;
	node_context.word[0] = 0x000fffffu;
	*(struct hcge_node_context **)(vendor_context + 0x150) = &node_context;
	hcge_construct_nodes(vendor_context, &vendor_output);
	vendor_words = (size_t)(vendor_output - vendor_node);
	if (hcge_node_build(&node_context, source_node, HCGE_NODE_MAX_WORDS,
			    &source_words))
		return 1;
	if (vendor_words != source_words ||
	    memcmp(vendor_node, source_node, source_words * sizeof(uint32_t))) {
		fprintf(stderr, "vendor/source mismatch: %lu/%lu words\n",
			(unsigned long)vendor_words, (unsigned long)source_words);
		for (i = 0; i < vendor_words && i < source_words; i++)
			if (vendor_node[i] != source_node[i]) {
				fprintf(stderr, "word %u: %08x != %08x\n", i,
					vendor_node[i], source_node[i]);
				break;
			}
		return 1;
	}
	puts("hcge vendor/source node serializer: identical");
	return 0;
}
