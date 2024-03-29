﻿TwoCan plug-in for OpenCPN
==========================

Inspired by Canboat and Openskipper, frustrated by those who claim "OpenCPN is not a hardware experimenter's platform", and desiring to give back something to the OpenCPN community.

TwoCan - An OpenCPN Plugin for integrating OpenCPN with NMEA2000® networks. It enables some NMEA2000® data to be directly integrated with OpenCPN by converting some NMEA2000® messages to NMEA 183 sentences and inserting them into the OpenCPN data stream. TwoCan supports Windows, Linux and Mac OSX.

Installation
------------
Versions 1.9 and newer are included in the Master Catalog of the Plugin Manager which is now featured with OpenCPN 5.2. This makes for a simple and easy installation experience for most users.

Windows
-------

For Windows it uses a "plug-in" driver model to support different CAN bus adapters and different log file formats. 
Four hardware adapters are currently supported:

| CAN Adapter | Manufacturer Web Site |
|-------------|-----------------------|
|Kvaser Leaflight HS v2|https://www.kvaser.com/product/kvaser-leaf-light-hs-v2/|
|Canable Cantact|http://canable.io/|
|Axiomtek AX92903|http://www.axiomtek.com/Default.aspx?MenuId=Products&FunctionId=ProductView&ItemId=8270&upcat=318&C=AX92903|
|Rusoku Toucan Marine|http://www.rusoku.com/products/toucan-marine|

Five log file formats are currently supported (with examples of their file format):

| Format | Format example|
|--------|---------------|
|TwoCan|0x01,0x01,0xF8,0x09,0x64,0xD9,0xDF,0x19,0xC7,0xB9,0x0A,0x04|
|Candump (Linux utility)|(1542794025.315691) can0 1DEFFF03#A00FE59856050404|
|Kees Verruijt's Canboat|2014-08-14T19:00:00.042,3,128267,1,255,8,0B,F5,0D,8D,24,01,00,00|
|Yacht Devices|19:07:47.607 R 0DF80503 00 2B 2D 9E 44 5A A0 A1|
|Packet Capture (Wireshark etc.)| binary format, refer to https://tools.ietf.org/id/draft-gharris-opsawg-pcap-00.html|

Linux
-----

For Linux it does not use a "plug-in" driver model, rather it uses three "baked in" classes that support the SocketCAN interface, PCAP packet capture format  and a generic Log File reader. Any CAN bus adapter that supports the SocketCAN interface "should" work. It has been tested with native interfaces (eg. can0), serial interfaces (eg. slcan0) and virtual interfaces (eg. vcan0). 

I have successfully used several hardware adapters under Linux:

Canable Cantact - (see above)

Rusoku Toucan Marine - (see above)

Waveshare CAN Hat for Raspberry PI - https://www.waveshare.com/rs485-can-hat.htm

Pican-M NMEA 0183 & NMEA 2000 Hat for Raspberry Pi - https://copperhilltech.com/pican-m-nmea-0183-nmea-2000-hat-for-raspberry-pi/

Another user is succesfully using the following adapter:

USBtin - https://www.fischl.de/usbtin/

On Linux, the Rusoku Toucan Marine device requires an installable kernel module. The source code and make files can be downloaded from github, a link is provided from http://www.rusoku.com. 

For example if using the Raspberry Pi with the Rusoku Toucan Marine device, instructions for building an installable kernel module can be found at https://raspberrypi.stackexchange.com/questions/39845/how-compile-a-loadable-kernel-module-without-recompiling-kernel. In addition I found that I also needed to install bison & flex (eg. sudo apt-get install bison, sudo apt-get flex) to compile the installable kernel module. 

For Linux, the generic Log File Reader automagically™ detects the four test text based log file formats described above whilst the PCAP Log File Reader will only parse packet capture log files containing SocketCAN data.

Linux - SocketCAN
-----------------

A good reference to using & configuring the CAN interfaces on Linux can be found at: https://elinux.org/Bringing_CAN_interface_up.

To summarise,
To bring up Native CAN Interface (eg. Waveshare, Rusoku Toucan Marine or Canable Cantact using the candlelight firmware)

sudo ip link set can0 type can bitrate 250000

sudo ip link set up can0

To bring up Serial CAN interface (eg. Canable Cantact using the slcan firmware)

