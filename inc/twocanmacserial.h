// Copyright(C) 2018 by Steven Adler
//
// This file is part of TwoCan, a plugin for OpenCPN.
//
// TwoCan is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TwoCan is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with TwoCan. If not, see <https://www.gnu.org/licenses/>.
//
// NMEA2000® is a registered Trademark of the National Marine Electronics Association

#ifndef TWOCAN_MACSERIAL_H
#define TWOCAN_MACSERIAL_H

#include "twocaninterface.h"

// OSX stuff 
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>

// Known Vendor & Product Id for SLCan USB devices
#define LAWICELL_VENDOR_ID 0x0403
#define LAWICELL_PRODUCT_ID 0x6001

#define USBTIN_VENDOR_ID 0x04d8
#define USBTIN_PRODUCT_ID 0x000a

#define CANABLE_VENDOR_ID 0xAD50
#define CANABLE_PRODUCT_ID 0x60C4

// Linux USB Serial port
#include <termios.h>
#include <unistd.h>

// Cantact constants for opening & closing the bus, setting the bus speed, line endings
#define CANTACT_OPEN 'O'
#define CANTACT_250K 'S5'
#define CANTACT_CLOSE 'C'
#define CANTACT_LINE_TERMINATOR '\r'

// Cantact character used to indicate a CAN 2.0 extended frame
#define CANTACT_EXTENDED_FRAME 'T'
// Just for completeness
#define CANTACT_STANDARD_FRAME 't'
#define CANTACT_REMOTE_FRAME 'r'


// Implements a Canable Cantact CAN bus adapter serial USB modem interface on Mac OSX
class TwoCanMacSerial : public TwoCanInterface {

public:
	// Constructor and destructor
	TwoCanMacSerial(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanMacSerial(void);

	// Open, Close, Read and Write to the USB Modem interface
	int Open(const wxString& portName);
	int Close(void);
	void Read();
	int Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload);
	
	// Derive a 29 bit Unique Number form the computer's MAC Address
	int GetUniqueNumber(unsigned long *uniqueNumber);
	
protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	// USB Modem file handle
	int serialPortHandle;

	static kern_return_t FindEthernetInterfaces(io_iterator_t *matchingServices);
	static kern_return_t GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress, UInt8 bufferSize);
	
	// Find the Canable Cantact device and determine its tty port name
	bool FindTTYDevice(wxString& ttyDevice, const int vid, const int pid);
};

#endif
