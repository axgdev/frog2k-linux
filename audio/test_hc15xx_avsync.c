/* SPDX-License-Identifier: MIT */
#include "hc15xx_avsync.h"

#include <assert.h>
#include <string.h>

struct fake {
	uint32_t regs[0x100 / 4];
};

static uint32_t fake_read(void *cookie, uintptr_t address)
{
	return ((struct fake *)cookie)->regs[address / 4];
}

static void fake_write(void *cookie, uintptr_t address, uint32_t value)
{
	((struct fake *)cookie)->regs[address / 4] = value;
}

int main(void)
{
	struct fake fake;
	struct hc15xx_audio audio;
	struct hc15xx_avsync sync;
	struct hc15xx_audio_io io = {
		.cookie = &fake,
		.read32 = fake_read,
		.write32 = fake_write,
	};

	memset(&fake, 0, sizeof(fake));
	hc15xx_audio_init(&audio, &io, 0, 0, 0);
	hc15xx_avsync_init(&sync, &audio);

	/* The recovered vendor defaults are 2, 100, 180 and 120000 ms. */
	assert(sync.audio_update_threshold[0] == 90);
	assert(sync.video_update_threshold[0] == 4500);
	assert(sync.audio_sync_threshold[0] == 8100);
	assert(sync.audio_valid_threshold[0] == 5400000);

	hc15xx_audio_set_stc_tick(&audio, HC15XX_STC0, 1000);
	hc15xx_avsync_audio_update(&sync, HC15XX_STC0, 1089);
	assert(hc15xx_audio_stc_tick(&audio, HC15XX_STC0) == 1000);
	hc15xx_avsync_audio_update(&sync, HC15XX_STC0, 1090);
	assert(hc15xx_audio_stc_tick(&audio, HC15XX_STC0) == 1090);

	hc15xx_avsync_video_update(&sync, HC15XX_STC0, 5589);
	assert(hc15xx_audio_stc_tick(&audio, HC15XX_STC0) == 1090);
	hc15xx_avsync_video_update(&sync, HC15XX_STC0, 5590);
	assert(hc15xx_audio_stc_tick(&audio, HC15XX_STC0) == 5590);

	assert(hc15xx_avsync_audio_check(&sync, HC15XX_STC0, 5590) ==
		HC15XX_AVSYNC_NORMAL);
	assert(hc15xx_avsync_audio_check(&sync, HC15XX_STC0, 13691) ==
		HC15XX_AVSYNC_HOLD);
	assert(hc15xx_avsync_audio_check(&sync, HC15XX_STC0, 0) ==
		HC15XX_AVSYNC_NORMAL);
	hc15xx_audio_set_stc_tick(&audio, HC15XX_STC0, 20000);
	assert(hc15xx_avsync_audio_check(&sync, HC15XX_STC0, 1000) ==
		HC15XX_AVSYNC_DROP);
	assert(hc15xx_avsync_audio_check(&sync, HC15XX_STC0, 6000000) ==
		HC15XX_AVSYNC_INVALID);
	return 0;
}
