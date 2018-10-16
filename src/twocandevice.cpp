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

// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: NMEA2000 Device - Receives NMEA2000 PGN's and converts to NMEA 183 Sentences
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// Outstanding Features: 
// 1. Implement NetworkMap (requires NMEA2000 devices to send 60928 Address Claim & 12996 Product Information PGN's
// 2. Implement Active Device (Handle Address Claim, Product Information, ISO Commands etc.)
// 3. Rewrite/Port Adapter drivers to C++
// 4. Additional PGN's such as AIS, DSC
//

#include "twocandevice.h"

// Globally accessible list of Network devices
DeviceInformation networkMap[CONST_MAX_DEVICES];

TwoCanDevice::TwoCanDevice(wxEvtHandler *handler) : wxThread(wxTHREAD_DETACHED) {
	// Save a reference to our "parent", the plugin event handler so we can pass events to it
	eventHandlerAddress = handler;
	
	// Initialize the FastMessages buffer
	MapInitialize();
	
	// Initialize the statistics
	receivedFrames = 0;
	transmittedFrames = 0;
	droppedFrames = 0;
	
	// Any raw logging ?
	if (logLevel & FLAGS_LOG_RAW) {
		if (!rawLogFile.Open(wxString::Format("%s\\%s", wxStandardPaths::Get().GetDocumentsDir(), CONST_LOGFILE_NAME), wxFile::write)) {
			wxLogError(_T("Unable to open raw log file %s"), CONST_LOGFILE_NAME);
		}
	}
}

TwoCanDevice::~TwoCanDevice(void) {
	// Anything to do here ??
	// Not sure about the order of the Destructor or OnExit routines being called ??
}

// Init, Load the CAN Adapter DLL and get ready to start reading from the CAN bus.
int TwoCanDevice::Init(wxString driverPath) {
	// Load the CAN Adapter DLL
	int returnCode = LoadDriver(driverPath);
	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("TwoCan Device, Error loading driver %s: %lu"), driverPath, returnCode);
	}
	else {
		wxLogMessage(_T("TwoCan Device, Loaded driver %s"), driverPath);
	}
	return returnCode;
}

// DeInit, Unload the DLL
int TwoCanDevice::Deinit() {
	return TWOCAN_RESULT_SUCCESS;
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode TwoCanDevice::Entry() {
	// Merely loops continuously waiting for frames to be received by the CAN Adapter
	return (wxThread::ExitCode)ReadDriver();
}

// OnExit, called when thread is being destroyed
void TwoCanDevice::OnExit() {

	// BUG BUG Should this be moved to DeInit ??
	// Unload the CAN Adapter DLL
	int returnCode = UnloadDriver();

	wxLogMessage(_T("TwoCan Device, Driver Unload Result: %lu"), returnCode);

	eventHandlerAddress = NULL;


	// If logging, close log file
	if (logLevel & FLAGS_LOG_RAW) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Close();
		}
	}
}

