Rock_Paper_Scissors
=================================


:link_to_translation:`zh_CN:[中文]`

1. Overview
---------------------------------

This project is a rock-paper-scissors game demo based on an open-source gesture detection model and the TensorFlow Lite lightweight deep learning framework. It uses the camera to detect hand gestures, which are then compared to a randomly generated rock-paper-scissors category. The game images are displayed on two screens, and the outcome is announced through voice broadcasting.


1.1 Features
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    * Hardware:
        * SPI LCD X2 (GC9D01)
        * MIC
        * Speaker
        * SD NAND 128MB
        * NFC (MFRC522)
        * G-Sensor (SC7A20H)
        * PMU (ETA3422)
        * Battery
        * DVP (gc2145)


2. Test Description
---------------------------------

When testing the project, you need to stretch out your hand in the direction shown in the picture, and you need to place your hand gesture directly in front of the camera about 50cm above.

.. figure:: ../../../_static/rock_paper_scissors_pic.jpg
    :align: center
    :alt: Rock_Paper_Scissors Gesture Direction
    :figclass: align-center

    Figure 1. Rock_Paper_Scissors Gesture Direction


3. Test Steps
---------------------------------

    - 1. After the board power on, a voice prompt will be heard to prepare to punch, place your hand gesture directly in front of the camera  and keep it still until the voice prompt to put your hand down.

    - 2. After the camera captures the image, the device will start detecting and comparing the detection result with the device's random punch result.

    - 3. If the detection is successful, the two LCD screens will display the punch results of both sides, and the voice will announce the win or lose result. Otherwise, it will prompt that the detection was not successful.
