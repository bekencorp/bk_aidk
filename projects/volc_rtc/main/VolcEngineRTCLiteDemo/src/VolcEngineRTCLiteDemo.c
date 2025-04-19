#include "FreeRTOS_POSIX.h"
#include "posix/pthread.h"
#include "VolcEngineRTCLite.h"
#include "file_parser.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"

#define DEFAULT_FPS_VALUE                   25
#define NUMBER_OF_OPUS_FRAME_FILES          618
#define SAMPLE_AUDIO_FRAME_DURATION         20

#define SIMPLE_CHANENL_NAME                 "xyzz"

#define MAX_APPID_LEN       64
#define MAX_ROOMID_LEN      64
#define MAX_USERID_LEN      64
#define MAX_video_file_LEN   256
#define MAX_audio_file_LEN   256
#define MAX_TOKEN_LEN       256
#define MAX_PATH_LEN        1024

typedef struct {
    char app_id[MAX_APPID_LEN];
    char room_id[MAX_ROOMID_LEN];
    char user_id[MAX_USERID_LEN];
    char video_file[MAX_video_file_LEN];
    char audio_file[MAX_audio_file_LEN];
    char token[MAX_TOKEN_LEN];

    bool channel_joined;
    bool is_send_video;
    bool is_send_audio;
    pthread_t  send_video_thread_id;
    pthread_t  send_audio_thread_id;
    int audio_codec;
    int fps;
    byte_rtc_engine_t engine;
    bool fini_notifyed;
} byte_rtc_data;

byte_rtc_data g_byte_rtc_data;

int demo_read_file(char * file_path, bool bin_mode, unsigned char * pbuffer, long * psize)
{
    long file_len;
    int ret = -1;
    FILE *fp = NULL;
    do {
        if (file_path == NULL && psize == NULL) break;
        fp = fopen(file_path, bin_mode ? "rb" : "r");
        if (fp == NULL) break;
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        if (pbuffer == NULL) {
            *psize = file_len;
            ret = 0;
            break;
        }
        if (file_len > *psize) break;
        fseek(fp, 0, SEEK_SET);
        if (fread(pbuffer, (size_t)file_len, 1, fp) != 1) break;
        ret = 0;
    } while (false);

    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    return ret;
}

void init_default_byte_rtc_data(){
    g_byte_rtc_data.channel_joined = false;
    strncpy(g_byte_rtc_data.app_id,"",MAX_APPID_LEN);
    strncpy(g_byte_rtc_data.room_id,SIMPLE_CHANENL_NAME,MAX_ROOMID_LEN);
    strncpy(g_byte_rtc_data.user_id,"u001",MAX_USERID_LEN);
    strncpy(g_byte_rtc_data.video_file,"resource/send_video.h264",MAX_video_file_LEN);
    g_byte_rtc_data.is_send_video =   false;
    g_byte_rtc_data.is_send_audio =   false;
    g_byte_rtc_data.send_video_thread_id = 0;
    g_byte_rtc_data.send_audio_thread_id = 0;

    g_byte_rtc_data.audio_codec = AUDIO_CODEC_TYPE_OPUS;

    if(g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_G711A) {
        strncpy(g_byte_rtc_data.audio_file,"resource/send_audio.pcma",MAX_audio_file_LEN);    
    } else if(g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_AACLC) {
        strncpy(g_byte_rtc_data.audio_file,"resource/send_audio.aac",MAX_audio_file_LEN);    
    }
    g_byte_rtc_data.fps = DEFAULT_FPS_VALUE;
    g_byte_rtc_data.engine  =   NULL;
    g_byte_rtc_data.fini_notifyed = false;
}

int read_frame_from_disk(unsigned char * pbuffer, long * psize, char * frame_file_path){
    int ret = 0;
    long size = 0;
    do{
        if (psize == NULL) break;
        size = *psize;
        ret = demo_read_file(frame_file_path, true, pbuffer, &size);
        if (ret != 0) {
            printf("operation returned int code: %d \n", ret);
            break;
        }
    }while(false);

    if (psize != NULL) {
        *psize = (int) size;
    }
    return 0;
}

