/* SPDX-License-Identifier: MIT */
#include "hc15xx_avsync.h"

#include <string.h>

enum {
	AUDIO_UPDATE_THRESHOLD = 2 * HC15XX_STC_TICKS_PER_MS,
	AUDIO_SYNC_THRESHOLD = 180 * HC15XX_STC_TICKS_PER_MS,
	AUDIO_VALID_THRESHOLD = 120000 * HC15XX_STC_TICKS_PER_MS,
	VIDEO_UPDATE_THRESHOLD = 100 * HC15XX_STC_TICKS_PER_MS,
};

static int valid_id(enum hc15xx_stc_id id)
{
	return id == HC15XX_STC0 || id == HC15XX_STC1;
}

static uint32_t tick_distance(uint32_t left, uint32_t right)
{
	return left >= right ? left - right : right - left;
}

void hc15xx_avsync_init(struct hc15xx_avsync *sync,
	struct hc15xx_audio *audio)
{
	unsigned id;

	memset(sync, 0, sizeof(*sync));
	sync->audio = audio;
	for (id = 0; id < 2; id++) {
		sync->audio_update_threshold[id] = AUDIO_UPDATE_THRESHOLD;
		sync->audio_sync_threshold[id] = AUDIO_SYNC_THRESHOLD;
		sync->audio_valid_threshold[id] = AUDIO_VALID_THRESHOLD;
		sync->video_update_threshold[id] = VIDEO_UPDATE_THRESHOLD;
		sync->audio_hold[id] = 1;
	}
}

void hc15xx_avsync_audio_update(struct hc15xx_avsync *sync,
	enum hc15xx_stc_id id, uint32_t audio_tick)
{
	uint32_t delayed;
	uint32_t current;

	if (!sync || !sync->audio || !valid_id(id))
		return;
	delayed = audio_tick > sync->audio_update_delay[id] ?
		audio_tick - sync->audio_update_delay[id] : 0;
	current = hc15xx_audio_stc_tick(sync->audio, id);
	if (tick_distance(current, delayed) >=
			sync->audio_update_threshold[id])
		hc15xx_audio_set_stc_tick(sync->audio, id, delayed);
}

void hc15xx_avsync_video_update(struct hc15xx_avsync *sync,
	enum hc15xx_stc_id id, uint32_t video_tick)
{
	uint32_t current;

	if (!sync || !sync->audio || !valid_id(id))
		return;
	current = hc15xx_audio_stc_tick(sync->audio, id);
	if (tick_distance(current, video_tick) >=
			sync->video_update_threshold[id])
		hc15xx_audio_set_stc_tick(sync->audio, id, video_tick);
}

enum hc15xx_avsync_status hc15xx_avsync_audio_check(
	const struct hc15xx_avsync *sync, enum hc15xx_stc_id id,
	uint32_t audio_tick)
{
	uint32_t stc;
	uint32_t distance;

	if (!sync || !sync->audio || !valid_id(id))
		return HC15XX_AVSYNC_INVALID;
	stc = hc15xx_audio_stc_tick(sync->audio, id) +
		sync->audio_sync_delay[id];
	distance = tick_distance(stc, audio_tick);
	if (distance > sync->audio_valid_threshold[id])
		return HC15XX_AVSYNC_INVALID;
	if (stc > audio_tick &&
			stc - audio_tick > sync->audio_sync_threshold[id])
		return HC15XX_AVSYNC_DROP;
	if (audio_tick > stc &&
			audio_tick - stc > sync->audio_sync_threshold[id])
		return sync->audio_hold[id] ?
			HC15XX_AVSYNC_HOLD : HC15XX_AVSYNC_NORMAL;
	return HC15XX_AVSYNC_NORMAL;
}
