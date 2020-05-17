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
// Unit: TwoCan Mac Serial USB - Implements Mac OSX serial USB modem interface for Canable Cantact device (and may work with other SLCAN compatible USB serial devices)
// Owner: twocanplugin@hotmail.com
// Date: 10/5/2020
// Version History:
// 1.8 - Initial Release, Mac OSX support
//

#include <twocanmacserial.h>

TwoCanMacSerial::TwoCanMacSerial(wxMessageQueue<std::vector<byte>> *messageQueue) : TwoCanInterface(messageQueue) {
}

TwoCanMacSerial::~TwoCanMacSerial() {
}

// Open the serial device
int TwoCanMacSerial::Open(const wxString& portName) {
	// portName parameter is ignored as we automagically detect the Canable Cantact tty device name
	// BUG BUG Could automagically look for any known serial USB CAN Adapter that uses the SLCAN commands
	// In addition to Canable, USBTin, Lawicell
	wxString ttyDeviceName;	
	
	if (!FindTTYDevice(ttyDeviceName, CANABLE_VENDOR_ID, CANABLE_PRODUCT_ID)) {
		wxLogMessage(_T("TwoCan Mac Serial USB, Error detecting port"));
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_ADAPTER_NOT_FOUND);
	}

	serialPortHandle = open(ttyDeviceName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (serialPortHandle == -1)	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error Opening %s"), portName);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_SERIALPORT);
	}

	// Ensure it is a tty device
	if (!isatty(serialPortHandle)) {
		wxLogMessage(_T("TwoCan Mac Serial USB, %s is not a TTY device\n"), portName.c_str());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// Configure the serial port settings
	struct termios serialPortSettings;

	// Get the current configuration of the serial interface
	if (tcgetattr(serialPortHandle, &serialPortSettings) == -1)	{
		wxLogMessage(_T("TwoCan USB Modem, Error getting serial port configuration"));
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}


	// Input flags, ignore parity
	serialPortSettings.c_iflag |= IGNPAR;
	serialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);

	// Output flags
	serialPortSettings.c_oflag = 0;

	// Local Mode flags
	//serialPortSettings.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	serialPortSettings.c_lflag = 0;

	// Control Mode flags
	// 8 bit character size, ignore modem control lines, enable receiver
	serialPortSettings.c_cflag |= CS8 | CLOCAL | CREAD;

	// Special character flags
	serialPortSettings.c_cc[VMIN] = 32;
	serialPortSettings.c_cc[VTIME] = 0;

	// Set baud rate
	if ((cfsetispeed(&serialPortSettings, B115200) == -1) || (cfsetospeed(&serialPortSettings, B115200) == -1))	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error setting baud  rate"));
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// Apply the configuration
	if (tcsetattr(serialPortHandle, TCSAFLUSH, &serialPortSettings) == -1)	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error applying tty device settings"));
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// BUG BUG Refactor into a separate function ??
	// Configure the Canable adapter for the NMEA 2000 network
	int bytesWritten = 0;

	// Close the CAN adapter
	bytesWritten = write(serialPortHandle, "C\r", 2);
	if (bytesWritten != 2)	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error closing CAN Bus %d"), bytesWritten);
	}
	else	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Closed CAN Bus"));
	}

	wxThread::Sleep(CONST_TEN_MILLIS);

	// Set CAN Adapter bus speed to 250 kbits/sec
	bytesWritten = write(serialPortHandle, "S5\r", 3);
	if (bytesWritten != 3)	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error setting CAN Bus speed %d"), bytesWritten);
	}
	else	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Set CAN Bus speed 250k"));
	}

	wxThread::Sleep(CONST_TEN_MILLIS);

	// Open the CAN adapter
	bytesWritten = write(serialPortHandle, "O\r", 2);
	if (bytesWritten != 2)
	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error Opening CAN Bus %d"), bytesWritten);
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	wxLogMessage(_T("TwoCan Mac Serial USB, Successfully opened CAN bus"));
	return TWOCAN_RESULT_SUCCESS;
}

