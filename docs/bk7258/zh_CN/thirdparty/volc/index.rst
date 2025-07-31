火山RTC
=================================

:link_to_translation:`en:[English]`

**1. 火山RTC简介**
---------------------------------
火山引擎实时音视频（Volcengine Real Time Communication，veRTC）提供全球范围内高可靠、高并发、低延时的实时音视频通信能力，实现多种类型的实时交流和互动。
详细介绍请参考在线资料：https://www.volcengine.com/product/veRTC 

**2. 火山RTC相关代码分布**
---------------------------------
 - 火山rtc hal层及lib代码目录： ``/bk_ai/bk_avdk/components/bk_thirdparty/VolcEngineRTCLite``
 - BK、火山rtc传输整合： ``/bk_ai/project/common_components/network_transfer/volc_rtc``
 - 火山rtc demo工程目录： ``/bk_ai/project/volc_rtc``

**3. 使用火山RTC前置准备**
---------------------------------
使用火山RTC和大模型进行AI对话需要开通相关服务和配置权限，具体请参考火山在线文档：
https://www.volcengine.com/docs/6348/1315561 

**4. Agent(智能体)启动方式**
---------------------------------
AIDK中包含两种启动Agent方法，分别是通过BK服务器启动Agent和通过开发者部署服务器启动agent。AIDK默认通过BK服务器启动Agent，用于向开发者展示火山RTC效果。

.. attention::
    *通过BK服务器启动Agent进行AI对话为demo展示，每次限制对话限制时长为5分钟。*
    *如果需要获取更长对话时长体验，可联系开发板淘宝客服人员获取license文件，详细请参考第5章节的license功能描述。*
    **我们强烈推荐开发者搭建自己服务器，以方便进行时长控制、设备管理和差异化部署**

**4.1 通过BK服务器启动Agent**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
在配网完成后，程序会调用 ``bk_sconf_wakeup_agent()`` 向BK服务器请求启动Agent和启动设备端RTC，随后要求设备加入Agent所在房间，进行AI对话。

**4.2 通过开发者部署服务器启动Agent**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
为了方便开发者进行设备管理，AIDK还支持开发者通过个人部署服务器启动Agent。服务器部署请参考火山开源代码和指南：https://github.com/volcengine/rtc-aigc-embedded-demo 
使用该方法启动Agent需要做如下配置：

4.2.1 修改beken_genie火山RTC工程配置文件
++++++++++++++++++++++++++++++++++++++++++

修改 ``/bk_ai/project/volc_rtc/config/bk7258/config`` 文件，通过将CONFIG_BK_DEV_STARTUP_AGENT=y开启向自定义部署服务器启动Agent相关代码。

.. code::

    #@ Enable start agent on device
    CONFIG_BK_DEV_STARTUP_AGENT=y

4.2.2 修改火山配置文件
++++++++++++++++++++++++++++++++++++++++++
修改 ``/bk_ai/project/common_components/network_transfer/volc_rtc/volc_config.h`` 配置向自定义服务器请求需要参数。

.. code-block:: c

    // RTC APP ID
    #define CONFIG_RTC_APP_ID    "67********************d1"
    // 服务端的地址
    #define CONFIG_AGENT_SERVER_HOST   "1**.1**.**.**:8080"


**5. License功能说明**
---------------------------------
火山RTC支持license计费，License计费为特殊计费规则，使用前请向火山沟通确认。
AIDK在v2.0.1.9节点开始支持或是License模式，使用License计费请确保 ``/bk_ai/project/volc_rtc/config/bk7258/config`` 文件中CONFIG_VOLC_RTC_ENABLE_LICENSE宏已经配置为y。
License默认与设备UID绑定，设备UID可通过 ``byte_print_finger()`` 获取。

在获取License文件后，请将License文件命名为VolcEngineRTCLite.lic，并将License文件复制到文件系统的根目录。

.. note::
    BK7258 AIDK文件系统默认使用SD-NAND，License文件需要复制到SD-NAND根目录，
    SD NAND具体使用方法可参考 `Nand磁盘使用注意事项 <../../api-reference/nand_disk_note.html>`_ 。
    开发者可联系获取AIDK开发板的工作人员获取license文件体验。

**6. 个性化配置**
---------------------------------

**6.1 音频codec配置**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
火山支持常见的音频编解码格式，AIDK火山RTC工程默认使用opus，如果希望修改为G722编解码，可以通过修改
``/bk_ai/project/volc_rtc/config/bk7258/config`` 和 ``/bk_ai/projects/volc_rtc/config/bk7258_cp1/config`` 两个文件中
的AUDIO_ENCODER/AUDIO_DECODER配置实现。

.. code::

    #@ DAC sample rate
    CONFIG_AUDIO_ENCODER_TYPE="G722"
    #@ DAC sample rate
    CONFIG_AUDIO_DECODER_TYPE="G722"

火山RTC与音频codec相关操作包含如下几点：
 - 在 ``byte_rtc_join_room`` 前调使用 ``byte_rtc_set_audio_codec`` 接口设置音频编码类型
 - 通过 ``byte_rtc_send_audio_data`` 填写对应的video_frame_info，详细可参考 ``bk_byte_rtc_audio_data_send`` 函数实现
 - 启动Agent时显示通过audio_codec参数配置对应音频编码类型，详细可参考 ``start_voice_bot`` 函数实现

**6.2 视觉识别功能**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
火山工程默认支持视觉识别功能，AIDK火山工程配网后默认没有开启视觉识别功能，可通过S2按键切换视觉识别和普通AI对话功能。
如果希望配网后默认开启视觉识别功能，可在 ``/bk_ai/project/volc_rtc/config/bk7258/config`` 文件中配置CONFIG_VOLC_ENABLE_VISION_BY_DEFAULT=y 实现。

开启视觉识别需要注意如下几点：
 - 在 ``byte_rtc_join_room`` 时options参数需要将auto_publish_video设置为true
 - 通过 ``byte_rtc_send_video_data`` 填写对应的audio_data_type，详细可参考 ``bk_byte_rtc_video_data_send`` 函数实现
 - 启动Agent时显示通过vision_enable参数是能视觉理解，并填写对应的image配置，详细可参考``start_voice_bot`` 函数实现
 - 后台大模型需要支持视觉识别功能

**6.3 字幕功能**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
火山工程默认关闭字幕功能，如果希望开启字幕功能，可在 ``/bk_ai/project/volc_rtc/config/bk7258/config`` 文件中配置CONFIG_VOLC_ENABLE_SUBTITLE_BY_DEFAULT=y 实现。
字幕接收函数为 ``/bk_ai/project/common_components/network_transfer/volc_rtc/volc_rtc.c`` 文件中的__on_message_received函数。
火山字幕格式请参考火山文档：https://www.volcengine.com/docs/6348/1337284

**7. 参考链接**
--------------------

火山参考文档：https://www.volcengine.com/product/veRTC 

火山相关账号申请链接：https://www.volcengine.com/docs/6348/1315561 

火山Agent服务器部署：https://github.com/volcengine/rtc-aigc-embedded-demo 

火山Demo工程：`BK7258火山RTC Demo <../../projects/volc_rtc/index.html>`_ 
