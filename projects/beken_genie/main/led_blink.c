#include "bk_gpio.h"
#include <os/os.h>
#include <os/mem.h>
#include <common/bk_kernel_err.h>
#include <driver/gpio.h>
#include <driver/hal/hal_gpio_types.h>
#include "gpio_driver.h"
#include <led_blink.h>
#include <string.h>

static LedControlBlock led_pool[MAX_LED_NUM];
static beken_mutex_t led_mutex = NULL;

typedef struct {
    int led1;          // 第一个LED句柄
    int led2;          // 第二个LED句柄
    beken_timer_t timer; // 共享定时器
    uint32_t interval;  // 交替间隔
    bool current_state; // 当前主导状态
} AlternateGroup;
static AlternateGroup s_alt_group;

int led1 = 0;
int led2 = 0;


static int led_register(uint8_t gpio);

static void led_set_state(int led_handle, LedState new_state);

static void led_unregister(int led_handle);

static void led_set_alternate(int led1, int led2, uint32_t interval_ms);

static void alternate_timer_cb(void *arg) {
    rtos_lock_mutex(&led_mutex);
    
    // 交替状态
    s_alt_group.current_state = !s_alt_group.current_state;
    
    // 更新LED1状态
    LedControlBlock* led1 = &led_pool[s_alt_group.led1];
    led1->led_status = s_alt_group.current_state;

    if (led1->led_status == 0)
    {
        bk_gpio_set_value(led1->gpio_num,0x0);
    } else {
        bk_gpio_set_value(led1->gpio_num,0x2);
    }
    // 更新LED2状态（取反）
    LedControlBlock* led2 = &led_pool[s_alt_group.led2];
    led2->led_status = !s_alt_group.current_state;
    if (led2->led_status == 0)
    {
        bk_gpio_set_value(led2->gpio_num,0x0);
    } else {
        bk_gpio_set_value(led2->gpio_num,0x2);
    }
    
    rtos_unlock_mutex(&led_mutex);
}

//设置交替闪
void led_set_alternate(int led1, int led2, uint32_t interval_ms) {
    // 参数校验
    if(led1 == led2 || led1 <0 || led2 <0 || 
       led1 >= MAX_LED_NUM || led2 >= MAX_LED_NUM) return;
    
    rtos_lock_mutex(&led_mutex);
    
    // 配置交替组
    s_alt_group.led1 = led1;
    s_alt_group.led2 = led2;
    s_alt_group.interval = interval_ms;
    
    // 初始化LED状态
    led_pool[led1].state = LED_ALTERNATE;
    led_pool[led2].state = LED_ALTERNATE;
    led_pool[led1].alt_partner = led2;
    led_pool[led2].alt_partner = led1;
    
    // 创建/重置定时器
    
    rtos_init_timer(&s_alt_group.timer,interval_ms, alternate_timer_cb, NULL);
    
    // 强制设置初始状态
    s_alt_group.current_state = true;
    bk_gpio_set_value(led_pool[led1].gpio_num, 0x2);
    bk_gpio_set_value(led_pool[led2].gpio_num, 0x0);

    rtos_start_timer(&s_alt_group.timer);
    
    rtos_unlock_mutex(&led_mutex);
}

static void timer_callback(void *arg) {
    int led_id = (int)arg;
    LedControlBlock* led = &led_pool[led_id];
    
    if(led->state == LED_FAST_BLINK || led->state == LED_SLOW_BLINK){
        led->led_status = !led->led_status;
        if (led->led_status == 1)
        {
            bk_gpio_set_value(led->gpio_num,0x2);
        } else {
            bk_gpio_set_value(led->gpio_num,0x0);
        }
        
    }
}

void led_driver_init() {
    int ret = rtos_init_mutex(&led_mutex);
    BK_ASSERT(kNoErr == ret);
    memset(led_pool, 0, sizeof(led_pool));
    led1 = led_register(RED_LED);
    led2 = led_register(GREEN_LED);
}


int led_register(uint8_t gpio) {
    rtos_lock_mutex(&led_mutex);
     // 查找空闲控制块
    for(int i=0; i<MAX_LED_NUM; i++){
        if(led_pool[i].gpio_num == 0){
            led_pool[i].gpio_num = gpio;
            rtos_init_timer(&led_pool[i].timer,100, timer_callback, (void*)i);
            rtos_unlock_mutex(&led_mutex);
            return i; // 返回LED句柄
        }
    }
    
    rtos_unlock_mutex(&led_mutex);
    return -1; // 注册失败
}


void led_set_state(int led_handle, LedState new_state) {
    if(led_handle < 0 || led_handle >= MAX_LED_NUM) return;

    rtos_lock_mutex(&led_mutex);
    LedControlBlock* led = &led_pool[led_handle];

    if(led->state == LED_ALTERNATE && new_state != LED_ALTERNATE) {
        // 停止交替定时器
        if(s_alt_group.led1 == led_handle || 
            s_alt_group.led2 == led_handle) {
            rtos_stop_timer(&s_alt_group.timer);
        }
        // 解除伙伴关系
        if(led->alt_partner != -1) {
            led_pool[led->alt_partner].alt_partner = -1;
            led->alt_partner = -1;
        }
    }
    // 状态转换处理
    switch (new_state) {
        case LED_OFF:
            rtos_stop_timer(&led->timer);
            bk_gpio_set_value(led->gpio_num,0x0);
            break;
            
        case LED_ON:
            rtos_stop_timer(&led->timer);
            bk_gpio_set_value(led->gpio_num,0x2);
            break;
            
        case LED_FAST_BLINK:
           
            rtos_change_period(&led->timer, 200);
            rtos_start_timer(&led->timer);
            break;
            
        case LED_SLOW_BLINK:
            
            rtos_change_period(&led->timer, 1000);
            rtos_start_timer(&led->timer);
            break;

        default:
            break;
        
    }
    
    led->state = new_state;
    rtos_unlock_mutex(&led_mutex);
}

void led_unregister(int led_handle) {
    if(led_handle < 0 || led_handle >= MAX_LED_NUM) return;
    
    rtos_lock_mutex(&led_mutex);
    rtos_deinit_timer(&led_pool[led_handle].timer);
    memset(&led_pool[led_handle], 0, sizeof(LedControlBlock));
    rtos_unlock_mutex(&led_mutex);
}

void led_app_set(led_operate_t oper)
{
    switch(oper)
    {
        case LED_OFF_GREEN:
            led_set_state(led2,LED_OFF);
            break;

        case LED_ON_GREEN:
            led_set_state(led2,LED_ON);
            break;

        case LED_FAST_BLINK_GREEN:
            led_set_state(led2,LED_FAST_BLINK);
            break;

        case LED_SLOW_BLINK_GREEN:
            led_set_state(led2,LED_SLOW_BLINK);
            break;

        case LED_OFF_RED:
            led_set_state(led1,LED_OFF);
            break;

        case LED_ON_RED:
            led_set_state(led1,LED_ON);
            break;

        case LED_FAST_BLINK_RED:
            led_set_state(led1,LED_FAST_BLINK);
            break;

        case LED_SLOW_BLINK_RED:
            led_set_state(led1,LED_SLOW_BLINK);
            break;

        case LED_REG_GREEN_ALTERNATE:
            led_set_alternate(led1,led2,500);
            break;

        case LED_REG_GREEN_ALTERNATE_OFF:
            led_set_state(led1,LED_OFF);
            led_set_state(led2,LED_OFF);
            break;

        default:
            break;
    }

}