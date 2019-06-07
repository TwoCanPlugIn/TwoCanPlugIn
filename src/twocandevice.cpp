// Copyright(C) 2018-2019 by Steven Adler
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
// Version History: 
// 1.0 Initial Release
// 1.1 - 13/11/2018, Added AIS & DSC support
// 1.2 - 30/11/2018 , Bug fixes
//     - Use abs/fabs to fix sign of Lat/long
//     - Corrected MWV and WindAngle, angle expresed 0-359 rather than +ve/-ve
//     - Added PGN's 127251 (Rate of Turn), 127258 (Magnetic Variation), 129283 (Cross Track Error), 130577 (Direction Data)
//     - Simplify totalDataLength calculation in MapInsertEntry
//     - Change to DecodeHeader, misunderstood role of DataPage and PDU-F > 240
// 1.3 - 16/3/2019, Linux support via SocketCAN
// 1.4 - 25/4/2019. Active Mode implemented.
// Outstanding Features: 
// 1. Bi-directional gateway ??
// 2. Rewrite/Port Adapter drivers to C++
//

#include "twocandevice.h"

TwoCanDevice::TwoCanDevice(wxEvtHandler *handler) : wxThread(wxTHREAD_DETACHED) {
	// Save a reference to our "parent", the plugin event handler so we can pass events to it
	eventHandlerAddress = handler;
	
#ifdef __LINUX__
	// initialise Message Queue to receive frames from either the Log reader or SockeTCAN interface
	canQueue = new wxMessageQueue<std::vector<byte>>();
#endif
	
	// FastMessage buffer is used to assemble the multiple frames of a fast message
	MapInitialize();
	
	// Initialize the statistics
	receivedFrames = 0;
	transmittedFrames = 0;
	droppedFrames = 0;
	
	// monotonically incrementing counter
	heartbeatCounter = 0;

	// Each AIS multi sentence message has a sequential Message ID
	AISsequentialMessageId = 0;
	
	// Any raw logging ?
	if (logLevel & FLAGS_LOG_RAW) {
		wxDateTime tm = wxDateTime::Now();
		wxString fileName = tm.Format("twocan-%Y-%m-%d_%H%M%S.log");
		// construct a filename with the following format twocan-2018-12-31_210735.log
		if (!rawLogFile.Open(wxString::Format("%s//%s", wxStandardPaths::Get().GetDocumentsDir(), fileName), wxFile::write)) {
			wxLogError(_T("TwoCan Device, Unable to open raw log file %s"), fileName);
		}
	}
}

TwoCanDevice::~TwoCanDevice(void) {
	// Anything to do here ??
	// Not sure about the order of exiting the Entry, executing the OnExit or Destructor functions ??
}

// wxTimer notifications used to send my heartbeatand and to maintain our network map
void TwoCanDevice::OnHeartbeat(wxEvent &event) {
	int returnCode;
	returnCode = SendHeartbeat();
	if (returnCode == TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("TwoCan Device, Sent heartbeat"));
	}
	else {
		wxLogMessage(_T("TwoCan Device, Error sending heartbeat: %lu"), returnCode);
	}
	wxThread::Sleep(CONST_TEN_MILLIS);
	// Iterate through the network map
	int numberOfDevices = 0;
	for (int i = 0; i < CONST_MAX_DEVICES; i++) {
		// We have logged an address claim from a device and this device is not us
		if ((networkMap[i].uniqueId > 0) && (i != networkAddress)) {
			if (strlen(networkMap[i].productInformation.modelId) == 0) {
				// No product info has been received yet for this device, so request it
				returnCode = SendISORequest(i, 126996);
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("TwoCan Device, Sent ISO Request for 126996 to %lu"), i);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Error sending ISO Request for 126996 to %lu: %lu"), i, returnCode);
				}
				wxThread::Sleep(CONST_TEN_MILLIS);
			}
			if (wxDateTime::Now() > (networkMap[i].timestamp + wxTimeSpan::Seconds(60))) {
			// If an entry is stale, send an address claim request.
			// BUG BUG Perhaps add an extra field in which to store the devices' hearbeat interval, rather than comparing against 60'
				returnCode = SendISORequest(i, 60928);
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("TwoCan Device, Sent ISO Request for 60928 to %lu"), i);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Error sending ISO Request  for 60928 to %lu: %lu"), i, returnCode);
				}
				wxThread::Sleep(CONST_TEN_MILLIS);
			}
			numberOfDevices += 1;
		}
		else { // in case we've received a product info frame but have yet to observe an address claim
			if ((strlen(networkMap[i].productInformation.modelId) > 0) && (i != networkAddress)) {
				// No address claim has been received for this device
				returnCode = SendISORequest(i, 60928);
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("TwoCan Device, Sent ISO Request for 60928 to %lu"), i);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Error sending ISO Request  for 60928 to %lu: %lu"), i, returnCode);
				}
				wxThread::Sleep(CONST_TEN_MILLIS);
				numberOfDevices += 1;
			}
		}
	}
	
	
	if (numberOfDevices == 0) {
		// No devices present on the network (yet)
		returnCode = SendISORequest(CONST_GLOBAL_ADDRESS, 60928);
		if (returnCode == TWOCAN_RESULT_SUCCESS) {
			wxLogMessage(_T("TwoCan Device, Sent ISO Request for 60928 to %lu"), CONST_GLOBAL_ADDRESS);
		}
		else {
			wxLogMessage(_T("TwoCan Device, Error sending ISO Request for 60928  to %lu: %lu"), CONST_GLOBAL_ADDRESS, returnCode);
		}
	}	

	
	// BUG BUG Should I delete stale entries ??
	// BUG BUG should I use a map instead of an array with the uniqueID as the key ??
	for (int i = 0; i < CONST_MAX_DEVICES - 1; i++) {
		for (int j = i + 1; j < CONST_MAX_DEVICES; j++) {
			if (networkMap[i].uniqueId == networkMap[j].uniqueId) {
				// Remove whichever one has the stale entry
				if (wxDateTime::Now() >(networkMap[i].timestamp + wxTimeSpan::Seconds(60))) {
					networkMap[i].manufacturerId = 0;
					networkMap[i].uniqueId = 0;
				}
				if (wxDateTime::Now() >(networkMap[j].timestamp + wxTimeSpan::Seconds(60))) {
					// remove the duplicated but stale entry
					networkMap[j].manufacturerId = 0;
					networkMap[j].uniqueId = 0;
				}
			}
		
		}
	}
}


// Init, Load the CAN Adapter (either a Windows DLL or for Linux the baked-in drivers; Log File Reader or SocketCAN
// and get ready to start reading from the CAN bus.
int TwoCanDevice::Init(wxString driverPath) {
	int returnCode;
	
#ifdef  __WXMSW__ 
	// Load the CAN Adapter DLL
	returnCode = LoadWindowsDriver(driverPath);
	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("TwoCan Device, Error loading driver %s: %lu"), driverPath, returnCode);
	}
	else {
		wxLogMessage(_T("TwoCan Device, Loaded driver %s"), driverPath);
		// If we are an active device, claim an address on the network (PGN 60928)  and send our product info (PGN 126996)
		if (deviceMode == TRUE) {
			TwoCanUtils::GetUniqueNumber(&uniqueId);
			wxLogMessage(_T("TwoCan Device, Unique Number: %lu"), uniqueId);
			returnCode = SendAddressClaim(networkAddress);
			if (returnCode != TWOCAN_RESULT_SUCCESS) {
				wxLogError(_T("TwoCan Device, Error sending address claim: %lu"), returnCode);
			}
			else {
				wxLogMessage(_T("TwoCan Device, Claimed network address: %lu"), networkAddress);
				// Broadcast our product information on the network
				returnCode = SendProductInformation();
				if (returnCode != TWOCAN_RESULT_SUCCESS) {
					wxLogError(_T("TwoCan Device, Error sending Product Information %lu"), returnCode);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Sent Product Information"));

				}
				// If we have at least successfully claimed an address and sent our product info, and the heartbeat is enabled, 
				// start a timer that will send PGN 126993 NMEA Heartbeat every 60 seconds
				if (enableHeartbeat == TRUE) {
					heartbeatTimer = new wxTimer();
					heartbeatTimer->Bind(wxEVT_TIMER, &TwoCanDevice::OnHeartbeat, this);
					heartbeatTimer->Start(CONST_ONE_SECOND * 60, wxTIMER_CONTINUOUS);
				}
			}
		}
	}
#endif

#ifdef __LINUX__
	// For Linux, using "baked in" classes instead of the plug-in model that the Window's version uses
	// Save the driver name to pass to the Open, Close, Read functions
	linuxDriverName = driverPath;
	// Determine whether using logfile reader or CAN hardware interface
	if (linuxDriverName.CmpNoCase("Log File Reader") == 0) {
		// Load the logfile reader
		linuxLogReader = new TwoCanLogReader(canQueue);
		returnCode = linuxLogReader->Open(CONST_LOGFILE_NAME);
	}
	else { 
		// Load the SocketCAN interface
		linuxSocket = new TwoCanSocket(canQueue);
		returnCode = linuxSocket->Open(driverPath);
	}
	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("TwoCan Device, Error loading CAN Interface %s: %lu"), driverPath, returnCode);
	}
	else {
		wxLogMessage(_T("TwoCan Device, Loaded CAN Interface %s"),driverPath);
		// if we are an active device, claim an address
		if (deviceMode == TRUE) {
			if (TwoCanSocket::GetUniqueNumber(&uniqueId) == TWOCAN_RESULT_SUCCESS) {
				wxLogMessage(_T("TwoCan Device, Unique Number: %lu"),uniqueId);
				returnCode = SendAddressClaim(networkAddress);
				if (returnCode != TWOCAN_RESULT_SUCCESS) {
					wxLogError(_T("TwoCan Device, Error sending address claim: %lu"), returnCode);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Claimed network address: %lu"), networkAddress);
					// If we have at least successfully claimed an address and the heartbeat is enabled, 
					// start a timer that will send PGN 126993 NMEA Heartbeat every 60 seconds
					if (enableHeartbeat == TRUE) {
						heartbeatTimer = new wxTimer();
						heartbeatTimer->Bind(wxEVT_TIMER, &TwoCanDevice::OnHeartbeat, this);
						heartbeatTimer->Start(CONST_ONE_SECOND * 60, wxTIMER_CONTINUOUS);;
					}
					returnCode = SendProductInformation();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogError(_T("TwoCan Device, Error sending Product Information %lu"), returnCode);
					}
					else {
						wxLogMessage(_T("TwoCan Device, Sent Product Information"));
					}
				}
			}
			else {
				// unable to generate unique number
				// perhaps should just generate a random number
				wxLogError(_T("TwoCan Device, Unable to generate unique address: %lu"), returnCode);
			}
		}
		
	}
	
#endif

	return returnCode;
}

// DeInit
// BUG BUG should we move unloading the Windows DLL to this function ??
// BUG BUG At present DeInit is not called by the TwoCanPlugin
int TwoCanDevice::DeInit() {
	return TWOCAN_RESULT_SUCCESS;
}

// Entry, the method that is executed upon thread start
// Merely loops continuously waiting for frames to be received by the CAN Adapter
wxThread::ExitCode TwoCanDevice::Entry() {
	
#ifdef  __WXMSW__ 
	return (wxThread::ExitCode)ReadWindowsDriver();
#endif

#ifdef __LINUX__
	return (wxThread::ExitCode)ReadLinuxDriver();
#endif
}

// OnExit, called when thread->delete is invoked, and entry returns
void TwoCanDevice::OnExit() {
	// BUG BUG Should this be moved to DeInit ??
	int returnCode;

	// Terminate the heartbeat timer
	// BUG BUG Do we need to delete it ??
	if ((enableHeartbeat == TRUE) && (heartbeatTimer != nullptr)) {
			heartbeatTimer->Stop();
			heartbeatTimer->Unbind(wxEVT_TIMER, &TwoCanDevice::OnHeartbeat, this);
	}


	
#ifdef  __WXMSW__ 
	// Unload the CAN Adapter DLL
	returnCode = UnloadWindowsDriver();
#endif

#ifdef __LINUX__
	wxThread::ExitCode threadExitCode;
	if (linuxDriverName.CmpNoCase("Log File Reader") == 0) {
		returnCode = linuxLogReader->Delete(&threadExitCode,wxTHREAD_WAIT_BLOCK);
		returnCode = linuxLogReader->Close();
	} 
	else {
		returnCode = linuxSocket->Delete(&threadExitCode,wxTHREAD_WAIT_BLOCK);
		returnCode = linuxSocket->Close();
	}
#endif

	wxLogMessage(_T("TwoCan Device, Unloaded driver: %lu"), returnCode);

	eventHandlerAddress = NULL;

	// If logging, close log file
	if (logLevel & FLAGS_LOG_RAW) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Close();
		}
	}
}

