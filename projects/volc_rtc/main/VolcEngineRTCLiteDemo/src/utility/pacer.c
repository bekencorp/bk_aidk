
#include <stdlib.h>
#include <sys/time.h>
#include "utility.h"
#include "pacer.h"

typedef struct {
	uint32_t audio_send_interval_ms;
	uint32_t video_send_interval_ms;
	int64_t audio_predict_time_ms;
	int64_t video_predict_time_ms;
} pacer_t;

void *pacer_create(uint32_t audio_send_interval_ms, uint32_t video_send_interval_ms)
{
	pacer_t *pacer = (pacer_t *)malloc(sizeof(pacer_t));

	pacer->audio_send_interval_ms = audio_send_interval_ms;
	pacer->video_send_interval_ms = video_send_interval_ms;
	pacer->audio_predict_time_ms = 0;
	pacer->video_predict_time_ms = 0;

	return pacer;
}

void pacer_destroy(void *pacer)
{
	if (pacer) {
		free(pacer);
	}
}

bool is_time_to_send_audio(void *pacer)
{
	pacer_t *pc = pacer;
	int64_t cur_time_ms = util_get_time_ms();

	if (pc->audio_predict_time_ms == 0) {
		pc->audio_predict_time_ms = cur_time_ms;
	}

	if (cur_time_ms >= pc->audio_predict_time_ms) {
		pc->audio_predict_time_ms += pc->audio_send_interval_ms;
		return true;
	}

	return false;
}

bool is_time_to_send_video(void *pacer)
{
	pacer_t *pc = pacer;
	int64_t cur_time_ms = util_get_time_ms();

	if (pc->video_predict_time_ms == 0) {
		pc->video_predict_time_ms = cur_time_ms;
	}

	if (cur_time_ms >= pc->video_predict_time_ms) {
		pc->video_predict_time_ms += pc->video_send_interval_ms;
		return true;
	}

	return false;
}

void wait_before_next_send(void *pacer)
{
	pacer_t *pc = pacer;
	int64_t sleep_ms = 0;
	int64_t cur_time_ms = util_get_time_ms();

	if (pc->audio_predict_time_ms > 0 && pc->audio_predict_time_ms <= pc->video_predict_time_ms) {
		sleep_ms = pc->audio_predict_time_ms - cur_time_ms;
	} else {
		sleep_ms = pc->video_predict_time_ms - cur_time_ms;
	}

	if (sleep_ms < 0) {
		sleep_ms = 0;
	}

	util_sleep_ms(sleep_ms);
}