#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <components/shell_task.h>
#include <components/event.h>
#include <components/netif_types.h>
#include "bk_rtos_debug.h"
#include "aud_intf.h"
#include "aud_intf_types.h"
#include <modules/audio_process.h>
#include "cli.h"
#include "audio_dump_data.h"

#if (CONFIG_SYS_CPU0)
// #define AGORA_RTC_CMD_CNT   (sizeof(s_agora_rtc_commands) / sizeof(struct cli_command))

// extern void cli_agora_rtc_test_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);

// static const struct cli_command s_agora_rtc_commands[] =
// {
//     {"agora_test", "agora_test ...", cli_agora_rtc_test_cmd},
// };

// int agora_rtc_cli_init(void)
// {
//     return cli_register_commands(s_agora_rtc_commands, AGORA_RTC_CMD_CNT);
// }
#endif
