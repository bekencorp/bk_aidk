#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define KEY_DEFAULT_CONFIG_TABLE \
{ \
    { \
        .gpio_id = KEY_GPIO_13, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_UP, \
        .double_event = VOLUME_UP,  \
        .long_event = CONFIG_NETWORK \
    }, \
    { \
        .gpio_id = KEY_GPIO_12, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = AUDIO_BUF_APPEND, \
        .double_event = POWER_ON \
    },\
    { \
        .gpio_id = KEY_GPIO_8, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_DOWN, \
        .double_event = VOLUME_DOWN, \
        .long_event = FACTORY_RESET \
    } \
}

#ifdef __cplusplus
}
#endif