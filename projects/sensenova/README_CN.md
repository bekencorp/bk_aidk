# BK7258 sensenova工程使用说明


## 1. BK7258 AIDK下载及编译

请参考AIDK官方在线文档：
https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/get-started/index.html

## 2. BK7258 sensenova相关介绍
BK7258 sensenova工程使用商汤SenseNova V6 Omni大模型，SenseNova V6 Omni是商汤“日日新”融合大模型的交互版本,详细请参考官方文档：
https://platform.sensenova.cn/product/APIService/document/344/

## 3. sensenova相关配置
sensenova主要配置项存放在 `/projects/common_components/bk_smart_config/include/bk_smart_config_sensenova_adapter.h` 文件中，主要配置如下：
- CONFIG_SENSENOVA_WSS_URL          //SENSENOVA websocket服务器地址
- CONFIG_SENSENOVA_LLM_CONFIG       //SENSENOVA LLM配置参数
- CONFIG_SENSENOVA_ISS              //SENSENOVA 客户端ID
- CONFIG_SENSENOVA_SECRET           //SENSENOVA 客户端密钥

其中iss和secret主要用于生成访问sensenova服务器的JWT token，iss和secret需要向商汤申请，详细请参考官方文档：
https://platform.sensenova.cn/product/APIService/document/344/

申请后请将申请到的iss和secret替换到bk_smart_config_sensenova_adapter.h配置文件中。

```c
// Sensenova issuer identification
#define CONFIG_SENSENOVA_ISS "xxx"
// Sensenova secret key
#define CONFIG_SENSENOVA_SECRET "xxx"
```

为了方便开发者体验，我们暂时使用一组临时的iss和secret用来生成JWT token。

## 4. sensenova工程核心流程介绍
sensenova音视频传输使用声网RTC，声网RTC所需要的appid、token、channelname、uid等参数通过websocket从sensenova服务器获取。
BK7258 beken_genie工程同样使用声网RTC，与beken_genie工程不同的是，beken_genie工程默认向BK服务器获取声网RTC参数，而sensenova工程向sensenova服务器获取声网RTC参数。

beken_genie工程介绍请参考如下连接：
https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/projects/beken_genie/index.html

sensenova工程介绍核心代码介绍：
`/projects/common_components/bk_smart_config/src/adapter/sensenova/bk_smart_config_sensenova_adapter.c`文件主要实现向BK服务器获取JWT token和启动声网RTC，主要函数如下：
- int bk_sconf_wakeup_agent(uint8_t reset) //向BK服务器确认设备合法性和获取JWT token
- int bk_sconf_rsp_parse_update(char *rsp) //解析BK服务器返回的JWT token
- int bk_sconf_sensenova_wss_txt_handle(char *json_text, unsigned int size) //处理sensenova服务器返回的文本数据，获取声网RTC参数
- int bk_sconf_sensenova_wss_send_text(void *wss, char *text) //向sensenova服务器发送文本数据
- void agora_auto_run(uint8_t reset) //启动声网RTC

## 5. sensenova工程编译

通过如下命令可以编译sensenova工程：make bk7258 PROJECT=sensenova

## 6. 工程运行调试
启动程序步骤如下：

- 通过BekenIoT APP进行配网，详细操作请参考：https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#ai
- 通过“Hi Armino”唤醒系统，开始与智能体对话
