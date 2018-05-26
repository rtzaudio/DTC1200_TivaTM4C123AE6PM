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

Download and install the LMFLASH utility. Follow the DTC bootloader firmware
update instructions in the owners manual for instructions. You will need to
download the LMFLASH utility from the link on the RTZ page, or search the
web and download free from TI. Note the tension parameters have changed
in scaling. You'll notice these values look higher than the previous
release.

=== VERSION 2.14 ===========================================================

Code for the tension sensor works differently now and can be monitored
on the DTC diagnostics screen. The tension sensor trimmer should be 
adjusted to read roughly around zero with the sensor arm at the cal 
mark on the deck (around 9oz force on the arm). Note the tension sensor 
gain parameter in the configuration can be set to zero to ignore the 
tension sensor in the servo loop calculations. We will set this to zero 
while calibrating the base tape tension levels with the Tentelometer. 
This way we can remove the effect of the tension sensor to set our 
base tension levels.

Make sure the 15V/27V rails are on spec before adjusting things. Use 
the DTC diagnostic function to align the MDA offset as described in 
the RTZ MDA Owners Manual. Adjusted the MDA trimmers till you can 
just see some signal starting to develop around DAC level of 75. 
Then, the reels should be begin to start motion around DAC 100 level.

Go the DTC TENSIONS menu item "16) Tension Sensor Gain" and note 
the value. This should be 0.13 by default and controls how much of 
the tension arm gets added. Set this gain to ZERO and spool the 
machine to mid position on the tape. Observe the tension sensor 
level in the diag monitor menu and adjust gain to center around 
zero as possible. The value is always jumping around due to sampling 
and changes from electronics/sensor. Go back and check tape tension 
values again and adjust again as necessary. Things are sort of a 
balancing act at this point, so try to balance out as best you can. 
You want about 6.5oz force on each reel and the tension sensor 
bouncing around near zero. 