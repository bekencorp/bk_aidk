#include <os/os.h>
#include <os/str.h>
#include "lcd_act.h"
#include "media_app.h"
#if CONFIG_LVGL
#include "lv_vendor.h"
#include "lvgl.h"
#include "lv_img_utility.h"
#include "bk_posix.h"
#endif
#include "driver/drv_tp.h"
#include <driver/lcd.h>
#include "yuv_encode.h"
#include "media_evt.h"


#define TAG "AVI"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#if (CONFIG_SYS_CPU1)
static lv_vnd_config_t lv_vnd_config = {0};
static lv_obj_t *label = NULL;
static lv_timer_t *label_timer = NULL;
static int current_pos = 0; // current position
static int text_length = 0; // text total length
static char *text_data = NULL;
static char *buffer = NULL;
static uint8_t label_timer_is_running = 0;

typedef struct {
    uint8_t text_type;
    char *text_data;
} lv_text_info_t;

static int lv_get_utf8_char_length(const char *str)
{
    if ((*str & 0x80) == 0x00) return 1;
    if ((*str & 0xE0) == 0xC0) return 2;
    if ((*str & 0xF0) == 0xE0) return 3;
    if ((*str & 0xF8) == 0xF0) return 4;

    return 1;
}

static void lvgl_label_timer_cb(lv_timer_t *timer)
{
    if (current_pos < text_length) {
        int char_len = lv_get_utf8_char_length(&text_data[current_pos]);
        os_strncpy(buffer, text_data, current_pos + char_len);
        lv_label_set_text(label, buffer);
        current_pos += char_len;
    } else {
        lv_timer_del(timer);
        label_timer = NULL;
        label_timer_is_running = 0;
    }
}

bk_err_t lvgl_event_send_data_handle(media_mailbox_msg_t *msg)
{
    lv_text_info_t *text_info = (lv_text_info_t *)msg->param;
    current_pos = 0;
    text_length = os_strlen(text_info->text_data);
    LOGI("text_length = %d\r\n", text_length);

    if (label_timer_is_running) {
        lv_timer_del(label_timer);
        label_timer = NULL;
        label_timer_is_running = 0;
    }

    if (text_data != NULL) {
        psram_free(text_data);
        text_data = NULL;
        psram_free(buffer);
        buffer = NULL;
    }

    buffer = psram_malloc(text_length + 1);
    if (buffer == NULL) {
        LOGE("%s %d buffer malloc failed\r\n", __func__, __LINE__);
        return BK_FAIL;
    }
    os_memset(buffer, 0x00, text_length + 1);

    text_data = psram_malloc(text_length + 1);
    if (text_data == NULL) {
        LOGE("%s %d text_data malloc failed\r\n", __func__, __LINE__);
        return BK_FAIL;
    }
    os_memset(text_data, 0x00, text_length + 1);
    os_memcpy(text_data, text_info->text_data, text_length + 1);

    lv_vendor_disp_lock();
    label_timer = lv_timer_create(lvgl_label_timer_cb, 200, NULL);
    lv_vendor_disp_unlock();

    return BK_OK;
}

bk_err_t lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    lv_vendor_stop();
    lcd_display_close();

    return BK_OK;
}

bk_err_t lvgl_event_open_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \n", __func__);

    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    if (lv_vnd_config.draw_pixel_size == 0) {
#ifdef CONFIG_LVGL_USE_PSRAM
#define PSRAM_DRAW_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
        lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi);
        lv_vnd_config.draw_buf_2_1 = (lv_color_t *)PSRAM_DRAW_BUFFER;
        lv_vnd_config.draw_buf_2_2 = (lv_color_t *)(PSRAM_DRAW_BUFFER + lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
#else
#define PSRAM_FRAME_BUFFER ((0x60000000UL) + 5 * 1024 * 1024)
        lv_vnd_config.draw_pixel_size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) / 10;
        lv_vnd_config.draw_buf_2_1 = LV_MEM_CUSTOM_ALLOC(lv_vnd_config.draw_pixel_size * sizeof(lv_color_t));
        lv_vnd_config.draw_buf_2_2 = NULL;
        lv_vnd_config.frame_buf_1 = (lv_color_t *)PSRAM_FRAME_BUFFER;
        lv_vnd_config.frame_buf_2 = (lv_color_t *)(PSRAM_FRAME_BUFFER + ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * sizeof(lv_color_t));
#endif
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
        lv_vnd_config.rotation = ROTATE_NONE;

        lv_vendor_init(&lv_vnd_config);
    }

    lv_vendor_fs_init();

    uint32_t file_len = lv_img_read_filelen("/simhei.ttf");
    LOGI("file_len = %d\r\n", file_len);
    if (file_len <= 0) {
        LOGE("file len read failed\r\n");
        lv_vendor_fs_deinit();
        return BK_FAIL;
    }

    uint32_t *file_content = psram_malloc(file_len);
    if (file_content == NULL) {
        LOGE("file_content malloc failed\r\n");
        lv_vendor_fs_deinit();
        return BK_FAIL;
    }

    int fd = open("/simhei_new1.ttf", O_RDONLY);
    if (fd < 0) {
        LOGE("file_content malloc failed\r\n");
        psram_free(file_content);
        lv_vendor_fs_deinit();
        return BK_FAIL;
    }

    uint32_t read_len = read(fd, file_content, file_len);
    LOGI("read_len = %d \r\n", read_len);
    close(fd);

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    lv_vendor_disp_lock();

    static lv_ft_info_t info;
    info.name = "/simhei.ttf";
    info.weight = 24;
    info.style = FT_FONT_STYLE_NORMAL;
    info.mem = file_content;
    info.mem_size = file_len;
    if(!lv_ft_font_init(&info)) {
        LV_LOG_ERROR("create failed.");
    }

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, info.font);
    lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

    label = lv_label_create(lv_scr_act());
    lv_obj_add_style(label, &style, 0);
    lv_label_set_text(label, "欢迎来到BEKEN AI精灵");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 300);
    lv_obj_scroll_to_view(label, LV_ANIM_OFF);
    lv_obj_center(label);

    lv_vendor_disp_unlock();

    lv_vendor_start();

    return BK_OK;
}

void lvgl_event_handle(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_FAIL;

    switch (msg->event)
    {
        case EVENT_LVGL_OPEN_IND:
            ret = lvgl_event_open_handle(msg);
            break;

        case EVENT_LVGL_CLOSE_IND:
            ret = lvgl_event_close_handle(msg);
            break;

        case EVENT_LVGL_SEND_DATA_IND:
            ret = lvgl_event_send_data_handle(msg);
            break;

        default:
            break;
    }

    msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
}
#endif

#if (CONFIG_SYS_CPU0)
static uint8_t lvgl_app_init_flag = 0;

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_360X360,
    .device_name = "gc9c01",
};

void lvgl_app_init(void)
{
    bk_err_t ret;

    if (lvgl_app_init_flag == 1)
    {
        LOGW("lvgl_app_init has inited\r\n");
        return;
    }

    ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK)
    {
        LOGE("media_app_lvgl_open failed\r\n");
        return;
    }

    lvgl_app_init_flag = 1;
}

void lvgl_app_deinit(void)
{
    bk_err_t ret;

    if (lvgl_app_init_flag == 0)
    {
        LOGW("lvgl_app_deinit has deinited or init failed\r\n");
        return;
    }

    ret = media_app_lvgl_close();
    if (ret != BK_OK)
    {
        LOGE("media_app_lvgl_close failed\r\n");
        return;
    }

    lvgl_app_init_flag = 0;
}

#endif