// Load CAN adapter DLL.
int TwoCanDevice::LoadDriver(wxString driverPath) {

	// The DLL function pointers
	LPFNDLLManufacturerName manufacturerName = NULL;
	LPFNDLLDriverName driverName = NULL;
	LPFNDLLDriverVersion driverVersion = NULL;
	LPFNDLLOpen open = NULL;

	// Load the DLL and keep a reference to the handle
	fileHandle = FindFirstFile(driverPath, &findFileData);

	if (fileHandle != NULL) {
		// BUG BUG Remove for production
		wxLogMessage(_T("TwoCan Device, Found driver %s"),driverPath);
	}
	else {
		// BUG BUG Log Fatal Error
		wxLogError(_T("TwoCan Device, Cannot find driver %s"), driverPath);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE,TWOCAN_ERROR_DRIVER_NOT_FOUND);
	}

	dllHandle = LoadLibrary(driverPath);

	// If the handle is valid, try to get the function addresses. 
	if (dllHandle != NULL)	{

		// Get pointer to manufacturerName function
		manufacturerName = (LPFNDLLManufacturerName)GetProcAddress(dllHandle, "ManufacturerName");

		// If the function address is valid, call the function. 
		if (manufacturerName != NULL)	{
			// BUG BUG Log information
			wxLogMessage(_T("TwoCan Device, Driver Manufacturer: %s"), manufacturerName());
		}
		else {
			// BUG BUG Log non fatal error
			wxLogError(_T("TwoCan Device, Invalid Driver Manufacturer function %d"), GetLastError());
		}

		// Get pointer to driverName function
		driverName = (LPFNDLLDriverName)GetProcAddress(dllHandle, "DriverName");

		// If the function address is valid, call the function. 
		if (driverName != NULL)	{
			// BUG BUG Log Information
			wxLogMessage(_T("TwoCan Device, Driver Name: %s"), driverName());
		}
		else {
			// BUG BUG Log non fatal error
			wxLogError(_T("TwoCan Device, Invalid  Driver Name function %d"), GetLastError());
		}

		// Get pointer to driverVersion function
		driverVersion = (LPFNDLLDriverVersion)GetProcAddress(dllHandle, "DriverVersion");

		// If the function address is valid, call the function. 
		if (driverVersion != NULL)	{
			// BUG BUG Log Information
			wxLogMessage(_T("TwoCan Device, Driver Version: %s"), driverVersion());
		}
		else {
			// BUG BUG Log non fatal error
			wxLogError(_T("TwoCan Device, Invalid Driver Version function %d"), GetLastError());
		}

		// mutex for synchronizing access to the Frame
		// initial state set to false, meaning we don't "own" the initial state
		mutexHandle = CreateMutex(NULL, FALSE, CONST_MUTEX_NAME);

		if (mutexHandle == NULL) {
			// BUG BUG Log fatal error
			wxLogError(_T("TwoCan Device, Create Mutex failed %d"), GetLastError());
			// Free the library:
			freeResult = FreeLibrary(dllHandle);
			return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_CREATE_FRAME_RECEIVED_MUTEX);
		}


		// Get pointer to the open function 
		open = (LPFNDLLOpen)GetProcAddress(dllHandle, "OpenAdapter");

		// If the function address is valid, call the function. 
		if (open != NULL)	{
	
			int openResult = open();

			if (openResult != TWOCAN_RESULT_SUCCESS) {
				// Fatal error so clean up and free the library
				freeResult = FreeLibrary(dllHandle);
				// BUG BUG Log Fatal Error
				wxLogError(_T("TwoCan Device, Driver Open error: %lu"), openResult);
				return openResult;
			}

			// The driver's open function creates the Data Received event, so wire it up
			eventHandle = OpenEvent(EVENT_ALL_ACCESS, TRUE, CONST_EVENT_NAME);

			if (eventHandle == NULL) {
				// Fatal error so clean up and free the library
				freeResult = FreeLibrary(dllHandle);
				// BUG BUG Log error
				wxLogError(_T("TwoCan Device, Create DataReceivedEvent failed %d"), GetLastError());
				return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE,TWOCAN_ERROR_OPEN_DATA_RECEIVED_EVENT);
			}

			return TWOCAN_RESULT_SUCCESS;

		}
		else {
			// BUG BUG Log Fatal Error
			wxLogError(_T("TwoCan Device, Invalid Open function: %d\n"), GetLastError());
		    return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE ,TWOCAN_ERROR_INVALID_OPEN_FUNCTION);
		}

	}

	else {
		// BUG BUG Log Fatal Error
		wxLogError(_T("TwoCan Device, Invalid DLL handle %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_LOAD_LIBRARY);
	}
	
}

// Thread waits to receive CAN frames from the adapter
int TwoCanDevice::ReadDriver() {
	LPFNDLLRead read = NULL;

	// Get pointer to the read function
	read = (LPFNDLLRead)GetProcAddress(dllHandle, "ReadAdapter");

	// If the function address is valid, call the function. 
	if (read != NULL)	{

		// Starts the read thread in the adapter
		int readResult = read(canFrame);

		if (readResult != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("TwoCan Device, Driver Read error: %lu"), readResult);
			return readResult;
		}

		wxLogMessage(_T("TwoCan Device, Driver Read result: %lu"), readResult);

		DWORD eventResult;
		DWORD mutexResult;

		while (this->IsRunning())
		{
			// Wait for a valid CAN Frame to be forwarded by the driver
			eventResult = WaitForSingleObject(eventHandle, 200);

			if (eventResult == WAIT_OBJECT_0) {
				// Signaled that a valid CAN Frame has been received

				// Wait for a lock on the CAN Frame buffer, so we can process it
				//BUG BUG What is a suitable time limit
				mutexResult = WaitForSingleObject(mutexHandle, 200);

				if (mutexResult == WAIT_OBJECT_0) {

					CanHeader header;
					DecodeCanHeader(canFrame, &header);

					byte payload[8];
					memcpy(payload, &canFrame[4], 8);

					// Log received frames
					if (logLevel & FLAGS_LOG_RAW) {
						if (rawLogFile.IsOpened()) {
							for (int j = 0; j < CONST_FRAME_LENGTH; j++) {
								rawLogFile.Write(wxString::Format("0x%02X", canFrame[j]));
								if (j < CONST_FRAME_LENGTH - 1) {
									rawLogFile.Write(",");
								}
							}
						}
						rawLogFile.Write("\r\n");
					}
					 
					AssembleFastMessage(header, payload);

					
					// Release the CAN Frame buffer
					if (!ReleaseMutex(mutexHandle)) {
						// BUG BUG Log error
						wxLogError(_T("TwoCan Device, Release mutex error: %d"), GetLastError());
					}
				}

				else {

					// BUG BUG Log error
					// An unexpected mutex return code, timeout or abandoned ??
					wxLogError(_T("TwoCan Device, Unexpected Mutex result : %d"), mutexResult);
				}

			}
			// Reset the FrameReceived event
			ResetEvent(eventHandle);

		} // end while

		wxLogMessage(_T("TwoCan Device, Read Thread terminating"));

		// Thread is terminating, so close the mutex & the event handles
		int closeResult;

		closeResult = CloseHandle(eventHandle);
		if (closeResult == 0) {
			// BUG BUG Log Error
			wxLogMessage(_T("TwoCan Device, Close Event Handle: %d, Error Code: %d"), closeResult, GetLastError());
		}

		closeResult = CloseHandle(mutexHandle);
		if (closeResult == 0) {
			// BUG BUG Log Error
			wxLogMessage(_T("TwoCan Device, Close Mutex Handle: %d, Error Code: %d"), closeResult, GetLastError());
		}

		return TWOCAN_RESULT_SUCCESS;

	}
	else {
		// BUG BUG Log Fatal error
		wxLogError(_T("TwoCan Device, Invalid Read function %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_READ_FUNCTION);
	}
}

int TwoCanDevice::UnloadDriver() {
	LPFNDLLClose close = NULL;

	// Get pointer to the close function
	close = (LPFNDLLClose)GetProcAddress(dllHandle, "CloseAdapter");

	// If the function address is valid, call the function. 
	if (close != NULL)	{

		int closeResult = close();
	
		if (closeResult == TWOCAN_RESULT_SUCCESS ) {
			// Free the library:
			freeResult = FreeLibrary(dllHandle);
			
			if (freeResult == 0) {
				// BUG BUG Log Error
				wxLogError(_T("TwoCan Device, Free Library: %d Error Code: %d"), freeResult, GetLastError());
				return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_UNLOAD_LIBRARY);
			}
			else {
				return TWOCAN_RESULT_SUCCESS;
			}

		}
		else {
			/// BUG BUG Log error
			wxLogError(_T("TwoCan Device, Close Error: %lu"), closeResult);
			return closeResult;
		}
	}
	else {
		// BUG BUG Log error
		wxLogError(_T("TwoCan Device, Invalid Close function: %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_CLOSE_FUNCTION);
	}

}



// Queue the FRAME_RECEIVED_EVENT to the plugin where it will push the NMEA 0183 sentence into OpenCPN
void TwoCanDevice::RaiseEvent(wxString sentence) {
	wxCommandEvent *event = new wxCommandEvent(wxEVT_FRAME_RECEIVED_EVENT, FRAME_RECEIVED_EVENT);
	event->SetString(sentence);
	wxQueueEvent(eventHandlerAddress, event);
}

// PGNS that are Fast Messages
// Wouldn't it be nice if NMEA used a bit in the header to indicate fast messages, rather than apriori knowledge from the NMEA2000 standard !!
// It may be that ranges of PGN's are known to be Single or Multi-Frame messages, but I don't know
unsigned int nmeafastMessages[] = { 126996, 128275, 129029, 129038, 129039, 129284, 129285, 129794, 129809, 129810, 130074 };

// Checks whether a frame is a single frame message or multiframe Fast Packet message
bool TwoCanDevice::IsFastMessage(const CanHeader header) {
	for (int i = 0; i < sizeof(nmeafastMessages); i++) {
		if (header.pgn == nmeafastMessages[i])
			return TRUE;
	}
	return FALSE;
}

// Determine if message is a single frame message (if so parse it) otherwise
// assemble the sequence of frames into a multi-frame Fast Message
void TwoCanDevice::AssembleFastMessage(const CanHeader header, const byte *payload) {
	if (IsFastMessage(header)) {
		int position;
		position = MapFindMatchingEntry(header);
		if (position == NOT_FOUND) {
			MapInsertEntry(header, payload, MapFindFreeEntry());
		}
		else {
			MapAppendEntry(header, payload, position);
		}
	}
	else {
		ParseMessage(header, payload);
	}
}

// Initialize each entry in the Fast Message Map
void TwoCanDevice::MapInitialize(void) {
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		fastMessages[i].IsFree = TRUE;
		fastMessages[i].data = NULL;
	}
}

// Lock a range of entries
// BUG BUG Remove for production, just used for testing
void TwoCanDevice::MapLockRange(const int start, const int end) {
	if (start < end)  {
		for (int i = start; i < end; i++) {
			fastMessages[i].IsFree = FALSE;
			fastMessages[i].timeArrived = time(0);
		}
	}

}


// Find first free entry in fastMessages
int TwoCanDevice::MapFindFreeEntry(void) {
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		if (fastMessages[i].IsFree == TRUE) {
			return i;
		}
	}
	// Could also run the garbageCollection routine in a separate thread, would require locking etc.
	// But this will look for stale entries in case there are no free entries
	// If there are no free entries, then indicative that we are receiving more Fast messages
	// than I anticipated. As someone said, "Assumptions are the mother of all fuckups"
	int staleEntries;
	staleEntries = MapGarbageCollector();
	if (staleEntries == 0) {
		return NOT_FOUND;
		//BUG BUG Log this so as to increase the number of FastMessages that may be received
		wxLogError(_T("TwoCan Device, No free entries in Fast Message Map"));
	}
	else {
		return MapFindFreeEntry();
	}
}

// Insert the first message of a sequence of fast messages
void TwoCanDevice::MapInsertEntry(const CanHeader header, const byte *data, const int position) {
	// first message of fast packet 
	// data[0] Sequence Identifier (sid)
	// data[1] Length of data bytes
	// data[2..7] 6 data bytes

	int totalDataLength; // includes padding
	// int padding;
	// padding = 7 - (((unsigned int)data[1] - 6) % 7);
	// total length = expected length - len of first frame(6), divided by number. of data bytes in subsequent frames (7)
	totalDataLength = (((unsigned int)data[1] - 6) / 7) * 7;
	// plus the length of the first frame (6) and if padding > 0, an extra frame (7)
	totalDataLength += ((((unsigned int)data[1] - 6) % 7) == 0) ? 6 : 13; //add first frame and if necessary the last frame (incl padding)

	fastMessages[position].sid = (unsigned int)data[0];
	fastMessages[position].expectedLength = (unsigned int)data[1];
	fastMessages[position].header = header;
	fastMessages[position].timeArrived = time(NULL);
	fastMessages[position].IsFree = FALSE;
	// Remember to free after we have processed the final frame
	fastMessages[position].data = (byte *)malloc(totalDataLength);
	memcpy(&fastMessages[position].data[0], &data[2], 6);
	// First frame of a multi-frame Fast Message contains six data bytes, position the cursor ready for next message
	fastMessages[position].cursor = 6; 
}

// Append subsequent messages of a sequence of fast messages
int TwoCanDevice::MapAppendEntry(const CanHeader header, const byte *data, const int position) {
	// Check that this is the next message in the sequence
	if ((fastMessages[position].sid + 1) == data[0]) { 
		memcpy(&fastMessages[position].data[fastMessages[position].cursor], &data[1], 7);
		fastMessages[position].sid = data[0];
		fastMessages[position].timeArrived = time(NULL);
		// Subsequent messages contains seven data bytes (last message may be padded with 0xFF)
		fastMessages[position].cursor += 7; 
		// Is this the last message ?
		if (fastMessages[position].cursor >= fastMessages[position].expectedLength) {
			// Send for parsing
			ParseMessage(header, fastMessages[position].data);
			// Clear the entry
			free(fastMessages[position].data);
			fastMessages[position].IsFree = TRUE;
			fastMessages[position].data = NULL;
		}
		return TRUE;
	}
	else {

		// This is not the next message in the sequence. Must have dropped a message ?
		if (droppedFrames == 0) {
			droppedFrameTime = wxDateTime::Now();
			droppedFrames++;
		}
		if ((droppedFrames > TWOCAN_CONST_DROPPEDFRAME_THRESHOLD) && (wxDateTime::Now() < (droppedFrameTime + wxTimeSpan::Seconds(TWOCAN_CONST_DROPPEDFRAME_PERIOD) ) ) ) {
			wxLogError(_T("TwoCan Device, Dropped Frames rate exceeded"));
			wxLogError(wxString::Format(_T("Frame: %d %d %d %d"),header.source,header.destination,header.priority,header.pgn));
			droppedFrames = 0;
		}
		return FALSE;

	}
}

// Determine whether an entry with a matching header exists. If not, then assume this is the first frame of a multi-frame Fast Message
int TwoCanDevice::MapFindMatchingEntry(const CanHeader header) {
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		if ((fastMessages[i].IsFree == FALSE) && (fastMessages[i].header.pgn == header.pgn) && \
			(fastMessages[i].header.source == header.source) && (fastMessages[i].header.destination == header.destination)) {
			return i;
		}
	}
	return NOT_FOUND;
}

