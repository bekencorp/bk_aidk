Beken AI SDK
=================================


* [中文版](./README_CN.md)


1. Overview
---------------------------------

    This solution is based on an end-to-cloud and cloud-to-large-model architecture.

    It supports dual-screen display and provides a companionship experience with both visual and voice interaction, offering emotional value.

    It supports end-side integration and design schemes for various general-purpose large models, allowing direct connection to OpenAI, Bao (Doubao), DeepSeek, and others.

    Furthermore, it effectively leverages the distributed deployment of the cloud to reduce network latency and improve the interactive experience.

    It supports end-side AEC (Acoustic Echo Cancellation), NS (Noise Suppression) and other audio processing algorithms, G711/G722 encoding formats, KWS (Keyword Spotting) interrupt wake-up, and prompt sound playback.

    It includes reference designs and demos for common peripherals, such as gyroscopes, NFC, buttons, vibration motors, Nand Flash, LED lighting effects, charging management, and DVP cameras, as well as dual-QPSI screens.


2. Specifications
---------------------------------

    * Hardware specs：
        * SPI LCD X2 (GC9D01)
        * Mic
        * Speaker
        * SD NAND 60MB
        * NFC (MFRC522)
        * Gyroscope (SC7A20H)
        * Battery Management Chip (ETA3422)
        * Li-ion battery
        * DVP (gc2145)

    * Software features：
        * AEC
        * NS
        * G722 / G711u
        * Customizable wake word
        * WIFI Station
        * BLE
        * BT PAN


For the complete solution, please refer to the documentation:
https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/en/v2.0.1/projects/beken_genie/index.html


3. Armino AIDK SDK Code Download
------------------------------------

The Armino AIDK SDK is available for download from GitLab. The branch details are provided below::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules https://gitlab.bekencorp.com/armino/bk_ai/bk_aidk.git -b ai_release/v2.0.1

The AIDK branch on GitLab is ai_release/v2.0.1. To retrieve a specific version, replace branch_name with a tag::


    git clone --recurse-submodules https://gitlab.bekencorp.com/armino/bk_ai/bk_aidk.git -b ai_release/v2.0.1.x


If you don't have a GitLab account yet, you can download the Armino AIDK SDK from https://github.com/bekencorp/bk_aidk. Here are the branch details::


    mkdir -p ~/armino
    cd ~/armino
    git clone --recurse-submodules git@github.com:bekencorp/bk_aidk.git -b ai_release/v2.0.1


4. The Armino AIDK SDK repository on GitHub
---------------------------------------------

bk_avdk_ai and bk_idk_ai on GitHub are sub-repositories of bk_aidk. Use git clone --recurse-submodules to download bk_aidk and its submodules::


    bk_aidk
        |____bk_avdk_ai
            |____bk_idk_ai


5. Firmware build
------------------------------------

Taking the beken_genie project as an example, the compilation method is as follows::


    cd ~/armino/bk_aidk
    make bk7258 PROJECT=beken_genie



6. Environment configuration and firmware flashing
-----------------------------------------------------

Armino supports firmware flashing on Windows/Linux platforms. Please refer to the documentation within the flashing tool for instructions.

Taking Windows as an example, Armino currently supports UART flashing.

Please refer to the following documentation for the specific flashing process:
 https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/en/v_ai_2.0.1/get-started/index.html#burn-code