#include "media_main.h"
#include "mux_pipeline.h"
#include "modules/image_scale.h"
#include "frame_buffer.h"
#include "modules/tjpgd.h"
#include "modules/jpeg_decode_sw.h"
#include "driver/jpeg_dec_types.h"
#include <os/os.h>
#include <os/mem.h>

#if CONFIG_LVGL
#include "lvgl.h"
#include "lv_vendor.h"
#endif
#include "main_functions.h"
#include "media_audio.h"
#include <stdlib.h>

enum
{
    MEDIA_TS_DATA,
    MEDIA_TS_DATA_COVERT,
};

enum
{
    TS_PROCESS_IDLE,
    TS_PROCESS_WAIT_TASK_READY,
    TS_PROCESS_ING,
};

typedef struct
{
    uint32_t event;
    uint32_t param;
    void *data;
    uint32_t width;
    uint32_t height;
} jdec_data_to_app_data_msg_t;

#define DEC_SEQ_LINE_COUNT 16
#define TS_PIXEL 192
#define AUDIO_PROMPT 1

static void media_ts_notify(void *arg);

static beken_queue_t s_media_ts_queue;
static beken_thread_t s_media_ts_thread;
static mux_callback_t s_release_cb;
static beken_timer_t s_process_timer;
static uint8_t s_ts_process_status = TS_PROCESS_IDLE;
static uint8_t *display_frame = NULL;
static uint16_t *scale_frame = NULL;

#if CONFIG_LVGL
static uint8_t lv_label_flag = 0;

extern lv_obj_t *camera_img;
extern lv_obj_t *my_img;
extern lv_obj_t *label1;
extern lv_obj_t *label2;

LV_IMG_DECLARE(lv_rock);
LV_IMG_DECLARE(lv_paper);
LV_IMG_DECLARE(lv_scissors);
#endif

__attribute__((section(".itcm_sec_code"))) static int rgb565_to_rgb888_convert_ext(uint16_t *src_buffer, uint8_t *dst_buffer, int img_width, int img_height, uint8_t sign)
{
    for (int j = 0; j < img_height; j++)
    {
        uint16_t *src_row = src_buffer + j *img_width;
        uint8_t *dst_row = dst_buffer + j *img_width * 3;

        for (int i = 0; i < img_width; i++)
        {
            uint16_t rgb565 = *src_row++;
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
            uint8_t b = (rgb565 & 0x1F) << 3;

            *dst_row++ = r - (sign ? 128 : 0);
            *dst_row++ = g - (sign ? 128 : 0);
            *dst_row++ = b - (sign ? 128 : 0);
        }
    }

    return 0;
}

static bk_err_t media_ts_thread_send_msg(uint8_t type, uint32_t param, void *data, uint32_t width, uint32_t height)
{
    int32_t ret = BK_OK;

    if (s_media_ts_queue)
    {
        jdec_data_to_app_data_msg_t msg = {0};

        msg.event = type;
        msg.param = param;
        msg.data = data;
        msg.width = width;
        msg.height = height;

        ret = rtos_push_to_queue(&s_media_ts_queue, &msg, 0);//BEKEN_WAIT_FOREVER);

        if (ret != BK_OK)
        {
            appm_loge("push failed");
        }
    }

    return ret;
}

static bk_err_t pipeline_user_data_compl_cb(pipeline_encode_request_t *request, mux_callback_t cb)
{
    complex_buffer_t *tmp_buff = NULL;
    int32_t ret = 0;

    if (!request || !cb)
    {
        appm_loge("request or cb NULL !!!");
        return -1;
    }

    if (!request->buffer)
    {
        appm_loge("request buffer NULL !!!");
        return -1;
    }

    s_release_cb = cb;

    {
        uint32_t print_max_index = 0;

        if (!request->jdec_type)
        {
            print_max_index = ((uint32_t)(PIXEL_HEIGHT / DEC_SEQ_LINE_COUNT * 30));
        }
        else
        {
            print_max_index = 30;
        }

        static int32_t tmp_count = -1;

        if (tmp_count < 0)
        {
            appm_logi("jdec_type %d %dx%d fmt %d state %d ok %d id %d line %d", request->jdec_type, request->width, request->height, request->fmt,
                      request->buffer->state,
                      request->buffer->ok,
                      request->buffer->id,
                      request->buffer->index);
            tmp_count++;
        }
        else if (tmp_count++ < print_max_index)
        {

        }
        else
        {
            //fmt default PIXEL_FMT_YUYV
            appm_logi("jdec_type %d %dx%d fmt %d state %d ok %d id %d line %d", request->jdec_type, request->width, request->height, request->fmt,
                      request->buffer->state,
                      request->buffer->ok,
                      request->buffer->id,
                      request->buffer->index);
            tmp_count = 0;
        }
    }

    tmp_buff = (typeof(tmp_buff))os_malloc(sizeof(*tmp_buff));

    if (!tmp_buff)
    {
        appm_loge("alloc err");
        return -1;
    }

    os_memcpy(tmp_buff, request->buffer, sizeof(*tmp_buff));

    ret = media_ts_thread_send_msg(MEDIA_TS_DATA, 0, tmp_buff, request->width, request->height);

    if (ret)
    {
        cb(tmp_buff);
        tmp_buff = NULL;
    }

    return 0;
}

