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

#ifndef TWOCAN_MAC_TOUCAN_H
#define TWOCAN_MAC_TOUCAN_H

#include "twocaninterface.h"

// Mac-Can interfaces
// Changed from <TouCAN.h> as usr/local/inc no longer exists??
#include "TouCAN.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>


// Implements a MacCan Interface for the Rusoku CAN bus adapter on Mac OSX
class TwoCanMacToucan : public TwoCanInterface, public CTouCAN {

public:
	// Constructor and destructor
	TwoCanMacToucan(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanMacToucan(void);

	// Open, Close, Read and Write to the USB Modem interface
	int Open(const wxString& portName);
	int Close(void);
	void Read();
	int Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload);
	
	// Derive a 29 bit Unique Number from the computer's MAC Address
	int GetUniqueNumber(unsigned long *uniqueNumber);

	// Mac-Can
	CTouCAN toucanInterface;
	
protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	static kern_return_t FindEthernetInterfaces(io_iterator_t *matchingServices);
	static kern_return_t GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress, UInt8 bufferSize);
	

};

#endif
