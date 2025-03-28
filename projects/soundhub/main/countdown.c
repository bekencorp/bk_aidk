#include <os/os.h>
#include <os/mem.h>
#include "countdown.h"
#include "bk_factory_config.h"
#include <components/log.h>

#define TAG "count_down"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)



beken2_timer_t g_countdown_timer;

void CountdownCallback()
{
    LOGI("Standy time is up, start enter deepsleep\r\n");
	extern void bk_reboot_ex(uint32_t reset_reason);
    bk_reboot_ex(RESET_SOURCE_FORCE_DEEPSLEEP);
}

void start_countdown(uint32_t time_ms)
{
    bk_err_t result;

	bk_config_sync_flash();

	if (g_countdown_timer.handle == NULL)
	{
		result = rtos_init_oneshot_timer(&g_countdown_timer, time_ms, CountdownCallback, NULL, NULL);
		if(kNoErr != result)
		{
			LOGI("rtos_init_timer fail\r\n");
			return;
		}

		result = rtos_start_oneshot_timer(&g_countdown_timer);
		if(kNoErr != result)
		{
			LOGI("rtos_start_timer fail\r\n");
			return;
		}
	} else {
		rtos_oneshot_reload_timer(&g_countdown_timer);
	}
    
}

void stop_countdown()
{
    
	bk_err_t ret;

	if (rtos_is_oneshot_timer_init(&g_countdown_timer)) {
		if (rtos_is_oneshot_timer_running(&g_countdown_timer)) {
			ret = rtos_stop_oneshot_timer(&g_countdown_timer);
			if(kNoErr != ret)
			{
				LOGI("rtos_stop_timer fail\r\n");
				return;
			}
		}

		ret = rtos_deinit_oneshot_timer(&g_countdown_timer);
		if(kNoErr != ret)
		{
			LOGI("rtos_deinit_timer fail\r\n");
			return;
		}

	}
}