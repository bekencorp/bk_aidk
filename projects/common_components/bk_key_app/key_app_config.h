#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if !CONFIG_BK_WSS_TRANS_NOPSRAM
#define KEY_DEFAULT_CONFIG_TABLE \
{ \
    { \
        .gpio_id = KEY_GPIO_13, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_UP, \
        .double_event = VOLUME_UP,	\
        .long_event = CONFIG_NETWORK \
    }, \
    { \
        .gpio_id = KEY_GPIO_12, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = IR_MODE_SWITCH, \
        .double_event = POWER_ON, \
        .long_event = SHUT_DOWN \
    },\
    { \
        .gpio_id = KEY_GPIO_8, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_DOWN, \
        .double_event = VOLUME_DOWN, \
        .long_event = FACTORY_RESET \
    } \
}
#else
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
        .short_event = IR_MODE_SWITCH, \
        .double_event = POWER_ON, \
        .long_event = AUDIO_BUF_APPEND, \
        .long_press_up_event = AUDIO_BUF_COMMIT \
    },\
    { \
        .gpio_id = KEY_GPIO_8, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_DOWN, \
        .double_event = VOLUME_DOWN, \
        .long_event = FACTORY_RESET \
    } \
}
#endif

#ifdef __cplusplus
}
#endif