#include <os/os.h>

#include "FreeRTOS.h"
#include "task.h"
#include "media_main.h"

static char *TAG = "main";

void app_cpu1_main(void)
{
    media_ts_main();
}
