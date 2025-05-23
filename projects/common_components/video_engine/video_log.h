#ifndef __VIDEO_LOG_H__
#define __VIDEO_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif


#define TAG "vdo_eng"
#define VDOE_LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define VDOE_LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define VDOE_LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define VDOE_LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_LOG_H__ */
