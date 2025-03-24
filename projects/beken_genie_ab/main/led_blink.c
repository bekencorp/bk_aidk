#include "bk_gpio.h"
#include <os/os.h>
#include <os/mem.h>
#include <common/bk_kernel_err.h>
#include <driver/gpio.h>
#include <driver/hal/hal_gpio_types.h>
#include "gpio_driver.h"
#include <led_blink.h>
#include <string.h>
#include <components/log.h>

#define TAG "led_blink"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

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

static int led_handle_1 = 0;
static int led_handle_2 = 0;


static int led_register(uint8_t gpio);

static void led_set_state(uint8_t led_handle, LedState new_state, uint32_t time);

static void led_unregister(uint8_t led_handle);

static void led_set_alternate(uint8_t led_handle_1, uint8_t led_handle_2, uint32_t interval_ms);

static void led_vote_error(uint8_t led_handle,ErrorType err_type);

static void led_resolve_error(uint8_t led_handle,ErrorType err_type);

static void update_active_error(LedControlBlock* led);

static void restore_normal_state(LedControlBlock* led);

static void alternate_timer_cb(void *arg) {
    rtos_lock_mutex(&led_mutex);
    
    // 交替状态
    s_alt_group.current_state = !s_alt_group.current_state;
    
    // 更新LED1状态
    LedControlBlock* led_1 = &led_pool[s_alt_group.led1];
    led_1->led_status = s_alt_group.current_state;

    if (led_1->led_status == 0)
    {
        bk_gpio_set_value(led_1->gpio_num,0x0);
    } else {
        bk_gpio_set_value(led_1->gpio_num,0x2);
    }
    // 更新LED2状态（取反）
    LedControlBlock* led_2 = &led_pool[s_alt_group.led2];
    led_2->led_status = !s_alt_group.current_state;
    if (led_2->led_status == 0)
    {
        bk_gpio_set_value(led_2->gpio_num,0x0);
    } else {
        bk_gpio_set_value(led_2->gpio_num,0x2);
    }
    
    rtos_unlock_mutex(&led_mutex);
}

//设置交替闪
static void led_set_alternate(uint8_t led_handle_1, uint8_t led_handle_2, uint32_t interval_ms) {
    // 参数校验
    if(led_handle_1 == led_handle_2 || led_handle_1 <0 || led_handle_2 <0 || 
       led_handle_1 >= MAX_LED_NUM || led_handle_2 >= MAX_LED_NUM) return;

    if((led_pool[led_handle_1].state == LED_ALTERNATE) && (led_pool[led_handle_2].state == LED_ALTERNATE))
        return;
    
    rtos_lock_mutex(&led_mutex);
    
    // 配置交替组
    s_alt_group.led1 = led_handle_1;
    s_alt_group.led2 = led_handle_2;
    s_alt_group.interval = interval_ms;
    
    // 初始化LED状态
    led_pool[led_handle_1].state = LED_ALTERNATE;
    led_pool[led_handle_2].state = LED_ALTERNATE;
    led_pool[led_handle_1].alt_partner = led_handle_2;
    led_pool[led_handle_2].alt_partner = led_handle_1;
    
    // 创建/重置定时器
    
    rtos_init_timer(&s_alt_group.timer,interval_ms, alternate_timer_cb, NULL);
    
    // 强制设置初始状态
    s_alt_group.current_state = true;
    bk_gpio_set_value(led_pool[led_handle_1].gpio_num, 0x2);
    bk_gpio_set_value(led_pool[led_handle_2].gpio_num, 0x0);

    rtos_start_timer(&s_alt_group.timer);
    
    rtos_unlock_mutex(&led_mutex);
}

