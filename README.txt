============================================================================

 DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines

 Copyright (C) 2018, RTZ Professional Audio, LLC

 All Rights Reserved

 RTZ is registered trademark of RTZ Professional Audio, LLC

 Visit us on the web at http://www.rtzaudio.com
 
============================================================================

This archive contains firmware for the RTZ Audio DTC-1200 digital transport
controller based on the Tiva TM4C123AE6PM M4 ARM processor. 

The file "DTC1200_TivaTM4C123AE6PM.bin" contains the binary image and is
programmed to the part using the LMFLASH utility and the DTC RS232 serial
port boot loader. Please read the DTC owners manual instructions using 
the LMFLASH utility for firmware updates over RS-232.

The file "DTC1200_TivaTM4C123AE6PM.out" contains the object format version
of the firmware image for use with the XDS JTAG programming pods. These
are generally used for development systems only.

Download and install the LMFLASH utility. Follow the DTC bootloader firmware
update instructions in the owners manual for instructions. You will need to
download the LMFLASH utility from the link on the RTZ page, or search the
web and download free from TI.

=== VERSION 2.23 (07/02/2018) ==============================================

Version 2.23 is the first official public release of the DTC firmware. This
includes all changes from recent beta site testing and updates.


