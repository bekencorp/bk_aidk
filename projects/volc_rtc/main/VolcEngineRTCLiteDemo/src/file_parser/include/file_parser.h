
#ifndef __MEDIA_PARSER_H__
#define __MEDIA_PARSER_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * The definition of the ago_av_data_type_e enum.
 */
typedef enum {
  /**
     * 0: YUV420
     */
  MEDIA_FILE_TYPE_YUV420 = 0,
  /**
     * 1: H264
     */
  MEDIA_FILE_TYPE_H264 = 1,
  /**
     * 2: JPEG
     */
  MEDIA_FILE_TYPE_JPEG = 2,
  /**
     * 10: PCM
     */
  MEDIA_FILE_TYPE_PCM = 10,
  /**
     * 11: OPUS
     */
  MEDIA_FILE_TYPE_OPUS = 11,
  /**
    * 12: PCMA
    */
  MEDIA_FILE_TYPE_PCMA = 12,
  /**
    * 13: PCMU
    */
  MEDIA_FILE_TYPE_G711 = 13,
  /**
     * 14: G722
     */
  MEDIA_FILE_TYPE_G722 = 14,
  /**
     * 15: AACLC
     */
  MEDIA_FILE_TYPE_AACLC = 15,
  /**
     * 16: HEAAC
     */
  MEDIA_FILE_TYPE_HEAAC = 16,
} media_file_type_e;

typedef struct {
  union {
    struct {
      int sampleRateHz;
      int numberOfChannels;
      int framePeriodMs;
    } audio_cfg;
  } u;
} parser_cfg_t;

typedef struct {
  int type;
  uint8_t *ptr;
  uint32_t len;

  union {
    struct {
      bool is_key_frame;
    } video;

    struct {
    } audio;
  } u;
} frame_t;

void *create_file_parser(media_file_type_e type, const char *path, parser_cfg_t *p_parser_cfg);
int file_parser_obtain_frame(void *p_parser, frame_t *p_frame);
int file_parser_release_frame(void *p_parser, frame_t *p_frame);
void destroy_file_parser(void *p_parser);

#endif /* __MEDIA_PARSER_H__ */