#ifdef __LINUX__
int TwoCanDevice::ReadLinuxDriver(void) {
	CanHeader header;
	byte payload[CONST_PAYLOAD_LENGTH];
	wxMessageQueueError queueError;
	std::vector<byte> receivedFrame(CONST_FRAME_LENGTH);
		
	// Start the Linux Device's Read thread
	if (linuxDriverName.CmpNoCase("Log File Reader") == 0) {
		linuxLogReader->Run();
	}
	else {
		linuxSocket->Run();
	}
	
	while (!TestDestroy())	{
		
		// Wait for a CAN Frame
		// BUG BUG, If on an empty network, will never check TestDestroy, so use ReceiveWait
		
		queueError = canQueue->Receive(receivedFrame);
		
		if (queueError == wxMSGQUEUE_NO_ERROR) {
					
			TwoCanUtils::DecodeCanHeader(&receivedFrame[0], &header);
			
			memcpy(payload, &receivedFrame[CONST_HEADER_LENGTH], CONST_PAYLOAD_LENGTH);
			AssembleFastMessage(header, payload);

			// Log received frames
			if (logLevel & FLAGS_LOG_RAW) {
				if (rawLogFile.IsOpened()) {
					for (int j = 0; j < CONST_FRAME_LENGTH; j++) {
						rawLogFile.Write(wxString::Format("0x%02X", receivedFrame[j]));
						if (j < CONST_FRAME_LENGTH - 1) {
							rawLogFile.Write(",");
						}
					}
				}
				rawLogFile.Write("\r\n");
			} // end if logging
					 
		
		} // end if Queue Error

	} // end while

	wxLogMessage(_T("TwoCan Device, Read Thread Exiting"));

	return TWOCAN_RESULT_SUCCESS;	
}
#endif

#ifdef  __WXMSW__ 

// Load CAN adapter DLL.
int TwoCanDevice::LoadWindowsDriver(wxString driverPath) {

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
			wxLogError(_T("TwoCan Device, Error creating mutex: %d"), GetLastError());
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
				wxLogError(_T("TwoCan Device, Error opening driver: %lu"), openResult);
				return openResult;
			}

			// The driver's open function creates the Data Received event, so wire it up
			eventHandle = OpenEvent(EVENT_ALL_ACCESS, TRUE, CONST_DATARX_EVENT);

			if (eventHandle == NULL) {
				// Fatal error so clean up and free the library
				freeResult = FreeLibrary(dllHandle);
				// BUG BUG Log error
				wxLogError(_T("TwoCan Device, Error creating data received event: %d"), GetLastError());
				return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE,TWOCAN_ERROR_OPEN_DATA_RECEIVED_EVENT);
			}

			// Get and save the pointer to the write function if we are an active device
			if (deviceMode == TRUE) {
				writeFrame = (LPFNDLLWrite)GetProcAddress(dllHandle, "WriteAdapter");

				if (writeFrame == NULL)	{
					// BUG BUG Log non fatal error, the plug-in can still receive data.
					wxLogError(_T("TwoCan Device, Invalid Write function: %d\n"), GetLastError());
					deviceMode = FALSE;
					enableHeartbeat = FALSE;
					//return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_WRITE_FUNCTION);
				}
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
		wxLogError(_T("TwoCan Device, Invalid DLL handle: %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_LOAD_LIBRARY);
	}
	
}

#endif

#ifdef  __WXMSW__ 

// Thread waits to receive CAN frames from the adapter
int TwoCanDevice::ReadWindowsDriver() {
	LPFNDLLRead read = NULL;

	// Get pointer to the read function
	read = (LPFNDLLRead)GetProcAddress(dllHandle, "ReadAdapter");

	// If the function address is valid, call the function. 
	if (read != NULL)	{

		// Which starts the read thread in the adapter
		int readResult = read(canFrame);

		if (readResult != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("TwoCan Device, Error starting driver read thread: %lu"), readResult);
			return readResult;
		}
		
		// Log the fact that the driver has started its read thread successfully
		wxLogMessage(_T("TwoCan Device, Driver read thread started: %lu"), readResult);

		DWORD eventResult;
		DWORD mutexResult;

		while (!TestDestroy())
		{
			// Wait for a valid CAN Frame to be forwarded by the driver
			eventResult = WaitForSingleObject(eventHandle, 200);

			if (eventResult == WAIT_OBJECT_0) {
				// Signaled that a valid CAN Frame has been received

				// Wait for a lock on the CAN Frame buffer, so we can process it
				// BUG BUG What is a suitable time limit
				mutexResult = WaitForSingleObject(mutexHandle, 200);

				if (mutexResult == WAIT_OBJECT_0) {

					CanHeader header;
					TwoCanUtils::DecodeCanHeader(canFrame, &header);

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
						wxLogError(_T("TwoCan Device, Error releasing mutex: %d"), GetLastError());
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

		} // end while TestDestory

		wxLogMessage(_T("TwoCan Device, Read Thread exiting"));

		// Thread is terminating, so close the mutex & the event handles
		int closeResult;

		closeResult = CloseHandle(eventHandle);
		if (closeResult == 0) {
			// BUG BUG Log Error
			wxLogMessage(_T("TwoCan Device, Error closing event handle: %d, Error Code: %d"), closeResult, GetLastError());
		}

		closeResult = CloseHandle(mutexHandle);
		if (closeResult == 0) {
			// BUG BUG Log Error
			wxLogMessage(_T("TwoCan Device, Error closing mutex handle: %d, Error Code: %d"), closeResult, GetLastError());
		}

		return TWOCAN_RESULT_SUCCESS;

	}
	else {
		// BUG BUG Log Fatal error
		wxLogError(_T("TwoCan Device, Invalid Driver Read function %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_READ_FUNCTION);
	}
}

#endif

#ifdef  __WXMSW__ 

int TwoCanDevice::UnloadWindowsDriver() {
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
				wxLogError(_T("TwoCan Device, Error freeing lbrary: %d Error Code: %d"), freeResult, GetLastError());
				return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_UNLOAD_LIBRARY);
			}
			else {
				return TWOCAN_RESULT_SUCCESS;
			}

		}
		else {
			/// BUG BUG Log error
			wxLogError(_T("TwoCan Device, Error closing driver: %lu"), closeResult);
			return closeResult;
		}
	}
	else {
		// BUG BUG Log error
		wxLogError(_T("TwoCan Device, Invalid Close function: %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_CLOSE_FUNCTION);
	}

}

#endif

// Queue the SENTENCE_RECEIVED_EVENT to the plugin where it will push the NMEA 0183 sentence into OpenCPN
void TwoCanDevice::RaiseEvent(wxString sentence) {
	wxCommandEvent *event = new wxCommandEvent(wxEVT_SENTENCE_RECEIVED_EVENT, SENTENCE_RECEIVED_EVENT);
	event->SetString(sentence);
	wxQueueEvent(eventHandlerAddress, event);
	
}

// PGNS that are Fast Messages
// Wouldn't it be nice if NMEA used a bit in the header to indicate fast messages, rather than apriori knowledge from the NMEA2000 standard !!
// It may be that ranges of PGN's are known to be Single or Multi-Frame messages, but I don't know

// List of Fast Messages
// 65240 - Commanded Address
// 126208 - NMEA Request/Command/Acknowledge group function
// 126464 - PGN List (Transmit and Receive)
// 126996 - Product information
// 126998 - Configuration information
// 127237 - Heading/Track control
// 127489 - Engine parameters dynamic 
// 127506 - DC Detailed status 
// 128275 - Distance log 
// 129029 - GNSS Position Data 
// 129038 - AIS Class A Position Report
// 129039 - AIS Class B Position Report
// 129284 - Navigation info
// 129285 - Waypoint list
// 129540 - GNSS Sats in View 
// 129794 - AIS Class A Static data
// 129802 - AIS Safety Related Broadcast Message
// 129808 - DSC Call Information
// 129809 - AIS Class B Static Data: Part A
// 129810 - AIS Class B Static Data Part B
// 130074 - Waypoint list
// 130577 - Direction Data

// Checks whether a frame is a single frame message or multiframe Fast Packet message
bool TwoCanDevice::IsFastMessage(const CanHeader header) {
	static const unsigned int nmeafastMessages[] = { 65240, 126208, 126464, 126996, 126998, 127237, 127489, 127506, 128275, 129029, 129038, 129039, 129284, 129285, 129540, 129794, 129802, 129808, 129809, 129810, 130074, 130577 };
	for (int i = 0; i < sizeof(nmeafastMessages)/sizeof(unsigned int); i++) {
		if (nmeafastMessages[i] == header.pgn) {
			return TRUE;
		}
	}
	return FALSE;
}

// Determine if message is a single frame message (if so parse it) otherwise
// Assemble the sequence of frames into a multi-frame Fast Message
void TwoCanDevice::AssembleFastMessage(const CanHeader header, const byte *payload) {
	if (IsFastMessage(header) == TRUE) {
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
		// BUG BUG Log this so as to increase the number of FastMessages that may be received
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

	int totalDataLength; // will also include padding as we memcpy all of the frame, because I'm lazy
	totalDataLength = (unsigned int)data[1];
	totalDataLength += 7 - ((totalDataLength - 6) % 7);

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
// Subsequent messages of fast packet 
// data[0] Sequence Identifier (sid)
// data[1..7] 7 data bytes
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
		if ((droppedFrames > CONST_DROPPEDFRAME_THRESHOLD) && (wxDateTime::Now() < (droppedFrameTime + wxTimeSpan::Seconds(CONST_DROPPEDFRAME_PERIOD) ) ) ) {
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

// BUG BUG if this gets run in a separate thread, need to lock the fastMessages 
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
	std::vector<wxString> nmeaSentences;
	bool result = FALSE;

	// If we receive a frame from a device, then by definition it is still alive!
	networkMap[header.source].timestamp = wxDateTime::Now();
	
	switch (header.pgn) {
		
	case 59392: // ISO Ack
		// No need for us to do anything as we don't send any requests (yet)!
		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;
		
	case 59904: // ISO Request
		unsigned int requestedPGN;
		
		DecodePGN59904(payload, &requestedPGN);
		// What has been requested from us ?
		switch (requestedPGN) {
		
			case 60928: // Address Claim
				// BUG BUG The bastard's are using an address claim as a heartbeat !!
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendAddressClaim(networkAddress);
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Address Claim: %lu", returnCode);
					}
				}
				break;
		
			case 126464: // Supported PGN
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendSupportedPGN();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Supported PGN: %lu", returnCode);
					}
				}
				break;
		
			case 126993: // Heartbeat
				// BUG BUG I don't think sn ISO Request is allowed to request a heartbeat ??
				break;
		
			case 126996: // Product Information 
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendProductInformation();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Product Information: %lu", returnCode);
					}
				}
				break;
		
			default:
				// BUG BUG For other requested PG's send a NACK/Not supported
				break;
		}
		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;
		
	case 60928: // ISO Address Claim
		DecodePGN60928(payload, &deviceInformation);
		// if another device is not claiming our address, just log it
		if (header.source != networkAddress) {
			
			// Add the source address so that we can  construct a "map" of the NMEA2000 network
			deviceInformation.networkAddress = header.source;
			
			// BUG BUG Extraneous Noise Remove for production
			
#ifndef NDEBUG

			wxLogMessage(_T("TwoCan Network, Address: %d"), deviceInformation.networkAddress);
			wxLogMessage(_T("TwoCan Network, Manufacturer: %d"), deviceInformation.manufacturerId);
			wxLogMessage(_T("TwoCan Network, Unique ID: %lu"), deviceInformation.uniqueId);
			wxLogMessage(_T("TwoCan Network, Class: %d"), deviceInformation.deviceClass);
			wxLogMessage(_T("TwoCan Network, Function: %d"), deviceInformation.deviceFunction);
			wxLogMessage(_T("TwoCan Network, Industry %d"), deviceInformation.industryGroup);
			
#endif
		
			// Maintain the map of the NMEA 2000 network.
			// either this is a newly discovered device, or it is resending its address claim
			if ((networkMap[header.source].uniqueId == deviceInformation.uniqueId) || (networkMap[header.source].uniqueId == 0)) {
				networkMap[header.source].manufacturerId = deviceInformation.manufacturerId;
				networkMap[header.source].uniqueId = deviceInformation.uniqueId;
				networkMap[header.source].timestamp = wxDateTime::Now();
			}
			else {
				// or another device is claiming the address that an existing device had used, so clear out any product info entries
				networkMap[header.source].manufacturerId = deviceInformation.manufacturerId;
				networkMap[header.source].uniqueId = deviceInformation.uniqueId;
				networkMap[header.source].timestamp = wxDateTime::Now();
				networkMap[header.source].productInformation = {}; // I think this should initialize the product information struct;
			}
		}
		else {
			// Another device is claiming our address
			// If our NAME is less than theirs, reclaim our current address 
			if (deviceName < deviceInformation.deviceName) {
				int returnCode;
				returnCode = SendAddressClaim(networkAddress);
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("TwoCan Device, Reclaimed network address: %lu"), networkAddress);
				}
				else {
					wxLogMessage("TwoCan Device, Error reclaming network address %lu: %lu", networkAddress, returnCode);
				}
			}
			// Our uniqueId is larger (or equal), so increment our network address and see if we can claim the new address
			else {
				networkAddress += 1;
				if (networkAddress <= CONST_MAX_DEVICES) {
					int returnCode;
					returnCode = SendAddressClaim(networkAddress);
					if (returnCode == TWOCAN_RESULT_SUCCESS) {
						wxLogMessage(_T("TwoCan Device, Claimed network address: %lu"), networkAddress);
					}
					else {
						wxLogMessage("TwoCan Device, Error claiming network address %lu: %lu", networkAddress, returnCode);
					}
				}
				else {
					// BUG BUG More than 253 devices on the network, we send an unable to claim address frame (source address = 254)
					// Chuckles to self. What a nice DOS attack vector! Kick everyone else off the network!
					// I guess NMEA never thought anyone would hack a boat! What were they (not) thinking!
					wxLogError(_T("TwoCan Device, Unable to claim address, more than %d devices"), CONST_MAX_DEVICES);
					networkAddress = 0;
					int returnCode;
					returnCode = SendAddressClaim(CONST_NULL_ADDRESS);
					if (returnCode == TWOCAN_RESULT_SUCCESS) {
						wxLogMessage(_T("TwoCan Device, Claimed network address: %lu"), networkAddress);
					}
					else {
						wxLogMessage("TwoCan Device, Error claiming network address %lu: %lu", networkAddress, returnCode);
					}
				}
			}
		}
		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;
		
	case 65240: // ISO Commanded address
		// A device is commanding another device to use a specific address
		DecodePGN65240(payload, &deviceInformation);
		// If we are being commanded to use a specific address
		// BUG BUG Not sure if an ISO Commanded Address frame is broadcast or if header.destination == networkAddress
		if (deviceInformation.uniqueId == uniqueId) {
			// Update our network address to the commanded address and send an address claim
			networkAddress = deviceInformation.networkAddress;
			int returnCode;
			returnCode = SendAddressClaim(networkAddress);
			if (returnCode == TWOCAN_RESULT_SUCCESS) {
				wxLogMessage(_T("TwoCan Device, Claimed commanded network address: %lu"), networkAddress);
			}
			else {
				wxLogMessage("TwoCan Device, Error claiming commanded network address %lu: %lu", networkAddress, returnCode);
			}
		}
		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;
		
	case 126992: // System Time
		if (supportedPGN & FLAGS_ZDA) {
			result = DecodePGN126992(payload, &nmeaSentences);
		}
		break;
		
	case 126993: // Heartbeat
		DecodePGN126993(header.source, payload);
		// Update the matching entry in the network map
		// BUG BUG what happens if we are yet to have populated this entry with the device details ?? Probably nothing...
		networkMap[header.source].timestamp = wxDateTime::Now();
		result = FALSE;
		break;
		
	case 126996: // Product Information
		DecodePGN126996(payload, &productInformation);
		
		// BUG BUG Extraneous Noise
		
