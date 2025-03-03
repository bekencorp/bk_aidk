Agora-Wanson-AI
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是基于声网Agora AI Agent和华镇离线语音唤醒的AI demo，支持离线语音唤醒（基于唤醒词），唤醒后可以向AI提问，并播放语音回答的结果。AI服务端支持语音聊天，支持OpenAI、豆包等常用的国内外大模型。

1.1 规格
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	* 硬件配置：
		* 核心板，**BK7258_QFN88_9X9_V3.2**
		* 麦克小板，**BK_Module_Microphone_V1.1**
        * 喇叭小板，**BK_Module_Speaker_V1.1**
		* PSRAM 8M/16M

.. note::
	- 1、Agora AI Agent要求音频流格式为：单声道、8K采样率、16bit位宽。
	- 2、目前方案和小度音箱、天猫精灵、小爱音箱类似，唤醒后进行提问，提问时间是固定值。

1.2 路径
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	工程路径: ``<bk_aidk源代码路径>/project/agora_wanson_ai``

	Agora-iot-sdk库API接口的详细说明请参考源文件: ``<bk_aidk源代码路径>/bk_avdk/components/bk_thirdparty/agora-iot-sdk/include/bk7258/agora_rtc_api.h``

	project编译指令: ``make bk7258 PROJECT=agora_wanson_ai``


2. 框架图
---------------------------------


2.1 软件模块架构图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


    此AI demo方案和门锁方案类似，设备端和和AI大模型端双向语音通话，同时设备端向AI大模型端单向图传，门锁方案中的对端apk变为了AI Agent机器人。
	软件模块架构如下图所示：

.. figure:: ../../../_static/agora_wanson_ai_arch.png
    :align: center
    :alt: module architecture Overview
    :figclass: align-center

    Figure 1. software module architecture
    agora_wanson_ai software module architecture

..

    * 方案中，设备端采集mic语音，通过agora sdk将语音数据发送至声网服务器，声网服务器负责和AI Agent大模型的交互，将mic语音发送至AI Agent并获取回复，再将语音回复发送至设备端喇叭播放。
    * 方案中，设备端采集图像，通过agora sdk将每帧图像发送至声网服务器，声网服务器再将图像送至AI Agent大模型进行识别。


2.2 代码模块关系图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    如下图所示，方案使用的多媒体的接口，都定义在 **media_app.h** 和 **aud_intf.h** 中。

.. figure:: ../../../_static/agora_wanson_ai_sw_relationship_diag.png
    :align: center
    :alt: relationship diagram Overview
    :figclass: align-center

    Figure 1. module relationship diagram

    agora_wanson_ai module relationship diagram

3. 配置
---------------------------------

	打开声网功能库和华镇语音识别库需要打开以下配置:

    +----------------------------------------+----------------+---------------+----------------+
    |Kconfig                                 |   CPU          |   Format      |      Value     |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_AGORA_IOT_SDK                    |   CPU0         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_WANSON_ASR                       |   CPU1         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+

4. 演示说明
---------------------------------

demo支持的命令如下表:

+-------------------------------------------------------+-------------------------------------+
|Command                                                |Description                          |
+-------------------------------------------------------+-------------------------------------+
|agora_test {start|stop appid 0 channel_name}           |语音通话                             |
+-------------------------------------------------------+-------------------------------------+


命令参数说明如下：

    +--------------------+-------------------------------------------------+
    |appid               | 注册申请的appid                                 |
    +--------------------+-------------------------------------------------+
    |channel_name        | 频道号                                          |
    +--------------------+-------------------------------------------------+

demo执行的准备工作如下:
    - 1、在声网官网注册，并获取以下参数 `Beken声网注册文档 <../../thirdparty/agora/index.html#id1>`_

        * AGORA_APPID
        * AGORA_RESTFUL_TOKEN


    - 2、根据Agora AI Agent提供的操作手册，启动服务端的AI Agent，可根据需要配置AI Agent是否支持识图。

        a.目前采用PC端启动Agora AI Agent的方式，POST指令请参考声网提供的使用手册 ``<bk_aidk源代码路径>/docs/thirdparty/agora_ai_agent``

        b.也可以， 参考 `Beken AI Agent启动文档 <../../thirdparty/agora/index.html#ai-agent>`_


demo执行的步骤如下:

	1.在PC端打开Agora AI Agent
	 - PC端发送post指令启动Agora AI Agent

	2.设备端wifi连接
	 - demo板发送指令 ``sta test xxxxxx`` 连接2.4GHz名为test的热点

	3.打开语音通话
	 - demo板发送指令 ``agora_test start 0 appid`` 打开语音通话和识图

	4.唤醒并语音交互
	 - 对着mic说唤醒词 ``armino`` ，设备端唤醒后会回一句 ``啊哈`` ，然后可以进行提问，目前设置的提问时间是5s，结束后可以听到AI的回答。

	5.关闭语音通话
	 - demo板发送指令 ``agora_test stop 0 appid`` 关闭语音通话和识图

5、参考链接
--------------------

	声网参考文档：https://docs.agora.io/cn/Agora%20Platform/manage_projects?platform=Android

	声网APPID申请链接：https://sso2.agora.io/cn/v5/login?_gl=1%2ardr355%2a_ga%2aMzkyNDM4ODYyLjE2NzM1MTM3MTU.%2a_ga_BFVGG7E02W%2aMTY3ODg1MjM0My4xMi4wLjE2Nzg4NTIzNDYuMC4wLjA.

	声网AI Agent指导手册： ``<bk_aidk源代码路径>/components/docs/agora_ai_agent``

