# BK7258火山RTC工程使用说明


## 1. BK7258 AIDK下载及编译

请参考AIDK官方在线文档：
https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/get-started/index.html

## 2. BK7258火山rtc相关代码介绍

火山rtc hal层及lib代码目录: /bk_ai/bk_avdk/components/bk_thirdparty/VolcEngineRTCLite
火山rtc demo工程目录: /bk_ai/project/volc_rtc


## 3. 火山RTC工程配置

火山rtc demo默认使用HTTP请求方式启动agent并获取RTC room相关信息。HTTP请求方式需要部署服务器，同时设备端配置相关宏相关宏。
服务器介绍及代码请参考火山开源代码：https://github.com/volcengine/rtc-aigc-demo，
设备端配置在/bk_ai/project/common_components/network_transfer/volc_rtc/volc_config.h中，请根据实际情况配置。

```c
// RTC APP ID
#define DEFAULT_RTC_APP_ID    "xxx"
// 服务端的地址
#define DEFAULT_SERVER_HOST   "xxx"
// 默认的智能体id
#define DEFAULT_END_POINT_ID  "xxx"
// 默认声音id
#define DEFAULT_VOICE_TYPE    "BV007_streaming"
```

## 4. 火山RTC工程编译

通过如下命令可以编译火山rtc工程：make bk7258 PROJECT=volc_rtc

## 5. 工程运行调试
启动程序步骤如下：

- 通过BekenIoT APP进行配网，详细操作请参考：https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#ai
- 通过“Hi Armino”唤醒系统，开始与智能体对话
