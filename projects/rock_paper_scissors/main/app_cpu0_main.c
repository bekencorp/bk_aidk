#include <os/os.h>
#include "modules/pm.h"
#include "media/media_main.h"

//#include "FreeRTOS.h"
//#include "task.h"

static char *TAG = "main";

void app_cpu0_main(void)
{
    os_printf("%s\n", __func__);

    media_main();
}