void *  send_video_routine(void * user_data) {
    const int sample_video_frame_duration = 1000/g_byte_rtc_data.fps;
    void * video_parser = create_file_parser(MEDIA_FILE_TYPE_H264, g_byte_rtc_data.video_file,NULL);
    video_frame_info_t video_frame_info = {
        .data_type      = VIDEO_DATA_TYPE_H264,
        .frame_rate     = 30,
        .frame_type     = VIDEO_FRAME_AUTO_DETECT,
        .stream_type    = VIDEO_STREAM_LOW
    };
    while(g_byte_rtc_data.is_send_video) {
        frame_t h264_frame;
        if (!g_byte_rtc_data.channel_joined) { usleep(200 * 1000); continue; }
        if(file_parser_obtain_frame(video_parser,&h264_frame) == 0) {
            byte_rtc_send_video_data(g_byte_rtc_data.engine,g_byte_rtc_data.room_id,h264_frame.ptr, h264_frame.len,&video_frame_info);
            file_parser_release_frame(video_parser, &h264_frame);
            usleep(sample_video_frame_duration * 1000);
        }
    }
    destroy_file_parser(video_parser);
    return 0;
}

void send_audio_frames_by_path() {
    int ret = 0;
    long file_index = 0, frame_size = 0,cur_audio_buffer_size = 0;
    char file_path[MAX_PATH_LEN + 1];
    uint8_t* p_audio_frame_buffer = NULL;
    audio_frame_info_t audio_frame_info;
    audio_frame_info.data_type  = g_byte_rtc_data.audio_codec;

    while (g_byte_rtc_data.is_send_audio) {
        if (!g_byte_rtc_data.channel_joined) { usleep(200 * 1000); continue; }

        file_index = file_index % NUMBER_OF_OPUS_FRAME_FILES + 1;
        if (g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_OPUS) {
            snprintf(file_path, MAX_PATH_LEN, "resource/opusSampleFrames/sample-%03ld.opus", file_index);
        } else if (g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_G722) {
            snprintf(file_path, MAX_PATH_LEN, "resource/g722SampleFrames/children_%ld.g722", file_index);
        }

        ret = read_frame_from_disk(NULL, &frame_size, file_path);
        if (ret != 0) {
            printf("read_frame_from_disk(): operation returned int code: %d\n", ret);
            break;
        }

        if (frame_size > cur_audio_buffer_size) {
            p_audio_frame_buffer = (unsigned char *) realloc(p_audio_frame_buffer, frame_size);
            cur_audio_buffer_size = frame_size;
        }

        ret = read_frame_from_disk(p_audio_frame_buffer, &frame_size, file_path);
        if (ret != 0) {
            printf("read_frame_from_disk(): operation returned int code: %d \n", ret);
            break;
        }

        byte_rtc_send_audio_data(g_byte_rtc_data.engine,g_byte_rtc_data.room_id,p_audio_frame_buffer,frame_size,&audio_frame_info);
        usleep(SAMPLE_AUDIO_FRAME_DURATION * 1000);
    }

    if(p_audio_frame_buffer != NULL) { 
        free(p_audio_frame_buffer);
    }
}

void send_audio_frames_by_file() {
    parser_cfg_t parser_cfg = { 0 };
    media_file_type_e fileType = MEDIA_FILE_TYPE_AACLC;
    audio_data_type_e dataType = AUDIO_DATA_TYPE_AACLC;

    if (g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_AACLC) {
        fileType = MEDIA_FILE_TYPE_AACLC;
        parser_cfg.u.audio_cfg.sampleRateHz         = 48000;
        parser_cfg.u.audio_cfg.numberOfChannels     = 1;
        parser_cfg.u.audio_cfg.framePeriodMs        = SAMPLE_AUDIO_FRAME_DURATION;
    } else if(g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_G711A){
        fileType = MEDIA_FILE_TYPE_G711;
        parser_cfg.u.audio_cfg.sampleRateHz         = 8000;
        parser_cfg.u.audio_cfg.numberOfChannels     = 1;
        parser_cfg.u.audio_cfg.framePeriodMs        = SAMPLE_AUDIO_FRAME_DURATION;
        dataType = AUDIO_DATA_TYPE_PCMA;

        printf("send_audio_frames_by_file AUDIO_CODEC_TYPE_G711A\n");
    } else {
        return;
    }
    
    void * audio_parser = create_file_parser(fileType, g_byte_rtc_data.audio_file,&parser_cfg);
    audio_frame_info_t audio_frame_info;
    audio_frame_info.data_type  = dataType;
    while(g_byte_rtc_data.is_send_audio) {
        frame_t audio_frame;
        if (!g_byte_rtc_data.channel_joined) { usleep(200 * 1000); continue; }

        if(file_parser_obtain_frame(audio_parser,&audio_frame) == 0) {
            byte_rtc_send_audio_data(g_byte_rtc_data.engine,g_byte_rtc_data.room_id,audio_frame.ptr, audio_frame.len,&audio_frame_info);
            file_parser_release_frame(audio_parser, &audio_frame);
            usleep(SAMPLE_AUDIO_FRAME_DURATION * 1000);
        }else{
            continue;
        }
    }

    destroy_file_parser(audio_parser);

}

