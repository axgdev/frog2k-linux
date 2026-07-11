/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "hcge_node.h"

struct group_layout {
	uint16_t context_word;
	uint16_t words;
	uint16_t second_word;
};

static const struct group_layout group_layouts[20] = {
	[HCGE_GROUP_FUNC]       = { 1,   1 },
	[HCGE_GROUP_DST_CTX]    = { 2,   2, 4 },
	[HCGE_GROUP_SRC_CTX]    = { 7,   2, 9 },
	[HCGE_GROUP_PTN_CTX]    = { 11,  2, 13 },
	[HCGE_GROUP_MSK_CTX]    = { 16,  2, 18 },
	[HCGE_GROUP_COLOR]      = { 28,  3 },
	[HCGE_GROUP_PAINT_TYPE] = { 33,  1 },
	[HCGE_GROUP_COLOR_KEY]  = { 26,  2 },
	[HCGE_GROUP_DST_POS]    = { 5,   2 },
	[HCGE_GROUP_SRC_POS]    = { 10,  1 },
	[HCGE_GROUP_PTN_POS]    = { 14,  2 },
	[HCGE_GROUP_MSK_POS]    = { 19,  2 },
	[HCGE_GROUP_RESERVED12] = { 0,   0 },
	[HCGE_GROUP_CLUT]       = { 21,  2 },
	[HCGE_GROUP_CLIP]       = { 23,  2 },
	[HCGE_GROUP_ROP]        = { 25,  1 },
	[HCGE_GROUP_RESERVED16] = { 0,   0 },
	[HCGE_GROUP_DFB_GCOLOR] = { 34,  1 },
	[HCGE_GROUP_MATRIX]     = { 35,  7 },
	[HCGE_GROUP_FILTER]     = { 42, 128 },
};

size_t hcge_node_words(uint32_t group_mask)
{
	size_t words = 1;
	unsigned int group;

	for (group = 0; group < 20; group++)
		if (group_mask & (1u << group))
			words += group_layouts[group].words;
	return words;
}

int hcge_node_build(const struct hcge_node_context *context,
	uint32_t *node, size_t capacity, size_t *words)
{
	uint32_t mask;
	size_t required;
	size_t output = 0;
	unsigned int group;

	if (!context || !node || !words)
		return -1;
	mask = context->word[0];
	required = hcge_node_words(mask);
	for (group = 0; group < 4; group++) {
		static const uint8_t base_words[4] = { 2, 7, 11, 16 };

		if (context->word[base_words[group]] & 0xf0000000u)
			required += 2;
	}
	if (required > capacity)
		return -1;

	for (group = 0; group < 4; group++) {
		static const uint8_t base_words[4] = { 2, 7, 11, 16 };
		unsigned int base = base_words[group];
		uint32_t high = context->word[base] >> 28;

		if (!high)
			continue;
		node[output++] = ((high + 0x2fu) << 10) | 0x81000001u;
		node[output++] = context->word[base + 1];
	}
	node[output++] = mask | 0x02000000u;
	for (group = 0; group < 20; group++) {
		const struct group_layout *layout = &group_layouts[group];
		unsigned int index;

		if (!(mask & (1u << group)))
			continue;
		for (index = 0; index < layout->words; index++) {
			unsigned int context_word = layout->context_word + index;

			if (index == 1 && layout->second_word)
				context_word = layout->second_word;
			node[output++] = context->word[context_word];
		}
	}
	*words = output;
	return 0;
}
