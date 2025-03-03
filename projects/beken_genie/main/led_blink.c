#include "bk_gpio.h"
#include <os/os.h>
#include <os/mem.h>
#include <common/bk_kernel_err.h>
#include <driver/gpio.h>
#include <driver/hal/hal_gpio_types.h>
#include "gpio_driver.h"
#include <led_blink.h>

#define BTI1_MASK (1<<1)

beken_timer_t g_led_timer;
volatile uint8_t s_led_blink_enable = 0;
static uint8_t is_led_first_enter = 1;
static uint8_t s_led_id = 0;
static uint8_t timer_initialized = 0;

void gpio_toggle(uint32_t gpio_id){
    if (is_led_first_enter){
        bk_gpio_set_value(s_led_id,0x2);
        is_led_first_enter = 0;
    }else{
        uint32_t current_value = bk_gpio_get_value(gpio_id);
	    current_value ^= BTI1_MASK;
		bk_gpio_set_value(s_led_id,current_value);
    }   
}

void vLedCallback(void *param1){

    if(s_led_blink_enable){
        gpio_toggle(s_led_id); 
    }
}

int led_service_init(uint8_t led_id, uint32_t blink_interval_ms) {

	if (timer_initialized)
	{
		os_printf("led timer has inited \r\n");
		return 0;
	}
	
    if (rtos_init_timer(&g_led_timer, blink_interval_ms, vLedCallback, NULL) != kNoErr) {
		os_printf("rtos_init_timer fail\r\n");
        return -1; 
    }
    s_led_id = led_id;
	timer_initialized = 1;
    return 0;
}


void led_set_mode(uint8_t led_id, LedMode mode) {
    if (led_id != s_led_id) return; 
    
    switch (mode) {
        case LED_MODE_OFF:
            s_led_blink_enable = 0;
            rtos_stop_timer(&g_led_timer);
			bk_gpio_set_value(s_led_id,0x0);
            break;
        case LED_MODE_ON:
            s_led_blink_enable = 0;
          	rtos_stop_timer(&g_led_timer);
			bk_gpio_set_value(s_led_id,0x2);
            break;
        case LED_MODE_BLINK:
            s_led_blink_enable = 1;
            rtos_start_timer(&g_led_timer);
            break;
        default:
            // 处理未支持模式
            break;
    }
}