void * send_audio_frame_routine(void * user_data)
{
    if(g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_AACLC || g_byte_rtc_data.audio_codec == AUDIO_CODEC_TYPE_G711A) {
        send_audio_frames_by_file();
        return NULL;
    }
    send_audio_frames_by_path();
    return NULL;
}

int start_send_video(){
    if (g_byte_rtc_data.is_send_video) return -1;
    g_byte_rtc_data.is_send_video = true;
    pthread_create(&g_byte_rtc_data.send_video_thread_id, NULL, send_video_routine,0);
    return 0;
}
int start_send_audio(){
    if(g_byte_rtc_data.is_send_audio) return -1;
    g_byte_rtc_data.is_send_audio = true;
    pthread_create(&g_byte_rtc_data.send_audio_thread_id,NULL,send_audio_frame_routine,0);
    return 0;
}

int stop_send_video(){
    if(!g_byte_rtc_data.is_send_video) return -1;
    g_byte_rtc_data.is_send_video = false;
    pthread_join(g_byte_rtc_data.send_video_thread_id,NULL);
    return 0;
}
int stop_send_audio(){
    if(!g_byte_rtc_data.is_send_audio ) return -1;
    g_byte_rtc_data.is_send_audio = false;
    pthread_join(g_byte_rtc_data.send_audio_thread_id,NULL);
    return 0;
}

static void byte_rtc_on_join_channel_success(byte_rtc_engine_t engine,const char *channel, int elapsed_ms, bool rejoin) {
    g_byte_rtc_data.channel_joined = true;
    printf("\njoin channel success %s elapsed %d ms\n", channel, elapsed_ms);
};

static void byte_rtc_on_user_joined(byte_rtc_engine_t engine,const char *channel, const char *user_name,int elapsed_ms){
    printf("\nremote user joined  %s:%s\n",channel,user_name);
};

static void byte_rtc_on_user_offline(byte_rtc_engine_t engine,const char *channel, const char *user_name , int reason){
    printf("\nremote user offline  %s:%s\n",channel,user_name);
};

static void byte_rtc_on_user_mute_audio(byte_rtc_engine_t engine,const char *channel, const char *user_name ,int muted){
    printf("\nremote user mute audio  %s:%s %d\n",channel,user_name,muted);

};

static void byte_rtc_on_user_mute_video(byte_rtc_engine_t engine,const char *channel, const char *user_name ,int muted){
    printf("\nremote user mute video  %s:%s %d\n",channel,user_name,muted);
};

static void byte_rtc_on_connection_lost(byte_rtc_engine_t engine,const char *channel){
    printf("\nconnection Lost  %s\n",channel);
};

static void byte_rtc_on_audio_data(byte_rtc_engine_t engine,const char *channel, const char *user_name ,uint16_t sent_ts,
                      audio_data_type_e codec, const void *data_ptr, size_t data_len){
                    
};

static void byte_rtc_on_video_data(byte_rtc_engine_t engine,const char *channel, const char *user_name ,uint16_t sent_ts,
                      video_data_type_e codec,  int is_key_frame,
                      const void *data_ptr, size_t data_len) 
{
    if (is_key_frame) {
        printf("\nreceived key frame of user: %s\n", user_name);
    }
};

