AI Dashboard
=================================


:link_to_translation:`zh_CN:[中文]`

1. Overview
---------------------------------

This project is based on the beken_genie project and implements a single-screen display project. The functions included in this project are the same as those of the beken_genie project. The difference is that the beken_genie project is a dual-screen display, while this project is a single-screen display. This project uses a 360X360 resolution QSPI LCD, and the IC model is ST77916.

For detailed instructions and introductions, please refre to `Beken Genie AI Project <../../projects/beken_genie/index.html>`_

2. Code description
---------------------------------

 - 1.Compared with project beken_genie, this project needs to configure the relevant macros of QSPI LCD, as shown below:

    +----------------------------------------+----------------+---------------+----------------+
    |Kconfig                                 |   CPU          |   Format      |      Value     |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_LCD_QSPI_ST77916                 |   CPU1         |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_LCD_BACKLIGHT_GPIO               |   CPU1         |   int         |        25      |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_LCD_QSPI_RESET_PIN               |   CPU1         |   int         |        45      |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_LCD_QSPI_ID                      |   CPU1         |   int         |        1       |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_LCD_QSPI_DEVICE_NUM              |   CPU1         |   int         |        1       |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_SINGLE_SCREEN_AVI_PLAY           |   CPU0 & CPU1  |   bool        |        y       |
    +----------------------------------------+----------------+---------------+----------------+
    |CONFIG_DUAL_SCREEN_AVI_PLAY             |   CPU0 & CPU1  |   bool        |        n       |
    +----------------------------------------+----------------+---------------+----------------+

    Among them, the three macros ``CONFIG_LCD_BACKLIGHT_GPIO`` , ``CONFIG_LCD_QSPI_RESET_PIN`` and ``CONFIG_LCD_QSPI_ID`` need to be defined in the Kconfig.projbuild file to take effect.

 - 2.The main difference between the code of this project and the code of beken_genie project is the ``avi_play.c`` file. You can view the specific differences by comparing the file.

     - The LCD ppi and name passed in need to be modified to the lcd resolution and name used.
     - The two parameters lcd_hor_res and lcd_ver_res that passed in the lv_vendor_init() function during lvgl initialization need to be set to the width and height of the LCD.
     - The width and height in the img_dsc structure need to be set to the width and height of the resource file. The width and height of the resource file can be obtained after calling the function bk_avi_play_open().
     - The resource file name in the function bk_avi_play_open() needs to be modified to the actual resource file name used, and the third parameter needs to be passed as 0, indicating that no image segmentation operation is performed.

 - 3.In addition, since the resolution of lcd is larger, more memory is consumed. Pay attention to the SRAM memory allocation on CPU1. 


3. Q&A
---------------------------------

Q: Why doesn't the application layer report mic data?

A: It can support different types of QSPI/SPI LCD, but the resolution of the animation or video that need to display must be a multiple of 16 in width and a multiple of 8 in height.


Q: How to understand the third parameter of function bk_avi_play_open()?

A: The third parameter indicates whether the image needs to be split into consecutive address. For this project, the parameter needs to be set to 0.