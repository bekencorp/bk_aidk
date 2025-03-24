/**
 * @file pan_service.h
 *
 */

#ifndef PAN_SERVICE_H
#define PAN_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int pan_service_init(void);
int cli_pan_demo_init(void);
void bt_start_pan_reconnect(void);
void bk_bt_enter_pairing_mode(uint8_t is_visible);
void pan_show_tx_data_cache_count(void);
int pan_service_deinit(void);
int cli_pan_demo_deinit(void);
void bt_pan_reconnect_failure_handler(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*PAN_SERVICE_H*/