static void byte_rtc_on_channel_error(byte_rtc_engine_t engine,const char *channel, int code, const char *msg){
    printf("\nerror occur %s %d %s\n",channel,code,msg?msg:"");
};

static void byte_rtc_on_gloable_error(byte_rtc_engine_t engine,int code, const char *msg){
    printf("\nglobal error occur %d %s\n",code,msg?msg:"");
};

static void byte_rtc_on_keyframe_gen_req(byte_rtc_engine_t engine,const char *channel, const char *user_name){
    printf("\nremote req key frame %s:%s\n",channel,user_name);
};

static void byte_rtc_on_target_bitrate_changed(byte_rtc_engine_t engine,const char *channel, uint32_t target_bps){
};

static void byte_rtc_on_token_privilege_will_expire(byte_rtc_engine_t engine,const char *token) {
    printf("\ntoken privilege will expire %s",token);
};
static void byte_rtc_on_fini_notify(byte_rtc_engine_t engine) {
    g_byte_rtc_data.fini_notifyed = true;
}

void print_help() {
    printf("Usage ./VolcEngineRTCLiteDemo --appid appid [--roomid|--userid|--fps|--help|--token|--video]\n");
    printf("--appid  XXXX,must be set\n");
    printf("--roomid XXXX, default is xyzz , it must match your token \n");
    printf("--userid XXXX, default is u001 , it must match you token \n");
    printf("--video XXXX, default is resource/send_vedio.h264 \n");
    printf("--token XXXX, token must be set  \n");
    printf("--fps   XXXX, default is 25 frams per second \n");
}

