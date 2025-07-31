Volcengine RTC
=================================

:link_to_translation:`zh_CN:[中文]`

**1. Introduction to Volcengine RTC**
---------------------------------
Volcengine Real Time Communication (veRTC) provides highly reliable, high-concurrency, low-latency real-time audio and video communication capabilities worldwide, enabling various types of real-time communication and interaction.
For detailed introduction, please refer to online materials: https://www.volcengine.com/product/veRTC

**2. Distribution of Volcengine RTC Related Code**
---------------------------------
 - Volcengine RTC HAL layer and library code directory: ``/bk_ai/bk_avdk/components/bk_thirdparty/VolcEngineRTCLite``
 - BK and Volcengine RTC transmission integration: ``/bk_ai/project/common_components/network_transfer/volc_rtc``
 - Volcengine RTC demo project directory: ``/bk_ai/project/volc_rtc``

**3. Prerequisites for Using Volcengine RTC**
---------------------------------
To use Volcengine RTC and large models for AI dialogue, you need to activate related services and configure permissions. For details, please refer to Volcengine's online documentation:
https://www.volcengine.com/docs/6348/1315561

**4. Agent Startup Methods**
---------------------------------
AIDK includes two methods to start the Agent: starting the Agent through the BK server and starting the agent through a developer-deployed server. By default, AIDK starts the Agent through the BK server to demonstrate the Volcengine RTC effect to developers.

.. attention::
    *Starting the Agent through the BK server for AI dialogue is a demo display, with each conversation limited to 5 minutes.*
    *If you need a longer conversation experience, you can contact the staff who provided the AIDK development board to obtain a license file. For details, please refer to the license function description in Chapter 5.*
    **We strongly recommend that developers set up their own servers to facilitate duration control, device management, and differentiated deployment.**

**4.1 Starting Agent through BK Server**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
After network configuration is completed, the program will call ``bk_sconf_wakeup_agent()`` to request the BK server to start the Agent and the device-side RTC, and then ask the device to join the room where the Agent is located for AI dialogue.

**4.2 Starting Agent through Developer-Deployed Server**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
To facilitate device management for developers, AIDK also supports developers to start the Agent through a personally deployed server. For server deployment, please refer to Volcengine's open-source code and guidelines: https://github.com/volcengine/rtc-aigc-embedded-demo
To start the Agent using this method, the following configurations are required:

4.2.1 Modify beken_genie Volcengine RTC Project Configuration File
++++++++++++++++++++++++++++++++++++++++++

Modify the ``/bk_ai/project/volc_rtc/config/bk7258/config`` file, and enable the code related to starting the Agent from the custom deployment server by setting CONFIG_BK_DEV_STARTUP_AGENT=y.

.. code::

    #@ Enable start agent on device
    CONFIG_BK_DEV_STARTUP_AGENT=y

4.2.2 Modify Volcengine Configuration File
++++++++++++++++++++++++++++++++++++++++++
Modify the ``/bk_ai/project/common_components/network_transfer/volc_rtc/volc_config.h`` configuration to request required parameters from the custom server.

.. code-block:: c

    // RTC APP ID
    #define CONFIG_RTC_APP_ID    "67********************d1"
    // Server address
    #define CONFIG_AGENT_SERVER_HOST   "1**.1**.**.**:8080"


**5. License Function Description**
---------------------------------
Volcengine RTC supports license-based billing. License billing follows special billing rules; please confirm with Volcengine before use.
To use license-based billing, ensure that the CONFIG_VOLC_RTC_ENABLE_LICENSE macro in the ``/bk_ai/project/volc_rtc/config/bk7258/config`` file is configured as y.
The license is bound to the device UID by default. The device UID can be obtained through ``byte_print_finger()``.

After obtaining the license file, name it VolcEngineRTCLite.lic and copy it to the root directory of the file system.

.. note::
    The BK7258 AIDK file system uses SD-NAND by default. The license file needs to be copied to the root directory of SD-NAND.
    For specific usage methods of SD NAND, please refer to `Nand Disk Usage Notes <../../api-reference/nand_disk_note.html>`_.
    Developers can contact the staff who provided the AIDK development board to obtain a license file for experience.

**6. Personalized Configuration**
---------------------------------

**6.1 Audio Codec Configuration**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
Volcengine supports common audio encoding and decoding formats. The AIDK Volcengine RTC project uses opus by default. If you want to change to G722 encoding and decoding, you can modify the AUDIO_ENCODER/AUDIO_DECODER configurations in the two files ``/bk_ai/project/volc_rtc/config/bk7258/config`` and ``/bk_ai/projects/volc_rtc/config/bk7258_cp1/config``.

.. code::

    #@ DAC sample rate
    CONFIG_AUDIO_ENCODER_TYPE="G722"
    #@ DAC sample rate
    CONFIG_AUDIO_DECODER_TYPE="G722"

Volcengine RTC operations related to audio codec include the following:
 - Use the ``byte_rtc_set_audio_codec`` interface to set the audio encoding type before ``byte_rtc_join_room``
 - Fill in the corresponding video_frame_info through ``byte_rtc_send_audio_data``, for details, refer to the implementation of the ``bk_byte_rtc_audio_data_send`` function
 - When starting the Agent, configure the corresponding audio encoding type through the audio_codec parameter, for details, refer to the implementation of the ``start_voice_bot`` function

**6.2 Visual Recognition Function**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
The Volcengine project supports visual recognition by default. The AIDK Volcengine project does not enable visual recognition by default after network configuration. You can switch between visual recognition and ordinary AI dialogue functions through the S2 button.
If you want to enable visual recognition by default after network configuration, you can set CONFIG_VOLC_ENABLE_VISION_BY_DEFAULT=y in the ``/bk_ai/project/volc_rtc/config/bk7258/config`` file.

The following points should be noted when enabling visual recognition:
 - The auto_publish_video in the options parameter needs to be set to true when calling ``byte_rtc_join_room``
 - Fill in the corresponding audio_data_type through ``byte_rtc_send_video_data``, for details, refer to the implementation of the ``bk_byte_rtc_video_data_send`` function
 - When starting the Agent, enable visual understanding through the vision_enable parameter and fill in the corresponding image configuration, for details, refer to the implementation of the ``start_voice_bot`` function
 - The backend large model needs to support visual recognition function

**6.3 Subtitle Function**
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
The Volcengine project disables the subtitle function by default. If you want to enable the subtitle function, you can set CONFIG_VOLC_ENABLE_SUBTITLE_BY_DEFAULT=y in the ``/bk_ai/project/volc_rtc/config/bk7258/config`` file.
The subtitle receiving function is the __on_message_received function in the ``/bk_ai/project/common_components/network_transfer/volc_rtc/volc_rtc.c`` file.
For the Volcengine subtitle format, please refer to the Volcengine documentation: https://www.volcengine.com/docs/6348/1337284

**7. Reference Links**
--------------------

Volcengine Reference Documentation: https://www.volcengine.com/product/veRTC

Volcengine Related Account Application Link: https://www.volcengine.com/docs/6348/1315561

Volcengine Agent Server Deployment: https://github.com/volcengine/rtc-aigc-embedded-demo

Volcengine Demo Project: `BK7258 Volcengine RTC Demo <../../projects/volc_rtc/index.html>`_