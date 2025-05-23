#include <countdown_app.h>
#include <countdown.h>

#include <components/log.h>

#if CONFIG_COUNTDOWN
#define TAG "countdown_app"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)



/* 各票源对应的倒计时时长(毫秒) */
static const uint32_t s_ticket_durations[COUNTDOWN_TICKET_MAX] = {
    [COUNTDOWN_TICKET_PROVISIONING] = 5 * 60 * 1000,  // 5分钟
    [COUNTDOWN_TICKET_NETWORK_ERROR] = 5 * 60 * 1000,  // 5分钟
    [COUNTDOWN_TICKET_STANDBY]      = 3 * 60 * 1000,  // 3分钟
    [COUNTDOWN_TICKET_OTA]    = COUNTDOWN_INFINITE,              // 暂停倒计时
};



/* 更新倒计时状态 */
void update_countdown(uint32_t s_active_tickets)
{

    // 检查OTA暂停票（最高优先级）
    if(s_active_tickets & (1 << COUNTDOWN_TICKET_OTA)) {
        LOGI("ota event start, stop countdown\r\n");
        stop_countdown();
        return;
    }

    static countdown_ticket_t last_selected_ticket = COUNTDOWN_TICKET_MAX;
    countdown_ticket_t selected_ticket = COUNTDOWN_TICKET_MAX;


    for(int i = 0; i < COUNTDOWN_TICKET_MAX; i++) {
        if (i == COUNTDOWN_TICKET_OTA)
            continue;

        if(s_active_tickets & (1 << i))
        {
            selected_ticket = i;
            break;
        }
    }

    // 应用倒计时裁决结果
    if(selected_ticket != COUNTDOWN_TICKET_MAX) {
        const uint32_t max_duration = s_ticket_durations[selected_ticket];

        bool is_same_event = (selected_ticket == last_selected_ticket);
        bool is_network_error = (selected_ticket == COUNTDOWN_TICKET_NETWORK_ERROR);

        if (selected_ticket != last_selected_ticket || (is_same_event && !is_network_error))
        {
            const uint32_t duration = max_duration;
            start_countdown(duration);
            LOGI("selected_ticket is %d, last_selected_ticket is %d,duration is %d\r\n",selected_ticket , last_selected_ticket, duration);
            last_selected_ticket = selected_ticket;
        }
    } else {
        LOGI("selected_ticket is %d, COUNTDOWN_TICKET_MAX is %d\r\n",selected_ticket , COUNTDOWN_TICKET_MAX);
        LOGI("stop countdown\r\n");
        stop_countdown();
        last_selected_ticket = COUNTDOWN_TICKET_MAX;
    }

}
#endif