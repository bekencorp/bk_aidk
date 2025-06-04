Notes on using Nand disk
============================================

:link_to_translation:`zh_CN:[中文]`

Notes on Nand disk connection
------------------------------

When the USB cable is first connected to the development board through the Type-C cable, due to the default USB DP/DN data cable detection device of the Type-C cable, the USB driver of the board is not initialized, causing the USB Host (computer) to be unable to directly communicate with the board. You can refresh the USB device again in the PC device manager to recognize the USB drive function of the board.


Notes on Nand disk format
---------------------------

On the Windows platform, when formatting the NAND disk for the first time, it may not be possible to format it directly due to the lack of a file system. Only the drive letter appears in the file browser and cannot be formatted, as shown below.

.. figure:: ../../_static/disk_fomat_computer_en.png
    :align: center
    :alt: nand disk not formatted.
    :figclass: align-center

    nand disk not formatted

When this situation occurs, you need to open the disk management window and locate the Nand disk, as shown in the following figure.

.. figure:: ../../_static/disk_fomat_disk_manager_en.png
    :align: center
    :alt: disk management windows.
    :figclass: align-center

    disk management windows

Format the disk by creating a new volume, select FAT32 file system format, and set the drive letter and allocation unit size.

.. figure:: ../../_static/disk_fomat_new_disk_en.png
    :align: center
    :alt: add disk for nand disk .
    :figclass: align-center

    add disk for nand disk

.. figure:: ../../_static/disk_fomat_type_en.png
    :align: center
    :alt: set file system format for nand disk.
    :figclass: align-center

    set file system format for nand disk

Afterwards, the nand disk can be used normally in the file manager, as shown below.

.. figure:: ../../_static/disk_fomat_normal_en.png
    :align: center
    :alt: normal nand disk.
    :figclass: align-center

    normal nand disk
