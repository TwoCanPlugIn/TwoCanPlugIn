// Copyright(C) 2020 by Steven Adler
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

// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCan Kvaser - Implements Mac OSX Mac-Can API for Kvase Leaflight using MacCan driver
// Owner: twocanplugin@hotmail.com
// Date: 10/04/2021
// Version History:
// 2.0 - Initial Release of Kvaser Mac OSX interface
// 2.1 - 04-07-2021 Updated for KvaserCAN 0.1.1 - Change read timeout value,  No Initializer or Finalizer API's, change to EChannelState
//
// Refer to https://github.com/mac-can/kvaserCAN

#include <twocanmackvaser.h>

TwoCanMacKvaser::TwoCanMacKvaser(wxMessageQueue<std::vector<byte>> *messageQueue) : TwoCanInterface(messageQueue) {
	// The Mac-Can Kvaser driver interface
	kvaserInterface = CKvaserCAN();
}

TwoCanMacKvaser::~TwoCanMacKvaser() {
	// Do I need to delete anything ??
}

// Open the Kvaser device
int TwoCanMacKvaser::Open(const wxString& portName) {
	MacCAN_Return_t returnCode;

	// Initialize the Kvaser interface
	//returnCode = CMacCAN::Initializer();
	// BUG BUG What is the difference ?? returnCode = kvaserInterface.Initializer();
	//if ((returnCode == CMacCAN::NoError) || (returnCode == CMacCAN::AlreadyInitialized)) {
	//	wxLogMessage(_T("TwoCan Mac Kvaser, Successfully Initialized Kvaser Driver"));
	//}
	//else {
	//	wxLogMessage(_T("TwoCan Mac Kvaser, Error Initializing Kvaser Driver: %d"), returnCode);
	//	return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_DRIVER_NOT_FOUND);
	//}

	// Assumes we use Channel 0
	int32_t channel = 0;

	MacCAN_OpMode_t opMode = {};
	opMode.byte = CANMODE_DEFAULT;

	CKvaserCAN::EChannelState state;

    // Probe the interface, perhaps unnecessary, we assumes channel 0.
    returnCode = kvaserInterface.ProbeChannel(channel, opMode, state);
    
	if (returnCode == CMacCAN::NoError) {
		if (state == CKvaserCAN::ChannelAvailable)  {
			wxLogMessage(_T("TwoCan Mac Kvaser, Channel %d is available"), channel);
		}
		else {
			wxLogMessage(_T("TwoCan Mac Kvaser, Channel %d, CAN Board Error: %d"), channel, state);
			// Not sure whether this is a fatal error
			return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_ADAPTER_NOT_FOUND);
		}
	}
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error probing channel %d: %d"), channel, returnCode);
    }
	
	// Initialize the channel
	returnCode = kvaserInterface.InitializeChannel(channel, opMode);
	if (returnCode == CMacCAN::NoError) {
		wxLogMessage(_T("TwoCan Mac Kvaser, Successfully Initialized Channel %d"), channel);
	}
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error Initializing Channel %d: %d"), channel, returnCode);
		// Not sure whether this is a fatal error
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// Set the CAN Bus speed to 250 k and start the CAN Bus adapter
	MacCAN_Bitrate_t bitrate;
	bitrate.index = CANBTR_INDEX_250K;

	returnCode = kvaserInterface.StartController(bitrate);
	if (returnCode == CMacCAN::NoError) {
		wxLogMessage(_T("TwoCan Mac Kvaser, Successfully Started Controller"));
	}
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error Starting Controller: %d"), returnCode);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_SET_BUS_SPEED);
	}

	// Retrieve/Confirm the bus speed. Just for informational purposes
	MacCAN_BusSpeed_t speed;
    returnCode = kvaserInterface.GetBusSpeed(speed);
    if (returnCode == CMacCAN::NoError) {
		wxLogMessage(_T("TwoCan Mac Kvaser, CAN Bus Speed: %.2f"), speed.data.speed);
		// Should really confirm the bus speed is 250K (NMEA 2000)
	}
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error Retrieving CAN Bus Speed: %d"), returnCode);
	}

	// Retrieve the channel status
	MacCAN_Status_t status;
	status = {};
	returnCode = kvaserInterface.GetStatus(status);
	if (returnCode == CMacCAN::NoError) {
		// Should really confirm that status == 0x00 Controller Started
		wxLogMessage(_T("TwoCan Mac Kvaser, CAN Bus status: %d"), status.byte);
	}
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error retrieving CAN Bus status: %d"), returnCode);
	}

	// Some additional debugging info
	wxLogMessage(_T("TwoCan Mac Kvaser, Hardware Version: %s"), kvaserInterface.GetHardwareVersion());
	wxLogMessage(_T("TwoCan Mac Kvaser, Firmware Version: %s"), kvaserInterface.GetFirmwareVersion());
	wxLogMessage(_T("TwoCan Mac Kvaser, CANAPI Version: %s"), CMacCAN::GetVersion());
    wxLogMessage(_T("TwoCan Mac Kvaser, Kvaser Version: %s"), CKvaserCAN::GetVersion());


	return TWOCAN_RESULT_SUCCESS;
}

