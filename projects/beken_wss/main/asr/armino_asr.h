/*************************************************************
 *
 * This is a part of the Agora Media Framework Library.
 * Copyright (C) 2021 Agora IO
 * All rights reserved.
 *
 *************************************************************/
#ifndef __ARMINO_ASR_H__
#define __ARMINO_ASR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    unsigned char *data;
    unsigned int size;
} asr_data_t;


/* API */
bk_err_t armino_asr_open(void);
bk_err_t armino_asr_close(void);
//bk_err_t audio_tras_deinit(void);

#ifdef __cplusplus
}
#endif
#endif /* __ARMINO_ASR_H__ */
