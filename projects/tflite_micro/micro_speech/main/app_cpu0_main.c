#include <os/os.h>

#include "FreeRTOS.h"
#include "task.h"

static char *TAG = "main";

void app_cpu0_main(void)
{

    os_printf("app_cpu0_main\n");

    void app_main(void *arg);
    xTaskCreate(app_main, "app_main", 2048, NULL, 4, NULL);
}