#ifndef NDEBUG
		wxLogMessage(_T("TwoCan Node, Network Address %d"), header.source);
		wxLogMessage(_T("TwoCan Node, DB Ver: %d"), productInformation.dataBaseVersion);
		wxLogMessage(_T("TwoCan Node, Product Code: %d"), productInformation.productCode);
		wxLogMessage(_T("TwoCan Node, Cert Level: %d"), productInformation.certificationLevel);
		wxLogMessage(_T("TwoCan Node, Load Level: %d"), productInformation.loadEquivalency);
		wxLogMessage(_T("TwoCan Node, Model ID: %s"), productInformation.modelId);
		wxLogMessage(_T("TwoCan Node, Model Version: %s"), productInformation.modelVersion);
		wxLogMessage(_T("TwoCan Node, Software Version: %s"), productInformation.softwareVersion);
		wxLogMessage(_T("TwoCan Node, Serial Number: %s"), productInformation.serialNumber);
#endif
		
		// Maintain the map of the NMEA 2000 network.
		networkMap[header.source].productInformation = productInformation;
		networkMap[header.source].timestamp = wxDateTime::Now();

		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;
		
	case 127250: // Heading
		if (supportedPGN & FLAGS_HDG) {
			result = DecodePGN127250(payload, &nmeaSentences);
		}
		break;
		
	case 127251: // Rate of Turn
		if (supportedPGN & FLAGS_ROT) {
			result = DecodePGN127251(payload, &nmeaSentences);
		}
		break;
		
	case 127258: // Magnetic Variation
		// BUG BUG needs flags 
		// BUG BUG Not actually used anywhere
		result = DecodePGN127258(payload, &nmeaSentences);
		break;
		
	case 128259: // Boat Speed
		if (supportedPGN & FLAGS_VHW) {
			result = DecodePGN128259(payload, &nmeaSentences);
		}
		break;
		
	case 128267: // Water Depth
		if (supportedPGN & FLAGS_DPT) {
			result = DecodePGN128267(payload, &nmeaSentences);
		}
		break;
		
	case 129025: // Position - Rapid Update
		if (supportedPGN & FLAGS_GLL) {
			result = DecodePGN129025(payload, &nmeaSentences);
		}
		break;
	
	case 129026: // COG, SOG - Rapid Update
		if (supportedPGN & FLAGS_VTG) {
			result = DecodePGN129026(payload, &nmeaSentences);
		}
		break;
	
	case 129029: // GNSS Position
		if (supportedPGN & FLAGS_GGA) {
			result = DecodePGN129029(payload, &nmeaSentences);
		}
		break;
	
	case 129033: // Time & Date
		if (supportedPGN & FLAGS_ZDA) {
			result = DecodePGN129033(payload, &nmeaSentences);
		}
		break;
		
	case 129038: // AIS Class A Position Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129038(payload, &nmeaSentences);
		}
		break;
	
	case 129039: // AIS Class B Position Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129039(payload, &nmeaSentences);
		}
		break;
	
	case 129040: // AIS Class B Extended Position Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129040(payload, &nmeaSentences);
		}
		break;
	
	case 129041: // AIS Aids To Navigation (AToN) Position Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129041(payload, &nmeaSentences);
		}
		break;
	
	case 129283: // Cross Track Error
		// BUG BUG Needs a flag ??
		result = DecodePGN129283(payload, &nmeaSentences);
		break;

	case 129793: // AIS Position and Date Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129793(payload, &nmeaSentences);
		}
		break;
	
	case 129794: // AIS Class A Static & Voyage Related Data
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129794(payload, &nmeaSentences);
		}
		break;
	
	case 129798: // AIS Search and Rescue (SAR) Position Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129798(payload, &nmeaSentences);
		}
		break;
	
	case 129808: // Digital Selective Calling (DSC)
		if (supportedPGN & FLAGS_DSC) {
			result = DecodePGN129808(payload, &nmeaSentences);
		}
		break;
	
	case 129809: // AIS Class B Static Data, Part A
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129809(payload, &nmeaSentences);
		}
		break;
	
	case 129810: // Class B Static Data, Part B
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129810(payload, &nmeaSentences);
		}
		break;
	
	case 130306: // Wind data
		if (supportedPGN & FLAGS_MWV) {
			result = DecodePGN130306(payload, &nmeaSentences);
		}
		break;
	
	case 130310: // Environmental Parameters
		if (supportedPGN & FLAGS_MWT) {
			result = DecodePGN130310(payload, &nmeaSentences);
		}
		break;
		
	case 130311: // Environmental Parameters (supercedes 130310)
		if (supportedPGN & FLAGS_MWT) {
			result = DecodePGN130311(payload, &nmeaSentences);
		}
		break;
	
	case 130312: // Temperature
		if (supportedPGN & FLAGS_MWT) {
			result = DecodePGN130312(payload, &nmeaSentences);
		}
		break;
		
	case 130316: // Temperature Extended Range
		if (supportedPGN & FLAGS_MWT) {
			result = DecodePGN130316(payload, &nmeaSentences);
		}
		break;
			
	default:
		// BUG BUG Should we log an unsupported PGN error ??
		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;
	}
	// Send each NMEA 0183 Sentence to OpenCPN
	if (result == TRUE) {
		for (std::vector<wxString>::iterator it = nmeaSentences.begin(); it != nmeaSentences.end(); ++it) {
			SendNMEASentence(*it);
		}
	}
}

