#ifndef __AUDIO_TRANSFER_H__
#define __AUDIO_TRANSFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "audio_config.h"

typedef int (*user_audio_tx_data_func)(unsigned char *data, unsigned int size);


/* API */
void audio_tras_register_tx_data_func(user_audio_tx_data_func func);
int send_audio_data_to_net_transfer(uint8_t *data, unsigned int len);
bk_err_t audio_tras_init(void);
bk_err_t audio_tras_deinit(void);

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_TRANSFER_H__ */
