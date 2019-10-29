============================================================================

 DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines

 Copyright (C) 2016-2019, RTZ Professional Audio, LLC

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
are generally used for development systems or recovery only.

Download and install the LMFLASH utility. Follow the DTC bootloader firmware
update instructions in the owners manual for instructions. You will need to
download the LMFLASH utility from the link on the RTZ page, or search the
web and download free from TI.

=== VERSION 2.35(10/27/2019) ==============================================

Added minor fixes and changes needed for better support of the new 
DRCWIN software remote application for the STC-1200. Some IPC messages 
and flags to go between the DTC and STC required some changes and fixes.
The STC-1200 firmware v1.04 requires this v2.35 build of the DTC to
work properly.
 
=== VERSION 2.34(07/06/2019) ==============================================

Added new default config parameter values for 1" tape configuration. 

Added an ADC mid-scale configuration parameter under the tensions menu, 
which allows shifting the ADC mid-scale offset, for zero reference of
tape tension arm while under correction tension. This can be used to shift
the tension arm readings to compensate for less tension with 1" tape. The
tension sensor ADC is 12-bit, so a value of 2047 is treated as midscale.
The tape tension sensor should read around zero when the tape is under
proper tension on the transport with equal tape pack on the reels.

Added new tape threading mode. When the tape out arm is open, the user
can press STOP on the transport to release the brakes and the controller
applies a small amount of torque to the reels to allow easier threading
of tape on the transport. Thread mode exits by pressing the STOP again
or the tape out arm is released back to tape.

=== VERSION 2.33(05/03/2019) ==============================================

Changed Tensions->Servo->reel offset gain (item #15) to allow for greater
input range. Some users need more range to adjust for creep at the very 
ends of the tape reels. 

=== VERSION 2.32 (03/09/2019) ==============================================

Lots of chanages,fixes and improvements to servo loop shuttle modes. Also 
fixed auto-slow features for shuttling so the controller slows the tape
automatically near the ends of the reels in shuttle modes. The auto-slow
config parameters specifies the velocity to slow to. The slow-at velocity
contig parameters specifies the velocity the emptying reel must reach
before auto-slow can trigger. Likewise the slow-at offset parameter
specifies the offset the reels must be at to trigger auto-slow. Normally
both of these conditions must be met to trigger auto-slow during
high speed shuttle. However, either slow-at parameter can be set to 
zero to disable the test condition. For instance, slow-at offset could
be set to zero and the auto-slow would trigger any time the reel reached
the minimum auto-slow trigger velocity config parameter.

=== VERSION 2.31 (02/11/2019) ==============================================

Modified to use new AS5047P high resolution reel motor encoders. Note
this version only work with new 1024 CPR resolution encoders. Support for
the older 500 CPR encoders has been removed.

=== VERSION 2.30 (11/05/2018) ==============================================

This version includes additions for the IPC layer to support the STC 
timer/cue card. Additionally, the QEI interface has been changed
to support the AS5047P encoders for the reel motors with higher resolution.
The older AS5047D only provides 500 CPR resolution, but the newer AS5047P
provides 1024 CPR. DIP SWITCH #3 MUST BE ON TO USE THE NEW HIGHER 
RESOLUTION ENCODERS. Future versions will have support for the older
500 CPR encoders removed.

=== VERSION 2.29 (10/05/2018) ==============================================

This version fixes a bug when jogging between FWD/REW for braking purposes.
A problem appeared when the PID output was negative (overshoot) which
cause the servo loop to accelerate if the opposite direction was requested
while the servo loop PID was in the overshoot state. Now we force this  
to the opposite (undershoot) state based on the previous shuttle mode which
gives the effect of dynamic braking (rather than accelerate).

=== VERSION 2.28 (09/30/2018) ==============================================

This version contains mostly adjustments to the shuttle logic to improve
the back tension at higher velocities. New default PID parameters have 
been assigned and it's recommended to use these values as is.

=== VERSION 2.27 (09/28/2018) ==============================================

Fixed bugs/issues with jog logic when toggling between shuttle, play and 
stop during rewind operations. Add IPC logic and communications tasks
so the DTC and STC locator can communicate and send commands between each
other efficiently from multiple tasks. Changed DIPSW #1 function to select 
TTY console between 19,200 or 115,200 baud. Enable SW1 to use high speed 
115200 baud. DIPSW #4 overrides loading of EPROM config data if enabled.

=== VERSION 2.26 (09/20/2018) ==============================================

Implemented back tension gain for shuttle mode. This helps to keep constant
tension as the reels gain velocity and motor torque required to maintain 
velocity decrease. A new back tension gain parameter was added to the
shuttle mode configuration parameters.

=== VERSION 2.25 (09/04/2018) ==============================================

Improved play boost performance by changing play boost to use PID 
algorithm instead of counters. New P/I parameters were added to the
PLAY confuration menu parameters.

=== VERSION 2.24 (08/14/2018) ==============================================

Implemented back tension gain for shuttle mode. This helps to keep constant
tension as the reels gain velocity and motor torque required to maintain 
velocity decrease. A new back tension gain parameter was added to the
shuttle mode configuration parameters.

=== VERSION 2.23 (07/02/2018) ==============================================

Version 2.23 is the first official public release of the DTC firmware. This
includes all changes from recent beta site testing and updates.