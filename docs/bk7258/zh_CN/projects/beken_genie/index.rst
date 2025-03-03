Beken Genie AI
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是基于，端对云，云对大模型的设计方案。

支持双屏显示，提供视觉加语音的陪伴体验和情绪价值。

支持端侧打通，各种通用大模型的设计方案，直接对接Open AI、豆包、DeepSeek等。

并且能够有效，利用云的分布式部署，降低网络延迟，提高交互体验。



1.1 规格
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    * 硬件配置：
        * SPI LCD X2 (GC9D01)
        * 麦克
        * 喇叭
        * SD NAND 60MB
        * NFC (MFRC522)
        * 陀螺仪 (SC7A20H)
        * 充电管理芯片 (ETA3422)
        * 锂电池
        * DVP (gc2145)

.. figure:: ../../../_static/beken_genie_pic.jpg
    :align: center
    :alt: Hardware Development Board
    :figclass: align-center

    Figure 1. Hardware Development Board



1.2 路径
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	工程路径: ``<bk_aidk源代码路径>/project/beken_genie``

	project编译指令: ``make bk7258 PROJECT=beken_genie``


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

    Figure 2. software module architecture

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

    Figure 3. module relationship diagram


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

        b.也可以参考 `Beken AI Agent启动文档 <../../thirdparty/agora/index.html#ai-agent>`_

    - 3、烧录：

        a.将要播放的avi视频文件存放到SD NAND中，SD NAND具体使用方法可参考 `Nand磁盘使用注意事项 <../../api-reference/nand_disk_note.html>`_

        b.将SDK中的 ``<bk_aidk源代码路径>/project/beken_genie/main/resource/genie_eye.avi`` 文件存放到SD NAND中

        c.烧录编译好的all-app.bin文件并上电执行即可。


demo执行的步骤如下:

	1.在PC端打开Agora AI Agent
	 - PC端发送post指令启动Agora AI Agent  参考 `Beken AI Agent启动文档 <../../thirdparty/agora/index.html#ai-agent>`_

	2.设备端wifi连接
	 - demo板发送指令 ``sta test xxxxxx`` 连接2.4GHz名为test的热点

	3.启动设备端进入AI对话频道
	 - demo板发送指令 ``agora_test start appid 0 channel_name`` 加入指定的AI对话频道并打开音频通路

        其中appid和channel_name需要替换成实际值，参考 `Beken声网注册文档 <../../thirdparty/agora/index.html#id1>`_

	4.唤醒设备端，进行AI对话
	 - 对板载mic说唤醒词 ``hi armino`` ，设备唤醒后会播放提示音 ``啊哈`` ，然后可以进行AI对话

	5.退出设备端与AI的对话
	 - 对板载mic说关键词词 ``byebye armino`` ，设备检测到后会播放提示音 ``byebye`` ，然后进入睡眠，停止与AI的对话

	6.设备端离开AI对话频道
	 - demo板发送指令 ``agora_test stop`` 离开AI对话频道并关闭音频通路


喇叭音量控制:

	1.调大音量
	 - 单击 ``S1`` 按钮调大音量

	2.调小音量
	 - 单击 ``S2`` 按钮调小音量


AVI视频文件替换:

	- 1、将要使用的avi视频文件通过SDK中的 ``<bk_aidk源代码路径>/bk_avdk/components/multimedia/tools/aviconvert/bk_avi.7z`` 转换工具进行格式转换，具体使用方法可参考工具中的readme.txt说明

	- 2、将转换后的文件重新放进SD NAND中，并修改为只包含英文或数字的名称

	- 3、修改 ``<bk_aidk源代码路径>/project/beken_genie/main/av_play/avi_play.c`` 文件中传入函数 ``AVI_open_input_file("/genie_eye.avi", 1)`` 的文件名。


5、参考链接
--------------------

	声网参考文档：https://docs.agora.io/cn/Agora%20Platform/manage_projects?platform=Android

	声网APPID申请链接：https://sso2.agora.io/cn/v5/login?_gl=1%2ardr355%2a_ga%2aMzkyNDM4ODYyLjE2NzM1MTM3MTU.%2a_ga_BFVGG7E02W%2aMTY3ODg1MjM0My4xMi4wLjE2Nzg4NTIzNDYuMC4wLjA.

	声网AI Agent指导手册： ``<bk_aidk源代码路径>/components/docs/agora_ai_agent``

