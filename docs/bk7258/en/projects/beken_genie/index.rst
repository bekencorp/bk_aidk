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

1.2 Button
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Increase volume.
        - Press the S1 button to increase the volume.
    Decrease volume.
        - Press the S2 button to decrease the volume.

1.3 LED
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


1.4 ASR
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    1. ``Hi Armino`` is used to wake up, enabling interaction between local and cloud AI, while the LCD lights up and displays eye animations.

        Response phrase: ``A Ha``

    2. ``byebye armino`` is used to turn off, enabling interaction between local and cloud AI, while closing the LCD and no longer displaying eye animations.

        Response phrase: ``Byebye``


2. Development Guide
---------------------------------

2.1 Module Diagram
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    This AI demo solution is similar to the door lock solution,

    featuring two-way voice communication between the device and the AI large model,

     while the device also transmits images unidirectionally to the AI large model.

     In this solution, the counterpart APK on the other end in the door lock solution has been transformed into an AI Agent robot.

    The software module architecture is as shown in the figure below.

.. figure:: ../../../_static/agora_wanson_ai_arch.png
    :align: center
    :alt: module Overview
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

2.2 Network Provisioning and Communication
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

.. figure:: ../../../_static/demo_flow_sequence_en.png
    :align: center
    :alt: State Machine Overview
    :figclass: align-center

    Figure 3. Operation Flow Sequence


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


2.4 Kconfig
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    To enable the Agora function library, the following configurations need to be enabled on cpu0:

    +----------------------------------------+----------------+---------------+----------------+
    |Kconfig                                 |   CPU          |   Format      |      Value     |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_AGORA_IOT_SDK                    |   CPU0         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+

    To enable Network Provisioning and start agent, the following configurations need to be enabled on cpu0:

    +----------------------------------------+----------------+---------------+----------------+
    |Kconfig                                 |   CPU          |   Format      |      Value     |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_NETWORK_AUTO_RECONNECT           |   CPU0         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+


2.5 Critical Code Explanation
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

2.5.1 About Network Provisioning
+++++++++++++++++++++++++++++++++

 The Wi-Fi provisioning-related code is mainly located in bk_genie_smart_config.c and boarding_core.c. Customers can
 choose to replace the following functions with their own implementation, while the rest can follow beken solution.

1. bk_genie_smart_config_init: for the initialization of Network Provisioning and auto reconnect.

.. code::

    int bk_genie_smart_config_init(void)
    {
        int flag;

        event_handler_init();
        flag = demo_network_auto_reconnect();   //judge whether to reconnect
        if (flag != 0x71l && flag != 0x73l
    #if CONFIG_NET_PAN
            && flag != 0x74l
    #endif
        ) {
            bk_genie_prepare_for_smart_config();    //no need to reconnect, begin for Network Provisioning
        }
        return 0;
    }


2. bk_genie_prepare_for_smart_config: begin for Network Provisioning, customers may need to adapter their own solution

.. code::

    void bk_genie_prepare_for_smart_config(void)
    {
        smart_config_running = true;
        app_event_send_msg(APP_EVT_NETWORK_PROVISIONING, 0);
        network_provisioning_stop_timeout_check();
        agora_stop();
        demo_erase_network_auto_reconnect_info();       //erase AP info
        bk_genie_erase_agent_info();                    //erase agent info
        wifi_boarding_adv_start();
        network_provisioning_start_timeout_check();
    }


3. bk_genie_message_handle: switch agent info with Smart phone, customers may need to adapter their own solution

.. code::

    static void bk_genie_message_handle(void)
    {
            ……
        case DBEVT_START_AGORA_AGENT_START:
        {
            ……
            //upload uid to beken server, to generate channel_name
            ……
        }
        break;
        case DBEVT_START_AGORA_AGENT_RSP:
        {
            ……
            //receive channel_name
            ……
        }
        break;
            ……
    }

4. bk_genie_sconf_netif_event_cb: wakeup agent after wifi connected, save network and agent info

5. bk_genie_erase_agent_info, bk_genie_save_agent_info, bk_genie_get_agent_info, bk_genie_wakeup_agent: all for beken agent solution,
customers may need to adapter their own solution


3. Demonstration instructions
---------------------------------

3.1 Source code download & build
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    * `AIDK download <../../get-started/index.html#armino-aidk-sdk>`_

    * `Set Up SDK Build Environment <https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/zh_CN/v2.0.1/get-started/index.html>`_

    * Compile Code:: ``make bk7258 PROJECT=beken_genie``

        * Compile in the source code root directory
        * The project directory is located at``<source code>/project/beken_genie``

    * `Program Code <https://docs.bekencorp.com/arminodoc/bk_idk/bk7258/zh_CN/v2.0.1/get-started/index.html>`_

        * The binary file for programming is located at ``<source code>/build/beken_genie/bk7258/all-app.bin``

3.2 Adjusting UI Resource Formats
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    - 1. Convert the avi video files to be used using the format conversion tool located at ``<bk_aidk source code path>/bk_avdk/components/multimedia/tools/aviconvert/bk_avi.7z`` in the SDK. For detailed usage instructions, please refer to the readme.txt file included with the tool.

    - 2. Place the converted files back into the SD NAND and rename them to contain only English letters or numbers.

    - 3. Modify the file name passed to the function ``AVI_open_input_file("/genie_eye.avi", 1)`` in the file ``<bk_aidk source code path>/project/beken_genie/main/av_play/avi_play.c``.

3.3 APP Registration and Download
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    APP download: https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_download/index.html

    Registration: use email


