/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stddef.h>
#include <stdio.h>

struct hcge_name {
	unsigned int value;
	const char *name;
};

static const struct hcge_name pixel_formats[] = {
	{ HCGE_DSPF_UNKNOWN, "UNKNOWN" },
	{ HCGE_DSPF_ARGB1555, "ARGB1555" },
	{ HCGE_DSPF_RGB16, "RGB16" },
	{ HCGE_DSPF_RGB24, "RGB24" },
	{ HCGE_DSPF_RGB32, "RGB32" },
	{ HCGE_DSPF_ARGB, "ARGB" },
	{ HCGE_DSPF_A8, "A8" },
	{ HCGE_DSPF_LUT8, "LUT8" },
	{ HCGE_DSPF_ARGB4444, "ARGB4444" },
	{ HCGE_DSPF_RGB444, "RGB444" },
	{ HCGE_DSPF_RGB555, "RGB555" },
	{ HCGE_DSPF_BGR555, "BGR555" },
};

static const struct hcge_name blends[] = {
	{ HCGE_DSBF_UNKNOWN, "UNKNOWN" }, { HCGE_DSBF_ZERO, "ZERO" },
	{ HCGE_DSBF_ONE, "ONE" }, { HCGE_DSBF_SRCCOLOR, "SRCCOLOR" },
	{ HCGE_DSBF_INVSRCCOLOR, "INVSRCCOLOR" },
	{ HCGE_DSBF_SRCALPHA, "SRCALPHA" },
	{ HCGE_DSBF_INVSRCALPHA, "INVSRCALPHA" },
	{ HCGE_DSBF_DESTALPHA, "DESTALPHA" },
	{ HCGE_DSBF_INVDESTALPHA, "INVDESTALPHA" },
	{ HCGE_DSBF_DESTCOLOR, "DESTCOLOR" },
	{ HCGE_DSBF_INVDESTCOLOR, "INVDESTCOLOR" },
	{ HCGE_DSBF_SRCALPHASAT, "SRCALPHASAT" },
};

static const struct hcge_name drawing_flags[] = {
	{ HCGE_DSDRAW_BLEND, "BLEND" },
	{ HCGE_DSDRAW_DST_COLORKEY, "DST_COLORKEY" },
	{ HCGE_DSDRAW_SRC_PREMULTIPLY, "SRC_PREMULTIPLY" },
	{ HCGE_DSDRAW_DST_PREMULTIPLY, "DST_PREMULTIPLY" },
	{ HCGE_DSDRAW_DEMULTIPLY, "DEMULTIPLY" },
	{ HCGE_DSDRAW_XOR, "XOR" },
};