int TwoCanMacSerial::Close(void)
{
	// Close the CAN adapter
	int bytesWritten = 0;
	bytesWritten = write(serialPortHandle, "C\r", 2);
	if (bytesWritten != 2)	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Error closing CAN Bus %d"), bytesWritten);
	}
	else	{
		wxLogMessage(_T("TwoCan Mac Serial USB, Closed CAN Bus"));
	}
	close(serialPortHandle);
	return TWOCAN_RESULT_SUCCESS;
}

void TwoCanMacSerial::Read() {
	// Adapted from old cantact 'C' Windows code, converted to 'C++' using std::vector instead of char pointers and mallocs

	std::vector<char>::iterator serialBufferIterator;
	std::vector<char> serialBuffer(4096);
	
	std::vector<char>::iterator assemblyBufferIterator;
	std::vector<char> assemblyBuffer(4096);
	
	std::vector<byte> postedFrame(CONST_FRAME_LENGTH);

	int bytesRead = 0;
	int bytesRemaining = 0;
	
	bool start;
	bool end;
	bool partial;

	while (!TestDestroy()) {
			
		start = false;
		end = false;
		partial = false;
		assemblyBufferIterator = assemblyBuffer.begin();

		// set read timeouts
		struct timeval socketTimeout = { 0, 100 };
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(serialPortHandle, &readSet);

		if (select((serialPortHandle + 1), &readSet, NULL, NULL, &socketTimeout) >= 0)	{
			
			if (FD_ISSET(serialPortHandle, &readSet)) {

				bytesRead = read(serialPortHandle, serialBuffer.data(), serialBuffer.size());

				if (bytesRead > 0) {

					bytesRemaining = bytesRead;
					serialBufferIterator = serialBuffer.begin();

					while (bytesRemaining != 0) {

						// start character, Only interested in Extended Frame (standard frame 't', remote frames 'r'
						if (*serialBufferIterator == CANTACT_EXTENDED_FRAME) {
							start = true;
							*assemblyBufferIterator = *serialBufferIterator;
							assemblyBufferIterator++;
						}

						// normal character
						if ((*serialBufferIterator != '\n') && (*serialBufferIterator != '\r') && (*serialBufferIterator != CANTACT_EXTENDED_FRAME)) {
							*assemblyBufferIterator = *serialBufferIterator;
							assemblyBufferIterator++;
						}

						// end character
						if (*serialBufferIterator == '\r') {
							end = true;
						}

						// end character but no start or partial
						// must have caught a frame mid-stream
						if ((end) && (!start) && (!partial)) {
							start = false;
							end = false;
							partial = false;
							assemblyBufferIterator = assemblyBuffer.begin();
						}

						// if we now have a complete frame
						if (((start) && (end)) || ((partial) && (end))) {

							if (*assemblyBuffer.begin() == CANTACT_EXTENDED_FRAME) {

								// Copy the header as a hex string to a vector of bytes
								for (size_t t = 0; t < CONST_HEADER_LENGTH; t++) {
									//BUG BUG Alternative way.....
									//postedFrame.push_back(((((*(assemblyBuffer.begin() + 1 + (t * 2)) % 32) + 9) % 25) * 16) + (((*(assemblyBuffer.begin() + 2 + (t * 2)) % 32) + 9) % 25));
									int value = ((((assemblyBuffer[1 + (t * 2)] % 32) + 9) % 25) << 4 ) | (((assemblyBuffer[2 + (t * 2)] % 32) + 9) % 25);
									postedFrame.push_back(value);
								}

								// reverse the header bytes for Cantact device (I assume Endianess)
								std::reverse(postedFrame.begin(), postedFrame.end());

								// payload length is transmitted in byte 9
								size_t payload_len = *(assemblyBuffer.begin() + 9) - '0';
						
								// Convert the payload from a hex string and append to the vector
								for (size_t t = 0; t < payload_len; t++) {
									int value =((((assemblyBuffer[10 + (t * 2)] % 32) + 9) % 25) << 4 ) | (((assemblyBuffer[11 + (t * 2)] % 32) + 9) % 25);
									postedFrame.push_back(value);
								}
						
								// Post frame to the TwoCanDevice
								deviceQueue->Post(postedFrame);
								postedFrame.clear();
							}

							// processed a valid frame so reset all
							start = false;
							end = false;
							partial = false;
							assemblyBufferIterator = assemblyBuffer.begin();

						} // end complete frame

						serialBufferIterator++;
						bytesRemaining--;

					} // end while bytesRemaining !=0

					// had a start character but no end character, therefore a partial frame
					if ((start) && (!end)) {
						partial = true;
					}

				} // if bytesread > 0

			}

		}
		

	} // while thread is alive

}