int TwoCanMacKvaser::Close(void) {
	// Close the CAN Bus and the adapter
	MacCAN_Return_t returnCode;
	MacCAN_Status_t status;

	returnCode = kvaserInterface.TeardownChannel();
	if (returnCode == CMacCAN::NoError) {
		wxLogMessage(_T("TwoCan Mac Kvaser, Successfully closed CAN Bus"));
	} 
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error Closing CAN Bus: %d"), returnCode);
	}

	returnCode = kvaserInterface.GetStatus(status);
	if (returnCode == CMacCAN::NoError) {
		// Expect to see that status == 0x80 Controller Stopped or 0x40 Bus Off
		wxLogMessage(_T("TwoCan Mac Kvaser, CAN Bus status: %d"), status.byte);
	}
	else {
		wxLogMessage(_T("TwoCan Mac Kvaser, Error retrieving CAN Bus status %d"), returnCode);
	}

	//returnCode = kvaserInterface.Finalizer();
	// BUG BUG What is the difference ?? returnCode = kvaserInterface.Finalizer();
	//if (returnCode == CMacCAN::NoError) {
	//	wxLogMessage(_T("TwoCan Mac Kvaser, Successfully closed Kvaser driver"));
	//}
	//else {
	//	wxLogMessage(_T("TwoCan Mac Kvaser, Error closing Kvaser driver: %d"), returnCode);
	//}
		
	return TWOCAN_RESULT_SUCCESS;
}

void TwoCanMacKvaser::Read() {
	
	std::vector<byte> postedFrame(CONST_FRAME_LENGTH);
	MacCAN_Message_t message;

	while (!TestDestroy()) {
			
		if (kvaserInterface.ReadMessage(message, CONST_TEN_MILLIS) == CMacCAN::NoError) {
			//wxLogMessage(_T("%0x,%c,%i"), message.id, message.xtd ? 'X' : 'S', message.dlc);

			// Copy the CAN Header										
			postedFrame[0] = message.id & 0xFF;
            postedFrame[1] = (message.id >> 8) & 0xFF;
            postedFrame[2] = (message.id >> 16) & 0xFF;
            postedFrame[3] = (message.id >> 24) & 0xFF;
					
			// And the CAN Data
			for (unsigned int i = 0; i < message.dlc; i++) {
				postedFrame[CONST_HEADER_LENGTH + i] = message.data[i];
			}
					
			// Post frame to the TwoCanDevice
			deviceQueue->Post(postedFrame);

		} // if ReadMessage

	} // while thread is alive

}

// Write, Transmit a CAN frame onto the NMEA 2000 network
	int TwoCanMacKvaser::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
		
		MacCAN_Return_t returnCode;
		MacCAN_Message_t message;

		message.id = canId; // BUG BUG Bytes may need to be reversed
		message.xtd = 1; // CAN 2.0 29bit Extended Frame
		message.rtr = 0; // Not a remote frame
		message.dlc = payloadLength;
		
		for (unsigned int i = 0; i < payloadLength; i++) {
			message.data[i] = *payload;
			payload++;			
		}
		
		message.timestamp.tv_sec = 0;
		message.timestamp.tv_nsec = 0;

		returnCode = kvaserInterface.WriteMessage(message);
		
		if (returnCode == CMacCAN::NoError) {
			return TWOCAN_RESULT_SUCCESS;
		}
		else {
			wxLogMessage(_T("TwoCan Mac Kvaser, Transmit error %d"), returnCode);
			return SET_ERROR(TWOCAN_RESULT_WARNING, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_TRANSMIT_FAILURE);
		}
}