// Decode PGN 59904 ISO Request
int TwoCanDevice::DecodePGN59904(const byte *payload, unsigned int *requestedPGN) {
	if (payload != NULL) {
		*requestedPGN = payload[0] | (payload[1] << 8) | (payload[2] << 16);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 60928 ISO Address Claim
int TwoCanDevice::DecodePGN60928(const byte *payload, DeviceInformation *deviceInformation) {
	if ((payload != NULL) && (deviceInformation != NULL)) {
		
		// Unique Identity Number 21 bits
		deviceInformation->uniqueId = (payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24)) & 0x1FFFFF;
		
		// Manufacturer Code 11 bits
		deviceInformation->manufacturerId = ((payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24)) & 0xFFE00000) >> 21;

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

		// NAME
		deviceInformation->deviceName = (unsigned long long)payload[0] | ((unsigned long long)payload[1] << 8) | ((unsigned long long)payload[2] << 16) | ((unsigned long long)payload[3] << 24) | ((unsigned long long)payload[4] << 32) | ((unsigned long long)payload[5] << 40) | ((unsigned long long)payload[6] << 48) | ((unsigned long long)payload[7] << 54);
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65240 ISO Commanded Address
int TwoCanDevice::DecodePGN65240(const byte *payload, DeviceInformation *deviceInformation) {
	if ((payload != NULL) && (deviceInformation != NULL)) {
		byte *tmpBuf;
		unsigned int tmp;

		// Similar to PGN 60928 - ISO Address Claim, but with a network address field appended

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
		
		// Commanded Network Address
		deviceInformation->networkAddress = payload[8];
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126992 NMEA System Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
bool TwoCanDevice::DecodePGN126992(const byte *payload, std::vector<wxString> *nmeaSentences) {
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
		nmeaSentences->push_back(wxString::Format("$IIZDA,%s", tm.Format("%H%M%S.00,%d,%m,%Y,%z")));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126993 NMEA Heartbeat
bool TwoCanDevice::DecodePGN126993(const int source, const byte *payload) {
	if (payload != NULL) {

		unsigned int timeOffset;
		timeOffset = payload[0] | (payload[1] << 8);
		
		unsigned short counter;
		counter = payload[2];

		unsigned short class1CanState;
		class1CanState = payload[3];

		unsigned short class2CanState;
		class2CanState = payload[4];
		
		unsigned short equipmentState;
		equipmentState = payload[5] & 0xF0;

		// BUG BUG Remove for production once this has been tested
#ifndef NDEBUG
		wxLogMessage(wxString::Format("TwoCan Heartbeat, Source: %d, Time: %d, Count: %d, CAN 1: %d, CAN 2: %d", source, timeOffset, counter, class1CanState, class2CanState));
#endif
		
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

		// Each of the following strings are up to 32 bytes long, and NOT NULL terminated.

		// Model ID Bytes [4] - [35]
		memset(&productInformation->modelId[0], '\0' ,32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[4 + j])) {
				productInformation->modelId[j] = payload[4 + j];
			}
		}

		// Software Version Bytes [36] - [67]
		memset(&productInformation->softwareVersion[0], '\0', 32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[36 + j])) {
				productInformation->softwareVersion[j] = payload[36 + j];
			}
		}

		// Model Version Bytes [68] - [99]
		memset(&productInformation->modelVersion[0], '\0', 32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[68 + j])) {
				productInformation->modelVersion[j] = payload[68 + j];
			}
		}

		// Serial Number Bytes [100] - [131]
		memset(&productInformation->serialNumber[0], '\0', 32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[100 + j])) {
				productInformation->serialNumber[j] = payload[100 + j];
			}
		}

		productInformation->certificationLevel = payload[132];
		productInformation->loadEquivalency = payload[133];

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127250 NMEA Vessel Heading
// $--HDG, x.x, x.x, a, x.x, a*hh<CR><LF>
// $--HDT,x.x,T*hh<CR><LF>
bool TwoCanDevice::DecodePGN127250(const byte *payload, std::vector<wxString> *nmeaSentences) {
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
		nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,%.2f,%c,%.2f,%c", RADIANS_TO_DEGREES((float)heading / 10000), \
			RADIANS_TO_DEGREES((float)deviation / 10000), deviation >= 0 ? 'E' : 'W', \
			RADIANS_TO_DEGREES((float)variation / 10000), variation >= 0 ? 'E' : 'W'));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127251 NMEA Rate of Turn (ROT)
// $--ROT,x.x,A*hh<CR><LF>
bool TwoCanDevice::DecodePGN127251(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		long rateOfTurn;
		rateOfTurn = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		// convert radians per second to degress per minute
		// -ve sign means turning to port

		nmeaSentences->push_back(wxString::Format("$IIROT,%.2f,A", RADIANS_TO_DEGREES((float)rateOfTurn / 600000)));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127258 NMEA Magnetic Variation
bool TwoCanDevice::DecodePGN127258(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte variationSource; //4 bits
		variationSource = payload[1] & 0x0F;

		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[2] | (payload[3] << 8);

		short variation;
		variation = payload[5] | (payload[6] << 8);

		variation = RADIANS_TO_DEGREES((float)variation / 10000);

		// BUG BUG Needs to be added to other sentences such as HDG and RMC conversions
		// As there is no direct NMEA 0183 sentence just for variation
		return FALSE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 128259 NMEA Speed & Heading
// $--VHW, x.x, T, x.x, M, x.x, N, x.x, K*hh<CR><LF>
bool TwoCanDevice::DecodePGN128259(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short speedWaterReferenced;
		speedWaterReferenced = payload[1] | (payload[2] << 8);

		unsigned short speedGroundReferenced;
		speedGroundReferenced = payload[3] | (payload[4] << 8);

		// BUG BUG Maintain heading globally from other sources to insert corresponding values into sentence	
		nmeaSentences->push_back(wxString::Format("$IIVHW,,T,,M,%.2f,N,%.2f,K", (float)speedWaterReferenced * CONVERT_MS_KNOTS / 100, \
			(float)speedWaterReferenced * CONVERT_MS_KMH / 100));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 128267 NMEA Depth
// $--DPT,x.x,x.x,x.x*hh<CR><LF>
// $--DBT,x.x,f,x.x,M,x.x,F*hh<CR><LF>
bool TwoCanDevice::DecodePGN128267(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short depth;
		depth = payload[1] | (payload[2] << 8);

		short offset;
		offset = payload[3] | (payload[4] << 8);

		unsigned short maxRange;
		maxRange = payload[5] | (payload[6] << 8);

		// return wxString::Format("$IIDPT,%.2f,%.2f,%.2f", (float)depth / 100, (float)offset / 100, \
		//	((maxRange != 0xFFFF) && (maxRange > 0)) ? maxRange / 100 : (int)NULL);

		// OpenCPN Dashboard only accepts DBT sentence
		nmeaSentences->push_back(wxString::Format("$IIDBT,%.2f,f,%.2f,M,%.2f,F", CONVERT_METRES_FEET * (double)depth / 100, \
			(double)depth / 100, CONVERT_METRES_FATHOMS * (double)depth / 100));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 128275 NMEA Distance Log
// $--VLW, x.x, N, x.x, N, x.x, N, x.x, N*hh<CR><LF>
//          |       |       |       Total cumulative water distance, Nm
//          |       |       Water distance since reset, Nm
//          |      Total cumulative ground distance, Nm
//          Ground distance since reset, Nm

bool TwoCanDevice::DecodePGN128275(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[1] | (payload[2] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		unsigned int cumulativeDistance;
		cumulativeDistance = payload[7] | (payload[8] << 8) | (payload[9] << 16) | (payload[10] << 24);

		unsigned int tripDistance;
		tripDistance = payload[11] | (payload[12] << 8) | (payload[13] << 16) | (payload[14] << 24);

		nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,%.2f,N,%.2f,N", CONVERT_METRES_NATICAL_MILES * tripDistance, CONVERT_METRES_NATICAL_MILES * cumulativeDistance));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129025 NMEA Position Rapid Update
// $--GLL, llll.ll, a, yyyyy.yy, a, hhmmss.ss, A, a*hh<CR><LF>
//                                           Status A valid, V invalid
//                                               mode - note Status = A if Mode is A (autonomous) or D (differential)
bool TwoCanDevice::DecodePGN129025(const byte *payload, std::vector<wxString> *nmeaSentences) {
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
		
		nmeaSentences->push_back(wxString::Format("$IIGLL,%02d%07.4f,%c,%03d%07.4f,%c,%s,%c,%c", abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', \
			abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', tm.Format("%H%M%S.00").ToAscii(), gpsMode, ((gpsMode == 'A') || (gpsMode == 'D')) ? 'A' : 'V'));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129026 NMEA COG SOG Rapid Update
// $--VTG,x.x,T,x.x,M,x.x,N,x.x,K,a*hh<CR><LF>
bool TwoCanDevice::DecodePGN129026(const byte *payload, std::vector<wxString> *nmeaSentences) {
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

		nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,%.2f,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
			RADIANS_TO_DEGREES((float)courseOverGround / 10000), (float)speedOverGround * CONVERT_MS_KNOTS / 100, \
			(float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
		return TRUE;
	}
	else {
		return FALSE;
	}	
}

// Decode PGN 129029 NMEA GNSS Position
// $--GGA, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x, xx, x.x, x.x, M, x.x, M, x.x, xxxx*hh<CR><LF>
//                                             |  |   hdop         geoidal  age refID 
//                                             |  |        Alt
//                                             | sats
//                                           fix Qualty

bool TwoCanDevice::DecodePGN129029(const byte *payload, std::vector<wxString> *nmeaSentences) {
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

		nmeaSentences->push_back(wxString::Format("$IIGGA,%s,%02d%07.4f,%c,%03d%07.4f,%c,%d,%d,%.2f,%.1f,M,%.1f,M,,", \
			tm.Format("%H%M%S").ToAscii() , abs(latitudeDegrees), fabs(latitudeMinutes), latitudeDegrees >= 0 ? 'N' : 'S', \
			abs(longitudeDegrees), fabs(longitudeMinutes), longitudeDegrees >= 0 ? 'E' : 'W', \
			fixType, numberOfSatellites, (double)hDOP * 0.01f, (double)altitude * 1e-6, \
			(double)geoidalSeparation * 0.01f));
		return TRUE;
		
		// BUG BUG for the time being ignore reference stations, too lazy to code this
		//, \
		//	((referenceStations != 0xFF) && (referenceStations > 0)) ? referenceStationAge : "", \
		//	((referenceStations != 0xFF) && (referenceStations > 0)) ? referenceStationID : "");
		//
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129033 NMEA Date & Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
bool TwoCanDevice::DecodePGN129033(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {
		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[0] | (payload[1] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		int localOffset;
		localOffset = payload[6] | (payload[7] << 8);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);
		
		nmeaSentences->push_back(wxString::Format("$IIZDA,%s,%d,%d", tm.Format("%H%M%S,%d,%m,%Y"), (int)localOffset / 60, localOffset % 60));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Template for NMEA183 AIS VDM messages
//!--VDM,x,x,x,a,s--s,x*hh
//       | | | |   |  Number of fill bits
//       | | | |   Encoded Message
//       | | | AIS Channel
//       | | Sequential Message ID
//       | Sentence Number
//      Total Number of sentences


// Decode PGN 129038 NMEA AIS Class A Position Report
// AIS Message Types 1,2 or 3
bool TwoCanDevice::DecodePGN129038(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(168);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		int speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int communicationState;
		communicationState = (payload[18] | (payload[19] << 8) | (payload[20] << 16) & 0x7FFFF);

		int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		int trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		double rateOfTurn;
		rateOfTurn = payload[23] | (payload[24] << 8);

		int navigationalStatus;
		navigationalStatus = payload[25] & 0x0F;

		int reserved;
		reserved = (payload[25] & 0xF0) >> 4;

		// BUG BUG No idea about the bitlengths for the following, just guessing

		int manoeuverIndicator;
		manoeuverIndicator = payload[26] & 0x03;

		int spare;
		spare = (payload[26] & 0x0C) >> 2;

		int reservedForRegionalApplications;
		reservedForRegionalApplications = (payload[26] & 0x30) >> 4;

		int sequenceID;
		sequenceID = (payload[26] & 0xC0) >> 6;

		// Encode correct AIS rate of turn from sensor data as per ITU M.1371 standard
		// BUG BUG fix this up to remove multiple calculations. 
		int AISRateOfTurn;

		// Undefined/not available
		if (rateOfTurn == 0xFFFF) {
			AISRateOfTurn = -128;
		}
		// Greater or less than 708 degrees/min
		else if ((RADIANS_TO_DEGREES(rateOfTurn * 3.125e-5) * 60) > 708) {
			AISRateOfTurn = 127;
		}

		else if ((RADIANS_TO_DEGREES(rateOfTurn * 3.125e-5) * 60) < -708) {
			AISRateOfTurn = -127;
		}

		else {
			AISRateOfTurn = 4.733 * sqrt(RADIANS_TO_DEGREES(rateOfTurn * 3.125e-5) * 60);
		}


		// Encode VDM message using 6 bit ASCII 

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 4, navigationalStatus);
		AISInsertInteger(binaryData, 42, 8, AISRateOfTurn);
		AISInsertInteger(binaryData, 50, 10, CONVERT_MS_KNOTS * speedOverGround * 0.1f);
		AISInsertInteger(binaryData, 60, 1, positionAccuracy);
		AISInsertInteger(binaryData, 61, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 89, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 116, 12, RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f);
		AISInsertInteger(binaryData, 128, 9, RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f);
		AISInsertInteger(binaryData, 137, 6, timeStamp);
		AISInsertInteger(binaryData, 143, 2, manoeuverIndicator);
		AISInsertInteger(binaryData, 145, 3, spare);
		AISInsertInteger(binaryData, 148, 1, raimFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);

		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,0", AISEncodePayload(binaryData)));

		return TRUE;
	}

	else {
		return FALSE;
	}
}

// Decode PGN 129039 NMEA AIS Class B Position Report
// AIS Message Type 18
bool TwoCanDevice::DecodePGN129039(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(168);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);
						
		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;
		
		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);
		
		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround =payload[14] | (payload[15] << 8);

		int  speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int communicationState;
		communicationState = payload[18] | (payload[19] << 8) | ((payload[20] & 0x7) << 16);

		int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		int trueHeading;
		trueHeading = RADIANS_TO_DEGREES((payload[21] | (payload[22] << 8)));

		int regionalReservedA;
		regionalReservedA = payload[23];

		int regionalReservedB;
		regionalReservedB = payload[24] & 0x03;

		int unitFlag;
		unitFlag = (payload[24] & 0x04) >> 2;

		int displayFlag;
		displayFlag = (payload[24] & 0x08) >> 3;

		int dscFlag;
		dscFlag = (payload[24] & 0x10) >> 4;

		int bandFlag;
		bandFlag = (payload[24] & 0x20) >> 5;

		int msg22Flag;
		msg22Flag = (payload[24] & 0x40) >> 6;

		int assignedModeFlag;
		assignedModeFlag = (payload[24] & 0x80) >> 7;
		
		int	sotdmaFlag;
		sotdmaFlag = payload[25] & 0x01;
		
		// Encode VDM Message using 6bit ASCII
				
		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 8, 0xFF); // spare
		AISInsertInteger(binaryData, 46, 10, CONVERT_MS_KNOTS * speedOverGround * 0.1f);
		AISInsertInteger(binaryData, 56, 1, positionAccuracy);
		AISInsertInteger(binaryData, 57, 28, longitudeMinutes);
		AISInsertInteger(binaryData, 85, 27, latitudeMinutes);
		AISInsertInteger(binaryData, 112, 12, RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f);
		AISInsertInteger(binaryData, 124, 9, RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f);
		AISInsertInteger(binaryData, 133, 6, timeStamp);
		AISInsertInteger(binaryData, 139, 2, regionalReservedB);
		AISInsertInteger(binaryData, 141, 1, unitFlag);
		AISInsertInteger(binaryData, 142, 1, displayFlag);
		AISInsertInteger(binaryData, 143, 1, dscFlag);
		AISInsertInteger(binaryData, 144, 1, bandFlag);
		AISInsertInteger(binaryData, 145, 1, msg22Flag);
		AISInsertInteger(binaryData, 146, 1, assignedModeFlag); 
		AISInsertInteger(binaryData, 147, 1, raimFlag); 
		AISInsertInteger(binaryData, 148, 1, sotdmaFlag); 
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}

	else {
		return FALSE;
	}
}

// Decode PGN 129040 AIS Class B Extended Position Report
// AIS Message Type 19
bool TwoCanDevice::DecodePGN129040(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(312);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		int speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int regionalReservedA;
		regionalReservedA = payload[18];

		int regionalReservedB;
		regionalReservedB = payload[19] & 0x0F;

		int reservedA;
		reservedA = (payload[19] & 0xF0) >> 4;

		int shipType;
		shipType = payload[20];

		int trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		int reservedB;
		reservedB = payload[23] & 0x0F;

		int gnssType;
		gnssType = (payload[23] & 0xF0) >> 4;

		int shipLength;
		shipLength = payload[24] | (payload[25] << 8);
		
		int shipBeam;
		shipBeam = payload[26] | (payload[27] << 8);

		int refStarboard;
		refStarboard = payload[28] | (payload[29] << 8);

		int refBow;
		refBow = payload[30] | (payload[31] << 8);
		
		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName.append(1,(char)payload[32 + i]);
		}
		
		int dteFlag;
		dteFlag = payload[52] & 0x01;

		int assignedModeFlag;
		assignedModeFlag = payload[52] & 0x02 >> 1;

		int spare;
		spare = (payload[52] & 0x0C) >> 2;

		int AisTransceiverInformation;
		AisTransceiverInformation = (payload[52] & 0xF0) >> 4;

		// Encode VDM Message using 6bit ASCII
			
		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 8, regionalReservedA);
		AISInsertInteger(binaryData, 46, 10, CONVERT_MS_KNOTS * speedOverGround * 0.1f);
		AISInsertInteger(binaryData, 56, 1, positionAccuracy);
		AISInsertInteger(binaryData, 57, 28, longitudeMinutes);
		AISInsertInteger(binaryData, 85, 27, latitudeMinutes);
		AISInsertInteger(binaryData, 112, 12, RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f);
		AISInsertInteger(binaryData, 124, 9, RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f);
		AISInsertInteger(binaryData, 133, 6, timeStamp);
		AISInsertInteger(binaryData, 139, 4, regionalReservedB);
		AISInsertString(binaryData, 143, 120, shipName);
		AISInsertInteger(binaryData, 263, 8, shipType);
		AISInsertInteger(binaryData, 271, 9, refBow);
		AISInsertInteger(binaryData, 280, 9, shipLength - refBow);
		AISInsertInteger(binaryData, 289, 6, refStarboard);
		AISInsertInteger(binaryData, 295, 6, shipBeam - refStarboard);
		AISInsertInteger(binaryData, 301, 4, gnssType);
		AISInsertInteger(binaryData, 305, 1, raimFlag);
		AISInsertInteger(binaryData, 306, 1, dteFlag);
		AISInsertInteger(binaryData, 307, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 308, 4, spare);

		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send the VDM message, Note no fillbits
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + (encodedVDMMessage.Length() % 28) >  0 ? 1 : 0;
		for (int i = 0; i < numberOfVDMMessages; i++) {
			nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28)));
		}
		
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
	}

	else {
		return FALSE;
	}
}

