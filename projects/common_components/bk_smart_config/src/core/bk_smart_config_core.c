// Copyright 2020-2025 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <common/sys_config.h>
#include <components/log.h>
#include <modules/wifi.h>
#include <components/netif.h>
#include <components/event.h>
#include <string.h>
#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "components/bk_uid.h"
#include "bk_genie_comm.h"
#include "wifi_boarding_utils.h"
#include "bk_smart_config.h"
#if CONFIG_NET_PAN
#include "pan_service.h"
#endif
#include "app_event.h"
#include "boarding_service.h"
#include "components/bluetooth/bk_dm_bluetooth.h"
#include "bk_factory_config.h"
#include "driver/trng.h"
#if CONFIG_PSRAM_AS_SYS_MEMORY
#include "mbedtls/platform.h"
#endif

#define TAG "bk_sconf_core"
bool smart_config_running = false;
static beken2_timer_t network_reconnect_tmr = {0};
extern bool agora_runing;
uint8_t network_disc_evt_posted = 0;
bool first_time_for_network_provisioning = true;
bool first_time_for_network_reconnect = true;

void network_reconnect_check_status(void)
{
    BK_LOGI(TAG,"reconnect timeout!\n");
    if (network_disc_evt_posted == 0) {
        app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
        network_disc_evt_posted = 1;
    }
}

void network_reconnect_start_timeout_check(uint32_t timeout)
{
  bk_err_t err = kNoErr;
  uint32_t clk_time;

  clk_time = timeout*1000;		//timeout unit: seconds

  if (rtos_is_oneshot_timer_init(&network_reconnect_tmr)) {
     BK_LOGI(TAG,"network provisioning status timer reload\n");
    rtos_oneshot_reload_timer(&network_reconnect_tmr);
  } else {
    err = rtos_init_oneshot_timer(&network_reconnect_tmr, clk_time, (timer_2handler_t)network_reconnect_check_status, NULL, NULL);
    BK_ASSERT(kNoErr == err);

    err = rtos_start_oneshot_timer(&network_reconnect_tmr);
    BK_ASSERT(kNoErr == err);
     BK_LOGI(TAG,"network provisioning status timer:%d\n", clk_time);
  }

  return;
}


void network_reconnect_stop_timeout_check(void)
{
  bk_err_t ret = kNoErr;

  if (rtos_is_oneshot_timer_init(&network_reconnect_tmr)) {
    if (rtos_is_oneshot_timer_running(&network_reconnect_tmr)) {
      ret = rtos_stop_oneshot_timer(&network_reconnect_tmr);
      BK_ASSERT(kNoErr == ret);
    }

    ret = rtos_deinit_oneshot_timer(&network_reconnect_tmr);
    BK_ASSERT(kNoErr == ret);
  }
}

int is_wifi_sta_auto_restart_info_saved(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	if (info.flag == 0x71l)
		return 0;
	else
		return 1;
}

int bk_sconf_is_wifi_sta_configured(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	if ((info.flag & 0x71l) == 0x71l)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int bk_sconf_is_net_pan_configured(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
#if CONFIG_NET_PAN
	if ((info.flag & 0x74l) == 0x74l)
	{
		return 1;
	}
	else
#endif
	{
		return 0;
	}
}

void demo_erase_network_auto_reconnect_info(void)
{
	BK_FAST_CONNECT_D info_tmp = {0};

	bk_config_write("d_network_id", (const void *)&info_tmp, sizeof(BK_FAST_CONNECT_D));
}

extern int demo_sta_app_init(char *oob_ssid, char *connect_key);
extern int demo_softap_app_init(char *ap_ssid, char *ap_key, char *ap_channel);
int demo_network_auto_reconnect(bool val)	//val true means from disconnect to reconnecting
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	/*0x01110001:sta, 0x01110010:softap, 0x01110100:pan*/
	if ((info.flag & 0x71l) == 0x71l) {
		if (val == false) {
			network_reconnect_stop_timeout_check();
			network_reconnect_start_timeout_check(50);    //50s
			app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
			network_disc_evt_posted = 0;
		}
		demo_sta_app_init((char *)info.sta_ssid, (char *)info.sta_pwd);
		return 0x71l;
	}
	if ((info.flag & 0x72l)  == 0x72l) {
		demo_softap_app_init((char *)info.ap_ssid, (char *)info.ap_pwd, NULL);
		return 0x72l;
	}
#if CONFIG_NET_PAN
	if ((info.flag & 0x74l) == 0x74l) {
		if (val == false) {
			network_reconnect_stop_timeout_check();
			network_reconnect_start_timeout_check(50);    //50s
			app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
			network_disc_evt_posted = 0;
		}
		pan_service_init();
		bt_start_pan_reconnect();
		return 0x74l;
	}
#endif
#if CONFIG_BK_MODEM
	if ((info.flag & 0x78l) == 0x78l) {
		if (val == false) {
			network_reconnect_stop_timeout_check();
			network_reconnect_start_timeout_check(50);    //50s
			app_event_send_msg(APP_EVT_RECONNECT_NETWORK, 0);
			network_disc_evt_posted = 0;
		}
extern bk_err_t bk_modem_init(void);
		bk_modem_init();
		return 0x78l;
	}
#endif
	return 0;
}