// Entry, the method that is executed upon thread start
	wxThread::ExitCode TwoCanMacKvaser::Entry() {
	// Merely loops continuously waiting for frames to be received by the CAN Adapter
	Read();
	wxLogMessage(_T("TwoCan Mac Kvaser, CAN Bus read thread exiting"));
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
	void TwoCanMacKvaser::OnExit() {
	// Nothing to do ??
}

// Generate a unique number derived from the MAC Address to be used as a NMEA 2000 device's unique address
// Adapted from Apple sample GetPrimaryMacAdress
	int TwoCanMacKvaser::GetUniqueNumber(unsigned long *uniqueNumber) {
	*uniqueNumber = 0;

	kern_return_t kernResult = KERN_SUCCESS;
    io_iterator_t intfIterator;
    UInt8 MACAddress[kIOEthernetAddressSize];
 
    kernResult = FindEthernetInterfaces(&intfIterator);
    
    if (KERN_SUCCESS != kernResult) {
        wxLogMessage(_T("TwoCan Mac Kvaser, Error finding Ethernet Interfaces: 0x%08x"), kernResult);
    }
    else {
        kernResult = GetMACAddress(intfIterator, MACAddress, sizeof(MACAddress));
        
        if (KERN_SUCCESS != kernResult) {
            wxLogMessage(_T("TwoCan Mac Kvaser, Error Getting MAC Address: 0x%08x"), kernResult);
        }
		else {
			wxLogMessage(_T("TwoCan Mac Kvaser,  Generating Unique Id from Ethernet Address: %02x:%02x:%02x:%02x:%02x:%02x."),
					MACAddress[0], MACAddress[1], MACAddress[2], MACAddress[3], MACAddress[4], MACAddress[5]);
			
			//char temp[9];
			unsigned int pair1, pair2;
			pair1 = (MACAddress[0] << 16) | (MACAddress[1] << 8) | MACAddress[2];
			pair2 = (MACAddress[3] << 16) | (MACAddress[4] << 8) | MACAddress[5];
			//sprintf(temp,"%d%d%d",MACAddress[0], MACAddress[1], MACAddress[2]);
			//pair1 = atoi(temp); 
			//sprintf(temp,"%d%d%d",MACAddress[3], MACAddress[4], MACAddress[5]);
			//pair2 = atoi(temp); 
			*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;;		
		}
    }
    
    (void) IOObjectRelease(intfIterator);	// Release the iterator.


	if (*uniqueNumber == 0)	{
		srand(CONST_PRODUCT_CODE);
		unsigned int pair1, pair2;
		pair1 = rand();
		pair2 = rand();
		*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;
	}
	// Unique Number is a maximum of 29 bits in length;
	*uniqueNumber &= 0x1FFFFF;
	return TWOCAN_RESULT_SUCCESS;
}

	kern_return_t TwoCanMacKvaser::FindEthernetInterfaces(io_iterator_t *matchingServices) {
    kern_return_t           kernResult; 
    CFMutableDictionaryRef	matchingDict;
    CFMutableDictionaryRef	propertyMatchDict;
    
    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass. 
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and 
    // the specified value.
    matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);

    // Note that another option here would be:
    // matchingDict = IOBSDMatching("en0");
    // but en0: isn't necessarily the primary interface, especially on systems with multiple Ethernet ports.
        
    if (NULL == matchingDict) {
        wxMessageOutputDebug().Printf(_T("IOServiceMatching returned a NULL dictionary.\n"));
    }
    else {
        // Each IONetworkInterface object has a Boolean property with the key kIOPrimaryInterface. Only the
        // primary (built-in) interface has this property set to TRUE.
        
        // IOServiceGetMatchingServices uses the default matching criteria defined by IOService. This considers
        // only the following properties plus any family-specific matching in this order of precedence 
        // (see IOService::passiveMatch):
        //
        // kIOProviderClassKey (IOServiceMatching)
        // kIONameMatchKey (IOServiceNameMatching)
        // kIOPropertyMatchKey
        // kIOPathMatchKey
        // kIOMatchedServiceCountKey
        // family-specific matching
        // kIOBSDNameKey (IOBSDNameMatching)
        // kIOLocationMatchKey
        
        // The IONetworkingFamily does not define any family-specific matching. This means that in            
        // order to have IOServiceGetMatchingServices consider the kIOPrimaryInterface property, we must
        // add that property to a separate dictionary and then add that to our matching dictionary
        // specifying kIOPropertyMatchKey.
            
        propertyMatchDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
													  &kCFTypeDictionaryKeyCallBacks,
													  &kCFTypeDictionaryValueCallBacks);
    
        if (NULL == propertyMatchDict) {
            wxMessageOutputDebug().Printf(_T("CFDictionaryCreateMutable returned a NULL dictionary.\n"));
        }
        else {
            // Set the value in the dictionary of the property with the given key, or add the key 
            // to the dictionary if it doesn't exist. This call retains the value object passed in.
            CFDictionarySetValue(propertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue); 
            
            // Now add the dictionary containing the matching value for kIOPrimaryInterface to our main
            // matching dictionary. This call will retain propertyMatchDict, so we can release our reference 
            // on propertyMatchDict after adding it to matchingDict.
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
            CFRelease(propertyMatchDict);
        }
    }
    
    // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
    // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
    // the dictionary explicitly.
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, matchingServices);    
    if (KERN_SUCCESS != kernResult) {
        wxMessageOutputDebug().Printf(_T("IOServiceGetMatchingServices returned 0x%08x\n"), kernResult);
    }
        
    return kernResult;
}