sudo slcand -o -s5 -t hw -S 1000000 /dev/ttyACM0

sudo ip link set up slcan0

Mac OSX
-------

Similarly For Mac OSX it does not use a "plug-in" driver model, rather it also uses five "baked in" classes that support the generic Log File reader, PCAP packet capture format, a serial USB CAN interface such as the Canable Cantact adapter (see above) the Rusoku Toucan adapter (see above) and Kvaser Leaf Light adapter (also see above). For the serial USB interface while it was only developed with the Canable Cantact device in mind, it may work with any serial USB CAN bus adapter that supports the SLCAN command set such the USBTin adapter (see above) and the Lawicell adapters. The TwoCan plugin will attempt to automagically detect the correct serial USB port name as it includes the USB VendorId's and Product Id's for the three listed serial USB CAN adapters. For the Rusoku Toucan adapter it uses the MacCAN API interface, the driver for which is included and installed by the TwoCan plugin. 

Similar to Linux, the Mac OSX generic Log File Reader will also automagically™ detect the four supported log file formats.

Logging
-------

In addition, received NMEA 2000 frames may be logged by the plugin. Prior to Version 1.5 only the raw TwoCan log format was supported. With Version 1.5 onwards the following foru formats as described above are supported: TwoCan Raw, Canboat, Candump, Yacht Devices and Comma Separated Variable (CSV). All log files fils are written to the user's home directory and named using the following convention: twocan-YYYY-MM-DD_HHmmSS.log.

Active Mode
-----------

From version 1.4 onwards, the TwoCan plugin may be configured to actively participate on the NMEA 2000 network, claiming an address, sending heartbeats and responding to a few requests (such as product information). Please note that this is not fully tested and may interfere with the correct functioning of the NMEA 2000 network. If any problems occur, disable the Active Mode functionality and if possible, file a bug report.

If using the Active Mode feature, please be aware of the following:

For Windows, updated plugin drivers for the hardware adapters are required if you wish to use Active Mode. The only hardware adapters that have been updated and tested are the Kvaser Leaflight and Rusoku Toucan Marine. I have yet to update the plugin drivers for the Canable Cantact and Axiomtek AX92903 adapters to support Active Mode.

For Linux (or I should more correctly say for Raspbian), the WaveShare CAN hat did functions correctly, but required the additional following command:

sudo ifconfig can* txqueuelen 1000 (replace * with 0, 1 or whatever number your can interface is configured to use) 

For the Canable Cantact using the slcan firmware device it appears to drop frames (I may never have noticed this in previous TwoCan releases as the plugin never requested or received some of these NMEA 2000 fast message frames). The workaround is to reflash the Canable Cantact device with the candlelight firmware, which then enables the device to use the Native CAN interface rather than serial line CAN (slcan). It also requires the same command described above to increase the transmission queue length.

For Mac OSX, the supported CAN adapters "should" support Active Mode.

Bi Directional Gateway
----------------------

From version 2.0 onwards, TwoCan can operate as a bi-directional gateway, converting NMEA 183 sentences to their NMEA 2000 equivalents. The plugin receives NMEA 183 sentences from OpenCPN, converts them to NMEA 2000 and transmits them on the NMEA 2000 network. To determine what NMEA 183 sentences are converted, simply uncheck the corresponding PGN in the TwoCan options dialog. For example if you wish to convert depth data, enable the gateway feature and uncheck PGN 128267, which will then allow conversion of NMEA 183 DBT sentences to NMEA 2000 PGN 128267 messages. 

