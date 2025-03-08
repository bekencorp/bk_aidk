#ifndef __AGORA_LED_BLINK_H__
#define __AGORA_LED_BLINK_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <os/os.h>

#define MAX_LED_NUM  4
#define RED_LED      40
#define GREEN_LED      41
typedef enum {
    LED_OFF = 0, //off 
    LED_ON,      //on
    LED_FAST_BLINK,   //fast blinking
    LED_SLOW_BLINK,   // slow blinking
    LED_ALTERNATE
} LedState;

typedef struct {
    uint8_t gpio_num;       // GPIO编号
    LedState state;         // 当前状态
    beken_timer_t timer;    // 定时器句柄
    uint32_t interval;      // 当前闪烁间隔
    bool led_status;        // 当前物理状态
    int alt_partner;        // 绑定组
} LedControlBlock;

typedef enum
{
    LED_OFF_GREEN,
    LED_ON_GREEN,
    LED_FAST_BLINK_GREEN,
    LED_SLOW_BLINK_GREEN,

    LED_OFF_RED,
    LED_ON_RED,
    LED_FAST_BLINK_RED,
    LED_SLOW_BLINK_RED,

    LED_REG_GREEN_ALTERNATE,
    LED_REG_GREEN_ALTERNATE_OFF,
}led_operate_t;

void led_driver_init();

void led_app_set(led_operate_t oper);

#ifdef __cplusplus
}
#endif
#endif