int demo_save_network_auto_restart_info(netif_if_t type, void *val)
{
	BK_FAST_CONNECT_D info_tmp = {0};
	__maybe_unused wifi_ap_config_t *ap_config = NULL;
	__maybe_unused wifi_sta_config_t *sta_config = NULL;

	bk_config_read("d_network_id", (void *)&info_tmp, sizeof(BK_FAST_CONNECT_D));
	if ((info_tmp.flag & 0xf0l) != 0x70l) {
		BK_LOGI(TAG, "erase network provisioning info, %x\r\n", info_tmp.flag);
		info_tmp.flag = 0x70l;
	}
	if (type == NETIF_IF_STA) {
		info_tmp.flag |= 0x71l;
		sta_config = (wifi_sta_config_t *)val;
		os_memset((char *)info_tmp.sta_ssid, 0x0, 33);
		os_memset((char *)info_tmp.sta_pwd, 0x0, 65);
		os_strcpy((char *)info_tmp.sta_ssid, (char *)sta_config->ssid);
		os_strcpy((char *)info_tmp.sta_pwd, (char *)sta_config->password);
	} else if (type == NETIF_IF_AP) {
		info_tmp.flag |= 0x72l;
		ap_config = (wifi_ap_config_t *)val;
		os_memset((char *)info_tmp.ap_ssid, 0x0, 33);
		os_memset((char *)info_tmp.ap_pwd, 0x0, 65);
		os_strcpy((char *)info_tmp.ap_ssid, (char *)ap_config->ssid);
		os_strcpy((char *)info_tmp.ap_pwd, (char *)ap_config->password);
#if CONFIG_NET_PAN
	} else if (type == NETIF_IF_PAN) {
		info_tmp.flag |= 0x74l;
#endif
#if CONFIG_BK_MODEM
	} else if (type == NETIF_IF_PPP) {
		info_tmp.flag |= 0x78l;
#endif
	} else
		return -1;
	bk_config_write("d_network_id", (const void *)&info_tmp, sizeof(BK_FAST_CONNECT_D));

	return 0;
}

#if CONFIG_NET_PAN
static int bk_sconf_reselect_pan(void)
{
	BK_FAST_CONNECT_D info = {0};

	bk_config_read("d_network_id", (void *)&info, sizeof(BK_FAST_CONNECT_D));
	if (info.flag & 0x74l) {
		BK_LOGI(TAG, "%s\r\n", __func__);
		bk_wifi_sta_stop();
		bk_bluetooth_init();
		pan_service_init();
		bt_start_pan_reconnect();
		return 1;
	}
	return 0;
}
#endif

#if CONFIG_BK_BOARDING_SERVICE
void bk_sconf_gotip_and_startup_agent_by_ble(netif_if_t type)
{
    bk_genie_msg_t msg;
    //inform beken apk netif got ip
    msg.event = BOARDING_OP_STATION_START;
    //for BOARDING_OP_STATION_START,msg.param:
    //0 means start sta connect
    //low 16bit non-zero means got ip and need to notify apk, high 16bit store netif_index
    msg.param = ((type << 16) & 0xffff0000) | 0x1;
    bk_genie_send_msg(&msg);
#if CONFIG_STA_AUTO_RECONNECT
    if (!first_time_for_network_provisioning) {
        BK_LOGI(TAG, "first_time_for_network_provisioning\r\n");
        goto skip_agent_request;
    }
#endif
#if 1//!CONFIG_BK_DEV_STARTUP_AGENT
    msg.event = BOARDING_OP_SET_AGENT_INFO;
    bk_genie_send_msg(&msg);
#endif
    return;
#if CONFIG_STA_AUTO_RECONNECT
skip_agent_request:
    bk_sconf_trans_start();
#endif
}
#endif