static void timer_callback(void *arg) {
    int led_handle = (int)arg;
    LedControlBlock* led = &led_pool[led_handle];
    bool should_stop = false;

    if (led->blink_duration == LED_LAST_FOREVER)
    {
        should_stop = false;
    }else if(led->blink_duration >= 0)
    {
        uint32_t now = rtos_get_time();
        if (now - led->blink_start >= led->blink_duration)
        {
            should_stop = true;
        }
        
    }
    
    if (led->active_error != ERROR_TYPE_COUNT)
    {
        LOGI("enter the anomaly\r\n");
        led->led_status = !led->led_status;
        if (led->led_status == 1)
        {
            bk_gpio_set_value(led->gpio_num,0x2);
        } else {
            bk_gpio_set_value(led->gpio_num,0x0);
        }
    }else if (should_stop)
    {
        LOGI("blink time's up\r\n");
        rtos_stop_timer(&led->timer);
        bk_gpio_set_value(led->gpio_num,0x0);
        led->state = LED_OFF;
    }else if(led->state == LED_FAST_BLINK || led->state == LED_SLOW_BLINK)
    {
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
    led_handle_1 = led_register(RED_LED);
    led_handle_2 = led_register(GREEN_LED);
}


int led_register(uint8_t gpio) {
    rtos_lock_mutex(&led_mutex);
     // 查找空闲控制块
    for(int i=0; i<MAX_LED_NUM; i++){
        if(led_pool[i].gpio_num == 0){
            led_pool[i].gpio_num = gpio;
            led_pool[i].active_error = ERROR_TYPE_COUNT;
            rtos_init_timer(&led_pool[i].timer,100, timer_callback, (void*)i);
            rtos_unlock_mutex(&led_mutex);
            return i; // 返回LED句柄
        }
    }
    
    rtos_unlock_mutex(&led_mutex);
    return -1; // 注册失败
}


static void led_set_state(uint8_t led_handle, LedState new_state, uint32_t time) {
    if(led_handle < 0 || led_handle >= MAX_LED_NUM) 
        return;

    rtos_lock_mutex(&led_mutex);
    LedControlBlock* led = &led_pool[led_handle];

    if(led->state == LED_ALTERNATE && new_state != LED_ALTERNATE) {
        // 停止交替定时器
        if(s_alt_group.led1 == led_handle || 
            s_alt_group.led2 == led_handle) {
            rtos_stop_timer(&s_alt_group.timer);
        }

        //Another LED status clear
        bk_gpio_set_value(led_pool[led->alt_partner].gpio_num, 0x0);
        led_pool[led->alt_partner].state = LED_OFF;

        // 解除伙伴关系
        if(led->alt_partner != -1) {
            led_pool[led->alt_partner].alt_partner = -1;
            led->alt_partner = -1;
        }

    }


    if (led->active_error == ERROR_TYPE_COUNT){
        led->state = new_state;
        switch (new_state) {
            case LED_OFF:
                rtos_stop_timer(&led->timer);
                bk_gpio_set_value(led->gpio_num,0x0);
                break;
                
            case LED_ON:
                led->blink_duration = time;
                led->blink_start = rtos_get_time();
                rtos_start_timer(&led->timer);
                bk_gpio_set_value(led->gpio_num,0x2);
                break;
                
            case LED_FAST_BLINK:
                led->interval = LED_FAST_BLINK_TIME;
                led->blink_duration = time;
                led->blink_start = rtos_get_time();
                rtos_change_period(&led->timer, LED_FAST_BLINK_TIME);
                rtos_start_timer(&led->timer);
                break;
                
            case LED_SLOW_BLINK:
                led->interval = LED_SLOW_BLINK_TIME;
                led->blink_duration = time;
                led->blink_start = rtos_get_time();
                rtos_change_period(&led->timer, LED_SLOW_BLINK_TIME);
                rtos_start_timer(&led->timer);
                break;

            default:
                break;
        
        }
    }

    rtos_unlock_mutex(&led_mutex);
}


void led_unregister(uint8_t led_handle) {
    if(led_handle < 0 || led_handle >= MAX_LED_NUM) return;
    
    rtos_lock_mutex(&led_mutex);
    rtos_deinit_timer(&led_pool[led_handle].timer);
    memset(&led_pool[led_handle], 0, sizeof(LedControlBlock));
    rtos_unlock_mutex(&led_mutex);
}


void led_vote_error(uint8_t led_handle, ErrorType err_type) {
    if (led_handle < 0 || led_handle >= MAX_LED_NUM) 
    {
        LOGE("error's led_handle , led_handle is %d\r\n",led_handle);
        return;
    }

    rtos_lock_mutex(&led_mutex);
    LedControlBlock* led = &led_pool[led_handle];
    // 对应错误计数器增加
    if (led->error_counts[err_type]++ == 0) {
        // 首次投票此类型错误，可能触发状态变更
        update_active_error(led);
    }
    
    rtos_unlock_mutex(&led_mutex);
}

void led_resolve_error(uint8_t led_handle, ErrorType err_type) {
    if (led_handle < 0 || led_handle >= MAX_LED_NUM) return;

    rtos_lock_mutex(&led_mutex);
    LedControlBlock* led = &led_pool[led_handle];
    if (led->error_counts[err_type] > 0 && --led->error_counts[err_type] == 0) {
        // 该类型错误计数器归零，重新评估状态
        update_active_error(led);
    }
    
    rtos_unlock_mutex(&led_mutex);
}

static void update_active_error(LedControlBlock* led) {
    // 确定当前最高优先级错误
    ErrorType new_active = ERROR_TYPE_COUNT;
    for (int i = 0; i < ERROR_TYPE_COUNT; i++) {
        if (led->error_counts[i] > 0) {
            new_active = (ErrorType)i;
            break; // 按优先级顺序遍历，第一个非零即最高
        }
    }

    // 状态切换处理
    if (new_active != led->active_error) {
        rtos_stop_timer(&led->timer); // 停止当前定时器
        
        if (new_active != ERROR_TYPE_COUNT) {
            // 切换到新错误类型
            led->active_error = new_active;
            const int period = (new_active == ERROR_CRITICAL) ? LED_FAST_BLINK_TIME : LED_SLOW_BLINK_TIME;
            os_printf("period is %d\r\n",period);
            rtos_change_period(&led->timer, period);
            rtos_start_timer(&led->timer);
            led->state = (new_active == ERROR_CRITICAL) ? LED_FAST_BLINK : LED_SLOW_BLINK;
        } else {
            // 无错误，恢复正常状态
            LOGI("Abnormal recovery\r\n");
            restore_normal_state(led);
        }
    }
}

static void restore_normal_state(LedControlBlock* led) {

    led->active_error = ERROR_TYPE_COUNT;
    
    if (led->blink_duration > 0) {
        // 重新启动自动熄灭计时
        int period = led->interval;
        led->blink_start = rtos_get_time();
        rtos_change_period(&led->timer, period);
        rtos_start_timer(&led->timer);
    } else {
        bk_gpio_set_value(led->gpio_num,0x0);
    }
}

void led_app_set(led_operate_t oper, uint32_t time)
{
    LOGI("%s:oper=%d,t=%d\r\n", __func__, oper, time);

    switch(oper)
    {
        case LED_OFF_GREEN:
            led_set_state(led_handle_2,LED_OFF,0);
            break;

        case LED_ON_GREEN:
            led_set_state(led_handle_2,LED_ON,time);
            break;

        case LED_FAST_BLINK_GREEN:
            led_set_state(led_handle_2,LED_FAST_BLINK,time);
            break;

        case LED_SLOW_BLINK_GREEN:
            led_set_state(led_handle_2,LED_SLOW_BLINK,time);
            break;

        case LED_OFF_RED:
            led_set_state(led_handle_1,LED_OFF,0);
            break;

        case LED_ON_RED:
            led_set_state(led_handle_1,LED_ON,time);
            break;

        case LED_FAST_BLINK_RED:
            led_set_state(led_handle_1,LED_FAST_BLINK,time);
            break;

        case LED_SLOW_BLINK_RED:
            led_set_state(led_handle_1,LED_SLOW_BLINK,time);
            break;

        case LED_REG_GREEN_ALTERNATE:
            led_set_alternate(led_handle_1,led_handle_2,500);
            break;

        case LED_REG_GREEN_ALTERNATE_OFF:
            led_set_state(led_handle_1,LED_OFF,0);
            led_set_state(led_handle_2,LED_OFF,0);
            break;
        
        case LED_ERROR_CRITICAL:
            led_vote_error(led_handle_1,ERROR_CRITICAL);
            break;

        case LED_ERROR_WARNING:
            led_vote_error(led_handle_1,ERROR_WARNING);
            break;

        case LED_ERROR_CRITICAL_CLOSE:
            led_resolve_error(led_handle_1,ERROR_CRITICAL);
            break;

        case LED_ERROR_WARNING_CLOSE:
            led_resolve_error(led_handle_1,ERROR_WARNING);
            break;

        default:
            break;
    }

}