//BUG BUG if this gets run in a separate thread, need to lock the fastMessages 
int TwoCanDevice::MapGarbageCollector(void) {
	time_t now = time(0);
	int staleEntries;
	staleEntries = 0;
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		if ((fastMessages[i].IsFree == FALSE) && (difftime(now, fastMessages[i].timeArrived) > CONST_TIME_EXCEEDED)) {
			staleEntries++;
			free(fastMessages[i].data);
			fastMessages[i].IsFree = TRUE;
		}
	}
	return staleEntries;
}

// Big switch statement to parse received NMEA 2000 messages
void TwoCanDevice::ParseMessage(const CanHeader header, const byte *payload) {
	wxString nmeaSentence;
	switch (header.pgn) {
	case 60928: // ISO Address Claim
		DecodePGN60928(payload, &deviceInformation);
		// Add the source address so that we can  construct a "map" of the NMEA2000 network
		// BUG BUG To do properly should also parse ISO Commanded Address messages
		deviceInformation.networkAddress = header.source;
		
		wxLogMessage(_T("TwoCan Network, Address: %d"), deviceInformation.networkAddress);
		wxLogMessage(_T("TwoCan Network, Manufacturer: %d"), deviceInformation.manufacturerId);
		wxLogMessage(_T("TwoCan Network, Unique ID: %d"), deviceInformation.uniqueId);
		wxLogMessage(_T("TwoCan Network, Class: %d"), deviceInformation.deviceClass);
		wxLogMessage(_T("TwoCan Network, Function: %d"), deviceInformation.deviceFunction);
		wxLogMessage(_T("TwoCan Network, Industry %d"), deviceInformation.industryGroup);
		break;

		// Maintain a map of the network.
		networkMap[header.source] = deviceInformation;

	case 126992: // System Time
		if (supportedPGN & FLAGS_ZDA) {
			nmeaSentence = DecodePGN126992(payload);
		}
		break;
	case 126996: // Product Information
		DecodePGN126996(payload, &productInformation);
		wxLogMessage(_T("TwoCan Node, Network Address %d"), header.source);
		wxLogMessage(_T("TwoCan Node, DB Ver: %d"), productInformation.dataBaseVersion);
		wxLogMessage(_T("TwoCan Node, Product Code: %d"), productInformation.productCode);
		wxLogMessage(_T("TwoCan Node, Cert Level: %d"), productInformation.certificationLevel);
		wxLogMessage(_T("TwoCan Node, Load Level: %d"), productInformation.loadEquivalency);
		wxLogMessage(_T("TwoCan Node, Model ID: %s"), productInformation.modelId);
		wxLogMessage(_T("TwoCan Node, Model Version: %s"), productInformation.modelVersion);
		wxLogMessage(_T("TwoCan Node, Software Version: %s"), productInformation.softwareVersion);
		wxLogMessage(_T("TwoCan Node, Serial Number: %s"), productInformation.serialNumber);
		break;
	case 127250: // Heading
		if (supportedPGN & FLAGS_HDG) {
			nmeaSentence = DecodePGN127250(payload);
		}
		break;
	case 128259: // Boat Speed
		if (supportedPGN & FLAGS_VHW) {
			nmeaSentence = DecodePGN128259(payload);
		}
		break;
	case 128267: // Water Depth
		if (supportedPGN & FLAGS_DPT) {
			nmeaSentence = DecodePGN128267(payload);
		}
		break;
	case 129025: // Position - Rapid Update
		if (supportedPGN & FLAGS_GLL) {
			nmeaSentence = DecodePGN129025(payload);
		}
		break;
	case 129026: // COG, SOG - Rapid Update
		if (supportedPGN & FLAGS_VTG) {
			nmeaSentence = DecodePGN129026(payload);
		}
		break;
	case 129029: // GNSS Position
		if (supportedPGN & FLAGS_GGA) {
			nmeaSentence = DecodePGN129029(payload);
		}
		break;
	case 129033: // Time & Date
		if (supportedPGN & FLAGS_ZDA) {
			nmeaSentence = DecodePGN129033(payload);
		}
		break;
	case 130306: // Wind data
		if (supportedPGN & FLAGS_MWV) {
			nmeaSentence = DecodePGN130306(payload);
		}
		break;
	case 130310: // Environmental Parameters
		if (supportedPGN & FLAGS_MWT) {
			nmeaSentence = DecodePGN130310(payload);
		}
		break;
	case 130312: // Temperature
		if (supportedPGN & FLAGS_MWT) {
			nmeaSentence = DecodePGN130312(payload);
		}
		break;
	default:
		// BUG BUG Should we log an unsupported PGN error ??
		break;
	}
	if (nmeaSentence.Length() > 0 ) {
		SendNMEASentence(nmeaSentence);
		nmeaSentence.Clear();
	}
}

