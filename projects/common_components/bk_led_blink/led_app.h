#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <led_blink.h>
//red:if high priority conflicts with low-priority, should stay at high priority states
enum {
	WARNING_PROVIOSION_FAIL,	//LED_FAST_BLINK_RED
	WARNING_WIFI_FAIL,		//LED_FAST_BLINK_RED
	WARNING_RTC_CONNECT_LOST,	//LED_FAST_BLINK_RED
	WARNING_AGENT_OFFLINE,	//LED_FAST_BLINK_RED
  WARNING_AGENT_AGENT_START_FAIL,  //LED_FAST_BLINK_RED

	WARNING_LOW_BATTERY,	//LED_SLOW_BLINK_RED
};
#define HIGH_PRIORITY_WARNING_MASK ((1<<WARNING_PROVIOSION_FAIL) | (1<<WARNING_WIFI_FAIL) | (1<<WARNING_RTC_CONNECT_LOST) | (1<<WARNING_AGENT_OFFLINE) | (1<<WARNING_AGENT_AGENT_START_FAIL))
#define LOW_PRIORITY_WARNING_MASK ((1<<WARNING_LOW_BATTERY))
#define AI_RTC_CONNECT_LOST_FAIL   (1<<WARNING_RTC_CONNECT_LOST)
#define AI_AGENT_OFFLINE_FAIL      (1<<WARNING_AGENT_OFFLINE)
#define WIFI_FAIL  (1<<WARNING_WIFI_FAIL)
//green led, or red/green alternate led
enum {
	INDICATES_WIFI_RECONNECT,	//LED_FAST_BLINK_GREEN
	INDICATES_PROVISIONING,		//LED_REG_GREEN_ALTERNATE
	INDICATES_POWER_ON,  		//LED_ON_GREEN  default at power-on state, so green is always on
	INDICATES_STANDBY,			//LED_SLOW_BLINK_GREEN
	INDICATES_AGENT_CONNECT,	//LED_FAST_BLINK_GREEN at WIFI connection but no access to the AI AGENT
};


void led_blink(uint32_t* warning_state, uint32_t indicates_state);



#ifdef __cplusplus
}
#endif