// Decode PGN 129041 AIS Aids To Navigation (AToN) Report
// AIS Message Type 21
bool TwoCanDevice::DecodePGN129041(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(358);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int shipLength;
		shipLength = payload[14] | (payload[15] << 8);

		int shipBeam;
		shipBeam = payload[16] | (payload[17] << 8);

		int refStarboard;
		refStarboard = payload[18] | (payload[19] << 8);

		int refBow;
		refBow = payload[20] | (payload[21] << 8);
		
		int AToNType;
		AToNType = (payload[22] & 0xF8) >> 3;

		int offPositionFlag;
		offPositionFlag = (payload[22]  & 0x04) >> 2;

		int virtualAToN;
		virtualAToN = (payload[22] & 0x02) >> 1;;

		int assignedModeFlag;
		assignedModeFlag = payload[22] & 0x01;

		int spare;
		spare = payload[23] & 0x01;

		int gnssType;
		gnssType = (payload[23] & 0x1E) >> 1;

		int reserved;
		reserved = payload[23] & 0xE0 >> 5;

		int AToNStatus;
		AToNStatus = payload[24];

		int transceiverInformation;
		transceiverInformation = (payload[25] & 0xF8) >> 3;

		int reservedB;
		reservedB = payload[25] & 0x07;

		// BUG BUG This is variable up to 20 + 14 (34) characters
		std::string AToNName;
		int AToNNameLength = payload[26];
		if (payload[27] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			for (int i = 0; i < AToNNameLength - 1; i++) {
				AToNName.append(1, (char)payload[28 + i]);
			}
		} 
								
		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 5, AToNType);
		AISInsertString(binaryData, 43, 120, AToNNameLength <= 20 ? AToNName : AToNName.substr(0,20));
		AISInsertInteger(binaryData, 163, 1, positionAccuracy);
		AISInsertInteger(binaryData, 164, 28, longitudeMinutes);
		AISInsertInteger(binaryData, 192, 27, latitudeMinutes);
		AISInsertInteger(binaryData, 219, 9, refBow);
		AISInsertInteger(binaryData, 228, 9, shipLength - refBow);
		AISInsertInteger(binaryData, 237, 6, refStarboard);
		AISInsertInteger(binaryData, 243, 6, shipBeam - refStarboard);
		AISInsertInteger(binaryData, 249, 4, gnssType);
		AISInsertInteger(binaryData, 253, 6, timeStamp);
		AISInsertInteger(binaryData, 259, 1, offPositionFlag);
		AISInsertInteger(binaryData, 260, 8, AToNStatus);
		AISInsertInteger(binaryData, 268, 1, raimFlag);
		AISInsertInteger(binaryData, 269, 1, virtualAToN);
		AISInsertInteger(binaryData, 270, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 271, 1, spare);
		// Why is this called a spare (not padding) when in actual fact 
		// it functions as padding, Refer to the ITU Standard ITU-R M.1371-4 for clarification
		int fillBits = 0;
		if (AToNName.length() > 20) {
			// Add the AToN's name extension characters if necessary
			// BUG BUG Should check that shipName.length is not greater than 34
			AISInsertString(binaryData, 272, (AToNName.length() - 20) * 6,AToNName.substr(20,AToNName.length() - 20));
			fillBits = (272 + ((AToNName.length() - 20) * 6)) % 6;
			// Add padding to align on 6 bit boundary
			if (fillBits > 0) {
				AISInsertInteger(binaryData, 272 + (AToNName.length() - 20) * 6, fillBits, 0);
			}
		}
		else {
			// Add padding to align on 6 bit boundary
			fillBits = 272 % 6;
			if (fillBits > 0) {
				AISInsertInteger(binaryData, 272, fillBits, 0);
			}
		}
		
		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send the VDM message
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + (encodedVDMMessage.Length() % 28) >  0 ? 1 : 0;
		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages - 1) { // Is this the last message, if so set fillbits as appropriate
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28), fillBits));
			}
			else {
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28)));
			}
		}
		
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129283 NMEA Cross Track Error
// $--XTE, A, A, x.x, a, N, a*hh<CR><LF>
bool TwoCanDevice::DecodePGN129283(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short xteMode;
		xteMode = payload[1] & 0x0F;

		unsigned short navigationTerminated;
		navigationTerminated = payload[1] & 0xC0;

		int crossTrackError;
		crossTrackError = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		nmeaSentences->push_back(wxString::Format("$IIXTE,A,A,%.2f,%c,N", fabsf(CONVERT_METRES_NATICAL_MILES * crossTrackError * 0.01f), crossTrackError < 0 ? 'L' : 'R'));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129284 Navigation Data
//$--BWC, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, T, x.x, M, x.x, N, c--c, a*hh<CR><LF>
//$--BWR, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, T, x.x, M, x.x, N, c--c, a*hh<CR><LF>
//$--BOD, x.x, T, x.x, M, c--c, c--c*hh<CR><LF>
//$--WCV, x.x, N, c--c, a*hh<CR><LF>

// Not sure of this use case, as it implies there is already a chartplotter on board
bool TwoCanDevice::DecodePGN129284(const byte * payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short distance;
		distance = payload[1] | (payload[2] << 8);

		byte bearingRef; // Magnetic or True
		bearingRef = payload[3] & 0xC0;

		byte perpendicularCrossed; // Yes or No
		perpendicularCrossed = payload[3] & 0x30;

		byte circleEntered; // Yes or No
		circleEntered = payload[3] & 0x0C;

		byte calculationType; //Great Circle or Rhumb Line
		calculationType = payload[3] & 0x3C;

		int secondsSinceMidnight;
		secondsSinceMidnight = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);

		int daysSinceEpoch;
		daysSinceEpoch = payload[8] | (payload[9] << 8);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		unsigned short bearingOrigin;
		bearingOrigin = (payload[10] | (payload[11] << 8)) * 0.001;

		unsigned short bearingPosition;
		bearingPosition = (payload[12] | (payload[13] << 8)) * 0.001;

		int originWaypointId;
		originWaypointId = payload[14] | (payload[15] << 8) | (payload[16] << 16) | (payload[17] << 24);

		int destinationWaypointId;
		destinationWaypointId = payload[18] | (payload[19] << 8) | (payload[20] << 16) | (payload[21] << 24);

		double latitude;
		latitude = ((payload[22] | (payload[23] << 8) | (payload[24] << 16) | (payload[25] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = (latitude - latitudeDegrees) * 60;

		double longitude;
		longitude = ((payload[26] | (payload[27] << 8) | (payload[28] << 16) | (payload[29] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = (longitude - longitudeDegrees) * 60;

		int waypointClosingVelocity;
		waypointClosingVelocity = (payload[30] | (payload[31] << 8)) * 0.01;

		wxDateTime timeNow;
		timeNow = wxDateTime::Now();

		if (calculationType == GREAT_CIRCLE) { 
			if (bearingRef == HEADING_TRUE) {
				nmeaSentences->push_back(wxString::Format("$IIBWC,%s,%02d%05.2f,%c,%03d%05.2f,%c,%.2f,T,,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"), 
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', 
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', 
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), 
					CONVERT_METRES_NATICAL_MILES * distance, destinationWaypointId));
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIBWC,%s,%02d%05.2f,%c,%03d%05.2f,%c,,T,%.2f,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"), 
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', 
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', 
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), \
					CONVERT_METRES_NATICAL_MILES * distance, destinationWaypointId));
			}

		}
		else { 
			if (bearingRef == HEADING_TRUE) {
				nmeaSentences->push_back(wxString::Format("$IIBWR,%s,%02d%05.2f,%c,%03d%05.2f,%c,%.2f,T,,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"),
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), \
					CONVERT_METRES_NATICAL_MILES * distance, destinationWaypointId));
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIBWR,%s,%02d%05.2f,%c,%03d%05.2f,%c,,T,%.2f,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"),
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), \
					CONVERT_METRES_NATICAL_MILES * distance, destinationWaypointId));
			}
		}

	
		if (bearingRef == HEADING_TRUE) {
			nmeaSentences->push_back(wxString::Format("$IIBOD,%.2f,T,,M,%d,%d", 
				RADIANS_TO_DEGREES((float)bearingOrigin),destinationWaypointId, originWaypointId));
		}
		else {
			nmeaSentences->push_back(wxString::Format("$IIBOD,,T,%.2f,M,%d,%d", 
				RADIANS_TO_DEGREES((float)bearingOrigin),  destinationWaypointId, originWaypointId));
		}

		nmeaSentences->push_back(wxString::Format("$IIWCV,%.2f,N,%d,A",CONVERT_MS_KNOTS * waypointClosingVelocity, destinationWaypointId));

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129285 Route and Waypoint Information
// $--RTE,x.x,x.x,a,c--c,c--c, ..……... c--c*hh<CR><LF>
// and 
// //$--WPL,llll.ll,a,yyyyy.yy,a,c--c
bool TwoCanDevice::DecodePGN129285(const byte * payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		unsigned int rps;
		rps = payload[0] | (payload[1] << 8);

		unsigned int nItems;
		nItems = payload[2] | (payload[3] << 8);

		unsigned int databaseVersion;
		databaseVersion = payload[4] | (payload[5] << 8);

		unsigned int routeID;
		routeID = payload[6] | (payload[7] << 8);

		unsigned short direction; // I presume forward/reverse
		direction = payload[8] & 0xC0; 

		unsigned short supplementaryInfo;
		supplementaryInfo = payload[8] & 0x30;

		// NMEA reserved
		// unsigned short reservedA = payload[8} & 0x0F;

		std::string routeName;
		// BUG BUG If this is null terminated, just use strcpy
		for (int i = 0; i < 255; i++) {
			if (isprint(payload[9 + i])) {
				routeName.append(1, payload[9 + i]);
			}
		}

		// NMEA reserved payload[264]
		//unsigned int reservedB = payload[264];

		// repeated fields
		for (unsigned int i = 0; i < nItems; i++) {
			int waypointID;
			waypointID = payload[265 + (i * 265)] | (payload[265 + (i * 265) + 1] << 8);

			std::string waypointName;
			for (int j = 0; j < 255; j++) {
				if (isprint(payload[265 + (i * 265) + 266 + j])) {
					waypointName.append(1, payload[265 + (i * 265) + 266 + j]);
				}
			}
		
			double latitude = payload[265 + (i * 265) + 257] | (payload[265 + (i * 265) + 258] << 8) | (payload[265 + (i * 265) + 259] << 16) | (payload[265 + (i * 265) + 260] << 24);
			int latitudeDegrees = (int)latitude;
			double latitudeMinutes = (latitude - latitudeDegrees) * 60;

			double longitude = payload[265 + (i * 265) + 261] | (payload[265 + (i * 265) + 262] << 8) | (payload[265 + (i * 265) + 263] << 16) | (payload[265 + (i * 265) + 264] << 24);
			int longitudeDegrees = (int)longitude;
			double longitudeMinutes = (longitude - longitudeDegrees) * 60;

			nmeaSentences->push_back(wxString::Format("$IIWPL,%02d%05.2f,%c,%03d%05.2f,%c,%s",
				abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
				abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
				waypointName.c_str()));
		}
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129793 AIS Date and Time report
// AIS Message Type 4 and if date is present also Message Type 11
bool TwoCanDevice::DecodePGN129793(const byte * payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(168);

		// Should really check whether this is 4 (Base Station) or 
		// 11 (mobile station, but only in response to a request using message 10)
		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);
		
		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int reservedA;
		reservedA = (payload[13] & 0xFC) >> 2;

		int secondsSinceMidnight;
		secondsSinceMidnight = payload[14] | (payload[15] << 8) | (payload[16] << 16) | (payload[17] << 24);

		int communicationState;
		communicationState = payload[18] | (payload[19] << 8) | ((payload[20] & 0x7) << 16);

		int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		int daysSinceEpoch;
		daysSinceEpoch = payload[21] | (payload[21] << 8);

		int reservedB;
		reservedB = payload[22] & 0x0F;

		int gnssType;
		gnssType = (payload[22] & 0xF0) >> 4;

		int spare;
		spare = payload[23];

		int longRangeFlag = 0;

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		// Encode VDM message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 14, tm.GetYear());
		AISInsertInteger(binaryData, 52, 4, tm.GetMonth());
		AISInsertInteger(binaryData, 56, 5, tm.GetDay());
		AISInsertInteger(binaryData, 61, 5, tm.GetHour());
		AISInsertInteger(binaryData, 66, 6, tm.GetMinute());
		AISInsertInteger(binaryData, 72, 6, tm.GetSecond());
		AISInsertInteger(binaryData, 78, 1, positionAccuracy);
		AISInsertInteger(binaryData, 79, 28, longitudeMinutes);
		AISInsertInteger(binaryData, 107, 27, latitudeMinutes);
		AISInsertInteger(binaryData, 134, 4, gnssType);
		AISInsertInteger(binaryData, 138, 1, longRangeFlag); // Long Range flag doesn't appear to be set anywhere
		AISInsertInteger(binaryData, 139, 9, spare);
		AISInsertInteger(binaryData, 148, 1, raimFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 129794 NMEA AIS Class A Static and Voyage Related Data
// AIS Message Type 5
bool TwoCanDevice::DecodePGN129794(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(1024);
	
		unsigned int messageID;
		messageID = payload[0] & 0x3F;

		unsigned int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		unsigned int imoNumber;
		imoNumber = payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24);

		std::string callSign;
		for (int i = 0; i < 7; i++) {
			callSign.append(1, (char)payload[9 + i]);
		}

		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName.append(1, (char)payload[16 + i]);
		}

		unsigned int shipType;
		shipType = payload[36];

		unsigned int shipLength;
		shipLength = payload[37] | payload[38] << 8;

		unsigned int shipBeam;
		shipBeam = payload[39] | payload[40] << 8;

		unsigned int refStarboard;
		refStarboard = payload[41] | payload[42] << 8;

		unsigned int refBow;
		refBow = payload[43] | payload[44] << 8;

		// BUG BUG Just guessing that this is correct !!
		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[45] | (payload[46] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[47] | (payload[48] << 8) | (payload[49] << 16) | (payload[50] << 24);

		wxDateTime eta;
		eta.ParseDateTime("00:00:00 01-01-1970");
		eta += wxDateSpan::Days(daysSinceEpoch);
		eta += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		unsigned int draft;
		draft = payload[51] | payload[52] << 8;

		std::string destination;
		for (int i = 0; i < 20; i++) {
			destination.append(1, (char)payload[53 + i]);
		}
		
		// BUG BUG These could be back to front
		unsigned int aisVersion;
		aisVersion = (payload[73] & 0xC0) >> 6;

		unsigned int gnssType;
		gnssType = (payload[73] & 0x3C) >> 2;

		unsigned int dteFlag;
		dteFlag = (payload[73] & 0x02) >> 1;

		unsigned int transceiverInformation;
		transceiverInformation = payload[74] & 0x1F;

		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, aisVersion );
		AISInsertInteger(binaryData, 40, 30, imoNumber);
		AISInsertString(binaryData, 70, 42, callSign);
		AISInsertString(binaryData, 112, 120, shipName);
		AISInsertInteger(binaryData, 232, 8, shipType);
		AISInsertInteger(binaryData, 240, 9, refBow);
		AISInsertInteger(binaryData, 249, 9, shipLength - refBow);
		AISInsertInteger(binaryData, 258, 6, shipBeam - refStarboard);
		AISInsertInteger(binaryData, 264, 6, refStarboard);
		AISInsertInteger(binaryData, 270, 4, gnssType);
		AISInsertString(binaryData, 274, 20, eta.Format("%d%m%Y").ToStdString());
		AISInsertInteger(binaryData, 294, 8, draft);
		AISInsertString(binaryData, 302, 120, destination);
		AISInsertInteger(binaryData, 422, 1, dteFlag);
		AISInsertInteger(binaryData, 423, 1, 0xFF); //spare
		
		// Add padding to align on 6 bit boundary
		int fillBits = 0;
		fillBits = 424 % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 424, fillBits, 0);
		}
		
		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send the VDM message
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + (encodedVDMMessage.Length() % 28) >  0 ? 1 : 0;
		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages - 1) { // Is this the last message, if so set fillbits as appropriate
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,%d", numberOfVDMMessages, i,AISsequentialMessageId ,encodedVDMMessage.SubString(i * 28, 28),fillBits));
			}
			else {
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId,encodedVDMMessage.SubString(i * 28, 28)));
			}
		}

		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
	}
	else {
		return FALSE;
	}
}

