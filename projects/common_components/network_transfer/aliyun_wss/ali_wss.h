#ifndef __ALI_WSS_H__
#define __ALI_WSS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "aud_intf.h"
#include "audio_engine.h"
#include "bk_websocket_client.h"
#include "lib_c_sdk.h"
#include "media_app.h"

typedef struct
{
  transport clientHandler;
  int disconnecting_count; //0: running 1:disconnecting
  audio_info_t audio_info;
  beken_timer_t dummy_play_tmr;
  beken_thread_t dummy_record_thread;
  uint8_t dummy_record_running;
  uint8_t dummy_record_task;
  uint8_t dummy_play_running;
} DummyHandle;

/**
 * @brief text info
 */
typedef struct {
    uint8_t text_type;
    char *text_data;
    const char *devId;
    const char *nfcId;
    const char *input_audio_format;
    int input_audio_rate;
    const char *output_audio_format;
    int output_audio_rate;
    const char *user_text;
    const char *mode;
    bool playback_complete;
    bool multi_turn_mode;
} text_info_t;
void dummy_play_stop(DummyHandle *handle);
void dummy_play_start(DummyHandle *handle);
void dummy_record_start(DummyHandle *handle);
int dummy_record_work(DummyHandle *handle);
int dummy_record_data_send(unsigned char *data_ptr, unsigned int data_len);
DummyHandle *dummy_wss_init(audio_info_t *audio_info);
void dummy_wss_destroy(DummyHandle *handle);	
DummyHandle *get_client_handle(void);
void dummy_lcd_show(void *tips, void *data);
void ali_wss_event_handler(void* event_handler_arg, char *event_base, int32_t event_id, void* event_data);

#ifdef __cplusplus
}
#endif
#endif /* __AGORA_RTC_H__ */