// Given an iterator across a set of Ethernet interfaces, return the MAC address of the last one.
// If no interfaces are found the MAC address is set to an empty string.
// In this sample the iterator should contain just the primary interface.
	kern_return_t TwoCanMacKvaser::GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress, UInt8 bufferSize) {
    io_object_t		intfService;
    io_object_t		controllerService;
    kern_return_t	kernResult = KERN_FAILURE;
    
    // Make sure the caller provided enough buffer space. Protect against buffer overflow problems.
	if (bufferSize < kIOEthernetAddressSize) {
		return kernResult;
	}
	
	// Initialize the returned address
    bzero(MACAddress, bufferSize);
    
    // IOIteratorNext retains the returned object, so release it when we're done with it.
    while ((intfService = IOIteratorNext(intfIterator)))
    {
        CFTypeRef	MACAddressAsCFData;        

        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // since they are hardware nubs and do not participate in driver matching. In other words,
        // registerService() is never called on them. So we've found the IONetworkInterface and will 
        // get its parent controller by asking for it specifically.
        
        // IORegistryEntryGetParentEntry retains the returned object, so release it when we're done with it.
        kernResult = IORegistryEntryGetParentEntry(intfService,
												   kIOServicePlane,
												   &controllerService);
		
        if (KERN_SUCCESS != kernResult) {
            wxMessageOutputDebug().Printf(_T("IORegistryEntryGetParentEntry returned 0x%08x\n"), kernResult);
        }
        else {
            // Retrieve the MAC address property from the I/O Registry in the form of a CFData
            MACAddressAsCFData = IORegistryEntryCreateCFProperty(controllerService,
																 CFSTR(kIOMACAddress),
																 kCFAllocatorDefault,
																 0);
            if (MACAddressAsCFData) {
                CFShow(MACAddressAsCFData); // for display purposes only; output goes to stderr
                
                // Get the raw bytes of the MAC address from the CFData
                CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);
                CFRelease(MACAddressAsCFData);
            }
                
            // Done with the parent Ethernet controller object so we release it.
            (void) IOObjectRelease(controllerService);
        }
        
        // Done with the Ethernet interface object so we release it.
        (void) IOObjectRelease(intfService);
    }
        
    return kernResult;
}

