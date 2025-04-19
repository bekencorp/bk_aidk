#pragma once

#include "wifi_boarding_utils.h"

typedef struct
{
    ble_boarding_info_t boarding_info;
    uint16_t channel;
} bk_genie_boarding_info_t;


int bk_genie_boarding_init(void);
int bk_genie_boarding_deinit(void);
void bk_genie_boarding_event_notify(uint16_t opcode, int status);
int wifi_boarding_notify(uint8_t *data, uint16_t length);
void bk_genie_boarding_event_notify_with_data(uint16_t opcode, int status, char *payload, uint16_t length);

