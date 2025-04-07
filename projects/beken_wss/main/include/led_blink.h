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
#define LED_FAST_BLINK_TIME   200    //ms
#define LED_SLOW_BLINK_TIME   1000
#define LED_LAST_FOREVER      0xFFFFFFFF
typedef enum {
    LED_OFF = 0, //off 
    LED_ON,      //on
    LED_FAST_BLINK,   //fast blinking
    LED_SLOW_BLINK,   // slow blinking
    LED_ALTERNATE
} LedState;

// 错误类型定义（按优先级从高到低排序）
typedef enum {
    ERROR_CRITICAL = 0, // 最高优先级，快闪（如严重故障）
    ERROR_WARNING,      // 次优先级，慢闪（如普通警告）
    ERROR_TYPE_COUNT    // 无错误标记
} ErrorType;


typedef struct {
    uint8_t gpio_num;       // GPIO编号
    LedState state;         // 当前状态
    beken_timer_t timer;    // 定时器句柄
    uint32_t interval;      // 当前闪烁间隔
    bool led_status;        // 当前物理状态
    int alt_partner;        // 绑定组
    uint32_t blink_duration; //闪烁持续时间
    uint32_t blink_start;   //开始时间

    int error_counts[ERROR_TYPE_COUNT]; // 各类型错误计数器
    ErrorType active_error;    // 当前活跃的错误类型
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

    LED_ERROR_CRITICAL,
    LED_ERROR_WARNING,
    LED_ERROR_CRITICAL_CLOSE,
    LED_ERROR_WARNING_CLOSE,
}led_operate_t;

/// @brief led init
void led_driver_init();


/// @brief control led
/// @param oper the type of led event
/// @param time the duration of LED's luminescence
void led_app_set(led_operate_t oper, uint32_t time);

#ifdef __cplusplus
}
#endif
#endif