Agora-AI
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是基于声网Agora AI的AI demo，支持语音实时聊天和图像识别，支持OpenAI、豆包等常用的国内外大模型。

1.1 规格
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	* 硬件配置：
		* 核心板，**BK7258_QFN88_9X9_V3.2**
		* 麦克小板，**BK_Module_Microphone_V1.1**
        * 喇叭小板，**BK_Module_Speaker_V1.1**
		* PSRAM 8M/16M
        * DVP Camera GC2145

.. note::
	- 1、Agora AI要求音频流格式为：单声道、8K采样率、16bit位宽。
	- 2、Agpra AI识图功能支持H264和jpeg，对图片分辨率没有要求，为降低网络带宽，每秒发送1-2帧图片即可。
	- 3、Agora AI支持通话打断功能，因此正常工作时必须保证不会产生回声，因此喇叭外放时需要将AEC打开且调试好性能。

1.2 路径
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	工程路径: ``<bk_aidk源代码路径>/project/agora_ai``

	Agora-iot-sdk库API接口的详细说明请参考源文件: ``<bk_aidk源代码路径>/bk_avdk/components/bk_thirdparty/agora-iot-sdk/include/bk7258/agora_rtc_api.h``

	project编译指令: ``make bk7258 PROJECT=agora_ai``


2. 框架图
---------------------------------


2.1 软件模块架构图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


    此AI demo方案和门锁方案类似，设备端和和AI大模型端双向语音通话，同时设备端向AI大模型端单向图传，门锁方案中的对端apk变为了AI Agent机器人。
	软件模块架构如下图所示：

.. figure:: ../../../_static/agora_ai_arch.png
    :align: center
    :alt: module architecture Overview
    :figclass: align-center

    Figure 1. software module architecture
    agora_ai software module architecture

..

    * 方案中，设备端采集mic语音，通过agora sdk将语音数据发送至声网服务器，声网服务器负责和AI Agent大模型的交互，将mic语音发送至AI Agent并获取回复，再将语音回复发送至设备端喇叭播放。
    * 方案中，设备端采集图像，通过agora sdk将每帧图像发送至声网服务器，声网服务器再将图像送至AI Agent大模型进行识别。


2.2 代码模块关系图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    如下图所示，方案使用的多媒体的接口，都定义在 **media_app.h** 和 **aud_intf.h** 中。

.. figure:: ../../../_static/agora_ai_sw_relationship_diag.png
    :align: center
    :alt: relationship diagram Overview
    :figclass: align-center

    Figure 1. module relationship diagram

    agora_ai module relationship diagram

3. 配置
---------------------------------

	打开声网功能库需要在 ``cpu0`` 上打开以下配置:

    +----------------------------------------+----------------+---------------+----------------+
    |Kconfig                                 |   CPU          |   Format      |      Value     |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_AGORA_IOT_SDK                    |   CPU0         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+


4. 演示说明
---------------------------------

demo支持的命令如下表:

+-------------------------------------------------------+-------------------------------------+
|Command                                                |Description                          |
+-------------------------------------------------------+-------------------------------------+
|agora_test {start|stop appid video_en channel_name}    |语音通话+识图                        |
+-------------------------------------------------------+-------------------------------------+


命令参数说明如下：

    +--------------------+-------------------------------------------------+
    |appid               | 注册申请的appid                                 |
    +--------------------+-------------------------------------------------+
    |video_en            | 识图功能开关:                                   |
    |                    |  - 1: ``打开``                                  |
    |                    |  - 0: ``关闭``                                  |
    +--------------------+-------------------------------------------------+
    |channel_name        | 频道名                                          |
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

	3.打开语音通话和识图
	 - demo板发送指令 ``agora_test start 1 appid`` 打开语音通话和识图

	4.关闭语音通话和识图
	 - demo板发送指令 ``agora_test stop 1 appid`` 关闭语音通话和识图

5、参考链接
--------------------

	声网参考文档：https://docs.agora.io/cn/Agora%20Platform/manage_projects?platform=Android

	声网APPID申请链接：https://sso2.agora.io/cn/v5/login?_gl=1%2ardr355%2a_ga%2aMzkyNDM4ODYyLjE2NzM1MTM3MTU.%2a_ga_BFVGG7E02W%2aMTY3ODg1MjM0My4xMi4wLjE2Nzg4NTIzNDYuMC4wLjA.

	声网AI Agent指导手册： ``<bk_aidk源代码路径>/components/docs/agora_ai_agent``

