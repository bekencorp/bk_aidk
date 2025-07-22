#include <os/os.h>
#include <os/str.h>
#include "lcd_act.h"
#include "media_app.h"
#include "lv_vendor.h"
#include "lvgl.h"
#include "lv_img_utility.h"
#include "bk_posix.h"
#include "lv_comm_list.h"
#include "driver/drv_tp.h"
#include <driver/lcd.h>
#include "yuv_encode.h"
#include "media_evt.h"

#define TAG "lvgl_ui"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static lv_vnd_config_t lv_vnd_config = {0};
static lv_obj_t *label = NULL;
static lv_timer_t *label_timer = NULL;
static lv_ft_info_t info;
static lv_style_t style;
static uint32_t *file_content = NULL;
static lv_comm_list_t *g_lv_font_list = NULL;
static uint32_t reset_pos = 0;

typedef struct {
    uint8_t text_type;
    char *text_data;
} lv_text_info_t;

typedef struct {
    char *text_data;
    char *font_data;
    uint32_t current_pos;    // current position
    uint32_t text_length;    // text total length
} lv_font_info_t;

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
    static uint16_t list_empty_count = 0;

    if (!lv_comm_list_is_empty(g_lv_font_list)) {
        lv_font_info_t *font_info = lv_comm_list_front(g_lv_font_list);

        char font_buffer[512] = {0};
        uint32_t temp_pos = os_strlen(lv_label_get_text(label));
        LOGD("temp_pos = %d, current_pos = %d\r\n", temp_pos, font_info->current_pos);

        if (font_info->current_pos < font_info->text_length) {
            if (temp_pos >= 90) {
                reset_pos = font_info->current_pos;
                lv_label_set_text(label, "");
                lv_timer_set_period(label_timer, 400);
                return;
            }

            int char_len = lv_get_utf8_char_length(&font_info->text_data[font_info->current_pos]);
            os_strncpy(font_buffer, font_info->text_data + reset_pos, font_info->current_pos - reset_pos + char_len);
            lv_label_set_text(label, font_buffer);
            lv_timer_set_period(label_timer, 150);
            font_info->current_pos += char_len;
        } else {
            list_empty_count = 0;
            reset_pos = 0;
            lv_timer_set_period(label_timer, 320);
            lv_comm_list_remove(g_lv_font_list, font_info);
        }
    } else {
        list_empty_count++;
        reset_pos = 0;

        if (list_empty_count == 20) {
            lv_label_set_text(label, "");
            lv_timer_del(label_timer);
            label_timer = NULL;
        }
    }
}

bk_err_t lvgl_event_send_data_handle(media_mailbox_msg_t *msg)

{
    lv_text_info_t *text_info = (lv_text_info_t *)msg->param;
    uint32_t text_length = os_strlen(text_info->text_data);
    LOGI("text_length = %d\r\n", text_length);

    if (text_info->text_type == 0) {
        lv_comm_list_clear(g_lv_font_list);
        lv_vendor_disp_lock();
        reset_pos = 0;
        lv_label_set_text(label, "");
        if (label_timer) {
            lv_timer_del(label_timer);
            label_timer = NULL;
        }
        lv_vendor_disp_unlock();
    }

    lv_font_info_t *font = os_malloc(sizeof(lv_font_info_t));
    if (font == NULL) {
        LOGE("%s %d font malloc failed\r\n", __func__, __LINE__);
        return BK_FAIL;
    }

    char *text_data = psram_malloc(text_length);
    if (text_data == NULL) {
        LOGE("%s %d text_data malloc failed\r\n", __func__, __LINE__);
        return BK_FAIL;
    }
    os_memcpy(text_data, text_info->text_data, text_length);

    font->text_data = text_data;
    font->current_pos = 0;
    font->text_length = text_length;

    if (g_lv_font_list) {
        lv_comm_list_append(g_lv_font_list, (void *)font);
    }

    if (label_timer == NULL) {
        lv_vendor_disp_lock();
        label_timer = lv_timer_create(lvgl_label_timer_cb, 150, NULL);
        lv_vendor_disp_unlock();
    }

    return BK_OK;
}

bk_err_t lvgl_event_close_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \r\n", __func__);

    lv_vendor_disp_lock();
    if (label_timer) {
        lv_timer_del(label_timer);
        label_timer = NULL;
    }

    if (label) {
        lv_obj_del(label);
        label = NULL;
    }

    lv_style_reset(&style);

    lv_ft_font_destroy(info.font);

    if (g_lv_font_list) {
        lv_comm_list_free(g_lv_font_list);
        g_lv_font_list = NULL;
    }

    lv_vendor_disp_unlock();

    lv_vendor_stop();

    lcd_display_close();

    if (file_content) {
        psram_free(file_content);
        file_content = NULL;
    }

    return BK_OK;
}

bk_err_t lvgl_event_open_handle(media_mailbox_msg_t *msg)
{
    LOGI("%s \n", __func__);

    lcd_open_t *lcd_open = (lcd_open_t *)msg->param;

    g_lv_font_list = lv_comm_list_new();
    if (!g_lv_font_list) {
        LOGE("%s g_lv_font_list create failed\r\n", __func__);
        return BK_FAIL;
    }

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
#if (CONFIG_LCD_SPI_DEVICE_NUM > 1 || CONFIG_LCD_QSPI_DEVICE_NUM > 1)
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi) * 2;
#else
        lv_vnd_config.lcd_hor_res = ppi_to_pixel_x(lcd_open->device_ppi);
        lv_vnd_config.lcd_ver_res = ppi_to_pixel_y(lcd_open->device_ppi);
#endif
        lv_vnd_config.rotation = ROTATE_NONE;

        lv_vendor_init(&lv_vnd_config);
    }

    lv_vendor_fs_init();

    int fd = open("/simhei_new.ttf", O_RDONLY);
    if (fd < 0) {
        LOGE("file_content malloc failed\r\n");
        lv_vendor_fs_deinit();
        return BK_FAIL;
    }

    int file_len = lv_img_read_filelen("/simhei_new.ttf");
    if (file_len <= 0) {
        LOGE("file len read failed\r\n");
        close(fd);
        lv_vendor_fs_deinit();
        return BK_FAIL;
    }

    file_content = psram_malloc(file_len);
    if (file_content == NULL) {
        LOGE("file_content malloc failed\r\n");
        close(fd);
        lv_vendor_fs_deinit();
        return BK_FAIL;
    }

    uint32_t read_len = read(fd, file_content, file_len);
    LOGI("read_len = %d \r\n", read_len);
    close(fd);
    lv_vendor_fs_deinit();

    lcd_display_open(lcd_open);

#if (CONFIG_TP)
    drv_tp_open(ppi_to_pixel_x(lcd_open->device_ppi), ppi_to_pixel_y(lcd_open->device_ppi), TP_MIRROR_NONE);
#endif

    lv_vendor_disp_lock();

    info.name = "/simhei_new.ttf";
    info.weight = 24;
    info.style = FT_FONT_STYLE_NORMAL;
    info.mem = file_content;
    info.mem_size = file_len;
    if(!lv_ft_font_init(&info)) {
        LV_LOG_ERROR("create failed.");
    }

    lv_style_init(&style);
    lv_style_set_text_font(&style, info.font);
    lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

    label = lv_label_create(lv_scr_act());
    lv_obj_add_style(label, &style, 0);
    lv_label_set_text(label, "欢迎来到BEKEN AI精灵");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 300);
    lv_obj_scroll_to_view(label, LV_ANIM_ON);
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

