#ifndef __AUDIO_DUMP_DATA_H__
#define __AUDIO_DUMP_DATA_H__

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SYS_CPU0

//#define AUDIO_RX_SPK_DATA_DUMP
#ifdef AUDIO_RX_SPK_DATA_DUMP
#include "uart_util.h"
extern static uart_util_t g_audio_spk_uart_util = {0};
#define AUDIO_RX_SPK_DATA_DUMP_UART_ID                       (1)
#define AUDIO_RX_SPK_DATA_DUMP_UART_BAUD_RATE                (2000000)
#define AUDIO_RX_SPK_DATA_DUMP_OPEN()                        uart_util_create(&g_audio_spk_uart_util, AUDIO_RX_SPK_DATA_DUMP_UART_ID, AUDIO_RX_SPK_DATA_DUMP_UART_BAUD_RATE)
#define AUDIO_RX_SPK_DATA_DUMP_CLOSE()                       uart_util_destroy(&g_audio_spk_uart_util)
#define AUDIO_RX_SPK_DATA_DUMP_DATA(data_buf, len)           uart_util_tx_data(&g_audio_spk_uart_util, data_buf, len)
#else
#define AUDIO_RX_SPK_DATA_DUMP_OPEN()
#define AUDIO_RX_SPK_DATA_DUMP_CLOSE()
#define AUDIO_RX_SPK_DATA_DUMP_DATA(data_buf, len)
#endif  //AUDIO_RX_SPK_DATA_DUMP

#endif //ONFIG_SYS_CPU0

/* API */
int audio_dump_data_cli_init(void);

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_DUMP_DATA_H__ */
