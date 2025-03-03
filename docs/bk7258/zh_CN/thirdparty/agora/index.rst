Agora
=================================


:link_to_translation:`en:[English]`

1. 账号
---------------------------------

    本文是作者，按照自己的实践，来讲解账号注册，工程创建，APPID获取等操作说明。

    由于声网的网页设置，可能会随着时间变化，有一些更改。建议您可以结合声望的官方文档，来实际操作。

        https://docs.agora.io/cn/Agora%20Platform/manage_projects


1.1 账号注册
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	请登录 ``sso2.agora.io`` 使用邮箱和手机号进行注册登录。

1.2 创建工程
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    - 登录并进入到控制台

    - 在项目管理 -> 项目列表中，点击右侧 ``Create a Project``

    - 在Action栏目中，点击 ``Configure`` 可以进一步配置。

        要注意，声网可能根据IP地址自动跳转中文或英文网站，声网中文和英文网站的配置界面差异较大。

        英文网站，即 ``sso2.agora.io`` 。Demo阶段，推荐始终设置成，测试中。

        中文网站，即 ``shengwang.cn`` 。 项目名称/场景标签，根据自身情况填写。鉴权机制这里注意，demo阶段请使用调试模式。安全模式，需要和声网做好商务上确认再使用。


1.3 APPID
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	- 请使用声望账号登录 ``sso2.agora.io`` ，进入到控制台

	- 在项目管理 -> 项目列表中，可以看到App ID列表栏。请点击上面Copy按钮，获取对应Project的App ID

    - 后续文档，使用AGORA_APPID代称App ID。

1.4 RESTful
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	- 请使用声望账号登录 ``sso2.agora.io`` ，进入到控制台

	- 在控制台 -> 点击，右侧个人头像位置 -> RESTful API -> 添加密钥

    - 后续文档，使用AGORA_RESTFUL_TOKEN代称RESTful API密钥。

2. AI Agent
---------------------------------

   本文采用REST API，使用curl命令，在PC端启动Agora AI Agent的方式控制
   
   详细的参数指令，请参考声网提供的原始文档 ``<bk_aidk源代码路径>/docs/thirdparty/agora_ai_agent``


2.1 Open AI
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


- 启动Open AI Agent

使用前请先替换以下字符串：

    * AGORA_APPID，见1.3章节。

    * AGORA_RESTFUL_TOKEN，见1.4章节。

    * CHANNEL_NAME，自定义字符串（建议使用英文字母加数字组合，不要使用特殊字符），与BK7258开发板内设定的channel name保持一致。

    * OPEN_TOKEN，Open AI官网 ，后台获取的Token。

    * AZURE_TTS_KEY，microsoft官网申请azure tts账号及key。

    * AZURE_TTS_REGION，azure tts账号对应的服务器域。

::

        curl --location --request POST 'https://api.agora.io/api/conversational-ai-agent/v2/projects/AGORA_APPID/join' --header 'Content-Type: application/json' --header 'Authorization: Basic AGORA_RESTFUL_TOKEN' --data-raw '{
            "name": "CHANNEL_NAME",
            "properties": {
                "channel": "CHANNEL_NAME",
                "token": "",
                "agent_rtc_uid": "1234",
                "remote_rtc_uids": [
                    "123"
                ],
                "advanced_features": {
                    "enable_bhvs": true,
                    "enable_aivad": false
                },
                "parameters": {
                    "enable_dump": true,
                    "output_audio_codec": "G722"
                },
                "enable_string_uid": false,
                "idle_timeout": 0,
                "llm": {
                    "url": "https://api.openai.com/v1/chat/completions",
                    "api_key": "OPEN_TOKEN",
                    "system_messages": [
                    {
                        "role": "system",
                        "content": "You are a helpful chatbot."
                    }
                    ],
                    "max_history": 10,
                    "greeting_message": "Merry Christmas, how can I assist you today?",
                    "failure_message": "I am sorry!",
                    "params": {
                        "model": "gpt-4o-mini"
                    }
                },
                "tts": {
                    "vendor": "microsoft",
                    "params": {
                        "key": "AZURE_TTS_KEY",
                        "region": "AZURE_TTS_REGION",
                        "voice_name": "en-US-AndrewMultilingualNeural",
                        "vol": 10
                    }
                },
                "asr": {
                    "language": "en-US",
                    "vendor": "tencent"
                }
            }
        }'

启动成功后会收到以下返回消息：

    * AGORA_AGENT_ID，为系统自动分配的ID值，每个CHANNEL_NAME，有且仅有唯一不同的值。

        可以用于后续启动Agent的输入参数。

::

    {
        "agent_id":"AGORA_AGENT_ID","create_ts":1739609818,"status":"RUNNING"
    }

- 停止Open AI Agent

使用前请先替换以下字符串：

    * AGORA_APPID，见1.3章节。

    * AGORA_RESTFUL_TOKEN，见1.4章节。

    * AGORA_AGENT_ID，为启动AI Agent后，获取到值，见上文。

