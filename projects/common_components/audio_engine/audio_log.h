#ifndef __AUDIO_LOG_H__
#define __AUDIO_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif


#define TAG "aud_eng"
#define AUDE_LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define AUDE_LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define AUDE_LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define AUDE_LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_LOG_H__ */