3.4 Firmware Burning and Resource File Burning
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	- 1. Store the avi video file to be played in the SD NAND. For specific usage of SD NAND, refer to Nand Disk Usage Notes <../../api-reference/nand_disk_note.html>_.
	- 2. Store the file <bk_aidk source code path>/project/beken_genie/main/resource/genie_eye.avi from the SDK into the SD NAND.
	- 3. Burn the compiled all-app.bin file and power on to execute.

3.5 Operation Steps
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

3.5.1 Beken App
+++++++++++++++++++++++++++++++++

    a)operate beken app as follow pictures:

    .. figure:: ../../../_static/add_ai_device_1.png
        :scale: 30%

    .. figure:: ../../../_static/add_ai_device_2.png
        :scale: 30%

    .. figure:: ../../../_static/add_ai_device_3.png
        :scale: 30%

    .. figure:: ../../../_static/add_ai_device_4.png
        :scale: 30%

    b)long press Key 2 for 3s to enter network provisioning mode:

    .. figure:: ../../../_static/add_ai_device_9.png
        :scale: 70%

    c)click the device scan by smart phone

    .. figure:: ../../../_static/add_ai_device_5.png
        :scale: 30%

    .. figure:: ../../../_static/add_ai_device_6.png
        :scale: 30%

    .. figure:: ../../../_static/add_ai_device_7.png
        :scale: 30%

    d)Say the wake-up word ``hi armino`` to the onboard mic, the device will play the prompt tone ``aha`` after waking up,
    and then you can have an AI conversation

      Say the key word ``byebye armino`` to the onboard mic, the device will play the prompt tone ``byebye`` after detecting it,
      then go to sleep and stop talking to the AI


3.5.1 Command line
+++++++++++++++++++++++++++++++++

1.Start Agora AI Agent on PC
     - refer to `Beken AI Agent Start Document <../../thirdparty/agora/index.html#ai-agent>`_

     demo as below, appid, restful_key and xxx need customers to fill in their owns.

.. code::

    curl --location --request POST 'https://api.agora.io/api/conversational-ai-agent/v2/projects/{appid}/join' --header 'Content-Type: application/json' --header 'Authorization: Basic {restful_key}' --data-raw '{
        "name": "channel_name",
        "properties": {
            "channel": "channel_name",
            "token": "xxx",
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
                "url": "xxx",
                "api_key": "xxx",
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
                "vendor": "bytedance",
                "params": {
                    "token": "xxx",
                    "app_id": "xxx",
                    "cluster": "volcano_tts",
                    "speed_ratio": 1.0,
                    "volume_ratio": 10.0,
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


2.connect to AP on the device
     - enter ``sta test xxxxxx`` to connect a 2.4GHz AP

3.device(RTC) join the channel
     - enter ``agora_test start appid 0 channel_name`` to join the channel named "channel_name", addid need to fill by costomers

        refer to `Beken Agora Registration Document <../../thirdparty/agora/index.html#id1>`_

4.wakeup and communicate
     - Say the wake-up word ``hi armino`` to the onboard mic, the device will play the prompt tone ``aha`` after waking up, and then you can have an AI conversation

5.stop the communication
     - Say the key word ``byebye armino`` to the onboard mic, the device will play the prompt tone ``byebye`` after detecting it, then go to sleep and stop talking to the AI

6.leave the channel and stop audio
     - enter ``agora_test stop``



4. Debugging Commands
---------------------------------
.. warning::

	Notes for Reading This Chapter:

	Debugging commands are intended solely for developers who have a good understanding of the code.

	If you are not familiar with the code and process, please first go through the demonstration steps below to familiarize yourself with and understand the code before deciding whether to

..

4.1 Command List:
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    +-------------------------------------------------------+-------------------------------------+
    |Command                                                |Description                          |
    +-------------------------------------------------------+-------------------------------------+
    |agora_test {start|stop appid video_en channel_name}    | Voice call + image recognition      |
    +-------------------------------------------------------+-------------------------------------+

4.2 Start Command
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    +----------------+-------------+-------------------------------------------------+
    |agora_test      |  Mandatory  | Command                                         |
    +----------------+-------------+-------------------------------------------------+
    |start           |  Mandatory  | Parameter, enables connection to RTC Channel    |
    +----------------+-------------+-------------------------------------------------+
    |appid           |  Mandatory  | Parameter, Agora's APPID                        |
    +----------------+-------------+-------------------------------------------------+
    |video_en        |  Mandatory  | Parameter, image recognition feature toggle:    |
    |                |             |   - 1: ``Enable``                               |
    |                |             |   - 0: ``Disable``                              |
    +----------------+-------------+-------------------------------------------------+
    |channel_name    |  Mandatory  | Parameter, channel name                         |
    +----------------+-------------+-------------------------------------------------+

.. note::

    Before using this command, start the AI Agent on your PC end.

    - 1. Register on Agora's official website and obtain the following parameters `Beken Agora Registration Document <../../thirdparty/agora/index.html#id1>`_

        * AGORA_APPID
        * AGORA_RESTFUL_TOKEN

    - 2. According to the operation manual provided by Agora AI Agent, start the AI Agent on the server side.

        a. Currently, the AI Agent on the PC end is started, and the POST command reference please refer to the user manual provided by Agora ``<bk_aidk source code path>/docs/thirdparty/agora_ai_agent``

        b. You can also refer to `Beken AI Agent Start Document <../../thirdparty/agora/index.html#ai-agent>`_


4.2 Stop Command
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    +----------------+------------+-------------------------------------------------+
    |agora_test      |  Mandatory | Command                                         |
    +----------------+------------+-------------------------------------------------+
    |stop            |  Mandatory | Parameter, disconnects the current RTC Channel  |
    +----------------+------------+-------------------------------------------------+