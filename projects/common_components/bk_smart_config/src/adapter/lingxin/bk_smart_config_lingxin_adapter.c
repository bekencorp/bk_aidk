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
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include "components/webclient.h"
#include "cJSON.h"
#include "components/bk_uid.h"
#include "bk_genie_comm.h"
#include "bk_smart_config_lingxin_adapter.h"
#include "bk_smart_config.h"
#include "app_event.h"
#include "bk_factory_config.h"
#include "wifi_boarding_utils.h"
#include <stdio.h>

#define TAG "bk_sconf_lingxin"

extern void beken_auto_run(void);
void bk_sconf_trans_start(void)
{
     beken_auto_run();
}

void bk_sconf_trans_stop(void)
{
}

int bk_sconf_post_nfc_id(uint8_t *nfc_id)
{
    return 0;
}

void bk_sconf_begin_to_switch_ir_mode(void)
{
}

uint16_t bk_sconf_send_agent_info(char *payload, uint16_t max_len)
{
    return 0;
}

void  bk_sconf_prase_agent_info(char *payload, uint8_t reset)
{
}