static bk_err_t pipeline_user_data_reset_cb(mux_callback_t cb)
{
    appm_logi("");
    cb(NULL);
    return 0;
}

static int32_t yuyv_big_endian_to_rgb888(uint8_t *input, uint8_t **output, uint32_t width, uint32_t height, uint8_t *rgb565_data)
{
    int32_t ret = 0;
    uint8_t *tmp_buff1 = NULL, *tmp_buff2 = NULL;
    uint32_t final_pixel = TS_PIXEL;

    do
    {
        tmp_buff1 = psram_malloc(width *height * 2);

        if (!tmp_buff1)
        {
            appm_loge("alloc fail 1");
            ret = -1;
            break;
        }

        appm_logd("covert to rgb565");
        ret = yuyv_to_rgb565_convert(input, tmp_buff1, width, height);
        appm_logd("covert to rgb565 done");

        //            if (s_release_cb && request_buff)
        //            {
        //                s_release_cb(request_buff);
        //                request_buff = NULL;
        //            }

        uint32_t central_edge = (width < height ? width : height);
        tmp_buff2 = psram_malloc(central_edge *central_edge * 2);

        if (!tmp_buff2)
        {
            appm_loge("alloc fail 2");
            ret = -1;
            break;
        }

        appm_logd("cut to center");
        ret = image_center_crop(tmp_buff1, tmp_buff2, width, height, central_edge, central_edge);
        appm_logd("cut to center done");
        psram_free(tmp_buff1);
        tmp_buff1 = NULL;

        appm_logd("scale to %dx%d from %dx%d", final_pixel, final_pixel, central_edge, central_edge);
        image_scale_crop_compress(tmp_buff2, rgb565_data + 2 /* image_scale_crop_compress bug */, central_edge, central_edge, final_pixel, final_pixel);
        appm_logd("scale to %dx%d done", final_pixel, final_pixel);
        psram_free(tmp_buff2);

        tmp_buff2 = psram_malloc(final_pixel *final_pixel * 3);

        if (!tmp_buff2)
        {
            appm_loge("alloc fail 4");
            ret = -1;
            break;
        }

        appm_logd("covert to rgb888 signed");
        rgb565_to_rgb888_convert_ext((uint16_t *)rgb565_data, tmp_buff2, final_pixel, final_pixel, 1);
        appm_logd("covert to rgb888 signed done");

        if (output)
        {
            *output = tmp_buff2;
        }
        else
        {
            ret = -1;
            break;
        }
    }
    while (0);

    if (ret)
    {
        if (tmp_buff2)
        {
            psram_free(tmp_buff2);
            tmp_buff2 = NULL;
        }
    }

    return ret;
}

static int8_t hand_compare(gesture_result_t local_hande, gesture_result_t peer_hand)
{
    const int8_t compare_map[GESTURE_MAX][GESTURE_MAX] =
    {
        [GESTURE_ROCK] = {0, -1, 1},
        [GESTURE_PAPER] = {1, 0, -1},
        [GESTURE_SCISSORS] = {-1, 1, 0},
    };

    return compare_map[local_hande][peer_hand];
}


