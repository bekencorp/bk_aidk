
#ifndef __DEFUALT_RTC_API_H__
#define __DEFUALT_RTC_API_H__


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/** Error code. */
typedef enum {
  /** No error. */
  ERR_OKAY = 0,

  /** General error */
  ERR_FAILED = 1,

  /** Network is unavailable */
  ERR_NET_DOWN = 14,

  /**
   * Request to join channel is rejected.
   * It occurs when local user is already in channel and try to join the same channel again.
   */
  ERR_JOIN_CHANNEL_REJECTED = 17,

  /** App ID is invalid. */
  ERR_INVALID_APP_ID = 101,

  /** Channel is invalid. */
  ERR_INVALID_CHANNEL_NAME = 102,

  /** Fails to get server resources in the specified region. */
  ERR_NO_SERVER_RESOURCES = 103,

  /** Server rejected request to look up channel. */
  ERR_LOOKUP_CHANNEL_REJECTED = 105,

  /** Server rejected request to open channel. */
  ERR_OPEN_CHANNEL_REJECTED = 107,

  /**
   * Token expired due to reasons belows:
   * - Authorized Timestamp expired:      The timestamp is represented by the number of
   *                                      seconds elapsed since 1/1/1970. The user can use
   *                                      the Token to access the Agora service within five
   *                                      minutes after the Token is generated. If the user
   *                                      does not access the Agora service after five minutes,
   *                                      this Token will no longer be valid.
   * - Call Expiration Timestamp expired: The timestamp indicates the exact time when a
   *                                      user can no longer use the Agora service (for example,
   *                                      when a user is forced to leave an ongoing call).
   *                                      When the value is set for the Call Expiration Timestamp,
   *                                      it does not mean that the Token will be expired,
   *                                      but that the user will be kicked out of the channel.
   */
  ERR_TOKEN_EXPIRED = 109,

  /**
  * Token is invalid due to reasons belows:
  * - If application certificate is enabled on the Dashboard,
  *   valid token SHOULD be set when invoke.
  *
  * - If uid field is mandatory, users must specify the same uid as the one used to generate the token,
  *   when calling `agora_rtc_join_channel`.
  */
  ERR_INVALID_TOKEN = 110,

  /** Dynamic token has been enabled, but is not provided when joining the channel.
   *  Please specify the valid token when calling `agora_rtc_join_channel`.
   */
  ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY = 115,

  /** Switching roles failed.
   *  Please try to rejoin the channel.
   */
  ERR_SET_CLIENT_ROLE_NOT_AUTHORIZED = 119,

  /** Decryption fails. The user may have used a different encryption password to join the channel.
   *  Check your settings or try rejoining the channel.
   */
  ERR_DECRYPTION_FAILED = 120,

  /* Ticket to open channel is invalid */
  ERR_OPEN_CHANNEL_INVALID_TICKET = 121,

  /* Try another server. */
  ERR_OPEN_CHANNEL_TRY_NEXT_VOS = 122,

  /** Client is banned by the server */
  ERR_CLIENT_IS_BANNED_BY_SERVER = 123,

  /** Sending video data too fast and over the bandwidth limit.
   *  Very likely that packet loss occurs with this sending speed.
   */
  ERR_SEND_VIDEO_OVER_BANDWIDTH_LIMIT = 200,

  /** Audio decoder does not match incoming audio data type.
   *  Currently SDK built-in audio codec only supports G722 and OPUS.
   */
  ERR_AUDIO_DECODER_NOT_MATCH_AUDIO_FRAME = 201,

  /** Audio decoder does not match incoming audio data type.
   *  Currently SDK built-in audio codec only supports G722 and OPUS.
   */
  ERR_NO_AUDIO_DECODER_TO_HANDLE_AUDIO_FRAME = 202,
} agora_err_code_e;

/**
 * The definition of the user_offline_reason_e enum.
 */
typedef enum {
  /**
   * 0: Remote user leaves channel actively
   */
  USER_OFFLINE_QUIT = 0,
  /**
   * 1: Remote user is dropped due to timeout
   */
  USER_OFFLINE_DROPPED = 1,
} user_offline_reason_e;

/**
 * The definition of the video_data_type_e enum.
 */
typedef enum {
  /* 0: YUV420 */
  VIDEO_DATA_TYPE_YUV420 = 0,
  /* 2: H264 */
  VIDEO_DATA_TYPE_H264 = 2,
  /* 3: H265 */
  VIDEO_DATA_TYPE_H265 = 3,
  /* 6: generic */
  VIDEO_DATA_TYPE_GENERIC = 6,
  /* 20: generic JPEG */
  VIDEO_DATA_TYPE_GENERIC_JPEG = 20,
} video_data_type_e;

/**
 * The definition of the video_frame_type_e enum.
 */
typedef enum {
  /**
   * 0: unknow frame type
   * If you set it ,the SDK will judge the frame type
   */
  VIDEO_FRAME_AUTO_DETECT = 0,
  /**
   * 3: key frame
   */
  VIDEO_FRAME_KEY = 3,
  /*
   * 4: delta frame, e.g: P-Frame
   */
  VIDEO_FRAME_DELTA = 4,
} video_frame_type_e;

/**
 * The definition of the video_frame_rate_e enum.
 */
