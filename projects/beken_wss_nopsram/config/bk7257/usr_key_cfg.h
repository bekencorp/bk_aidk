#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <driver/hal/hal_gpio_types.h>

#define KEY_GPIO_0    GPIO_0
#define KEY_GPIO_21   GPIO_21
#define KEY_GPIO_1    GPIO_1


#define KEY_DEFAULT_CONFIG_TABLE \
{ \
    { \
        .gpio_id = KEY_GPIO_0, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_UP, \
        .double_event = VOLUME_UP,  \
        .long_event = CONFIG_NETWORK \
    }, \
    { \
        .gpio_id = KEY_GPIO_21, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = AUDIO_BUF_APPEND, \
        .double_event = POWER_ON \
    },\
    { \
        .gpio_id = KEY_GPIO_1, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_DOWN, \
        .double_event = VOLUME_DOWN, \
        .long_event = FACTORY_RESET \
    } \
}

#ifdef __cplusplus
}
#endif