// Decodes a 29 bit CAN header
int TwoCanDevice::DecodeCanHeader(const byte *buf, CanHeader *header) {
	if ((buf != NULL) && (header != NULL)) {
		// source address in b(0)
		header->source = buf[0];

		// I don't think any of NMEA 2000 messages set the Extended Data Page (EDP) bit
		// If (b(3) And &H2) = &H2 Then
		// Do something with it
		// End If

		if (buf[3] & 0x01) {
			// If Data Page (DP) bit is set, destination is assumed to be Global Address 255
			header->destination = CONST_GLOBAL_ADDRESS;
			// PGN is encoded over bit 1 of b(3) (aka Data Page), b(2) and if b(2) > 239  also b(1)
			header->pgn = (buf[3] & 0x01) << 16 | buf[2] << 8 | (buf[2] > 239 ? buf[1] : 0);
		}
		else {
			// Destination is in b(1) (aka PDU-S) and PGN is in b(2) (aka PDU-F)
			header->destination = buf[1];
			header->pgn = buf[2] << 8;
		}
		// Priority is bits 2,3,4 of b(3)
		header->priority = (buf[3] & 0x1c) >> 2;

		return TRUE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 60928 ISO Address Claim
int TwoCanDevice::DecodePGN60928(const byte *payload, DeviceInformation *deviceInformation) {
	if ((payload != NULL) && (deviceInformation != NULL)) {
		byte *tmpBuf;
		unsigned int tmp;

		// Need to use the first 4 bytes together as an integer to mask off the different values
		// BUG BUG Why can't I just memcpy directly to the int ? Endianess ??

		tmpBuf = (byte *)malloc(4 * sizeof(byte));
		memcpy(tmpBuf, payload, 4);
		TwoCanUtils::ConvertByteArrayToInteger(tmpBuf, &tmp);
		free(tmpBuf);

		// Unique Identity Number 21 bits
		deviceInformation->uniqueId = tmp & 0x1FFFFF;
		// Manufacturer Code 11 bits
		deviceInformation->manufacturerId = (tmp & 0xFFE00000) >> 21;

		// Not really fussed about these
		// ISO ECU Instance 3 bits()
		//(payload[4] & 0xE0) >> 5;
		// ISO Function Instance 5 bits
		//payload[4] & 0x1F;

		// ISO Function Class 8 bits
		deviceInformation->deviceFunction = payload[5];

		// Reserved 1 bit
		//(payload[6] & 0x80) >> 7

		// Device Class 7 bits
		deviceInformation->deviceClass = payload[6] & 0x7F;

		// System Instance 4 bits
		deviceInformation->deviceInstance = payload[7] & 0x0F;

		// Industry Group 3 bits - Marine == 4
		deviceInformation->industryGroup = (payload[7] & 0x70) >> 4;

		// ISO Self Configurable 1 bit
		//payload[7] & 0x80) >> 7
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126996 NMEA Product Information
int TwoCanDevice::DecodePGN126996(const byte *payload, ProductInformation *productInformation) {
	if ((payload != NULL) && (productInformation != NULL)) {

		// Should divide by 100 to get the correct displayable version
		productInformation->dataBaseVersion = payload[0] | (payload[1] << 8); 

		productInformation->productCode = payload[2] | (payload[3] << 8);

		size_t len;
		char tmp[32];

		// Each of the following strings are up to 32 bytes long, and NOT NULL terminated.
		// Should really check that sizeof(productInformation->modelId) <= len

		// Model ID Bytes [4] - [35]
		memcpy(tmp, &payload[4], 32);
		len = (size_t)_snprintf(NULL, 0, "%s", tmp);
		_snprintf(productInformation->modelId, len + 1, "%s", tmp);

		// Software Version Bytes [36] - [67]
		memcpy(tmp, &payload[36], 32);
		len = (size_t)_snprintf(NULL, 0, "%s", tmp);
		_snprintf(productInformation->softwareVersion, len + 1, "%s", tmp);

		// Model Version Bytes [68] - [99]
		memcpy(tmp, &payload[68], 32);
		len = (size_t)_snprintf(NULL, 0, "%s", tmp);
		_snprintf(productInformation->modelVersion, len + 1, "%s", tmp);

		// Serial Number Bytes [100] - [131]
		memcpy(tmp, &payload[100], 32);
		len = (size_t)_snprintf(NULL, 0, "%s", tmp);
		_snprintf(productInformation->serialNumber, len + 1, "%s", tmp);

		productInformation->certificationLevel = payload[132];
		productInformation->loadEquivalency = payload[133];

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126992 NMEA System Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
wxString TwoCanDevice::DecodePGN126992(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned int timeSource;
		timeSource = (payload[1] & 0xF) >> 4;

		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[2] | (payload[3] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight /10000);
		return wxString::Format("$IIZDA,%s", tm.Format("%H%M%S.00,%d,%m,%Y,%z"));
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 127250 NMEA Vessel Heading
// $--HDG, x.x, x.x, a, x.x, a*hh<CR><LF>
// $--HDT,x.x,T*hh<CR><LF>
wxString TwoCanDevice::DecodePGN127250(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short heading;
		heading = payload[1] | (payload[2] << 8);

		short deviation;
		deviation = payload[3] | (payload[4] << 8);

		short variation;
		variation = payload[5] | (payload[6] << 8);

		byte headingReference;
		headingReference = (payload[7] & 0x03);

		//BUG BUG sign of variation and deviation corresponds to East (E) or West (W)

		return wxString::Format("$IIHDG,%.2f,%.2f,%c,%.2f,%c", RADIANS_TO_DEGREES((float)heading / 10000), \
			RADIANS_TO_DEGREES((float)deviation / 10000), deviation >= 0 ? 'E' : 'W', \
			RADIANS_TO_DEGREES((float)variation / 10000), variation >= 0 ? 'E' : 'W');
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 128259 NMEA Speed & Heading
// $--VHW, x.x, T, x.x, M, x.x, N, x.x, K*hh<CR><LF>
wxString TwoCanDevice::DecodePGN128259(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short speedWaterReferenced;
		speedWaterReferenced = payload[1] | (payload[2] << 8);

		unsigned short speedGroundReferenced;
		speedGroundReferenced = payload[3] | (payload[4] << 8);

		// BUG BUG Maintain heading globally from other sources to insert corresponding values into sentence	
		return wxString::Format("$IIVHW,,T,,M,%.2f,N,%.2f,K", (float)speedWaterReferenced * CONVERT_MS_KNOTS / 100, \
			(float)speedWaterReferenced * CONVERT_MS_KMH / 100);
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 128267 NMEA Depth
// $--DPT,x.x,x.x,x.x*hh<CR><LF>
// $--DBT,x.x,f,x.x,M,x.x,F*hh<CR><LF>
wxString TwoCanDevice::DecodePGN128267(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short depth;
		depth = payload[1] | (payload[2] << 8);

		short offset;
		offset = payload[3] | (payload[4] << 8);

		unsigned short maxRange;
		maxRange = payload[5] | (payload[6] << 8);

		//return wxString::Format("$IIDPT,%.2f,%.2f,%.2f", (float)depth / 100, (float)offset / 100, \
		//	((maxRange != 0xFFFF) && (maxRange > 0)) ? maxRange / 100 : (int)NULL);

		// OpenCPN Dashboard only accepts DBT sentence
		return wxString::Format("$IIDBT,%.2f,f,%.2f,M,%.2f,F", CONVERT_METRES_FEET * (double) depth / 100, \
			(double)depth / 100, CONVERT_METRES_FATHOMS * (double)depth / 100);

	}
	else {
		return wxEmptyString;;
	}
}

// Decode PGN 129025 NMEA Position Rapid Update
// $--GLL, llll.ll, a, yyyyy.yy, a, hhmmss.ss, A, a*hh<CR><LF>
//                                           Status A valid, V invalid
//                                               mode - note Status = A if Mode is A (autonomous) or D (differential)
wxString TwoCanDevice::DecodePGN129025(const byte *payload) {
	if (payload != NULL) {


		double latitude;
		latitude = ((payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = (latitude - latitudeDegrees) * 60;

		double longitude;
		longitude = ((payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = (longitude - longitudeDegrees) * 60;

		char gpsMode;
		gpsMode = 'A';
		
		// BUG BUG Verify S & W values are indeed negative
		// BUG BUG Mode & Status are not available in PGN 129025
		// BUG BUG UTC Time is not available in  PGN 129025

		wxDateTime tm;
		tm = wxDateTime::Now();
		
		return wxString::Format("$IIGLL,%02d%05.2f,%c,%03d%05.2f,%c,%s,%c,%c", latitudeDegrees, latitudeMinutes, latitude >= 0 ? 'N' : 'S', \
			longitudeDegrees, longitudeMinutes, longitude >= 0 ? 'E' : 'W', tm.Format("%H%M%S.00").ToAscii(), gpsMode, ((gpsMode == 'A') || (gpsMode == 'D')) ? 'A' : 'V');
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 129026 NMEA COG SOG Rapid Update
// $--VTG,x.x,T,x.x,M,x.x,N,x.x,K,a*hh<CR><LF>
wxString TwoCanDevice::DecodePGN129026(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		// True = 0, Magnetic = 1
		byte headingReference;
		headingReference = (payload[1] & 0x03);

		unsigned int courseOverGround;
		courseOverGround = (payload[2] | (payload[3] << 8));

		unsigned int speedOverGround;
		speedOverGround = (payload[4] | (payload[5] << 8));

		// BUG BUG if Heading Ref = True (0), then ignore %.2f,M and vice versa if Heading Ref = Magnetic (1), ignore %.2f,T
		// BUG BUG GPS Mode should be obtained rather than assumed

		return wxString::Format("$IIVTG,%.2f,T,%.2f,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
			RADIANS_TO_DEGREES((float)courseOverGround / 10000), (float)speedOverGround * CONVERT_MS_KNOTS / 100, \
			(float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS);
	}
	else {
		return wxEmptyString;
	}	
}

// Decode PGN 129029 NMEA GNSS Position
// $--GGA, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x, xx, x.x, x.x, M, x.x, M, x.x, xxxx*hh<CR><LF>
//                                             |  |   hdop         geoidal  age refID 
//                                             |  |        Alt
//                                             | sats
//                                           fix Qualty

wxString TwoCanDevice::DecodePGN129029(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[1] | (payload[2] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		// BUG BUG Does casting to int work or should I use Math.Floor ??

		double latitude;
		latitude = 1e-16 * (((long long)payload[7] | ((long long)payload[8] << 8) | ((long long)payload[9] << 16) | ((long long)payload[10] << 24) \
			| ((long long)payload[11] << 32) | ((long long)payload[12] << 40) | ((long long)payload[13] << 48) | ((long long)payload[14] << 56)));

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = (latitude - latitudeDegrees) * 60;

		double longitude;
		longitude = 1e-16 * (((long long)payload[15] | ((long long)payload[16] << 8) | ((long long)payload[17] << 16) | ((long long)payload[18] << 24) \
			| ((long long)payload[19] << 32) | ((long long)payload[20] << 40) | ((long long)payload[21] << 48) | ((long long)payload[22] << 56)));

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = (longitude - longitudeDegrees) * 60;

		double altitude;
		altitude = 1e-6 * (((long long)payload[23] | ((long long)payload[24] << 8) | ((long long)payload[25] << 16) | ((long long)payload[26] << 24) \
			| ((long long)payload[27] << 32) | ((long long)payload[28] << 40) | ((long long)payload[29] << 48) | ((long long)payload[30] << 56)));


		byte fixType;
		byte fixMethod;

		fixType = (payload[31] & 0xF0) >> 4;
		fixMethod = payload[31] & 0x0F;

		byte fixIntegrity;
		fixIntegrity = payload[32] & 0x03;

		byte numberOfSatellites;
		numberOfSatellites = payload[33];

		unsigned short hDOP;
		hDOP = payload[34] | (payload[35] << 8);

		unsigned short pDOP;
		pDOP = payload[36] | (payload[37] << 8);

		unsigned long geoidalSeparation; //0.01
		geoidalSeparation = payload[38] | (payload[39] << 8);

		byte referenceStations;
		referenceStations = payload[40];

		unsigned short referenceStationType;
		unsigned short referenceStationID;
		unsigned short referenceStationAge;

		// We only need one reference station for the GGA sentence
		if (referenceStations != 0xFF && referenceStations > 0) {
			// BUG BUG Check this, may have bit orders wrong
			referenceStationType = payload[43] & 0xF0 >> 4;
			referenceStationID = (payload[43] & 0xF) << 4 | payload[44];
			referenceStationAge = (payload[45] | (payload[46] << 8));
		}

		return wxString::Format("$IIGGA,%s,%02d%05.4f,%c,%03d%05.4f,%c,%d,%d,%.2f,%.1f,M,%.1f,M,,", \
			tm.Format("%H%M%S").ToAscii() , latitudeDegrees, latitudeMinutes, latitudeDegrees >= 0 ? 'N' : 'S', \
			longitudeDegrees, longitudeMinutes, longitudeDegrees >= 0 ? 'E' : 'W', \
			fixType, numberOfSatellites, (double)hDOP * 0.01f, (double)altitude * 1e-6, \
			(double)geoidalSeparation * 0.01f);
		
		//ignore reference stations
		//, \
		//	((referenceStations != 0xFF) && (referenceStations > 0)) ? referenceStationAge : "", \
		//	((referenceStations != 0xFF) && (referenceStations > 0)) ? referenceStationID : "");
		//
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 129033 NMEA Date & Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
wxString TwoCanDevice::DecodePGN129033(const byte *payload) {
	if (payload != NULL) {
		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[0] | (payload[1] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		// BUG BUG Sign !!
		unsigned int localOffset;
		localOffset = payload[6] | (payload[7] << 8);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);
		
		//BUG BUG need to calculte & display the offset correctly
		return wxString::Format("$IIZDA,%s,%s,%s", tm.Format("%H%M%S,%d,%m,%Y"),(int)localOffset/60, (int)localOffset - (localOffset /60));
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 130306 NMEA Wind
// $--MWV,x.x,a,x.x,a,A*hh<CR><LF>
wxString TwoCanDevice::DecodePGN130306(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short windSpeed;
		windSpeed = payload[1] | (payload[2] << 8);

		double windAngle;
		windAngle = RADIANS_TO_DEGREES((double) (payload[3] | (payload[4] << 8)) /10000);

		byte windReference;
		windReference = (payload[5] & 0x07);

		if (windReference == WIND_REFERENCE_APPARENT) {

			return wxString::Format("$IIMWV,%.2f,%c,%.2f,N,A", \
				(windAngle <= 180) ? windAngle:windAngle -360 , \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T', (double)windSpeed * CONVERT_MS_KNOTS / 100);

		} 
		else {
			return wxEmptyString;
		}
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 130310 NMEA Water & Air Temperature and Pressure
// $--MTW,x.x,C*hh<CR><LF>
wxString TwoCanDevice::DecodePGN130310(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned int waterTemperature;
		waterTemperature = payload[1] | (payload[2] << 8);

		unsigned int airTemperature;
		airTemperature = payload[3] | (payload[4] << 8);

		unsigned int airPressure;
		airPressure = payload[5] | (payload[6] << 8);

		return wxString::Format("$IIMTW,%.2f,C", (float)(waterTemperature * 0.01f) + CONST_KELVIN);
	}
	else {
		return wxEmptyString;
	}
}

// Decode PGN 130312 NMEA Temperature
// $--MTW,x.x,C*hh<CR><LF>
wxString TwoCanDevice::DecodePGN130312(const byte *payload) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte instance;
		instance = payload[1];

		byte source;
		source = payload[2];

		unsigned int actualTemperature;
		actualTemperature = payload[3] | (payload[4] << 8);

		unsigned int setTemperature;
		setTemperature = payload[5] | (payload[6] << 8);

		if (source == TEMPERATURE_SEA) {

			return wxString::Format("$IIMTW,%.2f,C", (float)(actualTemperature * 0.01f) + CONST_KELVIN);
		}
		else {
			return wxEmptyString;
		}
	}
	else {
		return wxEmptyString;
	}
}

// BUG BUG For future versions to transmit data onto the NMEA2000 network
// BUG BUG Port my exisiting .Net methods.

// Claim an Address on the NMEA 2000 Network
int TwoCanDevice::ClaimAddress() {
	return 0;
}

// Transmit NMEA 2000 Product Information
int TwoCanDevice::TransmitProductInformation() {
	return 0;
}

// Respond to ISO Rqsts
int TwoCanDevice::ISORqstResponse() {
	return 0;
}

// Shamelessly copied from somewhere, another plugin ?
void TwoCanDevice::SendNMEASentence(wxString sentence) {
	sentence.Trim();
	wxString checksum = ComputeChecksum(sentence);
	sentence = sentence.Append(wxT("*"));
	sentence = sentence.Append(checksum);
	sentence = sentence.Append(wxT("\r\n"));
	RaiseEvent(sentence);
}

// Shamelessly copied from somewhere, another plugin ?
wxString TwoCanDevice::ComputeChecksum(wxString sentence) {
	unsigned char calculatedChecksum = 0;
	for (wxString::const_iterator i = sentence.begin() + 1; i != sentence.end(); ++i) {
		calculatedChecksum ^= static_cast<unsigned char> (*i);
	}
	return(wxString::Format(wxT("%02X"), calculatedChecksum));
}