typedef enum {
  /** 1: 1 fps. */
  VIDEO_FRAME_RATE_FPS_1 = 1,
  /** 7: 7 fps. */
  VIDEO_FRAME_RATE_FPS_7 = 7,
  /** 10: 10 fps. */
  VIDEO_FRAME_RATE_FPS_10 = 10,
  /** 15: 15 fps. */
  VIDEO_FRAME_RATE_FPS_15 = 15,
  /** 24: 24 fps. */
  VIDEO_FRAME_RATE_FPS_24 = 24,
  /** 30: 30 fps. */
  VIDEO_FRAME_RATE_FPS_30 = 30,
  /** 60: 60 fps. Applies to Windows and macOS only. */
  VIDEO_FRAME_RATE_FPS_60 = 60,
} video_frame_rate_e;

/**
 * Video stream types.
 */
typedef enum {
  /**
   * 0: The high-quality video stream, which has a higher resolution and bitrate.
   */
  VIDEO_STREAM_HIGH = 0,
  /**
   * 1: The low-quality video stream, which has a lower resolution and bitrate.
   */
  VIDEO_STREAM_LOW = 1,
} video_stream_type_e;

/**
 * The definition of the video_frame_info_t struct.
 */
typedef struct {
  /**
   * The video data type: #video_data_type_e.
   */
  video_data_type_e data_type;
  /**
   * The video stream type: #video_stream_type_e
   */
  video_stream_type_e stream_type;
  /**
   * The frame type of the encoded video frame: #video_frame_type_e.
   */
  video_frame_type_e frame_type;
  /**
   * The number of video frames per second.
   * -This value will be used for calculating timestamps of the encoded image.
   * - If frame_per_sec equals zero, then real timestamp will be used.
   * - Otherwise, timestamp will be adjusted to the value of frame_per_sec set.
   */
  video_frame_rate_e frame_rate;
} video_frame_info_t;

/**
 * Audio codec type list.
 */
typedef enum {
  /**
   * 0: Disable
   */
  AUDIO_CODEC_DISABLED = 0,
  /**
   * 1: OPUS
   */
  AUDIO_CODEC_TYPE_OPUS = 1,
  /**
   * 2: G722
   */
  AUDIO_CODEC_TYPE_G722 = 2,
  /**
   * 3: G711A
   */
  AUDIO_CODEC_TYPE_G711A = 3,
  /**
   * 4: G711U
   */
  AUDIO_CODEC_TYPE_G711U = 4,
} audio_codec_type_e;

/**
 * Audio data type list.
 */
typedef enum {
  /**
   * 1: OPUS
   */
  AUDIO_DATA_TYPE_OPUS = 1,
  /**
   * 2: OPUSFB
   */
  AUDIO_DATA_TYPE_OPUSFB = 2,
  /**
   * 3: PCMA
   */
  AUDIO_DATA_TYPE_PCMA = 3,
  /**
   * 4: PCMU
   */
  AUDIO_DATA_TYPE_PCMU = 4,
  /**
   * 5: G722
   */
  AUDIO_DATA_TYPE_G722 = 5,
  /**
   * 8: AACLC
   */
  AUDIO_DATA_TYPE_AACLC = 8,
  /**
   * 9: HEAAC
   */
  AUDIO_DATA_TYPE_HEAAC = 9,
  /**
   * 100: PCM (audio codec should be enabled)
   */
  AUDIO_DATA_TYPE_PCM = 100,
  /**
   * 253: GENERIC
   */
  AUDIO_DATA_TYPE_GENERIC = 253,
} audio_data_type_e;

/**
 * The definition of the audio_frame_info_t struct.
 */
typedef struct {
  /**
   * Audio data type, reference #audio_data_type_e.
   */
  audio_data_type_e data_type;
} audio_frame_info_t;

/**
 * The definition of log level enum
 */
typedef enum {
  RTC_LOG_DEFAULT = 0, // the same as RTC_LOG_NOTICE
  RTC_LOG_EMERG, // system is unusable
  RTC_LOG_ALERT, // action must be taken immediately
  RTC_LOG_CRIT, // critical conditions
  RTC_LOG_ERROR, // error conditions
  RTC_LOG_WARNING, // warning conditions
  RTC_LOG_NOTICE, // normal but significant condition, default level
  RTC_LOG_INFO, // informational
  RTC_LOG_DEBUG, // debug-level messages
} rtc_log_level_e;

/**
 * IP areas.
 */
typedef enum {
  /* the same as AREA_CODE_GLOB */
  AREA_CODE_DEFAULT = 0x00000000,
  /* Mainland China. */
  AREA_CODE_CN = 0x00000001,
  /* North America. */
  AREA_CODE_NA = 0x00000002,
  /* Europe. */
  AREA_CODE_EU = 0x00000004,
  /* Asia, excluding Mainland China. */
  AREA_CODE_AS = 0x00000008,
  /* Japan. */
  AREA_CODE_JP = 0x00000010,
  /* India. */
  AREA_CODE_IN = 0x00000020,
  /* Oceania */
  AREA_CODE_OC = 0x00000040,
  /* South-American */
  AREA_CODE_SA = 0x00000080,
  /* Africa */
  AREA_CODE_AF = 0x00000100,
  /* South Korea */
  AREA_CODE_KR = 0x00000200,
  /* The global area (except China) */
  AREA_CODE_OVS = 0xFFFFFFFE,
  /* (Default) Global. */
  AREA_CODE_GLOB = (0xFFFFFFFF),
} area_code_e;






#ifdef __cplusplus
}
#endif

#endif /* __DEFUALT_RTC_API_H__ */
