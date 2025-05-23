#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <key_main.h>
#include <key_adapter.h>


void bk_key_register_wakeup_source(void);

void bk_key_service_init(void);


void volume_init(void);
#ifdef __cplusplus
}
#endif