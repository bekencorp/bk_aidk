Beken AI SDK
=================================


* [English Version](./README.md)


1. 简介
---------------------------------

    本方案是基于，端对云，云对大模型的设计方案。

    支持双屏显示，提供视觉加语音的陪伴体验和情绪价值。

    支持端侧打通，各种通用大模型的设计方案，直接对接Open AI、豆包、DeepSeek等。

    并且能够有效，利用云的分布式部署，降低网络延迟，提高交互体验。

    支持端侧AEC，NS等音频处理算法，支持G711/G722编码格式，支持KWS关键字打断唤醒，支持提示音播放。

    包含常用外设的参考设计以及Demo，比如，陀螺仪，NFC，按键，震动马达，Nand Flash，LED灯效，充电管理，DVP camera，双QPSI屏。


2. 规格
---------------------------------

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

    * 软件特性：
        * AEC
        * NS
        * G722 / G711u
        * 唤醒词定制
        * WIFI Station
        * BLE
        * BT PAN

完整方案文档请参考:
https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/projects/beken_genie/index.html


3. Armino AIDK SDK代码下载
---------------------------------

您可从 gitlab 上下载 Armino AIDK SDK, 分支信息如下::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules https://gitlab.bekencorp.com/armino/bk_ai/bk_aidk.git -b ai_release/v2.0.1

AIDK在gitlab上的分支是ai_release/v2.0.1, 如果需要取特定的版本, 可以将branch_name替换成tag::


    git clone --recurse-submodules https://gitlab.bekencorp.com/armino/bk_ai/bk_aidk.git -b ai_release/v2.0.1.x


如果您还没有gitlab账号, 您可从 https://github.com/bekencorp/bk_aidk 下载 Armino AIDK SDK, 分支信息如下::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules git@github.com:bekencorp/bk_aidk.git -b ai_release/v2.0.1


4. Armino AIDK SDK在github上的仓库
------------------------------------

github上bk_avdk_ai仓库和bk_idk_ai仓库都是bk_aidk仓库的子仓库, 使用上述git clone --recurse-submodules命令下载bk_aidk仓库即可::


    bk_aidk
        |____bk_avdk_ai
            |____bk_idk_ai


5. 软件编译
------------------------------------

以 beken_genie 工程为例, 编译方法如下::


    cd ~/armino/bk_aidk
    make bk7258 PROJECT=beken_genie


6. 环境配置及固件烧录
---------------------------------

Armino 支持在 Windows/Linux 平台进行固件烧录, 烧录方法参考烧录工具中指导文档。
以Windows 平台为例， Armino 目前支持 UART 烧录。

具体烧录流程请参考 https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/zh_CN/v_ai_2.0.1/get-started/index.html#burn-code