//	Decode PGN 129798 AIS SAR Aircraft Position Report
// AIS Message Type 9
bool TwoCanDevice::DecodePGN129798(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(168);
		
		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		int speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int communicationState;
		communicationState = (payload[18] | (payload[19] << 8) | (payload[20] << 16)) & 0x7FFFF;

		int transceiverInformation; 
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		double altitude;
		altitude = 1e-6 * (((long long)payload[21] | ((long long)payload[22] << 8) | ((long long)payload[23] << 16) | ((long long)payload[24] << 24) \
			| ((long long)payload[25] << 32) | ((long long)payload[26] << 40) | ((long long)payload[27] << 48) | ((long long)payload[28] << 56)));
		
		int reservedForRegionalApplications;
		reservedForRegionalApplications = payload[29];

		int dteFlag; 
		dteFlag = payload[30] & 0x01;

		// BUG BUG Just guessing these to match NMEA2000 payload with ITU AIS fields

		int assignedModeFlag;
		assignedModeFlag = (payload[30] & 0x02) >> 1;

		int sotdmaFlag;
		sotdmaFlag = (payload[30] & 0x04) >> 2;

		int altitudeSensor;
		altitudeSensor = (payload[30] & 0x08) >> 3;

		int spare;
		spare = (payload[30] & 0xF0) >> 4;

		int reserved;
		reserved = payload[31];
		
		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 12, altitude);
		AISInsertInteger(binaryData, 50, 10, speedOverGround);
		AISInsertInteger(binaryData, 60, 1, positionAccuracy);
		AISInsertInteger(binaryData, 61, 28, longitudeMinutes);
		AISInsertInteger(binaryData, 89, 27, latitudeMinutes);
		AISInsertInteger(binaryData, 116, 12, courseOverGround);
		AISInsertInteger(binaryData, 128, 6, timeStamp);
		AISInsertInteger(binaryData, 134, 8, reservedForRegionalApplications); // 1 bit altitide sensor
		AISInsertInteger(binaryData, 142, 1, dteFlag);
		AISInsertInteger(binaryData, 143, 3, spare);
		// BUG BUG just guessing
		AISInsertInteger(binaryData, 146, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 147, 1, raimFlag);
		AISInsertInteger(binaryData, 148, 1, sotdmaFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}
	
//	Decode PGN 129801 AIS Addressed Safety Related Message
// AIS Message Type 12
bool TwoCanDevice::DecodePGN129801(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(1008);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int sourceID;
		sourceID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		int reservedA;
		reservedA = payload[4] & 0x01;

		int transceiverInfo;
		transceiverInfo = (payload[5] & 0x3E) >> 1;

		int sequenceNumber;
		sequenceNumber = (payload[5] & 0xC0) >> 6;

		int destinationId;
		destinationId = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		int reservedB;
		reservedB = payload[10] & 0x3F;

		int retransmitFlag;
		retransmitFlag = (payload[10] & 0x40) >> 6;

		int reservedC;
		reservedC = (payload[10] & 0x80) >> 7;

		std::string safetyMessage;
		for (int i = 0; i < 156; i++) {
			safetyMessage.append(1, (char)payload[11 + i]);
		}
		// BUG BUG Not sure if ths is encoded same as Addressed Safety Message
		//std::string safetyMessage;
		//int safetyMessageLength = payload[6];
		//if (payload[7] == 1) {
			// first byte of safetmessage indicates encoding; 0 for Unicode, 1 for ASCII
			//for (int i = 0; i < safetyMessageLength - 2; i++) {
			//	safetyMessage += (static_cast<char>(payload[8 + i]));
			//}
		//}

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, sourceID);
		AISInsertInteger(binaryData, 38, 2, sequenceNumber);
		AISInsertInteger(binaryData, 40, 30, destinationId);
		AISInsertInteger(binaryData, 70, 1, retransmitFlag);
		AISInsertInteger(binaryData, 71, 1, 0); // unused spare
		AISInsertString(binaryData, 72, 936, safetyMessage);

		// BUG BUG Calculate fill bits correcty as safetyMessage is variable in length

		int fillBits = 0;
		fillBits = 1008 % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 968, fillBits, 0);
		}

		wxString encodedVDMMessage = AISEncodePayload(binaryData);

		// Send the VDM message
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + (encodedVDMMessage.Length() % 28) >  0 ? 1 : 0;
		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages - 1) { // Is this the last message, if so set fillbits as appropriate
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28), fillBits));
			}
			else {
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28)));
			}
		}

		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
		
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129802 AIS Safety Related Broadcast Message 
// AIS Message Type 14
bool TwoCanDevice::DecodePGN129802(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(1008);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int sourceID;
		sourceID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | ((payload[4] & 0x3F) << 24);

		int reservedA;
		reservedA = (payload[4] & 0xC0) >> 6;

		int transceiverInfo;
		transceiverInfo = payload[5] & 0x1F;

		int reservedB;
		reservedB = (payload[5] & 0xE0) >> 5;

		std::string safetyMessage;
		int safetyMessageLength = payload[6];
		if (payload[7] == 1) { 
			// first byte of safetmessage indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < safetyMessageLength - 2; i++) {
				safetyMessage += (static_cast<char>(payload[8 + i]));
			}
		}

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, sourceID);
		AISInsertInteger(binaryData, 38, 2, 0); //spare
		int l = safetyMessage.size();
		// Remember 6 bits per character
		AISInsertString(binaryData, 40, l * 6, safetyMessage.c_str());

		// Calculate fill bits as safetyMessage is variable in length
		// According to ITU, maximum length of safetyMessage is 966 6bit characters
		int fillBits = (40 + (l * 6)) % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 40 + (l * 6), fillBits, 0);
		}

		// BUG BUG Should check whether the binary message is smaller than 1008 btes otherwise
		// we just need a substring from the binaryData
		std::vector<bool>::const_iterator first = binaryData.begin();
		std::vector<bool>::const_iterator last = binaryData.begin() + 40 + (l * 6) + fillBits;
		std::vector<bool> newVec(first, last);

		// Encode the VDM Message using 6bit ASCII
		wxString encodedVDMMessage = AISEncodePayload(newVec);

		// Send the VDM message, use 28 characters as an arbitary number for multiple NMEA 183 sentences
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + (encodedVDMMessage.Length() % 28) >  0 ? 1 : 0;
		if (numberOfVDMMessages == 1) {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,%d", encodedVDMMessage, fillBits));
		}
		else {
			for (int i = 0; i < numberOfVDMMessages; i++) {
				if (i == numberOfVDMMessages - 1) { // Is this the last message, if so append number of fillbits as appropriate
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,A,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28), fillBits));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,A,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.SubString(i * 28, 28)));
				}
			}
		}

		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 129808 NMEA DSC Call
