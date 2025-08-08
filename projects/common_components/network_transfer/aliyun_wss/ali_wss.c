#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "ali_wss.h"
#include "app_event.h"
#include <modules/wifi.h>
#include "modules/wifi_types.h"
#if CONFIG_DEBUG_DUMP
#include "debug_dump.h"
extern bool rx_spk_data_flag;
#endif//CONFIG_DEBUG_DUMP

#define TAG "ALI_WSS"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define WSS_TX_PER_PACKET_SIZE (8*1024)
#define WSS_RX_PER_PACKET_SIZE (8*1024)
#define WSS_TX_SEND_TIMEOUT (10*1000)

transport websocket_start(void)
{

  char *host = c_mmi_get_wss_host();
  char *api = c_mmi_get_wss_api();
  char *port = c_mmi_get_wss_port();
  char *update_header = c_mmi_get_wss_header();
  int port_num = os_strtoul(port, NULL, 10);
  char url[256];
  snprintf((char *)url, sizeof(url), "%s://%s:%d%s", "wss",
           host, port_num, api);
  LOGI("websocket server url=%s header=%s\r\n", url, update_header);
  websocket_client_input_t websocket_cfg = {0};
  websocket_cfg.uri = url;
  websocket_cfg.headers = update_header;
  websocket_cfg.ws_event_handler = ali_wss_event_handler;

  transport client = websocket_client_init(&websocket_cfg);
  if(websocket_client_start(client)) {
	  LOGE("%s fail\r\n", __func__);
	  websocket_client_destroy(client);
      return NULL;
  }

  return client;
}

int websocket_send_data(DummyHandle *handle, uint8_t data_type, const uint8_t *audioData, size_t dataSize)
{
  if (!audioData || !dataSize)
  {
    LOGE("Invalid parameters\r\n");
    return 0;
  }
  if (handle && handle->disconnecting_count)
  {
    LOGE("Websocket has been destroyed or disconnecting\r\n");
    return 0;
  }
  int bytesWritten = websocket_client_send_with_opcode(handle->clientHandler, data_type, audioData, dataSize, WSS_TX_SEND_TIMEOUT);
  if (bytesWritten < 0)
  {
    LOGE("Failed to send data， %d\r\n", bytesWritten);
    return 0;
  }
  return bytesWritten;
}

