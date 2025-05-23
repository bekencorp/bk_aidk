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
#include "video_engine.h"
#include "video_log.h"
#include "cli.h"

#if (CONFIG_IMAGE_DEBUG_DUMP)


#if CONFIG_SYS_CPU1
extern bool image_debug_en;
extern int transfer_major_storage_mount(void);
extern int transfer_major_storage_unmount(void);
void cli_bk_video_debug_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    if (argc != 3)
    {
        goto cmd_fail;
    }

    if (os_strcmp(argv[1], "video_debug") == 0)
    {
        if (os_strtoul(argv[2], NULL, 10))
        {
            if (image_debug_en)
            {
                return;
            }
            image_debug_en = true;
            transfer_major_storage_mount();
        }
        else
        {
            if (!image_debug_en)
            {
                return;
            }
            image_debug_en = false;
            transfer_major_storage_unmount();
        }
    }
    else
    {
        goto cmd_fail;
    }

    return;

cmd_fail:
    VDOE_LOGI("agora_video_debug {interval}\n");
}



#define VIDEO_DUMP_CMD_CNT   (sizeof(s_video_dump_commands) / sizeof(struct cli_command))
static const struct cli_command s_video_dump_commands[] =
{
    {"video_dump", "video_dump ...", cli_bk_video_debug_cmd},
};

int video_dump_data_cli_init(void)
{
    return cli_register_commands(s_video_dump_commands, VIDEO_DUMP_CMD_CNT);
}


#endif  //CONFIG_SYS_CPU1

#endif
