#include <os/os.h>

#include "FreeRTOS.h"
#include "task.h"

static char *TAG = "main";

void app_cpu1_main(void)
{

    void app_main_cpu1(void *arg);
    xTaskCreate(app_main_cpu1, "app_main", 2048, NULL, 4, NULL);
}

#include "media_mailbox_list_util.h"
void lvgl_event_handle(media_mailbox_msg_t *msg)
{

}