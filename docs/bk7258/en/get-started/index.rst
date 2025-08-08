Quick Start Guide
==============================================

:link_to_translation:`zh_CN:[中文]`



Armino AIDK SDK Code download
--------------------------------------------------------------------

We can download Armino AIDK SDK from gitlab::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules https://gitlab.bekencorp.com/armino/bk_ai/bk_aidk.git -b ai_release/v2.0.1

The branch of AIDK on gitlab is ai_release/v2.0.1. If you need to get a specific version, you can replace branch_name with tag::


    git clone --recurse-submodules https://gitlab.bekencorp.com/armino/bk_ai/bk_aidk.git -b ai_release/v2.0.1.x


If we don't have a gitlab account, we can download the Armino AIDK SDK from `github <https://github.com/bekencorp/bk_aidk#>`_ . The branch information is as follows::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules git@github.com:bekencorp/bk_aidk.git -b ai_release/v2.0.1


.. note::

    The bk_avdk_ai repository and bk_idk_ai repository on GitHub are both sub-repositories of the bk_aidk repository. Use the above git clone --recurse-submodules command to download the bk_aidk repository.

Armino AIDK Project Compilation
------------------------------------

Enter the AIDK directory and compile the beken_genie project::

    cd ~/armino
    make bk7258 PROJECT=beken_genie

After the compilation is complete, the all-app.bin file will be generated in the build/beken_genie/bk7258 directory. You can use the programming tool to flash it to the development board.

Build Compilation Environment:
--------------------------------------------------------------------

Armino supports firmware burning on Windows/Linux platforms.

For burning methods, please refer to the guidance documents within the burning tool.

For Windows platform, as an example, Armino currently supports UART burning.

 `Burning Process Documentation <https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/en/v_ai_2.0.1/get-started/index.html>`_ please refer to the IDK Documentation `IDK <https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/en/v_ai_2.0.1/index.html>`_