static void media_ts_thread_task(beken_thread_arg_t data)
{
    extern void tflite_task_init_c(void *arg);
    extern void tflite_process(uint8_t *data, uint32_t len, uint8_t *result);
    int ret = BK_OK;
    uint8_t *final_frame = NULL;
    uint32_t last_dec_index = 0;
    int32_t last_dec_id = -1;
    int8_t ignore_id = -1;
    uint8_t use_pipeline = 0;
    uint8_t *tmp_buff1 = NULL, *tmp_buff2 = NULL;
    complex_buffer_t *request_buff = NULL;
    frame_buffer_t *frame = NULL;
    uint8_t detect_result = GESTURE_MAX;

    appm_logi("");

    audio_init();

    if (CAMERA_TYPE == UVC_CAMERA)
    {
#if CONFIG_MEDIA_PIPELINE
        rtos_delay_milliseconds(2000);
        appm_logi("reg jdec cb");
        bk_jdec_buffer_request_register(PIPELINE_MOD_USER, pipeline_user_data_compl_cb, pipeline_user_data_reset_cb);
#endif
    }
    else if (CAMERA_TYPE == DVP_CAMERA)
    {
        //        ret = bk_jpeg_dec_sw_init(NULL, 0);
        //
        //        if (ret)
        //        {
        //            appm_loge("bk_jpeg_dec_sw_init err");
        //        }
        //
        //        jd_output_format jd_format =
        //        {
        //            .byte_order = JD_LITTLE_ENDIAN,
        //            .format = JD_FORMAT_RGB888,
        //        };
        //
        //        jd_set_output_format(&jd_format);
#if ENABLE_LCD_SHOW_CAMERA
        frame_buffer_fb_init(FB_INDEX_JPEG);
        frame_buffer_fb_register(MODULE_USER, FB_INDEX_JPEG);
#else
        frame_buffer_fb_init(FB_INDEX_DISPLAY);
        frame_buffer_fb_register(MODULE_LCD, FB_INDEX_DISPLAY);
#endif

    }

    tflite_task_init_c(NULL);
    rtos_init_timer(&s_process_timer, 3500, media_ts_notify, NULL);
    rtos_start_timer(&s_process_timer);

    while (1)
    {
        if (CAMERA_TYPE == UVC_CAMERA)
        {
            jdec_data_to_app_data_msg_t msg;

            ret = rtos_pop_from_queue(&s_media_ts_queue, &msg, BEKEN_WAIT_FOREVER);

            if (ret != BK_OK)
            {
                continue;
            }

            switch (msg.event)
            {
            case MEDIA_TS_DATA_COVERT:
                if (TS_PROCESS_WAIT_TASK_READY == s_ts_process_status)
                {
                    s_ts_process_status = TS_PROCESS_ING;
                }

                break;

            case MEDIA_TS_DATA:
            {
                request_buff = (typeof(request_buff))msg.data;

                if (s_ts_process_status != TS_PROCESS_ING)
                {
                    break;
                }

                if (!final_frame)
                {
                    appm_logi("psram_malloc final_frame");
                    final_frame = psram_malloc(msg.width *msg.height * 2);

                    if (!final_frame)
                    {
                        appm_loge("alloc fail final_frame");
                        break;
                    }
                }

                //            if(ignore_id == request_buff->id)
                //            {
                //                break;
                //            }
                //
                //            ignore_id = -1;
                //
                //            if (last_dec_id != request_buff->id)
                //            {
                //                //os_memset(final_frame, 0, msg.width *msg.height * 2);
                //                last_dec_index = 0;
                //                last_dec_id = request_buff->id;
                //            }

                if (request_buff->index != last_dec_index + 1)
                {
                    if (request_buff->index == 1)
                    {
                        appm_logw("index now new, last %d, cover last frame", last_dec_index, request_buff->index);
                        last_dec_index = 0;
                    }
                    else
                    {
                        appm_logw("index not match, last %d current %d, ignore current frame", last_dec_index, request_buff->index);
                        //os_memset(final_frame, 0, msg.width *msg.height * 2);
                        last_dec_index = 0;
                        ignore_id = request_buff->id;
                        break;
                    }
                }

                os_memcpy(final_frame + last_dec_index *PIXEL_WIDTH *DEC_SEQ_LINE_COUNT * 2, request_buff->data, PIXEL_WIDTH *DEC_SEQ_LINE_COUNT * 2);

                last_dec_index = request_buff->index;

                if (last_dec_index != PIXEL_HEIGHT / DEC_SEQ_LINE_COUNT)
                {
                    break;
                }

                appm_logd("completed line index %d covert now !", last_dec_index);
                last_dec_index = 0;

                ret = yuyv_big_endian_to_rgb888(final_frame, &tmp_buff2, msg.width, msg.height, display_frame);

                if (ret)
                {
                    appm_loge("yuyv_big_endian_to_rgb888 err");
                    break;
                }

                appm_logi("try ts");
                tflite_process(tmp_buff2, TS_PIXEL *TS_PIXEL * 3, &detect_result);
                appm_logi("ts done");
                s_ts_process_status = TS_PROCESS_IDLE;
            }
            break;

            default:
                break;
            }
        }
        else if (CAMERA_TYPE == DVP_CAMERA)
        {
#if AUDIO_PROMPT
            appm_logi("ready ?");
            audio_play(TONE_ENUM_GET_READY, 0);

            void *wait_cb(void)
            {
                frame = frame_buffer_fb_display_pop_wait();
                return frame;
            }

            void wait_free_cb(void *arg)
            {
#if ENABLE_LCD_SHOW_CAMERA
                frame_buffer_fb_free(arg, MODULE_USER);
#else
                frame_buffer_fb_direct_free(arg);
#endif
            }

            appm_logi("wait ready end");
            audio_wait_play_end(wait_cb, wait_free_cb, 0);
            appm_logi("ready end");
#else

#if ENABLE_LCD_SHOW_CAMERA
            frame = frame_buffer_fb_read(MODULE_USER);
#else
            frame = frame_buffer_fb_display_pop_wait();
#endif
#endif

            if (!frame)
            {
                appm_loge("frame NULL");
                goto END;
            }

#if AUDIO_PROMPT
            audio_play(TONE_ENUM_PLS_STOP_HAND, 1);
            audio_play(TONE_ENUM_DETECTING, 1);
#else
            if (s_ts_process_status != TS_PROCESS_ING)
            {
                goto END;
            }
#endif
            appm_logd("dvp frame %dx%d_fmt %d len %d", frame->width, frame->height, frame->fmt, frame->length);

            uint32_t start_time = rtos_get_time();

            if (frame->fmt == PIXEL_FMT_YUYV)
            {
                ret = yuyv_big_endian_to_rgb888(frame->frame, &tmp_buff2, frame->width, frame->height, display_frame);

                if (ret)
                {
                    appm_loge("yuyv_big_endian_to_rgb888 err");
                    goto END;
                }
            }
            else if (frame->fmt == PIXEL_FMT_JPEG)
            {
                tmp_buff1 = psram_malloc(frame->width *frame->height * 2);

                if (!tmp_buff1)
                {
                    appm_loge("alloc err %dx%d", frame->width, frame->height);
                    goto END;
                }

                sw_jpeg_dec_res_t jpeg_res = {0};
                ret = bk_jpeg_dec_sw_start_one_time(JPEGDEC_BY_FRAME, frame->frame, tmp_buff1,
                                                    frame->length, frame->width *frame->height * 2,
                                                    &jpeg_res, 0, JD_FORMAT_YUYV, ROTATE_NONE, NULL, NULL);

                if (ret != JDR_OK || JDR_OK != jpeg_res.ok)
                {
                    appm_loge("soft dec err %d %d", ret, jpeg_res.ok);
                    goto END;
                }

                appm_logd("soft jdec %dx%d %d", jpeg_res.pixel_x, jpeg_res.pixel_y, jpeg_res.size);

                ret = yuyv_big_endian_to_rgb888(tmp_buff1, &tmp_buff2, frame->width, frame->height, display_frame);

                if (ret)
                {
                    appm_loge("yuyv_big_endian_to_rgb888 err");
                    goto END;
                }
            }
            else
            {
                appm_loge("fmt err %d %dx%d", frame->fmt, frame->width, frame->height);
                goto END;
            }

            uint32_t end_time = rtos_get_time();
            appm_logi("try ts, covert time %d ms", end_time - start_time);//124ms(soft jpeg dec) + 78ms(covert) with soft covert
            tflite_process(tmp_buff2, TS_PIXEL *TS_PIXEL * 3, &detect_result);
            appm_logi("ts done");

            if (detect_result != GESTURE_MAX)
            {
                appm_logi("+++++++++detect_result = %d", detect_result);

#if AUDIO_PROMPT
                uint8_t my_hand = random() % 3;
                appm_logi("localhand %d peerhand %d", my_hand, detect_result);
                int8_t compare_res = hand_compare(my_hand, detect_result);

                lv_vendor_disp_lock();
                lv_obj_add_flag(label1, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(label2, LV_OBJ_FLAG_HIDDEN);
                lv_label_flag = 1;

                switch (detect_result)
                {
                case GESTURE_ROCK:
                    lv_img_set_src(camera_img, &lv_rock);
                    break;

                case GESTURE_PAPER:
                    lv_img_set_src(camera_img, &lv_paper);
                    break;

                case GESTURE_SCISSORS:
                    lv_img_set_src(camera_img, &lv_scissors);
                    break;
                }

                lv_obj_clear_flag(camera_img, LV_OBJ_FLAG_HIDDEN);

                switch (my_hand)
                {
                case GESTURE_ROCK:
                    lv_img_set_src(my_img, &lv_rock);
                    break;

                case GESTURE_PAPER:
                    lv_img_set_src(my_img, &lv_paper);
                    break;

                case GESTURE_SCISSORS:
                    lv_img_set_src(my_img, &lv_scissors);
                    break;
                }
                lv_obj_clear_flag(my_img, LV_OBJ_FLAG_HIDDEN);
                lv_vendor_disp_unlock();

                audio_play(TONE_ENUM_YOUR_SHOW, 1);

                switch (detect_result)
                {
                case GESTURE_ROCK:
                    audio_play(TONE_ENUM_ROCK, 1);
                    break;

                case GESTURE_PAPER:
                    audio_play(TONE_ENUM_PAPER, 1);
                    break;

                case GESTURE_SCISSORS:
                    audio_play(TONE_ENUM_SCISSORS, 1);
                    break;
                }

                audio_play(TONE_ENUM_MY_SHOW, 1);

                switch (my_hand)
                {
                case GESTURE_ROCK:
                    audio_play(TONE_ENUM_ROCK, 1);
                    break;

                case GESTURE_PAPER:
                    audio_play(TONE_ENUM_PAPER, 1);
                    break;

                case GESTURE_SCISSORS:
                    audio_play(TONE_ENUM_SCISSORS, 1);
                    break;
                }

                if (compare_res > 0)
                {
                    audio_play(TONE_ENUM_YOU_LOSS, 1);
                }
                else if (compare_res < 0)
                {
                    audio_play(TONE_ENUM_YOU_WIN, 1);
                }
                else
                {
                    audio_play(TONE_ENUM_DRAW, 1);
                }
#endif
            }
            else
            {
#if AUDIO_PROMPT
                appm_logi("tone start");
                audio_play(TONE_ENUM_CANT_DET, 1);
                appm_logi("tone done");
#endif
            }
        }

END:

        if (tmp_buff1)
        {
            psram_free(tmp_buff1);
            tmp_buff1 = NULL;
        }

        if (tmp_buff2)
        {
            psram_free(tmp_buff2);
            tmp_buff2 = NULL;
        }

        if (frame)
        {
#if ENABLE_LCD_SHOW_CAMERA
            frame_buffer_fb_free(frame, MODULE_USER);
#else
            frame_buffer_fb_direct_free(frame);
#endif
            frame = NULL;
        }

        if (s_release_cb && request_buff)
        {
            s_release_cb(request_buff);
            request_buff = NULL;
        }

        s_ts_process_status = TS_PROCESS_IDLE;

#if AUDIO_PROMPT
        rtos_delay_milliseconds(5000);
#endif
        if (lv_label_flag) {
            lv_vendor_disp_lock();
            lv_obj_add_flag(camera_img, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(my_img, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(label1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(label2, LV_OBJ_FLAG_HIDDEN);
            lv_label_flag = 0;
            lv_vendor_disp_unlock();
        }
    }
}

static void media_ts_notify(void *arg)
{
    int32_t ret = 0;

    if (s_ts_process_status != TS_PROCESS_IDLE)
    {
        return;
    }

    appm_logi("trigg ts");

    if (CAMERA_TYPE == UVC_CAMERA)
    {
        s_ts_process_status = TS_PROCESS_WAIT_TASK_READY;

        ret = media_ts_thread_send_msg(MEDIA_TS_DATA_COVERT, 0, NULL, 0, 0);

        if (ret)
        {
            appm_loge("push failed");
            s_ts_process_status = TS_PROCESS_IDLE;
        }
    }
    else if (CAMERA_TYPE == DVP_CAMERA)
    {
        s_ts_process_status = TS_PROCESS_ING;
    }
}

int32_t media_ts_main(void)
{
#if ENABLE_TS_PRASE
    int32_t ret = 0;

    appm_logi("");

    display_frame = psram_malloc(TS_PIXEL *TS_PIXEL * 2);

    if (display_frame == NULL)
    {
        appm_loge("%s display_frame malloc failed", __func__);
        return -1;
    }

    ret = rtos_init_queue(&s_media_ts_queue,
                          "s_media_ts_queue",
                          sizeof(jdec_data_to_app_data_msg_t),
                          16);

    if (ret != BK_OK)
    {
        appm_loge("init s_media_ts_queue failed");
        return -1;
    }

    ret = rtos_create_thread(&s_media_ts_thread,
                             BEKEN_DEFAULT_WORKER_PRIORITY - 3,
                             "s_media_ts_thread",
                             (beken_thread_function_t)media_ts_thread_task,
                             1024 * 16,
                             NULL);

    if (ret != BK_OK)
    {
        appm_loge("init s_media_ts_thread failed");
        return -1;
    }

#endif
    return 0;
}
