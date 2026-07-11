/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef HCGE_NODE_H
#define HCGE_NODE_H

#include <stddef.h>
#include <stdint.h>

#define HCGE_NODE_CONTEXT_WORDS 170u
#define HCGE_NODE_MAX_WORDS 172u

enum hcge_node_group {
	HCGE_GROUP_FUNC,
	HCGE_GROUP_DST_CTX,
	HCGE_GROUP_SRC_CTX,
	HCGE_GROUP_PTN_CTX,
	HCGE_GROUP_MSK_CTX,
	HCGE_GROUP_COLOR,
	HCGE_GROUP_PAINT_TYPE,
	HCGE_GROUP_COLOR_KEY,
	HCGE_GROUP_DST_POS,
	HCGE_GROUP_SRC_POS,
	HCGE_GROUP_PTN_POS,
	HCGE_GROUP_MSK_POS,
	HCGE_GROUP_RESERVED12,
	HCGE_GROUP_CLUT,
	HCGE_GROUP_CLIP,
	HCGE_GROUP_ROP,
	HCGE_GROUP_RESERVED16,
	HCGE_GROUP_DFB_GCOLOR,
	HCGE_GROUP_MATRIX,
	HCGE_GROUP_FILTER,
};

struct hcge_node_context {
	uint32_t word[HCGE_NODE_CONTEXT_WORDS];
};

size_t hcge_node_words(uint32_t group_mask);
int hcge_node_build(const struct hcge_node_context *context,
	uint32_t *node, size_t capacity, size_t *words);

#endif