// A mega confusing combination !!
// $--DSC, xx,xxxxxxxxxx,xx,xx,xx,x.x,x.x,xxxxxxxxxx,xx,a,a
//          |     |       |  |  |  |   |  MMSI        | | Expansion Specifier
//          |   MMSI     Category  Position           | Acknowledgement        
//          Format Specifer  |  |      |Time          Nature of Distress
//                           |  Type of Communication or Second telecommand
//                           Nature of Distress or First Telecommand

// and
// $--DSE

bool TwoCanDevice::DecodePGN129808(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte formatSpecifier;
		formatSpecifier = payload[0];

		byte dscCategory;
		dscCategory = payload[1];

		char mmsiAddress[5];
		sprintf(mmsiAddress, "%02d%02d%02d%02d%02d", payload[2], payload[3], payload[4], payload[5], payload[6]);

		byte firstTeleCommand; // or Nature of Distress
		firstTeleCommand = payload[7];

		byte secondTeleCommand; // or Communication Mode
		secondTeleCommand = payload[8];

		char receiveFrequency;
		receiveFrequency = payload[9]; // Encoded of 9, 10, 11, 12, 13, 14

		char transmitFrequency;
		transmitFrequency = payload[15]; // Encoded of 15, 16, 17, 18, 19, 20

		char telephoneNumber;
		telephoneNumber = payload[21]; // encoded over 8 or 16 bytes

		int index = 0;

		double latitude;
		latitude = ((payload[index + 1] | (payload[index + 2] << 8) | (payload[index + 3] << 16) | (payload[index + 4] << 24))) * 1e-7;

		index += 4;

		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = (latitude - latitudeDegrees) * 60;

		double longitude;
		longitude = ((payload[index + 1] | (payload[index + 2] << 8) | (payload[index + 3] << 16) | (payload[index + 4] << 24))) * 1e-7;

		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = (longitude - longitudeDegrees) * 60;

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		// note payload index.....
		char vesselInDistress[5];
		sprintf(vesselInDistress, "%02d%02d%02d%02d%02d", payload[2], payload[3], payload[4], payload[5], payload[6]);

		byte endOfSequence;
		endOfSequence = payload[101]; // 1 byte

		byte dscExpansionEnabled; // Encoded over two bits
		dscExpansionEnabled = (payload[102] & 0xC0) >> 6;

		byte reserved; // 6 bits
		reserved = payload[102] & 0x3F;

		byte callingRx; // 6 bytes
		callingRx = payload[103];

		byte callingTx; // 6 bytes
		callingTx = payload[104];

		unsigned int timeOfTransmission;
		timeOfTransmission = payload[105] | (payload[106] << 8) | (payload[107] << 16) | (payload[108] << 24);

		unsigned int dayOfTransmission;
		dayOfTransmission = payload[109] | (payload[110] << 8);

		unsigned int messageId;
		messageId = payload[111] | (payload[112] << 8);

		// The following pairs are repeated

		byte dscExpansionSymbol;
		dscExpansionSymbol = payload[113];

		// Now iterate through the DSE Expansion data

		for (size_t i = 120; i < sizeof(payload);) {
			switch (payload[i]) {
				// refer to ITU-R M.821 Table 1.
			case 100: // enhanced position
				// 4 characters (8 digits)
				i += 4;
				break;
			case 101: // Source and datum of position
				i += 9;
				break;
			case 102: // Current speed of the vessel - 4 bytes
				i += 4;
				break;
			case 103: // Current course of the vessel - 4 bytes
				i += 4;
				break;
			case 104: // Additional Station information - 10
				i += 10;
				break;
			case 105: // Enhanced Geographic Area - 12
				i += 12;
				break;
			case 106: // Numbr of persons onboard - 2 characters
				i += 2;
				break;

			}
		}
		
		return FALSE;
	}
	else {
		return FALSE;
	}

}

