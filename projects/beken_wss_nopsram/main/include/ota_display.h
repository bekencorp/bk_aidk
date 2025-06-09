#ifndef __OTA_DISPLAY_H__
#define __OTA_DISPLAY_H__
#include <common/bk_include.h>
#include <stdio.h>

bk_err_t bk_ota_image_disp_open(char *filename);
bk_err_t bk_ota_image_disp_close(void);
void media_ui_ota_event_handle(media_mailbox_msg_t *msg);

#endif