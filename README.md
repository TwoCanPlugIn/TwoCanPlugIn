TwoCan plug-in for OpenCPN
==========================

Inspired by Canboat and Openskipper, frustrated by those who claim "OpenCPN is not a hardware experimenter's platform", and desiring to give back something to the OpenCPN community.

TwoCan - An OpenCPN Plugin for integrating OpenCPN with NMEA2000® networks. It enables some NMEA2000® data to be directly integrated with OpenCPN by converting some NMEA2000® messages to NMEA 183 sentences and inserting them into the OpenCPN data stream.

For Windows it uses a "plug-in" driver model to support different CAN bus adapters and different log file formats. 
Three hardware adapters are currently supported:

Kvaser Leaflight HS v2 - https://www.kvaser.com/product/kvaser-leaf-light-hs-v2/

Canable Cantact - http://canable.io/

Axiomtek AX92903 - http://www.axiomtek.com/Default.aspx?MenuId=Products&FunctionId=ProductView&ItemId=8270&upcat=318&C=AX92903

Four log file formats are currently supported (with examples of their file format):
TwoCan: 0x01,0x01,0xF8,0x09,0x64,0xD9,0xDF,0x19,0xC7,0xB9,0x0A,0x04
Candump (Linux utilty): (1542794025.315691) can0 1DEFFF03#A00FE59856050404  
Kees Verruijt's Canboat: 2014-08-14T19:00:00.042,3,128267,1,255,8,0B,F5,0D,8D,24,01,00,00
Yacht Devices: 19:07:47.607 R 0DF80503 00 2B 2D 9E 44 5A A0 A1

For Linux it does not use a "plug-in" driver model, rather it uses two "baked in" classes that support the SocketCAN interface and the log file reader. Any CAN bus adapter that supports the SocketCAN interface "should" work. It has been tested with both native interfaces (eg. can0), serial interfaces (eg. slcan0) and virtual interfaces (eg. vcan0). 

Two hardware adapters have been successfully tested under Linux:
Canable Cantact - (see above) 
Waveshare CAN Hat for Raspberry PI - https://www.waveshare.com/product/modules/communication/rs232-rs485-can/rs485-can-hat.htm

A good reference to using & configuring the CAN interfaces on Linux can be found at: https://elinux.org/Bringing_CAN_interface_up

The Linux Log File Reader automagically™ detects the four supported log file formats.

In addition, NMEA 2000 frames may be logged by the plugin. By default they are written using the TwoCan format and are written to the user's home directory and named using the following convention: twocan-YYYY-MM-DD_HHmmSS.log

Obtaining the source code
-------------------------

git clone https://github.com/twocanplugin/twocanplugin.git


Build Environment
-----------------

Both Windows and Linux are currently supported.

This plugin builds outside of the OpenCPN source tree

For both Windows and Linux, refer to the OpenCPN developer manual for details regarding other requirements such as git, cmake and wxWidgets.

For Windows you must place opencpn.lib into the twocan_pi/build directory to be able to link the plugin DLL. opencpn.lib can be obtained from your local OpenCPN build, or alternatively downloaded from http://sourceforge.net/projects/opencpnplugins/files/opencpn_lib/

Build Commands
--------------
 mkdir twocan_pi/build

 cd twocan_pi/build

 cmake ..

 cmake --build . --config debug

  or

 cmake --build . --config release

Creating an installation package
--------------------------------
 cmake --build . --config release --target package

  or

 cpack

Note: I haven't tested creating Linux installation packages, however make install or simply copying the resulting libtwocan_pi.so shared library to the OpenCPN shared library directory (eg. /usr/lib/opencpn) works.

Problems
--------
Warning: In some cases, changing the TwoCan preferences causes OpenCPN to crash. The workaround is to manually edit the OpenCPN configuration file. Under the section [PlugIns/TwoCan], set the value Adapter=None. Clearly I've left something dangling and the plugin barfs when changing settings mid-stream.

Please send bug reports/questions to the opencpn forum or via email to twocanplugin@hotmail.com


License
-------
The plugin code is licensed under the terms of the GPL v3 or, at your convenience, a later version.

NMEA2000® is a registered trademark of the National Marine Electronics Association.
