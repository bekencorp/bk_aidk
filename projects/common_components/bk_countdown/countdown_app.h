#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define COUNTDOWN_INFINITE 0xFFFFFFFF
/* 新增倒计时票源类型,按照优先级排序*/
typedef enum {
    COUNTDOWN_TICKET_PROVISIONING,   // 配网倒计时(5分钟), 最高优先级
    COUNTDOWN_TICKET_NETWORK_ERROR,  //网络错误倒计时(5分钟) ，中优先级
    COUNTDOWN_TICKET_STANDBY,        // 待机倒计时(3分钟)，低优先级
    COUNTDOWN_TICKET_OTA,      // OTA 事件，特殊逻辑，不参与比较
    COUNTDOWN_TICKET_MAX
} countdown_ticket_t;


void update_countdown(uint32_t s_active_tickets);

#ifdef __cplusplus
}
#endif