// Decode PGN 129809 AIS Class B Static Data Report, Part A 
// AIS Message Type 24, Part A
bool TwoCanDevice::DecodePGN129809(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {
		
		std::vector<bool> binaryData(164);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName.append(1, (char)payload[5 + i]);
		}

		// Encode VDM Message using 6 bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, 0x0); // Part A = 0
		AISInsertString(binaryData, 40, 120, shipName);
		
		// Add padding to align on 6 bit boundary
		int fillBits = 0;
		fillBits = 160 % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 160, fillBits, 0);
		}
		
		// Send a single VDM sentence, note no sequential message Id		
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,%d", AISEncodePayload(binaryData), fillBits));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129810 AIS Class B Static Data Report, Part B 
// AIS Message Type 24, Part B
bool TwoCanDevice::DecodePGN129810(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(168);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		int shipType;
		shipType = payload[5];

		std::string vendorId;
		for (int i = 0; i < 7; i++) {
			vendorId.append(1, (char)payload[6 + i]);
		}

		std::string callSign;
		for (int i = 0; i < 7; i++) {
			callSign.append(1, (char)payload[12 + i]);
		}
		
		unsigned int shipLength;
		shipLength = payload[19] | payload[20] << 8;

		unsigned int shipBeam;
		shipBeam = payload[21] | payload[22] << 8;

		unsigned int refStarboard;
		refStarboard = payload[23] | payload[24] << 8;

		unsigned int refBow;
		refBow = payload[25] | payload[26] << 8;

		unsigned int motherShipID; // aka mother ship MMSI
		motherShipID = payload[27] | (payload[28] << 8) | (payload[29] << 16) | (payload[30] << 24);

		int reserved;
		reserved = (payload[31] & 0x03);

		int spare;
		spare = (payload[31] & 0xFC) >> 2;

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, 0x01); // Part B = 1
		AISInsertInteger(binaryData, 40, 8, shipType);
		AISInsertString(binaryData, 48, 42, vendorId);
		AISInsertString(binaryData, 90, 42, callSign);
		AISInsertInteger(binaryData, 132, 9, refBow);
		AISInsertInteger(binaryData, 141, 9, shipLength - refBow);
		AISInsertInteger(binaryData, 150, 6, shipBeam - refStarboard);
		AISInsertInteger(binaryData, 156, 6, refStarboard);
		AISInsertInteger(binaryData, 162 ,6 , 0); //spare
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130306 NMEA Wind
// $--MWV,x.x,a,x.x,a,A*hh<CR><LF>
bool TwoCanDevice::DecodePGN130306(const byte *payload, std::vector<wxString> *nmeaSentences) {
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

			nmeaSentences->push_back(wxString::Format("$IIMWV,%.2f,%c,%.2f,N,A", windAngle, \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T', (double)windSpeed * CONVERT_MS_KNOTS / 100));
			return TRUE;

		} 
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130310 NMEA Water & Air Temperature and Pressure
// $--MTW,x.x,C*hh<CR><LF>
bool TwoCanDevice::DecodePGN130310(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned int waterTemperature;
		waterTemperature = payload[1] | (payload[2] << 8);

		unsigned int airTemperature;
		airTemperature = payload[3] | (payload[4] << 8);

		unsigned int airPressure;
		airPressure = payload[5] | (payload[6] << 8);

		nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", (float)(waterTemperature * 0.01f) + CONST_KELVIN));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130311 NMEA Environment  (supercedes 130311)
// $--MTW,x.x,C*hh<CR><LF>
bool TwoCanDevice::DecodePGN130311(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte temperatureSource;
		temperatureSource = payload[1] & 0x3F;
		
		byte humiditySource;
		humiditySource = (payload[1] & 0xC0) >> 6;
		
		int temperature;
		temperature = payload[2] | (payload[3] << 8);
			
		int humidity;
		humidity = payload[4] | (payload[5] << 8);
		//	Resolution 0.004
			
		int pressure;
		pressure = payload[6] | (payload[7] << 8);
		
		if (temperatureSource == TEMPERATURE_SEA) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", (float)(temperature * 0.01f) + CONST_KELVIN));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}


// Decode PGN 130312 NMEA Temperature
// $--MTW,x.x,C*hh<CR><LF>
bool TwoCanDevice::DecodePGN130312(const byte *payload, std::vector<wxString> *nmeaSentences) {
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

			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", (float)(actualTemperature * 0.01f) + CONST_KELVIN));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130316 NMEA Temperature Extended Range
// $--MTW,x.x,C*hh<CR><LF>
bool TwoCanDevice::DecodePGN130316(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte instance;
		instance = payload[1];

		byte source;
		source = payload[2];

		unsigned int actualTemperature;
		actualTemperature = payload[3] | (payload[4] << 8) | (payload[5] << 16);

		unsigned int setTemperature;
		setTemperature = payload[6] | (payload[7] << 8);

		if (source == TEMPERATURE_SEA) {

			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", (float)(actualTemperature * 0.001f) + CONST_KELVIN));
			return TRUE; 
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}


// Decode PGN 130577 NMEA Direction Data
// BUG BUG Work out what to convert this to
bool DecodePGN130577(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		// 0 - Autonomous, 1 - Differential enhanced, 2 - Estimated, 3 - Simulated, 4 - Manual
		byte dataMode;
		dataMode = payload[0] & 0x0F;

		// True = 0, Magnetic = 1
		byte cogReference;
		cogReference = (payload[0] & 0x30);

		byte sid;
		sid = payload[1];

		unsigned int courseOverGround;
		courseOverGround = (payload[2] | (payload[3] << 8));

		unsigned int speedOverGround;
		speedOverGround = (payload[4] | (payload[5] << 8));

		unsigned int heading;
		heading = (payload[6] | (payload[7] << 8));

		unsigned int speedThroughWater;
		speedThroughWater = (payload[8] | (payload[9] << 8));

		unsigned int set;
		set = (payload[10] | (payload[11] << 8));

		unsigned int drift;
		drift = (payload[12] | (payload[13] << 8));


		nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,%.2f,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
			RADIANS_TO_DEGREES((float)courseOverGround / 10000), (float)speedOverGround * CONVERT_MS_KNOTS / 100, \
			(float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Send an ISO Request
int TwoCanDevice::SendISORequest(const byte destination, const unsigned int pgn) {
	CanHeader header;
	header.pgn = 59904;
	header.destination = destination;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id,&header);
		
	byte payload[3];
	payload[0] = pgn & 0xFF;
	payload[1] = (pgn >> 8) & 0xFF;
	payload[2] = (pgn >> 16) & 0xFF;
	
#ifdef __WXMSW__
	return (writeFrame(id, 3, payload));
#endif
	
#ifdef __LINUX__
	return (linuxSocket->Write(id,3,payload));
#endif
}

// Claim an Address on the NMEA 2000 Network
int TwoCanDevice::SendAddressClaim(const unsigned int sourceAddress) {
	CanHeader header;
	header.pgn = 60928;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = sourceAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id,&header);
	
	byte payload[8];
	int manufacturerCode = CONST_MANUFACTURER_CODE;
	int deviceFunction = CONST_DEVICE_FUNCTION;
	int deviceClass = CONST_DEVICE_CLASS;
	int deviceInstance = 0;
	int systemInstance = 0;

	payload[0] = uniqueId & 0xFF;
	payload[1] = (uniqueId >> 8) & 0xFF;
	payload[2] = (uniqueId >> 16) & 0x1F;
	payload[2] |= (manufacturerCode << 5) & 0xE0;
	payload[3] = manufacturerCode >> 3;
	payload[4] = deviceInstance;
	payload[5] = deviceFunction;
	payload[6] = deviceClass << 1;
	payload[7] = 0x80 | (CONST_MARINE_INDUSTRY << 4) | systemInstance;

	// Add my entry to the network map
	networkMap[header.source].manufacturerId = manufacturerCode;
	networkMap[header.source].uniqueId = uniqueId;
	// BUG BUG What to do for my time stamp ??

	// And while we're at it, calculate my deviceName (aka NMEA 'NAME')
	deviceName = (unsigned long long)payload[0] | ((unsigned long long)payload[1] << 8) | ((unsigned long long)payload[2] << 16) | ((unsigned long long)payload[3] << 24) | ((unsigned long long)payload[4] << 32) | ((unsigned long long)payload[5] << 40) | ((unsigned long long)payload[6] << 48) | ((unsigned long long)payload[7] << 54);
	
#ifdef __WXMSW__
	return (writeFrame(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif
	
#ifdef __LINUX__
	return (linuxSocket->Write(id,CONST_PAYLOAD_LENGTH,&payload[0]));
#endif
}

// Transmit NMEA 2000 Heartbeat
int TwoCanDevice::SendHeartbeat() {
	CanHeader header;
	header.pgn = 126993;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;

	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id, &header);

	byte payload[8];
	memset(payload, 0xFF, CONST_PAYLOAD_LENGTH);

	//BUG BUG 60000 milliseconds equals one minute. Should match this to the heartbeat timer interval
	payload[0] = 0x60; // 60000 & 0xFF;
	payload[1]  = 0xEA; // (60000 >> 8) & 0xFF;
	payload[2] = heartbeatCounter;
	payload[3] = 0xFF; // Class 1 CAN State, From observation of B&G devices indicates 255 ? undefined ??
	payload[4] = 0xFF; // Class 2 CAN State, From observation of B&G devices indicates 255 ? undefined ??
	payload[5] = 0xFF; // No idea, leave as 255 undefined
	payload[6] = 0xFF; //            "
	payload[7] = 0xFF; //            "

	heartbeatCounter++;
	if (heartbeatCounter == 253) {  // From observation of B&G devices, appears to rollover after 252 ??
		heartbeatCounter = 0;
	}
	
#ifdef __WXMSW__
	return (writeFrame(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif

#ifdef __LINUX__
	return (linuxSocket->Write(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif

}

// Transmit NMEA 2000 Product Information
int TwoCanDevice::SendProductInformation() {
	CanHeader header;
	header.pgn = 126996;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	byte payload[134];
		
	unsigned short dbver = CONST_DATABASE_VERSION;
	memcpy(&payload[0],&dbver,sizeof(unsigned short)); 

	unsigned short pcode = CONST_PRODUCT_CODE;
	memcpy(&payload[2],&pcode,sizeof(unsigned short));
	
	// Note all of the string values are stored without terminating NULL character
	// Model ID Bytes [4] - [35]
	memset(&payload[4],0,32);
	char *hwVersion = CONST_MODEL_ID;
	memcpy(&payload[4], hwVersion,strlen(hwVersion));
	
	// Software Version Bytes [36] - [67]
	// BUG BUG Should derive from PLUGIN_VERSION_MAJOR and PLUGIN_VERSION_MINOR
	memset(&payload[36],0,32);
	char *swVersion = CONST_SOFTWARE_VERSION;  
	memcpy(&payload[36], swVersion,strlen(swVersion));
		
	// Model Version Bytes [68] - [99]
	memset(&payload[68],0,32);
	memcpy(&payload[68], hwVersion,strlen(hwVersion));
	
	// Serial Number Bytes [100] - [131] - Let's reuse our uniqueId as the serial number
	memset(&payload[100],0,32);
	std::string tmp;
	tmp = std::to_string(uniqueId);
	memcpy(&payload[100], tmp.c_str(),tmp.length());
	
	payload[132] = CONST_CERTIFICATION_LEVEL;
	
	payload[133] = CONST_LOAD_EQUIVALENCY;

	wxString *mid;
	mid = new wxString(CONST_MODEL_ID);
		
	strcpy(networkMap[header.source].productInformation.modelId,mid->c_str());
	return FragmentFastMessage(&header,sizeof(payload),&payload[0]);

}

// Transmit Supported Parameter Group Numbers
int TwoCanDevice::SendSupportedPGN() {
	CanHeader header;
	header.pgn = 126464;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	// BUG BUG Should define our supported Parameter Group Numbers somewhere else, not compiled int the code ??
	unsigned int receivedPGN[] = {59904, 59392, 60928, 65240, 126464, 126992, 126993, 126996, 
		127250,	127251, 127258, 128259, 128267, 128275, 129025, 129026, 129029, 129033, 
		129028, 129039, 129040, 129041, 129283, 129793, 129794, 129798, 129801, 129802, 
		129808,	129809, 129810, 130306, 130310, 130312, 130577 };
	unsigned int transmittedPGN[] = {59392, 59904, 60928, 126208, 126464, 126993, 126996 };
	
	// Payload is a one byte function code (receive or transmit) and 3 bytes for each PGN 
	int arraySize;
	arraySize = sizeof(receivedPGN)/sizeof(unsigned int);
	
	byte *receivedPGNPayload;
	receivedPGNPayload = (byte *)malloc((arraySize * 3) + 1);
	receivedPGNPayload[0] = 0; // I think receive function code is zero

	for (int i = 0; i < arraySize; i++) {
		receivedPGNPayload[(i*3) + 1] = receivedPGN[i] & 0xFF;
		receivedPGNPayload[(i*3) + 2] = (receivedPGN[i] >> 8) & 0xFF;
		receivedPGNPayload[(i*3) + 3] = (receivedPGN[i] >> 16 ) & 0xFF;
	}
	
	FragmentFastMessage(&header,sizeof(receivedPGNPayload),receivedPGNPayload);
	
	free(receivedPGNPayload);

	arraySize = sizeof(transmittedPGN)/sizeof(unsigned int);
	byte *transmittedPGNPayload;
	transmittedPGNPayload = (byte *)malloc((arraySize * 3) + 1);
	transmittedPGNPayload[0] = 1; // I think transmit function code is 1
	for (int i = 0; i < arraySize; i++) {
		transmittedPGNPayload[(i*3) + 1] = transmittedPGN[i] & 0xFF;
		transmittedPGNPayload[(i*3) + 2] = (transmittedPGN[i] >> 8) & 0xFF;
		transmittedPGNPayload[(i*3) + 3] = (transmittedPGN[i] >> 16 ) & 0xFF;
	}
	
	FragmentFastMessage(&header,sizeof(transmittedPGNPayload),transmittedPGNPayload);

	free(transmittedPGNPayload);
	
	return TWOCAN_RESULT_SUCCESS;
}

// Respond to an ISO Rqst
// BUG BUG at present just NACK everything
int TwoCanDevice::SendISOResponse(unsigned int sender, unsigned int pgn) {
	CanHeader header;
	header.pgn = 59392;
	header.destination = sender;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id,&header);
		
	byte payload[8];
	payload[0] = 1; // 0 = Ack, 1 = Nack, 2 = Access Denied, 3 = Network Busy
	payload[1] = 0; // No idea, the field is called Group Function
	payload[2] = 0; // No idea
	payload[3] = 0;
	payload[4] = 0;
	payload[5] = pgn & 0xFF;
	payload[6] = (pgn >> 8) & 0xFF;
	payload[7] = (pgn >> 16) & 0xFF;
	
#ifdef __WXMSW__
	return (writeFrame(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif
	
#ifdef __LINUX__
	return (linuxSocket->Write(id,CONST_PAYLOAD_LENGTH,&payload[0]));
#endif
 
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
	for (wxString::const_iterator it = sentence.begin() + 1; it != sentence.end(); ++it) {
		calculatedChecksum ^= static_cast<unsigned char> (*it);
	}
	return(wxString::Format(wxT("%02X"), calculatedChecksum));
}

// Fragment a Fast Packet Message into 8 byte payload chunks
int TwoCanDevice::FragmentFastMessage(CanHeader *header, unsigned int payloadLength, byte *payload) {
	unsigned int id;
	int returnCode;
	byte data[8];
	
	TwoCanUtils::EncodeCanHeader(&id,header);
		
	// Send the first frame
	// BUG BUG should maintain a map of sequential ID's for each PGN
	byte sid = 0;
	data[0] = sid;
	data[1] = payloadLength;
	memcpy(&data[2], &payload[0], 6);
	
#ifdef __WXMSW__
		returnCode = writeFrame(id, CONST_PAYLOAD_LENGTH, &data[0]);
#endif
	
#ifdef __LINUX__
	returnCode = linuxSocket->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif

	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("TwoCan Device, Error sending fast message frame"));
		// BUG BUG Should we log the frame ??
		return returnCode;
	}
	
	sid += 1;
	wxThread::Sleep(CONST_TEN_MILLIS);
	
	// Now the intermediate frames
	int iterations;
	iterations = (int)((payloadLength - 6) / 7);
		
	for (int i = 0; i < iterations; i++) {
		data[0] = sid;
		memcpy(&data[1],&payload[6 + (i * 7)],7);
		
#ifdef __WXMSW__
		returnCode = writeFrame(id, CONST_PAYLOAD_LENGTH, &data[0]);
#endif

		
#ifdef __LINUX__
		returnCode = linuxSocket->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif

		if (returnCode != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("TwoCan Device, Error sending fast message frame"));
			// BUG BUG Should we log the frame ??
			return returnCode;
		}
		
		sid += 1;
		wxThread::Sleep(CONST_TEN_MILLIS);
	}
	
	// Is there a remaining frame ?
	int remainingBytes;
	remainingBytes = (payloadLength - 6) % 7;
	if (remainingBytes > 0) {
		data[0] = sid;
		memset(&data[1],0xFF,7);
		memcpy(&data[1], &payload[payloadLength - remainingBytes], remainingBytes );
		
#ifdef __WXMSW__
		returnCode = writeFrame(id, CONST_PAYLOAD_LENGTH, &data[0]);
#endif

		
#ifdef __LINUX__
		returnCode = linuxSocket->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif
		if (returnCode != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("TwoCan Device, Error sending fast message frame"));
			// BUG BUG Should we log the frame ??
			return returnCode;
		}
	}
	
	return TWOCAN_RESULT_SUCCESS;
}

	
// Encode an 8 bit ASCII character using NMEA 0183 6 bit encoding
char TwoCanDevice::AISEncodeCharacter(char value)  {
		char result = value < 40 ? value + 48 : value + 56;
		return result;
}

// Decode a NMEA 0183 6 bit encoded character to an 8 bit ASCII character
char TwoCanDevice::AISDecodeCharacter(char value) {
	char result = value - 48;
	result = result > 40 ? result - 8 : result;
	return result;
}

// Create the NMEA 0183 AIS VDM/VDO payload from the 6 bit encoded binary data
wxString TwoCanDevice::AISEncodePayload(std::vector<bool>& binaryData) {
	wxString result;
	int j = 6;
	char temp = 0;
	// BUG BUG should probably use std::vector<bool>::size_type
	for (std::vector<bool>::size_type i = 0; i < binaryData.size(); i++) {
		temp += (binaryData[i] << (j - 1));
		j--;
		if (j == 0) { // "gnaw" through each 6 bits
			result.append(AISEncodeCharacter(temp));
			temp = 0;
			j = 6;
		}
	}
	return result;
}

// Decode the NMEA 0183 ASCII values, derived from 6 bit encoded data to an array of bits
// so that we can gnaw through the bits to retrieve each AIS data field 
std::vector<bool> TwoCanDevice::AISDecodePayload(wxString SixBitData) {
	std::vector<bool> decodedData(168);
	for (wxString::size_type i = 0; i < SixBitData.length(); i++) {
		char testByte = AISDecodeCharacter((char)SixBitData[i]);
		// Perform in reverse order so that we store in LSB order
		for (int j = 5; j >= 0; j--) {
			// BUG BUG generates compiler warning, could use ....!=0 but could be confusing ??
			decodedData.push_back((testByte & (1 << j))); // sets each bit value in the array
		}
	}
	return decodedData;
}

// Assemble AIS VDM message, fragmenting if necessary
	std::vector<wxString> TwoCanDevice::AssembleAISMessage(std::vector<bool> binaryData, const int messageType) {
	std::vector<wxString> result;
	result.push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
	return result;
}

// Insert an integer value into AIS binary data, prior to AIS encoding
void TwoCanDevice::AISInsertInteger(std::vector<bool>& binaryData, int start, int length, int value) {
	for (int i = 0; i < length; i++) {
		// set the bit values, storing as MSB
		binaryData[start + length - i - 1] = (value & (1 << i));
	}
	return;
}

// Insert a date value, DDMMhhmm into AIS binary data, prior to AIS encoding
void TwoCanDevice::AISInsertDate(std::vector<bool>& binaryData, int start, int length, int day, int month, int hour, int minute) {
	AISInsertInteger(binaryData, start, 4, day);
	AISInsertInteger(binaryData, start + 4, 5, month);
	AISInsertInteger(binaryData, start + 9, 5, hour);
	AISInsertInteger(binaryData, start + 14, 6, minute);
	return;
}

// Insert a string value into AIS binary data, prior to AIS encoding
void TwoCanDevice::AISInsertString(std::vector<bool> &binaryData, int start, int length, std::string value) {

	// Should check that value.length is a multiple of 6 (6 bit ASCII encoded characters) and
	// that value.length * 6 is less than length.

	// convert to uppercase;
	std::transform(value.begin(), value.end(), value.begin(), ::toupper);

	// pad string with @ 
	// BUG BUG Not sure if this is correct. 
	value.append((length / 6) - value.length(), '@');

	// Encode each ASCII character to 6 bit ASCII according to ITU-R M.1371-4
	// BUG BUG Is this faster or slower than using a lookup table ??
	std::bitset<6> bitValue;
	for (int i = 0; i < static_cast<int>(value.length()); i++) {
		bitValue = value[i] >= 64 ? value[i] - 64 : value[i];
		for (int j = 0, k = 5; j < 6; j++, k--) {
			// set the bit values, storing as MSB
			binaryData.at((i * 6) + start + k) = bitValue.test(j);
		}
	}
}
