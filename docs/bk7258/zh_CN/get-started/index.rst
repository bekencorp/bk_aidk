快速入门
=======================

:link_to_translation:`en:[English]`



Armino AIDK SDK代码下载
------------------------------------

您可从 gitlab 上下载 Armino AIDK SDK，分支信息如下::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules http://gitlab.bekencorp.com/armino/bk_aidk.git -b branch_name

!Note:

    请将branch_name替换成实际的branch或tag。


环境配置及烧录代码
------------------------------------

Armino 支持在 Windows/Linux 平台进行固件烧录, 烧录方法参考烧录工具中指导文档。
以Windows 平台为例， Armino 目前支持 UART 烧录。

具体 `烧录流程 <https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/zh_CN/v2.0.1/get-started/index.html>`_ 请参考 `IDK <https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/zh_CN/v2.0.1/index.html>`_