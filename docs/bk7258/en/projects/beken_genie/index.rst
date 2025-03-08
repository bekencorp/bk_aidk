Beken Genie AI
=================================


:link_to_translation:`zh_CN:[中文]`

1. Overview
---------------------------------


This project is based on an end-to-cloud and cloud-to-large-model design solution.

It supports dual-screen display, providing a visual and voice companionship experience along with emotional value.

The solution enables seamless edge-to-cloud integration, supporting various general-purpose large model designs that can directly connect with platforms like OpenAI, DouBao, and DeepSeek.

It effectively leverages cloud-based distributed deployment to reduce network latency and enhance interaction experience.

The solution supports edge-side AEC (Acoustic Echo Cancellation) and NS (Noise Suppression) audio processing algorithms, as well as G711/G722 codec formats. It also supports KWS (Keyword Spotting) wake-up functions.

The design includes reference solutions and demos for common peripherals, such as gyroscopes, NFC, buttons, vibration motors, Nand Flash, LED light effects, power management, DVP cameras, and dual QPSI screens.


1.1 Features
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    * Hardware:
        * SPI LCD X2 (GC9D01)
        * MIC
        * Speaker
        * SD NAND 60MB
        * NFC (MFRC522)
        * G-Sensor (SC7A20H)
        * PMU (ETA3422)
        * Battery
        * DVP (gc2145)

    * Software:
        * AEC
        * NS
        * ASR
        * WIFI Station
        * BLE
        * BT PAN

.. figure:: ../../../_static/beken_genie_pic.jpg
    :align: center
    :alt: Hardware Development Board
    :figclass: align-center

    Figure 1. Hardware Development Board



1.2 Code Guide
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	Directory: ``<source code>/project/beken_genie``

	Build: ``make bk7258 PROJECT=beken_genie``


2. Architecture
---------------------------------


2.1 software diagram
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


    This AI demo solution is similar to the door lock solution,

    featuring two-way voice communication between the device and the AI large model,

     while the device also transmits images unidirectionally to the AI large model.

     In this solution, the counterpart APK on the other end in the door lock solution has been transformed into an AI Agent robot.

    The software module architecture is as shown in the figure below.

.. figure:: ../../../_static/agora_wanson_ai_arch.png
    :align: center
    :alt: module architecture Overview
    :figclass: align-center

    Figure 2. software module architecture

..

   For voice interaction::

    * The device collects voice through the microphone.
    * The voice data is transmitted to Agora's servers using the Agora SDK.
    * Agora's servers handle communication with the AI Agent large model.
    * The server sends the voice to the AI Agent, receives a response, and forwards the voice response to the device's speaker for playback.


    For image processing::

    * The device captures image data.
    * Each frame of the image is sent to Agora's servers via the Agora SDK.
    * The server then transmits the image to the AI Agent large model for recognition.

2.2 function diagram
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    As shown in the figure below, the interfaces of the multimedia used in the scheme are all defined in media_app.h and aud_intf.h.

.. figure:: ../../../_static/agora_wanson_ai_sw_relationship_diag.png
    :align: center
    :alt: relationship diagram Overview
    :figclass: align-center

    Figure 3. module relationship diagram


2.3 AI Work state machine
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

.. figure:: ../../../_static/bk_genie_statemachine.png
    :align: center
    :alt: State Machine Overview
    :figclass: align-center

    Figure 4. module state diagram

::

    1/2 Green light stays on.
    3/4/5 Green and red lights flash alternately
    6/7 Green light flashes quickly.
    8 Green light flashes quickly.
    9 LCD on, LED off.
    10 LCD off.
    13/14/15 Red light flashes quickly.


3. Kconfig
---------------------------------

    To enable the Agora function library, the following configurations need to be enabled on cpu0:

    +----------------------------------------+----------------+---------------+----------------+
    |Kconfig                                 |   CPU          |   Format      |      Value     |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_AGORA_IOT_SDK                    |   CPU0         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+


4. Demo Guide
---------------------------------

supported commands:

+-------------------------------------------------------+-------------------------------------+
|Command                                                |Description                          |
+-------------------------------------------------------+-------------------------------------+
|agora_test {start|stop appid video_en channel_name}    |Audio+Video                          |
+-------------------------------------------------------+-------------------------------------+


Commands Paramters:

    +--------------------+-------------------------------------------------+
    |appid               | Agora appid                                     |
    +--------------------+-------------------------------------------------+
    |video_en            | Enable Paramters:                               |
    |                    |  - 1: ``ON``                                    |
    |                    |  - 0: ``OFF``                                   |
    +--------------------+-------------------------------------------------+
    |channel_name        | Channel                                         |
    +--------------------+-------------------------------------------------+


The preparation work for the demo is as follows:

Register on the Agora official website and obtain the following parameters:

AGORA_APPID
AGORA_RESTFUL_TOKEN
For detailed information, refer to the Beken Agora registration document: Agora Registration Document.

Start the AI Agent on the server according to the operation manual provided by Agora AI Agent. You can configure whether the AI Agent supports image recognition based on your needs.

Currently, the AI Agent is started on the PC. For POST instructions, refer to the user manual provided by Agora: <bk_aidk source code path>/docs/thirdparty/agora_ai_agent.
You can also refer to the Beken AI Agent start document: AI Agent Start Document.
Burning:

Store the avi video file to be played in the SD NAND. For detailed usage of the SD NAND, refer to: Nand Disk Usage Notes.
Store the <bk_aidk source code path>/project/beken_genie/main/resource/genie_eye.avi file in the SD NAND.
Burn the compiled all-app.bin file and power on to execute.
The steps for demo execution are as follows:

Start Agora AI Agent on the PC.

Send a POST instruction on the PC to start Agora AI Agent. For detailed instructions, refer to: Beken AI Agent Start Document.
Connect the device to Wi-Fi.


Send the command sta test xxxxxx on the demo board to connect to the 2.4GHz hotspot named "test".
Start the device to join the AI chat channel.

Send the command agora_test start appid 0 channel_name on the demo board to join the specified AI chat channel and enable the audio path.
Replace appid and channel_name with actual values. For reference, see: Beken Agora Registration Document.

Wake up the device and start the AI conversation.

Say the wake word "hi armino" to the on-board MIC. After waking up, the device will play a prompt tone "A Ha" and you can start the AI conversation.
Exit the AI conversation.

Say the keyword "byebye armino" to the on-board MIC. After detection, the device will play a prompt tone "Byebye", then go to sleep and stop the AI conversation.
Leave the AI chat channel.

Send the command agora_test stop on the demo board to leave the AI chat channel and disable the audio path.
Speaker Volume Control:

Increase volume.

Press the S1 button to increase the volume.
Decrease volume.

Press the S2 button to decrease the volume.


AVI video file replacement:

1.Convert the avi video file to be used using the conversion tool located at <bk_aidk source code path>/bk_avdk/components/multimedia/tools/aviconvert/bk_avi.7z in the SDK.

    For specific usage instructions, refer to the readme.txt file included with the tool.

2.Place the converted file back into the SD NAND and rename it to contain only English letters or numbers.

3.Modify the file name in the function call AVI_open_input_file("/genie_eye.avi", 1) within the <bk_aidk source code path>/project/beken_genie/main/av_play/avi_play.c file.

