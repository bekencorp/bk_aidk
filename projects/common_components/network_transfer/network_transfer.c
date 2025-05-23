#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include "network_transfer.h"
#include "cli.h"

#define TAG "ntws"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#if CONFIG_AGORA_IOT_SDK
extern int agora_rtc_cli_init(void);
#elif CONFIG_VOLC_RTC_EN
extern int byte_rtc_cli_init(void);
#endif

int network_transfer_init(void)
{
    #if CONFIG_AGORA_IOT_SDK
    agora_rtc_cli_init();
    #elif CONFIG_VOLC_RTC_EN
    byte_rtc_cli_init();
    #endif

    return BK_OK;
}