static int bk_sconf_netif_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    netif_event_got_ip4_t *got_ip;
    __maybe_unused wifi_sta_config_t sta_config = {0};

    switch (event_id)
    {
        case EVENT_NETIF_GOT_IP4:
            network_disc_evt_posted = 0;
            got_ip = (netif_event_got_ip4_t *)event_data;
            BK_LOGI(TAG, "netif_idx %d\r got ip\n", got_ip->netif_if);
#if CONFIG_BK_MODEM
            {
             extern void ping_start(char* target_name, uint32_t times, size_t size);
             ping_start("baidu.com", 4, 0);
            }
#endif

            if (smart_config_running)
            {
                app_event_send_msg(APP_EVT_NETWORK_PROVISIONING_SUCCESS, 0);
                bk_wifi_sta_get_config(&sta_config);
                demo_save_network_auto_restart_info(got_ip->netif_if, &sta_config);
#if CONFIG_BK_BOARDING_SERVICE
                bk_sconf_gotip_and_startup_agent_by_ble(got_ip->netif_if);
#endif
            }
            else
            {
                app_event_send_msg(APP_EVT_RECONNECT_NETWORK_SUCCESS, 0);
                bk_sconf_trans_start();
            }

            break;
        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}

static int bk_sconf_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    wifi_event_sta_disconnected_t *sta_disconnected;
    wifi_event_sta_connected_t *sta_connected;

    switch (event_id)
    {
        case EVENT_WIFI_STA_CONNECTED:
            sta_connected = (wifi_event_sta_connected_t *)event_data;
            BK_LOGI(TAG, "STA connected to %s\n", sta_connected->ssid);
            break;

        case EVENT_WIFI_STA_DISCONNECTED:
            sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
            BK_LOGI(TAG, "STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
            /*drop local generated disconnect event by user*/
            if ((sta_disconnected->disconnect_reason == WIFI_REASON_DEAUTH_LEAVING &&
				sta_disconnected->local_generated == 1) ||
				(sta_disconnected->disconnect_reason == WIFI_REASON_RESERVED))
			break;
#if CONFIG_STA_AUTO_RECONNECT
            if (bk_sconf_is_net_pan_configured() && !smart_config_running) {
#if CONFIG_NET_PAN
			bk_sconf_reselect_pan();
#endif
            } else
#endif
            {
			if (network_disc_evt_posted == 0) {
				if (smart_config_running == false)
					app_event_send_msg(APP_EVT_RECONNECT_NETWORK_FAIL, 0);
				else {
					app_event_send_msg(APP_EVT_NETWORK_PROVISIONING_FAIL, 0);
				}
				network_disc_evt_posted = 1;
			}
#if CONFIG_STA_AUTO_RECONNECT
			bk_wifi_sta_connect();
#endif
            }
            break;

        default:
            BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
            break;
    }

    return BK_OK;
}


void event_handler_init(void)
{
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, bk_sconf_wifi_event_cb, NULL));
    BK_LOG_ON_ERR(bk_event_register_cb(EVENT_MOD_NETIF, EVENT_ID_ALL, bk_sconf_netif_event_cb, NULL));
}

void bk_sconf_prepare_for_smart_config(void)
{
    smart_config_running = true;
    first_time_for_network_reconnect = true;
#if CONFIG_STA_AUTO_RECONNECT
    first_time_for_network_provisioning = true;
#endif
    app_event_send_msg(APP_EVT_NETWORK_PROVISIONING, 0);
    network_reconnect_stop_timeout_check();
    bk_sconf_trans_stop();
    bk_wifi_sta_stop();
#if CONFIG_BK_MODEM
extern bk_err_t bk_modem_deinit(void);
    bk_modem_deinit();
#endif
#if !CONFIG_STA_AUTO_RECONNECT
    demo_erase_network_auto_reconnect_info();
    bk_sconf_erase_agent_info();
#endif

#if CONFIG_NET_PAN && !CONFIG_A2DP_SINK_DEMO && !CONFIG_HFP_HF_DEMO
    bk_bt_enter_pairing_mode(0);
#else
    BK_LOGW(TAG, "%s pan disable !!!\n", __func__);
#endif

#if CONFIG_BK_BOARDING_SERVICE
    extern bool ate_is_enabled(void);

    if (!ate_is_enabled())
    {
        bk_genie_boarding_init();
        wifi_boarding_adv_start();
    }
    else
    {
        BK_LOGW(TAG, "%s ATE is enable, ble will not enable!!!!!!\n", __func__);
    }
#endif
}