// Write, Transmit a CAN frame onto the NMEA 2000 network
int TwoCanMacSerial::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
	int bytesWritten = 0;
	wxString data;
	
	// "T" indicates a CAN v2.0 Extended Frame (29 bit Id) for SLCAN devices
	data.append("T");
	
	// Note the bytes are reversed for the CanId (Endianess)
	data.append(wxString::Format("%02X",(canId >> 24) & 0xFF));
	data.append(wxString::Format("%02X",(canId >> 16) & 0xFF));
	data.append(wxString::Format("%02X",(canId >> 8) & 0xFF));
	data.append(wxString::Format("%02X",canId & 0xFF));

	// The payload length
	data.append(wxString::Format("%1d",payloadLength));
	
	// The payload itself
	for (int i = 0; i < payloadLength; i++) {
		data.append(wxString::Format("%02X", *payload));
		payload++;
	}

	// Finally the carriage return (indicates end of frame for SLCAN devices)
	data.append("\r");

	bytesWritten = write(serialPortHandle, data.c_str(), data.length());
	if (bytesWritten != data.length()) {
		wxLogMessage(_T("TwoCan Mac Serial USB, Error transmitting frame: %d, %s "), bytesWritten, data);
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_TRANSMIT_FAILURE);
	}
	else {
		tcflush(serialPortHandle,TCIOFLUSH);
		return (TWOCAN_RESULT_SUCCESS);
	}
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode TwoCanMacSerial::Entry() {
	// Merely loops continuously waiting for frames to be received by the CAN Adapter
	Read();
	wxLogMessage(_T("TwoCan Mac Serial USB, CAN Bus read thread exiting"));
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void TwoCanMacSerial::OnExit() {
	// Nothing to do ??
}

// Generate a unique number derived from the MAC Address to be used as a NMEA 2000 device's unique address
// Adapted from Apple sample GetPrimaryMacAdress
int TwoCanMacSerial::GetUniqueNumber(unsigned long *uniqueNumber) {
	*uniqueNumber = 0;

	kern_return_t kernResult = KERN_SUCCESS;
    io_iterator_t intfIterator;
    UInt8 MACAddress[kIOEthernetAddressSize];
 
    kernResult = FindEthernetInterfaces(&intfIterator);
    
    if (KERN_SUCCESS != kernResult) {
        wxLogMessage(_T("TwoCan Mac Serial USB, Error finding Ethernet Interfaces: 0x%08x"), kernResult);
    }
    else {
        kernResult = GetMACAddress(intfIterator, MACAddress, sizeof(MACAddress));
        
        if (KERN_SUCCESS != kernResult) {
            wxLogMessage(_T("TwoCan Mac Serial USB, Error Getting MAC Address: 0x%08x"), kernResult);
        }
		else {
			wxLogMessage(_T("TwoCan Mac Serial USB,  Generating Unique Id from Ethernet Address: %02x:%02x:%02x:%02x:%02x:%02x."),
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

kern_return_t TwoCanMacSerial::FindEthernetInterfaces(io_iterator_t *matchingServices) {
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
kern_return_t TwoCanMacSerial::GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress, UInt8 bufferSize) {
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

// Find the ttyUSB device that the Canable Cantact is connected to
// To understand Mac OSX registry look at ioreg commands
bool TwoCanMacSerial::FindTTYDevice(wxString &ttyDevice, const int vid, const int pid) {

	CFMutableDictionaryRef serialBSDClientDictionary;
	io_iterator_t serialBSDClientIterator;
	io_object_t registryEntry;
	char path[PATH_MAX];
	CFTypeRef ttyPortName;
	bool foundDevice = false;

	if (!(serialBSDClientDictionary = IOServiceMatching(kIOSerialBSDServiceValue))) {
		wxLogMessage(_T("TwoCan Mac Serial USB, Error IOService Matching failed"));
		return false;
	}

	printf("Getting matching services\n");
	if (IOServiceGetMatchingServices(kIOMasterPortDefault, serialBSDClientDictionary, &serialBSDClientIterator) != KERN_SUCCESS) {
		wxLogMessage(_T("TwoCan Mac Serial USB, Error IOServiceGetMatchingServices failed"));
		return false;
	}

	while ((registryEntry = IOIteratorNext(serialBSDClientIterator))) {
		// Get the string value for the property IODialinDevice 
		// To understand the registry use the command ioreg IOSerialBSDClient 
		ttyPortName = IORegistryEntryCreateCFProperty(registryEntry, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0);

		if (ttyPortName) {
			
			if (CFStringGetCString((CFStringRef)ttyPortName, path, PATH_MAX, kCFStringEncodingASCII)) {
				// Save the tty device name
				ttyDevice = path;
				wxLogMessage(_T("TwoCan Mac Serial USB, Searching TTY Devices, Found candidate device: %s"), ttyDevice);

				// iterate parent so that we can check if it has the matching vendor & product id's
                io_object_t parentRegistryEntry = 0;

                if (KERN_SUCCESS == IORegistryEntryGetParentEntry(registryEntry, kIOServicePlane, &parentRegistryEntry))   {
					
					// Get a dictionary of all key/value pairs
                    CFMutableDictionaryRef ttyDeviceDictionary;

                    if (KERN_SUCCESS == IORegistryEntryCreateCFProperties(parentRegistryEntry, &ttyDeviceDictionary, kCFAllocatorDefault, kNilOptions) ) {

						int vendorId = 0;
						int productId = 0;

                    	// Get the vendor id from the dictionary
                    	CFTypeRef numericReference = CFDictionaryGetValue(ttyDeviceDictionary,CFSTR(kUSBVendorID));
                    	
                    	if (numericReference) {

                          	CFNumberGetValue((CFNumberRef)numericReference, kCFNumberSInt32Type, &vendorId);
                        	CFRelease(numericReference);
                    	}

						// Get the product id from the dictionary
						numericReference = CFDictionaryGetValue(ttyDeviceDictionary,CFSTR(kUSBProductID));
                    	
                    	if (numericReference) {

                        	CFNumberGetValue((CFNumberRef)numericReference, kCFNumberSInt32Type, &productId);
							CFRelease(numericReference);
                    	}

						// Perform the comparison
						// Note ignore the function parameters, check for currently known SLCan devices
						if ((vendorId == CANABLE_VENDOR_ID) && (productId == CANABLE_PRODUCT_ID)) {
							wxLogMessage(_T("TwoCan Mac Serial USB, found Canable Cantact adapter: %s"), ttyDevice);
							CFRelease(ttyDeviceDictionary);
							foundDevice = true;
							break;
						}

						else if ((vendorId == USBTIN_VENDOR_ID) && (productId == USBTIN_PRODUCT_ID)) {
							wxLogMessage(_T("TwoCan Mac Serial USB, found USBTin adapter: %s"), ttyDevice);
							CFRelease(ttyDeviceDictionary);
							foundDevice = true;
							break;
						}
						
						else if ((vendorId == LAWICELL_VENDOR_ID) && (productId == LAWICELL_PRODUCT_ID)) {
							wxLogMessage(_T("TwoCan Mac Serial USB, found Lawicell adapter: %s"), ttyDevice);
							CFRelease(ttyDeviceDictionary);
							foundDevice = true;
							break;
						}
						else {
							CFRelease(ttyDeviceDictionary);
						}

					} // if get properties
					
					IOObjectRelease(parentRegistryEntry);
                  
                } // if has parent

			} // if retrive string vaue of port name

			CFRelease(ttyPortName);

		}  // if port name

        IOObjectRelease(registryEntry);

	} // end while

	IOObjectRelease(serialBSDClientIterator);

	return foundDevice;
}
