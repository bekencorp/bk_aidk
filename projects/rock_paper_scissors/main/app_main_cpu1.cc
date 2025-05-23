#include <string.h>

extern "C" {
#include <os/os.h>
#include <os/mem.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

    //#include "frame_buffer.h"

#include "modules/pm.h"
}

#include "main_functions.h"
#include "model_settings.h"
#include "tensorflow/lite/micro/cortex_m_generic/debug_log_callback.h"
#include "gesture_detection_model_data.h"

static char *TAG = (char *)"main";

#define LOGD(TAG, format, ...) os_printf(format, ##__VA_ARGS__)
#define LOGI(TAG, format, ...) os_printf(format, ##__VA_ARGS__)
#define LOGE(TAG, format, ...) os_printf(format, ##__VA_ARGS__)

void debugLogCallback(const char *s)
{
    os_printf(s);
}

unsigned char *data_ptr = NULL;
void tflite_task(void *arg)
{
    LOGI(TAG, "tflite_task");

    RegisterDebugLogCallback(debugLogCallback);


    data_ptr = (unsigned char *)psram_malloc(g_gesture_detection_model_data_len);

    if (data_ptr == NULL)
    {
        os_printf("===data buffer malloc error====\r\n");
        return;
    }

    os_memcpy(data_ptr, g_gesture_detection_model_data, g_gesture_detection_model_data_len);

    setup();

    while (1)
    {
        //uint64_t before = (uint64_t)rtos_get_time();
        loop();
        //uint64_t after = (uint64_t)rtos_get_time();
        //LOGD(TAG, "detection time: %d ms\r\n", (uint32_t)(after - before));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }


    vTaskDelete(NULL);
}

extern "C" void tflite_task_init_c(void *arg)
{
    LOGI(TAG, "tflite_task");

    bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);
    RegisterDebugLogCallback(debugLogCallback);


    data_ptr = (unsigned char*)psram_malloc(g_gesture_detection_model_data_len);
    if(data_ptr == NULL)
    {
        os_printf("===data buffer malloc error====\r\n");
        return;
    }

    os_memcpy(data_ptr,g_gesture_detection_model_data,g_gesture_detection_model_data_len);

    setup();
}

extern "C" void tflite_process(uint8_t *data, uint32_t len, uint8_t *result)
{
    process_pic(data, len, result);
}

extern "C" void app_main_cpu1(void *arg)
{
    LOGI(TAG, "app_main_cpu1");

    bk_pm_module_vote_cpu_freq(PM_DEV_ID_LIN, PM_CPU_FRQ_480M);

    xTaskCreate(tflite_task, "test", (unsigned int)(1024 * 16), NULL, 3, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}