#if CONFIG_PSRAM_AS_SYS_MEMORY
void* bk_sconf_psram_calloc(size_t num, size_t size) {
    if (size && num > (~(size_t) 0) / size)
        return NULL;
    return psram_zalloc(num * size);
}

void bk_sconf_psram_free(void* ptr) {
    psram_free(ptr);
}
#endif

int bk_smart_config_init(void)
{
    int flag;

#if CONFIG_PSRAM_AS_SYS_MEMORY
    mbedtls_platform_set_calloc_free(bk_sconf_psram_calloc, bk_sconf_psram_free);
#endif
    event_handler_init();
    flag = demo_network_auto_reconnect(false);

    if (flag != 0x71l
#if CONFIG_NET_PAN
        && flag != 0x74l
#endif
#if CONFIG_BK_MODEM
	 && flag != 0x78l
#endif
    ) {
        bk_sconf_prepare_for_smart_config();
    }
    else
    {
#if CONFIG_NET_PAN
        if (flag != 0x74l)
#endif
        {
#if CONFIG_NET_PAN && !(CONFIG_A2DP_SINK_DEMO || CONFIG_HFP_HF_DEMO)
            bk_bluetooth_deinit();
#endif
        }
    }

    return 0;
}

#define CONFIG_NETWORK_TASK_PRIORITY 4
static beken_thread_t config_network_thread_handle = NULL;
void prepare_config_network_main(void)
{
    if(bk_bluetooth_init())
    {
        BK_LOGE(TAG, "bluetooth init err\n");
    }
    bk_sconf_prepare_for_smart_config();

    config_network_thread_handle = NULL;
    rtos_delete_thread(NULL);
}

void bk_sconf_start_to_config_network(void)
{
    int ret = 0;

    if (config_network_thread_handle) {
        BK_LOGW(TAG, "Config network ongoing, please try later!\n");
        return;
    }
    BK_LOGW(TAG, "Start to config network!\n");
#if CONFIG_PSRAM_AS_SYS_MEMORY
    ret = rtos_create_psram_thread(&config_network_thread_handle,
                                CONFIG_NETWORK_TASK_PRIORITY,
                                "wifi_config_network",
                                (beken_thread_function_t)prepare_config_network_main,
                                4096,
                                (beken_thread_arg_t)0);
#else
    ret = rtos_create_thread(&config_network_thread_handle,
                                CONFIG_NETWORK_TASK_PRIORITY,
                                "wifi_config_network",
                                (beken_thread_function_t)prepare_config_network_main,
                                4096,
                                (beken_thread_arg_t)0);
#endif
    if (ret != kNoErr)
    {
        BK_LOGE(TAG, "wifi config network task fail: %d\r\n", ret);
        config_network_thread_handle = NULL;
    }
}

void bk_smart_config_cli(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    demo_erase_network_auto_reconnect_info();
}

uint8_t bk_sconf_get_supported_engine(void)
{
#ifdef CONFIG_AGORA_IOT_SDK
    return 0;
#elif CONFIG_VOLC_RTC_EN
    return 1;
#elif CONFIG_BK_WSS_TRANS
    //TODO, will be changed to 2
    return 3;
#else // 3 means device startup agent
    return 3;
#endif
}

uint8_t * bk_sconf_get_supported_network(uint8_t *len)
{
    uint8_t tmp_val[3] = {0}, i = 0, *val = NULL;

#ifdef CONFIG_WIFI_ENABLE
    tmp_val[i++] = 0;
#endif
#ifdef CONFIG_BK_MODEM
    tmp_val[i++]  = 1;
#endif
#ifdef CONFIG_NET_PAN
    tmp_val[i++]  = 2;
#endif
    val = os_zalloc(i);
    os_memcpy(val, tmp_val, i);
    *len = i;
    return val;
}