Supported Parameter Group Numbers (PGN's)
-----------------------------------------

List of supported NMEA 2000 Parameter Group Numbers (PGN)
---------------------------------------------------------

|PGN|Description|
|---|-----------|
|59392| ISO Acknowledgement|
|59904| ISO Request|
|60928| ISO Address Claim|
|65240| ISO Commanded Address|
|126992| NMEA System Time|
|126993| NMEA Heartbeat|
|126996| NMEA Product Information|
|127245| NMEA Rudder|
|127250| NMEA Vessel Heading|
|127251| NMEA Rate of Turn|
|127257| NMEA Attitude|
|127258| NMEA Magnetic Variation|
|127488| NMEA Engine Parameters, Rapid Update|
|127489| NMEA Engine Paramters, Dynamic|
|127505| NMEA Fluid Levels|
|127508| NMEA Battery Status|
|128259| NMEA Speed & Heading|
|128267| NMEA Depth|
|128275| NMEA Distance Log|
|129025| NMEA Position Rapid Update|
|129026| NMEA COG SOG Rapid Update|
|129029| NMEA GNSS Position|
|129033| NMEA Date & Time|
|129038| AIS Class A Position Report|
|129039| AIS Class B Position Report
|129040| AIS Class B Extended Position Report|
|129041| AIS Aids To Navigation (AToN) Report|
|129283| NMEA Cross Track Error|
|129284| NMEA Navigation Data|
|129285| NMEA Navigation Route/Waypoint Information|
|129540| NMEA GNSS Satellites in View|
|129793| AIS Date and Time Report|
|129794| AIS Class A Static Data|
|129798| AIS SAR Aircraft Position Report|
|129801| AIS Addressed Safety Related Message|
|129802| AIS Safety Related Broadcast Message|
|129808| NMEA DSC Message|
|129809| AIS Class B Static Data Report, Part A|
|129810| AIS Class B Static Data Report, Part B|
|130064| NMEA Route & Waypoint Service - Route list|
|130074| NMEA Route & Waypoint Service - Waypoint list|
|130306| NMEA Wind|
|130310| NMEA Water & Air Temperature and Pressure|
|130311| NMEA Environmental Parameters (supercedes 130310)|
|130312| NMEA Temperature|
|130316| NMEA Temperature Extended Range|
|130577| NMEA Direction Data|

Obtaining the source code
-------------------------

git clone https://github.com/twocanplugin/twocanplugin.git


Build Environment
-----------------

Windows, Linux and Mac OSX are currently supported.

This plugin builds outside of the OpenCPN source tree

Refer to the OpenCPN developer manual for details regarding other requirements such as git, cmake and wxWidgets.

Prior to version 1.9, for Windows you must place opencpn.lib into the twocan_pi/build directory to be able to link the plugin DLL. opencpn.lib can be obtained from your local OpenCPN build, or alternatively downloaded from http://sourceforge.net/projects/opencpnplugins/files/opencpn_lib/

For version 1.9 onwards, the plugin supports the Continous Improvement (CI) build services provided by Travis, Appveyor and Circle CI and stores the binaries and xml metadata files (used by the OpenCPN 5.2 Plugin Manager) on Cloudsmith. If you fork the source code to your Github repository and configure your own Travis, Appveyor, CircleCI and Cloudsmith accounts you can automate the build process. Again refer to the OpenCPN developer manual for further details.

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
 
  or (for OSX)

 make create-pkg

Installation
------------

For versions prior to 1.9:

Run the resulting setup package created above for your platform.

Eg. For Windows run twocan\_pi\_1.8.0-ov50.exe

Eg. For Ubuntu (on PC's) run sudo dpkg -i twocan\_pi\_1.8.0-1_amd64.deb

Eg. For Raspberry Pi run sudo dpkg -i twocan\_pi\_1.8.0-1_armhf.deb

Eg. For Mac OSX, double click on the resulting package

For versions 1.9 and later you may also use the OpenCPN 5.2 Plugin Manager by either using the "Import Plugin" feature or by adding the TwoCan plugin metadata to the OpenCPN ocpn-plugins.xml file to alter the master catalog.

Problems
--------
If building using gcc, note that are many -Wwrite-strings, -Wunused-but-set-variable and -Wunused-variable warnings, that I'll get around to fixing one day, but at present can be safely ignored.

In some rare cases, changing the TwoCan preferences midstream causes OpenCPN to crash. If the settings can't be updated from the prefernces dialog, the workaround is to manually edit the OpenCPN configuration file. Under the section [PlugIns/TwoCan], set the value as appropriate. To reset the plugin with no adapterm set Adapter=None.

In the Linux version, for some bizarre reason known only unto the wxWidget's gods, the network dataview doesn't size correctly.

Please send bug reports/questions/comments to the opencpn forum or via email to twocanplugin@hotmail.com


License
-------
The plugin code is licensed under the terms of the GPL v3 or, at your convenience, a later version.

NMEA2000® is a registered trademark of the National Marine Electronics Association.
