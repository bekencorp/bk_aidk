#include <led_app.h>
#include <bat_monitor.h>
#include <components/log.h>

#if CONFIG_LED_BLINK
#define TAG "led_app"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

void led_blink(uint32_t* warning_state, uint32_t indicates_state)
{
	static uint32_t last_warning_state = 0;
	static uint32_t last_indicates_state = (1<<INDICATES_POWER_ON);

    //indicates
    if(indicates_state & (1<<INDICATES_PROVISIONING))
    {
        led_app_set(LED_REG_GREEN_ALTERNATE, LED_LAST_FOREVER);
    }else if (HIGH_PRIORITY_WARNING_MASK & (*warning_state))
    {
        led_app_set(LED_OFF_GREEN, 0);
    }else{
        if(indicates_state & ((1<<INDICATES_WIFI_RECONNECT) | (1<<INDICATES_AGENT_CONNECT)))
		{
			led_app_set(LED_FAST_BLINK_GREEN, LED_LAST_FOREVER);
		}else if(indicates_state & (1<<INDICATES_STANDBY))
		{
			led_app_set(LED_SLOW_BLINK_GREEN, LED_LAST_FOREVER);
		}else
        {
        led_app_set(LED_OFF_GREEN, 0);
        }
    }

    //warning
    if (indicates_state & (1<<INDICATES_PROVISIONING))
    {
        ;
    }else if(HIGH_PRIORITY_WARNING_MASK & (*warning_state)){
        led_app_set(LED_FAST_BLINK_RED, LED_LAST_FOREVER);
    }else if(LOW_PRIORITY_WARNING_MASK & (*warning_state)){
        led_app_set(LED_SLOW_BLINK_RED, LOW_VOLTAGE_BLINK_TIME);
        *warning_state = *warning_state & ~(1<<WARNING_LOW_BATTERY);
    }else{
        led_app_set(LED_OFF_RED, 0);
    }

    if(indicates_state != last_indicates_state)
	{
		LOGI("indicate=%d,last_indicat=%d,warning_state=%d\r\n", indicates_state, last_indicates_state, *warning_state);
		last_indicates_state = indicates_state;
	}

	if(*warning_state != last_warning_state)
	{
		LOGI("warning=%d,last_warning=%d,indicate=%d\r\n", *warning_state, last_warning_state, indicates_state);
		last_warning_state = *warning_state;
	}

}
#endif