::

    curl --location --request POST 'https://api.agora.io/api/conversational-ai-agent/v2/projects/AGORA_APPID/agents/AGORA_AGENT_ID/leave' --header 'Authorization: Basic AGORA_RESTFUL_TOKEN'



2.2 豆包
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


- 启动豆包 AI Agent

使用前请先替换以下字符串：

    * AGORA_APPID，见1.3章节。

    * AGORA_RESTFUL_TOKEN，见1.4章节。

    * CHANNEL_NAME，自定义字符串（建议使用英文字母加数字组合，不要使用特殊字符），与BK7258开发板内设定的channel name保持一致。

    * VOLC_API_KEY，火山引擎 ``volcengine.com`` ，后台获取的Token。

    * VOLC_MODEL_ID，是在线推理，获取的接入点名称ID，即AI模型的ID。Demo我们使用了，Doubao-pro-32k作为音频模型。

        创建方法：

                登录火山引擎后，在左侧火山方舟中，找到在线推理按钮，点击进入在线推理页面。

                然后点击，创建推理接入点，选择Doubao-pro-32k作为音频模型，您也可以选在其他适合您的模型。

                最后在返回页面中，就可以在模型名称下面，看到VOLC_MODEL_ID。

    * BYTEDANCE_TTS_APPID，火山引擎官网申请bytedance tts账号及Token，申请流程见结尾的参考链接。

    * BYTEDANCE_TTS_TOKEN，bytedance tts账号对应Token。

::

        curl --location --request POST 'https://api.agora.io/cn/api/conversational-ai-agent/v2/projects/AGORA_APPID/join' --header 'Content-Type: application/json' --header 'Authorization: Basic AGORA_RESTFUL_TOKEN' --data-raw '{
            "name": "CHANNEL_NAME",
            "properties": {
                "channel": "CHANNEL_NAME",
                "token": "",
                "agent_rtc_uid": "1234",
                "remote_rtc_uids": [
                    "123"
                ],
                "advanced_features": {
                    "enable_bhvs": true,
                    "enable_aivad": false
                },
                "parameters": {
                    "enable_dump": true,
                    "output_audio_codec": "G722"
                },
                "enable_string_uid": false,
                "idle_timeout": 0,
                "llm": {
                    "url": "https://ark.cn-beijing.volces.com/api/v3/chat/completions",
                    "api_key": "VOLC_API_KEY",
                    "system_messages": [
                    {
                        "role": "system",
                        "content": "你是一个有礼貌的AI助理。"
                    }
                    ],
                    "max_history": 10,
                    "greeting_message": "新春快乐，有什么可以帮您?",
                    "failure_message": "很抱歉.",
                    "params": {
                        "model": "VOLC_MODEL_ID"
                    }
                },
                "tts": {
                    "vendor": "bytedance",
                    "params": {
                        "token": "BYTEDANCE_TTS_TOKEN",
                        "app_id": "BYTEDANCE_TTS_APPID",
                        "cluster": "volcano_tts",
                        "speed_ratio": 1.0,
                        "volume_ratio": 0.6,
                        "pitch_ratio": 1.0,
                        "emotion": "happy"
                    }
                },
                "asr": {
                    "language": "zh-CN",
                    "vendor": "tencent"
                }
            }
        }'

启动成功后会收到以下返回消息：

    * AGORA_AGENT_ID，为系统自动分配的ID值，每个CHANNEL_NAME，有且仅有唯一不同的值。

        可以用于后续启动Agent的输入参数。

::

    {
        "agent_id":"AGORA_AGENT_ID","create_ts":1739609818,"status":"RUNNING"
    }

- 停止豆包 AI Agent

使用前请先替换以下字符串：

    * AGORA_APPID，见1.3章节。

    * AGORA_RESTFUL_TOKEN，见1.4章节。

    * AGORA_AGENT_ID，为启动AI Agent后，获取到值，见上文。

::

    curl --location --request POST 'https://api.agora.io/cn/api/conversational-ai-agent/v2/projects/AGORA_APPID/agents/AGORA_AGENT_ID/leave' --header 'Authorization: Basic AGORA_RESTFUL_TOKEN'



5、参考链接
--------------------

	声网参考文档：https://docs.agora.io/cn/Agora%20Platform/manage_projects?platform=Android

	声网APPID申请链接：https://sso2.agora.io/cn/v5/login?_gl=1%2ardr355%2a_ga%2aMzkyNDM4ODYyLjE2NzM1MTM3MTU.%2a_ga_BFVGG7E02W%2aMTY3ODg1MjM0My4xMi4wLjE2Nzg4NTIzNDYuMC4wLjA.

	声网AI Agent指导手册： ``<bk_aidk源代码路径>/docs/thirdparty/agora_ai_agent``

	字节TTS账号申请：https://www.volcengine.com/docs/6561/163043
