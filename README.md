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

A good reference to using & configuring the CAN interfaces on Linux can be found at: https://elinux.org/Bringing_CAN_interface_up.

To summarise,
To bring up Native CAN Interface (eg. Waveshare or Canable Cantact using the candlelight firmware)

sudo ip link set can0 type can bitrate 250000

sudo ip link set up can0

To bring up Serial CAN interface (eg. Canable Cantact using the slcan firmware)

sudo slcand -o -s5 -t hw -S 1000000 /dev/ttyACM0

sudo ip link set up slcan0

The Linux Log File Reader automagically™ detects the four supported log file formats.

In addition, NMEA 2000 frames may be logged by the plugin. By default they are written using the TwoCan format and are written to the user's home directory and named using the following convention: twocan-YYYY-MM-DD_HHmmSS.log.

The current release, version 1.4 allows TwoCan to be configured to actively participate on the NMEA 2000 network, claiming an address, sending heartbeats and responding to a few requests (such as product information). Please note that this is not fully tested and may interfere with the correct functioning of the NMEA 2000 network. If any problems occur, disable the active device functionality and if possible, file a bug report.

If using the active device feature in version 1.4 please be aware of the following:

For Linux (or I should more correctly say for Raspbian), the WaveShare CAN hat functions correctly, but requires the additional following command:

sudo ifconfig can* txqueuelen 1000 (replace * with 0, 1 or whatever number your can interface is configured to use) 

For the Canable Cantact device it appears to drop frames (I may never have noticed this in previous TwoCan releases as the plugin never requested or received some of these NMEA 2000 fast message frames). The workaround is to reflash the Canable Cantact device with the candlelight firmware, which then enables the device to use the Native CAN interface rather than serial line CAN (slcan). It also requires the same command described above to increase the transmission queue length.

For Windows, updated plugin drivers for the hardware adapters are required. The only hardware adapter that has been updated and tested is the Kvaser Leaflight. The plugin drivers for the other two hardware adapters have not yet been updated to support active mode.

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
If building using gcc, note that are many -Wwrite-strings, -Wunused-but-set-variable and -Wunused-variable warnings, that I'll get around to fixing one day, but at present can be safely ignored.

In some rare cases, changing the TwoCan preferences midstream causes OpenCPN to crash. If the settings can't be updated from the prefernces dialog, the workaround is to manually edit the OpenCPN configuration file. Under the section [PlugIns/TwoCan], set the value as appropriate. To reset the plugin with no adapterm set Adapter=None. Clearly I've left something dangling and the plugin barfs when changing settings mid-stream.

In the Linux version, for some bizarre reason known only unto the wxWidget's gods, the network dataview doesn't size correctly.

Please send bug reports/questions/comments to the opencpn forum or via email to twocanplugin@hotmail.com


License
-------
The plugin code is licensed under the terms of the GPL v3 or, at your convenience, a later version.

NMEA2000® is a registered trademark of the National Marine Electronics Association.
