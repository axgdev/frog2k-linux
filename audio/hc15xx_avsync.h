/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_AVSYNC_H
#define HC15XX_AVSYNC_H

#include "hc15xx_audio.h"

#include <stdint.h>

enum hc15xx_avsync_status {
	HC15XX_AVSYNC_NORMAL,
	HC15XX_AVSYNC_DROP,
	HC15XX_AVSYNC_HOLD,
	HC15XX_AVSYNC_INVALID,
};

struct hc15xx_avsync {
	struct hc15xx_audio *audio;
	uint32_t audio_update_delay[2];
	uint32_t audio_update_threshold[2];
	uint32_t audio_sync_delay[2];
	uint32_t audio_sync_threshold[2];
	uint32_t audio_valid_threshold[2];
	uint32_t video_update_threshold[2];
	unsigned audio_hold[2];
};

void hc15xx_avsync_init(struct hc15xx_avsync *sync,
	struct hc15xx_audio *audio);
void hc15xx_avsync_audio_update(struct hc15xx_avsync *sync,
	enum hc15xx_stc_id id, uint32_t audio_tick);
void hc15xx_avsync_video_update(struct hc15xx_avsync *sync,
	enum hc15xx_stc_id id, uint32_t video_tick);
enum hc15xx_avsync_status hc15xx_avsync_audio_check(
	const struct hc15xx_avsync *sync, enum hc15xx_stc_id id,
	uint32_t audio_tick);

#endif