static const struct hcge_name blitting_flags[] = {
	{ HCGE_DSBLIT_BLEND_ALPHACHANNEL, "BLEND_ALPHACHANNEL" },
	{ HCGE_DSBLIT_BLEND_COLORALPHA, "BLEND_COLORALPHA" },
	{ HCGE_DSBLIT_COLORIZE, "COLORIZE" },
	{ HCGE_DSBLIT_SRC_COLORKEY, "SRC_COLORKEY" },
	{ HCGE_DSBLIT_DST_COLORKEY, "DST_COLORKEY" },
	{ HCGE_DSBLIT_SRC_PREMULTIPLY, "SRC_PREMULTIPLY" },
	{ HCGE_DSBLIT_DST_PREMULTIPLY, "DST_PREMULTIPLY" },
	{ HCGE_DSBLIT_DEMULTIPLY, "DEMULTIPLY" },
	{ HCGE_DSBLIT_DEINTERLACE, "DEINTERLACE" },
	{ HCGE_DSBLIT_SRC_PREMULTCOLOR, "SRC_PREMULTCOLOR" },
	{ HCGE_DSBLIT_XOR, "XOR" },
	{ HCGE_DSBLIT_INDEX_TRANSLATION, "INDEX_TRANSLATION" },
	{ HCGE_DSBLIT_ROTATE180, "ROTATE180" },
	{ HCGE_DSBLIT_ROTATE90, "ROTATE90" },
	{ HCGE_DSBLIT_ROTATE270, "ROTATE270" },
	{ HCGE_DSBLIT_COLORKEY_PROTECT, "COLORKEY_PROTECT" },
	{ HCGE_DSBLIT_SRC_COLORKEY_EXTENDED, "SRC_COLORKEY_EXTENDED" },
	{ HCGE_DSBLIT_DST_COLORKEY_EXTENDED, "DST_COLORKEY_EXTENDED" },
	{ HCGE_DSBLIT_SRC_MASK_ALPHA, "SRC_MASK_ALPHA" },
	{ HCGE_DSBLIT_SRC_MASK_COLOR, "SRC_MASK_COLOR" },
	{ HCGE_DSBLIT_FLIP_HORIZONTAL, "FLIP_HORIZONTAL" },
	{ HCGE_DSBLIT_FLIP_VERTICAL, "FLIP_VERTICAL" },
	{ HCGE_DSBLIT_ROP, "ROP" },
	{ HCGE_DSBLIT_SRC_COLORMATRIX, "SRC_COLORMATRIX" },
	{ HCGE_DSBLIT_SRC_CONVOLUTION, "SRC_CONVOLUTION" },
	{ HCGE_CUST_DST_COLORKEY, "CUST_DST_COLORKEY" },
	{ HCGE_CUST_SRC_COLORKEY, "CUST_SRC_COLORKEY" },
};

static const char *find_name(const struct hcge_name *table, size_t count,
			     unsigned int value, const char *unknown)
{
	size_t i;

	for (i = 0; i < count; ++i)
		if (table[i].value == value)
			return table[i].name;
	return unknown;
}

static const char *flags_to_string(const struct hcge_name *table, size_t count,
				   unsigned int flags, const char *none,
				   char *buffer, size_t size)
{
	size_t i, used = 0;

	if (!flags)
		return none;
	buffer[0] = '\0';
	for (i = 0; i < count; ++i) {
		int written;
		if (!(flags & table[i].value))
			continue;
		written = snprintf(buffer + used, size - used, "%s%s",
				   used ? " | " : "", table[i].name);
		if (written < 0 || (size_t)written >= size - used)
			break;
		used += (size_t)written;
	}
	return buffer;
}

const char *hcge_pixelformat_name(HCGESurfacePixelFormat format)
{
	return find_name(pixel_formats, sizeof(pixel_formats) / sizeof(pixel_formats[0]),
			 format, "unknown pixelformat");
}

const char *hcge_blend_to_string(HCGESurfaceBlendFunction function)
{
	return find_name(blends, sizeof(blends) / sizeof(blends[0]), function,
			 "UNKNOWN");
}

const char *hcge_drawingflags_to_string(HCGESurfaceDrawingFlags flags)
{
	static char buffer[512];
	return flags_to_string(drawing_flags,
			       sizeof(drawing_flags) / sizeof(drawing_flags[0]), flags,
			       "NOFX", buffer, sizeof(buffer));
}

const char *hcge_blittingflags_to_string(HCGESurfaceBlittingFlags flags)
{
	static char buffer[2048];
	return flags_to_string(blitting_flags,
			       sizeof(blitting_flags) / sizeof(blitting_flags[0]), flags,
			       "NOFX", buffer, sizeof(buffer));
}

const char *hcge_state_modify_flags(HCGEStateModificationFlags flags)
{
	static char buffer[128];
	static const struct hcge_name modifications[] = {
		/* The vendor ABI's value 4 predates the reduced public enum. */
		{ HCGE_SMF_CLIP, "COLORIZE" },
	};
	return flags_to_string(modifications,
			       sizeof(modifications) / sizeof(modifications[0]), flags,
			       "NONE", buffer, sizeof(buffer));
}
