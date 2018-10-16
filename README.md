TwoCan plug-in for OpenCPN
==========================

Inspired by Canboat and Openskipper, frustrated by those who claim "OpenCPN is not a hardware experimenter's platform", and desiring to give back something to the OpenCPN community.

TwoCan - An OpenCPN Plugin for integrating OpenCPN with NMEA2000® networks. It enables some NMEA2000® data to be directly integrated with OpenCPN by converting some NMEA2000® messages to NMEA 183 sentences and inserting them into the OpenCPN data stream.

It uses a "plug-in" driver model for supporting different CAN bus hardware adapters on Windows. Three hardware adapters are currently supported:

Kvaser Leaflight HS v2 - https://www.kvaser.com/product/kvaser-leaf-light-hs-v2/

Canable Cantact - http://canable.io/

Axiomtek AX92903 - http://www.axiomtek.com/Default.aspx?MenuId=Products&FunctionId=ProductView&ItemId=8270&upcat=318&C=AX92903

In addition, raw NMEA 2000 frames may be logged by the plugin and replayed using the Logfile Reader driver. (Note: DO NOT select both the LogFile Reader driver and the "Log Raw Frames" option.)

The TwoCan PlugIn Drivers are available separately at https://github.com/twocanplugin/twocanplugindrivers.git

Future versions may support Linux by using the Socket CAN interface which may enable a broader range of CAN Bus adapters to be used. Future versions may support additional PGN's.


Obtaining the source code
-------------------------

git clone https://github.com/twocanplugin/twocanplugin.git


Build Environment
-----------------

Only Windows is currently supported.

This plugin builds outside of the OpenCPN source tree

You must place opencpn.lib into the twocan_pi/build directory to be able to link the plugin DLL. opencpn.lib can be obtained from your local OpenCPN build, or alternatively downloaded from http://sourceforge.net/projects/opencpnplugins/files/opencpn_lib/


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

Problems
--------

Please send bug reports/questions to the opencpn forum or via email to twocanplugin@hotmail.com


License
-------
The plugin code is licensed under the terms of the GPL v3 or, at your convenience, later version.

NMEA2000® is a registered trademark of the National Marine Electronics Association.
