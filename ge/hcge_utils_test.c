/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <stdio.h>

int main(void)
{
	unsigned int i;

	for (i = 0; i <= 30; ++i)
		printf("pixel %u %s\n", i,
		       hcge_pixelformat_name((HCGESurfacePixelFormat)i));
	for (i = HCGE_DSBF_UNKNOWN; i <= HCGE_DSBF_SRCALPHASAT; ++i)
		printf("blend %u %s\n", i,
		       hcge_blend_to_string((HCGESurfaceBlendFunction)i));
	printf("draw0 %s\n", hcge_drawingflags_to_string(HCGE_DSDRAW_NOFX));
	printf("drawfx %s\n", hcge_drawingflags_to_string(
		HCGE_DSDRAW_BLEND | HCGE_DSDRAW_XOR));
	printf("blit0 %s\n", hcge_blittingflags_to_string(HCGE_DSBLIT_NOFX));
	printf("blitfx %s\n", hcge_blittingflags_to_string(
		HCGE_DSBLIT_BLEND_ALPHACHANNEL | HCGE_DSBLIT_ROTATE90 |
		HCGE_DSBLIT_FLIP_VERTICAL));
	printf("mod0 %s\n", hcge_state_modify_flags(HCGE_SMF_NONE));
	printf("modclip %s\n", hcge_state_modify_flags(HCGE_SMF_CLIP));
	return 0;
}
