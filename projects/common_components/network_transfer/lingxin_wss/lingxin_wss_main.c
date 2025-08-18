#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/shell_task.h>
#include <components/event.h>
#include <components/netif_types.h>
#include "bk_rtos_debug.h"
#include "audio_transfer.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#include <driver/media_types.h>
#include <driver/lcd.h>
#include <modules/wifi.h>
#include "modules/wifi_types.h"
#include "media_app.h"
#include "lcd_act.h"
#include "components/bk_uid.h"
#include "aud_tras.h"
#include "media_app.h"
#include "bk_smart_config.h"
#include "app_event.h"
#include "cJSON.h"
#include <modules/audio_process.h>
#include "audio_engine.h"
#include "video_engine.h"
#include "app_main.h"
#include "app_event.h"


#define TAG "LINGXIN_WSS_MAIN"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP


#if CONFIG_USE_G722_CODEC || CONFIG_USE_OPUS_CODEC
#define AUDIO_SAMP_RATE         (16000)
#else
#define AUDIO_SAMP_RATE         (8000)
#endif
#define AEC_ENABLE              (1)

bool g_connected_flag = false;
extern bool smart_config_running;

/* call this api when wifi autoconnect */
void beken_auto_run(uint8_t reset)
{
#if !CONFIG_ENABLE_LINGXIN_COSTOM_AUTH
	if (bk_sconf_wakeup_agent(reset)) {
		LOGW("bk_sconf_wakeup_agent failed");
		return;
	}
#endif
	g_connected_flag = true;
	network_reconnect_stop_timeout_check();
	app_event_send_msg(APP_EVT_AGENT_JOINED, 0);
	smart_config_running = false;

}