bool parse_args(int argc,const char * argv[]) {
    int i = 1;
    for(i = 1; i < argc ; i++ ) {
        if(strcmp(argv[i],"--appid") == 0) {
            strncpy(g_byte_rtc_data.app_id,argv[i+1],MAX_APPID_LEN);
            ++i;
        } else if(strcmp(argv[i],"--roomid") == 0) {
            strncpy(g_byte_rtc_data.room_id,argv[i+1],MAX_APPID_LEN);
            ++i;
        } else if(strcmp(argv[i],"--userid") == 0){
            strncpy(g_byte_rtc_data.user_id,argv[i+1],MAX_USERID_LEN);
            ++i;
        } else if(strcmp(argv[i],"--video") == 0) {
            strncpy(g_byte_rtc_data.video_file,argv[i+1],MAX_video_file_LEN);
            ++i;
        } else if(strcmp(argv[i],"--audio") == 0) {
            strncpy(g_byte_rtc_data.audio_file,argv[i+1],MAX_audio_file_LEN);
            ++i;
        } else if(strcmp(argv[i],"--fps") == 0) {
            g_byte_rtc_data.fps = atoi(argv[i+1]);        
            ++i;
        } else if(strcmp(argv[i],"--token") == 0) {
            strncpy(g_byte_rtc_data.token,argv[i+1],MAX_TOKEN_LEN);
            ++i;
        } else if(strcmp(argv[i],"--help") == 0) {
            print_help();
            return false;
        } else{
            printf("unknow args %s\n",argv[i]); ++i;
        }
    }
    return true;
}
#if 0
int main(int argc,const char * argv[]) 
{
    byte_rtc_event_handler_t eventHandle = {0};
    
    eventHandle.on_global_error            =   byte_rtc_on_gloable_error;
    eventHandle.on_join_room_success       =   byte_rtc_on_join_channel_success;
    eventHandle.on_room_error              =   byte_rtc_on_channel_error;
    eventHandle.on_user_joined             =   byte_rtc_on_user_joined;
    eventHandle.on_user_offline            =   byte_rtc_on_user_offline;
    eventHandle.on_user_mute_audio         =   byte_rtc_on_user_mute_audio;
    eventHandle.on_user_mute_video         =   byte_rtc_on_user_mute_video;

    eventHandle.on_audio_data              =   byte_rtc_on_audio_data;
    eventHandle.on_video_data              =   byte_rtc_on_video_data;

    eventHandle.on_key_frame_gen_req       =   byte_rtc_on_keyframe_gen_req;
    eventHandle.on_target_bitrate_changed  =   byte_rtc_on_target_bitrate_changed;
    eventHandle.on_token_privilege_will_expire =   byte_rtc_on_token_privilege_will_expire;
    eventHandle.on_fini_notify       =   NULL;

    memset(&g_byte_rtc_data,0,sizeof(byte_rtc_data));
    init_default_byte_rtc_data();
    if(parse_args(argc,argv) == false) {
        return -1;
    };
    byte_rtc_engine_t engine = g_byte_rtc_data.engine = byte_rtc_create(g_byte_rtc_data.app_id,&eventHandle);
    byte_rtc_set_log_level(engine,BYTE_RTC_LOG_LEVEL_INFO);
#if defined BYTE_RTC_API_VERSION_NUM   && BYTE_RTC_API_VERSION_NUM >= 0x1003  
    byte_rtc_room_options_t option = { true,false,true,true};
#else 
    byte_rtc_room_options_t option = { true,false};
#endif

    int ret = byte_rtc_init(engine);
    byte_rtc_set_audio_codec(engine,g_byte_rtc_data.audio_codec);
    byte_rtc_join_room(engine,g_byte_rtc_data.room_id,g_byte_rtc_data.user_id,g_byte_rtc_data.token,&option);
    start_send_video();
    start_send_audio();
    if(ret == 0) {
        sleep(60 * 1000);
    }
    stop_send_video();
    stop_send_audio();
    byte_rtc_leave_room(engine,g_byte_rtc_data.room_id);
    byte_rtc_fini(engine);
    while(!g_byte_rtc_data.fini_notifyed) {
        sleep(1);
    }
    byte_rtc_destory(engine);
    return 0;
}
#endif
int volc_rtc_demo_main(void) 
{
    byte_rtc_event_handler_t eventHandle = {0};
    
    eventHandle.on_global_error            =   byte_rtc_on_gloable_error;
    eventHandle.on_join_room_success       =   byte_rtc_on_join_channel_success;
    eventHandle.on_room_error              =   byte_rtc_on_channel_error;
    eventHandle.on_user_joined             =   byte_rtc_on_user_joined;
    eventHandle.on_user_offline            =   byte_rtc_on_user_offline;
    eventHandle.on_user_mute_audio         =   byte_rtc_on_user_mute_audio;
    eventHandle.on_user_mute_video         =   byte_rtc_on_user_mute_video;

    eventHandle.on_audio_data              =   byte_rtc_on_audio_data;
    eventHandle.on_video_data              =   byte_rtc_on_video_data;

    eventHandle.on_key_frame_gen_req       =   byte_rtc_on_keyframe_gen_req;
    eventHandle.on_target_bitrate_changed  =   byte_rtc_on_target_bitrate_changed;
    eventHandle.on_token_privilege_will_expire =   byte_rtc_on_token_privilege_will_expire;
    eventHandle.on_fini_notify       =   NULL;

    memset(&g_byte_rtc_data,0,sizeof(byte_rtc_data));
    init_default_byte_rtc_data();
    // if(parse_args(argc,argv) == false) {
    //     return -1;
    // };
    byte_rtc_engine_t engine = g_byte_rtc_data.engine = byte_rtc_create(g_byte_rtc_data.app_id,&eventHandle);
    byte_rtc_set_log_level(engine,BYTE_RTC_LOG_LEVEL_INFO);
#if defined BYTE_RTC_API_VERSION_NUM   && BYTE_RTC_API_VERSION_NUM >= 0x1003  
    byte_rtc_room_options_t option = { true,false,true,true};
#else 
    byte_rtc_room_options_t option = { true,false};
#endif

    int ret = byte_rtc_init(engine);
    byte_rtc_set_audio_codec(engine,g_byte_rtc_data.audio_codec);
    byte_rtc_join_room(engine,g_byte_rtc_data.room_id,g_byte_rtc_data.user_id,g_byte_rtc_data.token,&option);
    start_send_video();
    start_send_audio();
    if(ret == 0) {
        sleep(60 * 1000);
    }
    stop_send_video();
    stop_send_audio();
    byte_rtc_leave_room(engine,g_byte_rtc_data.room_id);
    byte_rtc_fini(engine);
    while(!g_byte_rtc_data.fini_notifyed) {
        sleep(1);
    }
    byte_rtc_destory(engine);
    return 0;
}