int dummy_play_data_handle(unsigned char *data, unsigned int size)
{
    bk_err_t ret = BK_OK;

    #if CONFIG_DEBUG_DUMP
    if(rx_spk_data_flag)
    {
        uint32_t decoder_type = bk_aud_get_decoder_type();
        uint8_t dump_file_type = dbg_dump_get_dump_file_type((uint8_t)decoder_type);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DUMP_FILE_TYPE(DUMP_TYPE_RX_SPK,0,dump_file_type);
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_LEN(DUMP_TYPE_RX_SPK,0,size);
        DEBUG_DATA_DUMP_UPDATE_HEADER_TIMESTAMP(DUMP_TYPE_RX_SPK);
        #if CONFIG_DEBUG_DUMP_DATA_TYPE_EXTENSION
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_SAMP_RATE(DUMP_TYPE_RX_SPK,0,bk_aud_get_dac_sample_rate());
        DEBUG_DATA_DUMP_UPDATE_HEADER_DATA_FLOW_FRAME_IN_MS(DUMP_TYPE_RX_SPK,0,bk_aud_get_dec_frame_len_in_ms());
        #endif
        DEBUG_DATA_DUMP_BY_UART_HEADER(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_UPDATE_HEADER_SEQ_NUM(DUMP_TYPE_RX_SPK);
        DEBUG_DATA_DUMP_BY_UART_DATA(data, size);
    }
    #endif//CONFIG_DEBUG_DUMP
    ret = bk_aud_intf_write_spk_data((uint8_t *)data, (uint32_t)size);
    if (ret != BK_OK)
    {
        LOGE("write spk data fail \r\n");
    }

    return ret;
}

void dummy_play_data_timeout_check(void *param)
{
	DummyHandle *handle = (DummyHandle *) param;
	if(handle == NULL) {
		LOGE("%s client null...\n", __func__);
		return;
	}
	int size = WSS_RX_PER_PACKET_SIZE;
	uint8_t *packet = psram_malloc(size);
	int len = 0;
	if (packet != NULL && (size <= bk_aud_intf_get_dec_rb_free_size()))
	{
		len = c_mmi_get_player_data(packet, size);
		if (len  && handle->dummy_play_running) {
			LOGD("Play data coming...\n");
			dummy_play_data_handle(packet, len);
		} else {
			LOGD("Play buffer empty, waiting for data...\n");
		}
	}
	free(packet);
}

void dummy_play_data_start_check(uint32_t timeout, void *param)
{
	bk_err_t err = kNoErr;
	DummyHandle *handle = (DummyHandle *) param;
	if(handle == NULL) {
		LOGE("%s client null...\n", __func__);
		return;
	}
	LOGE("%s dec:%s\n", __func__, handle->audio_info.decoding_type);
	err = rtos_init_timer(&handle->dummy_play_tmr, timeout, (timer_handler_t)dummy_play_data_timeout_check, param);

	BK_ASSERT(kNoErr == err);
	err = rtos_start_timer(&handle->dummy_play_tmr);
	BK_ASSERT(kNoErr == err);
	LOGI("Play timer status timer:%d\n", timeout);

	return;
}

void dummy_play_data_stop_timeout_check(beken_timer_t *data_read_tmr)
{
	if(!data_read_tmr->handle) {
		LOGE("data_read_tmr deinit already...\n");
		return;
	}

	if (rtos_is_timer_init(data_read_tmr)) {
		if (rtos_is_timer_running(data_read_tmr))
		{
			rtos_stop_timer(data_read_tmr);
		}
	rtos_deinit_timer(data_read_tmr);
	}
}

void dummy_record_data_send_task(void *param)
{
	DummyHandle *handle = (DummyHandle *) param;
	if(handle == NULL) {
		LOGE("%s client null...\n", __func__);
		return;
	}
	handle->dummy_record_task = 1;
	LOGI("%s, enc:%s\r\n", __func__, handle->audio_info.encoding_type);
	while (handle->dummy_record_task) {
		if (handle->disconnecting_count == 0) {
			int size = WSS_TX_PER_PACKET_SIZE;
			uint8_t data_type = WS_DATA_TYPE_BINARY;
			uint8_t *packet = psram_malloc(size);
			int len = 0;
			if (packet != NULL)
			{
				len = c_mmi_get_send_data(&data_type, packet, size);
				if (len) {
					websocket_send_data(handle, data_type, packet, len);
					if (handle && handle->audio_info.enc_samp_interval)
						rtos_delay_milliseconds(handle->audio_info.enc_samp_interval);
				} else {
					LOGD("Send buffer empty, waiting for data...\n");
					rtos_delay_milliseconds(5);
				}
			}
			free(packet);
		}
		else
			rtos_delay_milliseconds(5);
	}
	LOGI("%s, exit\r\n", __func__);
	rtos_delete_thread(NULL);
}

int dummy_record_data_task_init(DummyHandle *handle)
{
	int ret = rtos_create_thread(&handle->dummy_record_thread,
								BEKEN_DEFAULT_WORKER_PRIORITY - 1,
								"record_send",
								(beken_thread_function_t)dummy_record_data_send_task,
								1024 * 6,
								(beken_thread_arg_t)handle);

	if (ret != BK_OK)
	{
		LOGE("%s, init thread failed\r\n", __func__);
		return BK_FAIL;
	}
	return BK_OK;

}

int dummy_record_data_send(unsigned char *data_ptr, unsigned int data_len)
{
	int rval = 0;
	if (dummy_record_work(get_client_handle())) {
	    rval = c_mmi_put_recorder_data(data_ptr, data_len);
	    LOGD("tx audio, data_len=%d rval=%d\r\n", (int)data_len, rval);
	    if (rval < 0)
	    {
	        LOGE("Failed to send record data, %d\n", rval);
	        return BK_FAIL;
	    }
	}
    return rval;
}

void dummy_lcd_show(void *tips, void *data)
{
#if CONFIG_SINGLE_SCREEN_FONT_DISPLAY
	extern uint8_t lvgl_app_init_flag;
	if (lvgl_app_init_flag == 1) {
		text_info_t *info = os_zalloc(sizeof(text_info_t));
		if (info == NULL) {
			LOGE("%s info malloc fail\r\n", __func__);
			return;
		}
		if (os_strncmp("ASR C", tips, 5) == 0)
			info->text_type = 0;
		else if (os_strncmp("ASR", tips, 3) == 0)
			info->text_type = 0;
		else if (os_strncmp("LLM C", tips, 5) == 0)
			info->text_type = 1;
		else if(os_strncmp("LLM", tips, 3) == 0)
			info->text_type = 1;
		info->text_data = os_strdup(data);
		media_app_lvgl_send_data((text_info_t *)info);
		free(info->text_data);
		free(info);
	}
#endif
	return;
}

void dummy_record_start(DummyHandle *handle)
{
	handle->dummy_record_running = 1;
}

void dummy_record_stop(DummyHandle *handle)
{
	handle->dummy_record_running = 0;
}

int dummy_record_work(DummyHandle *handle)
{
	return handle->dummy_record_running;
}

void dummy_play_start(DummyHandle *handle)
{
	handle->dummy_play_running = 1;
}

void dummy_play_stop(DummyHandle *handle)
{
	handle->dummy_play_running = 0;
}

DummyHandle *dummy_wss_init(audio_info_t *audio_info)
{
	DummyHandle *handle = os_zalloc(sizeof(DummyHandle));
	if (handle ==  NULL)
	{
		LOGE("%s, alloc DummyHandle fail\n", __func__);
		return NULL;
	}
	os_memcpy(&handle->audio_info, audio_info, sizeof(audio_info_t));
	LOGE("%s, enc:%s dec:%s enc_rate:%d dec_rate:%d enc_inv:%d dec_inv:%d enc_size:%d dec_size:%d\n", __func__,
		handle->audio_info.encoding_type, handle->audio_info.decoding_type,
		handle->audio_info.adc_samp_rate, handle->audio_info.dac_samp_rate,
		handle->audio_info.enc_samp_interval, handle->audio_info.dec_samp_interval,
		handle->audio_info.enc_node_size, handle->audio_info.dec_node_size);
	handle->disconnecting_count = 1;
	handle->clientHandler = websocket_start();
	if (handle->clientHandler == NULL)
	{
		LOGE("%s, startWebsocket fail\n", __func__);
		os_free(handle);
		return NULL;
	}
	dummy_record_data_task_init(handle);
	dummy_play_data_start_check(handle->audio_info.dec_samp_interval, handle);
	return handle;
}

void dummy_wss_destroy(DummyHandle *handle)
{
	if(handle == NULL) {
		BK_LOGE(TAG, "%s handle aleady null\r\n", __func__);
		return;
	}

	dummy_play_data_stop_timeout_check(&handle->dummy_play_tmr);
	handle->dummy_record_task = 0;
	if(handle->clientHandler) {
		LOGE("%s stop websocket client\r\n", __func__);
		handle->disconnecting_count++;
		handle->clientHandler->ws_event_handler = NULL;
		websocket_client_destroy(handle->clientHandler);
		handle->clientHandler = NULL;
	} else {
		LOGE("%s, websocket already free\r\n", __func__);
	}
	os_memset(handle, 0, sizeof(DummyHandle));
	if (handle) {
		os_free(handle);
	}
	return;
}
