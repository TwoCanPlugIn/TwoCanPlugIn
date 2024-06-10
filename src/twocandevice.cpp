// Copyright(C) 2018-2020 by Steven Adler
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
// 1.5 - 10/7/2019. Checks for valid data, flags for XTE, PGN Attitude, additional log formats
// 1.6 - 10/10/2019 Added PGN 127245 (Rudder), 127488 (Engine, Rapid), 127489 (Engine, Dynamic), 127505 (Fluid Levels)
// 1.7 - 10/12/2019 Aded PGN 127508 (Battery), AIS fixes
// 1.8 - 10/05/2020 AIS data validation fixes, Mac OSX support
// 1.9 - 20/08/2020 Rusoku adapter support on Mac OSX, OCPN 5.2 Plugin Manager support
// 1.91 - 20/10/2020 Add PGN 129540, Bug Fixes (random position - missing break, socket & queue timeouts)
// 1.92 - 10/04/2021 Fix for fast message assembly & SID generation
// 2.0 - 04/07/2021 Bi-directional gateway (incl AIS)
// 2.1 - 20/05/2022 Minor fix to GGA/DBT in Gateway, DSC & MOB Sentences, Waypoint creation, epoch time fixes (time_t)0
// wxWidgets 3.15 support for MacOSX, Fusion Media control, OCPN Messaging for NMEA 2000 Transmit, Extend PGN 130312 for Engine Exhaust
// 2.11 - 30/06/2022 - Change Attitude XDR sentence from HEEL to ROLL, Support DPT instead of DBT for depth,
// to mate with dashboard plugin. Prioritise GPS if multiple sources, Fix to PGN 129284 (distance)
// 2.2 - 20/03/2023 - Fix PGN  127251 (ROT) - forgot to convert to minutes, 
// Fix PGN 129038 (AIS Class A) - incorrect scale factor for ROT, Fix PGN 129810 (AIS Class B Static) - Append GPS Fixing Device
// 2.3 - 30/06/2023 - Support OpenCPN 5.8.x (Breaking change as now dependent on wxWidgets 3.2.x)
// Outstanding Features: 
// 1. Rewrite/Port Adapter drivers to C++
//

#include "twocandevice.h"

TwoCanDevice::TwoCanDevice(wxEvtHandler *handler) : wxThread(wxTHREAD_JOINABLE) {
	// Save a reference to our "parent", the plugin event handler so we can pass events to it
	eventHandlerAddress = handler;
	
#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
	// Initialise Message Queue to receive frames from either the Linux SocketCAN interface, Mac serial interface or Linux/Mac Log Reader
	// Note for Windows version we use a different mechanism, using Windows Events.
	canQueue = new wxMessageQueue<std::vector<byte>>();
#endif
	
	// FastMessage buffer is used to assemble the multiple frames of a fast message
	MapInitialize();
	
	// Initialize the statistics
	receivedFrames = 0;
	transmittedFrames = 0;
	droppedFrames = 0;
	
	// Initialise persisted values for constructed sentences such as RMC and GLL
	gpsTimeOffset = 0;
	magneticVariation = SHRT_MAX;
	vesselCOG = USHRT_MAX;
	vesselSOG = USHRT_MAX;

	// Timer to send PGN126993 heartbeats and a monotonically incrementing counter
	heartbeatTimer = nullptr;
	heartbeatCounter = 0;

	// Each AIS multi sentence message has a sequential Message ID
	AISsequentialMessageId = 0;

	// Until engineInstance > 0 then assume a single engined vessel
	IsMultiEngineVessel = FALSE;

	// Initialize Preferred GPS Sources
	preferredGPS.sourceAddress = CONST_GLOBAL_ADDRESS;
	preferredGPS.hdop = USHRT_MAX;
	preferredGPS.hdopRetry = 0;
	preferredGPS.lastUpdate = wxDateTime::Now();
	
	// Any raw logging ?
	if (logLevel > FLAGS_LOG_NONE) {
		wxDateTime tm = wxDateTime::Now();
		// construct a filename with the following format twocan-2018-12-31_210735.log
		wxString fileName = tm.Format("twocan-%Y-%m-%d_%H%M%S.log");
		if (rawLogFile.Open(wxString::Format("%s//%s", wxStandardPaths::Get().GetDocumentsDir(), fileName), wxFile::write)) {
			wxLogMessage(_T("TwoCan Device, Created log file: %s"), fileName);
			// If a CSV format initialize with a header row
			if (logLevel == FLAGS_LOG_CSV) {
				rawLogFile.Write("Source,Destination,PGN,Priority,D1,D2,D3,D4,D5,D6,D7,D8\r\n");
			}
		}
		else {
			wxLogError(_T("TwoCan Device, Unable to create raw log file: %s"), fileName);
		}
	}
}

TwoCanDevice::~TwoCanDevice(void) {
	// Not sure about the order of exiting the Entry, executing the OnExit or Destructor functions ??
}

// wxTimer notifications used to send my heartbeat and to maintain our network map
void TwoCanDevice::OnHeartbeat(wxEvent &event) {
	int returnCode;
	returnCode = SendHeartbeat();
	if (returnCode == TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("TwoCan Device, Sent heartbeat"));
	}
	else {
		wxLogMessage(_T("TwoCan Device, Error sending heartbeat: %d"), returnCode);
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
					wxLogMessage(_T("TwoCan Device, Sent ISO Request for 126996 to %d"), i);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Error sending ISO Request for 126996 to %d: %d"), i, returnCode);
				}
				wxThread::Sleep(CONST_TEN_MILLIS);
			}
			if (wxDateTime::Now() > (networkMap[i].timestamp + wxTimeSpan::Seconds(60))) {
			// If an entry is stale, send an address claim request.
			// BUG BUG Perhaps add an extra field in which to store the devices' hearbeat interval, rather than comparing against 60'
				returnCode = SendISORequest(i, 60928);
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("TwoCan Device, Sent ISO Request for 60928 to %d"), i);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Error sending ISO Request  for 60928 to %d: %d"), i, returnCode);
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
					wxLogMessage(_T("TwoCan Device, Sent ISO Request for 60928 to %d"), i);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Error sending ISO Request  for 60928 to %d: %d"), i, returnCode);
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
			wxLogMessage(_T("TwoCan Device, Sent ISO Request for 60928 to %d"), CONST_GLOBAL_ADDRESS);
		}
		else {
			wxLogMessage(_T("TwoCan Device, Error sending ISO Request for 60928  to %d: %d"), CONST_GLOBAL_ADDRESS, returnCode);
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


// Init, Load the CAN Adapter (either a Windows DLL or for Linux & Mac OSX the baked-in drivers; Log File Reader, SocketCAN or Mac OSX Serial USB Modem
// and get ready to start reading from the CAN bus.
int TwoCanDevice::Init(wxString driverPath) {
	int returnCode = TWOCAN_RESULT_SUCCESS;
	
#if defined (__WXMSW__) 
	// Load the CAN Adapter DLL
	returnCode = LoadWindowsDriver(driverPath);
	if (returnCode == TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("TwoCan Device, Loaded driver: %s"), driverPath);
		// If we are an active device, claim an address on the network (PGN 60928)  and send our product info (PGN 126996)
		if (deviceMode == TRUE) {
			TwoCanUtils::GetUniqueNumber(&uniqueId);
			wxLogMessage(_T("TwoCan Device, Unique Number: %d"), uniqueId);
			returnCode = SendAddressClaim(networkAddress);
			if (returnCode != TWOCAN_RESULT_SUCCESS) {
				wxLogError(_T("TwoCan Device, Error sending address claim: %d"), returnCode);
			}
			else {
				wxLogMessage(_T("TwoCan Device, Claimed network address: %d"), networkAddress);
				// Broadcast our product information on the network
				returnCode = SendProductInformation();
				if (returnCode != TWOCAN_RESULT_SUCCESS) {
					wxLogError(_T("TwoCan Device, Error sending Product Information: %d"), returnCode);
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
				if (enableMusic) {
					SendISORequest(CONST_GLOBAL_ADDRESS, 126998);
				}
			}
		}
	}
	else if (((returnCode & 0xFF0000) >> 16) == TWOCAN_ERROR_INVALID_WRITE_FUNCTION) {
		wxLogMessage(_T("TwoCan Device, Loaded driver %s in listen only mode"), driverPath);
		// If we have no write function, we can't be in Active Mode etc.
		deviceMode = FALSE;
		enableGateway = FALSE;
		enableHeartbeat = FALSE;
		autopilotModel === AUTOPILOT_MODEL::NONE;
		enableMusic = FALSE;
	}
	else {
		wxLogError(_T("TwoCan Device, Error loading driver %s: %d"), driverPath, returnCode);
	}
#endif

#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
	// Mac & Linux use "baked in" classes instead of the plug-in model that the Window's version uses
	// Save the driver name to pass to the Open function
	driverName = driverPath;
	if (driverName.CmpNoCase("Log File Reader") == 0) {
		// Load the Logfile reader
		adapterInterface = new TwoCanLogReader(canQueue);
		returnCode = adapterInterface->Open(CONST_LOGFILE_NAME);
	}
	else if (driverName.CmpNoCase("Pcap File Reader") == 0) {
		// Load the Pcap (Wireshark) reader
		adapterInterface = new TwoCanPcap(canQueue);
		returnCode = adapterInterface->Open(CONST_PCAPFILE_NAME);
	}
#if defined (__APPLE__) && defined (__MACH__)
	else if (driverName.CmpNoCase("Cantact") == 0) {
		// Load the MAC Serial USB interface for the Canable Cantact
		adapterInterface = new TwoCanMacSerial(canQueue);
		returnCode = adapterInterface->Open(canAdapter);
	}
	else if (driverName.CmpNoCase("Rusoku") == 0) {
		// Load the MAC interface for the Rusoku Toucan adapter
		adapterInterface = new TwoCanMacToucan(canQueue);
		returnCode = adapterInterface->Open(canAdapter);
	}
	else if (driverName.CmpNoCase("Kvaser") == 0) {
		// Load the MAC interface for the Kvaser adapter
		adapterInterface = new TwoCanMacKvaser(canQueue);
		returnCode = adapterInterface->Open(canAdapter);
	}
#endif

#if defined (__LINUX__)
	else if (driverName.MakeUpper().Matches(_T("CAN?")) || driverName.MakeUpper().Matches(_T("SLCAN?")) || driverName.MakeUpper().Matches(_T("VCAN?")) ) {
		// Load the SocketCAN interface (eg. Native Interface CAN0, Serial Interface SLCAN0, Virtual Interface VCAN0)
		adapterInterface = new TwoCanSocket(canQueue);
		returnCode = adapterInterface->Open(canAdapter);
	}
#endif 
	else { 
		// An invalid driver name !!  Fail and exit
		wxLogError(_T("TwoCan Device, Invalid driver %s"), driverName);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_DRIVER_NOT_FOUND);
	}

	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("TwoCan Device, Error loading driver %s: %d"), driverName, returnCode);
	}
	else {
		wxLogMessage(_T("TwoCan Device, Loaded driver %s"),driverName);
		// if we are an active device, claim an address
		if (deviceMode == TRUE) {
			if (adapterInterface->GetUniqueNumber(&uniqueId) == TWOCAN_RESULT_SUCCESS) {
				wxLogMessage(_T("TwoCan Device, Unique Number: %d"),uniqueId);
				returnCode = SendAddressClaim(networkAddress);
				if (returnCode != TWOCAN_RESULT_SUCCESS) {
					wxLogError(_T("TwoCan Device, Error sending address claim: %d"), returnCode);
				}
				else {
					wxLogMessage(_T("TwoCan Device, Claimed network address: %d"), networkAddress);
					returnCode = SendProductInformation();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogError(_T("TwoCan Device, Error sending Product Information: %d"), returnCode);
					}
					else {
						wxLogMessage(_T("TwoCan Device, Sent Product Information"));
					}
					// If we have at least successfully claimed an address and the heartbeat is enabled, 
					// start a timer that will send PGN 126993 NMEA Heartbeat every 60 seconds
					if (enableHeartbeat == TRUE) {
						heartbeatTimer = new wxTimer();
						heartbeatTimer->Bind(wxEVT_TIMER, &TwoCanDevice::OnHeartbeat, this);
						heartbeatTimer->Start(CONST_ONE_SECOND * 60, wxTIMER_CONTINUOUS);;
					}

				}
			}
			else {
				// unable to generate unique number
				// BUG BUG GetUniqueNumber always returns a number.
				wxLogError(_T("TwoCan Device, Unable to generate unique address: %d"), returnCode);
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
	
#if defined (__WXMSW__) 
	return (wxThread::ExitCode)ReadWindowsDriver();
#endif

#if (defined (__APPLE__) && defined (__MACH__)) || defined  (__LINUX__)
	return (wxThread::ExitCode)ReadLinuxOrMacDriver();
#endif
}

// OnExit, called when thread->delete is invoked, and entry returns
void TwoCanDevice::OnExit() {
	int returnCode;

#if (defined (__APPLE__) && defined (__MACH__)) || defined  (__LINUX__)
	wxThread::ExitCode threadExitCode;
	wxThreadError threadError;

	wxLogMessage(_T("TwoCan Device, Terminating driver thread id: (0x%x)\n"), adapterInterface->GetId());
	threadError = adapterInterface->Delete(&threadExitCode, wxTHREAD_WAIT_BLOCK);
	if (threadError == wxTHREAD_NO_ERROR) {
		wxLogMessage(_T("TwoCan Device, Terminated driver thread: %d"), threadExitCode);
	}
	else {
		wxLogMessage(_T("TwoCan Device, Error terminating driver thread: %d"), threadError);
	}
	// Wait for the interface thread to exit
	adapterInterface->Wait(wxTHREAD_WAIT_BLOCK);

	// Can only invoke close if it is a joinable thread as detached threads would have already deleted themselves
	returnCode = adapterInterface->Close();
	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("TwoCan Device, Error closing driver: %d"), returnCode);
	}

	// Can only delete the interface class if it is a joinable thread.
	delete adapterInterface;

#endif

#if defined (__WXMSW__) 
	// Unload the CAN Adapter DLL
	returnCode = UnloadWindowsDriver();
#endif

	wxLogMessage(_T("TwoCan Device, Unloaded driver: %d"), returnCode);

	eventHandlerAddress = NULL;

	// Terminate the heartbeat timer
	// BUG BUG Do we need to delete it ??
	if ((enableHeartbeat == TRUE) && (heartbeatTimer != nullptr)) {
		heartbeatTimer->Stop();
		heartbeatTimer->Unbind(wxEVT_TIMER, &TwoCanDevice::OnHeartbeat, this);
	}

	// If logging, close log file
	if (logLevel > FLAGS_LOG_NONE) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Flush();
			rawLogFile.Close();
			wxLogMessage(_T("TwoCan Device, Closed Log File"));
		}
	}
	wxLog::FlushActive();
}


#if (defined (__APPLE__) && defined (__MACH__) ) || defined (__LINUX__)

int TwoCanDevice::ReadLinuxOrMacDriver(void) {
	CanHeader header;
	byte payload[CONST_PAYLOAD_LENGTH];
	wxMessageQueueError queueError;
	std::vector<byte> receivedFrame(CONST_FRAME_LENGTH);
		
	// Start the CAN Interface
	adapterInterface->Run();
	
	while (!TestDestroy())	{
		
		// Wait for a CAN Frame. timeout if on an idle network
		queueError = canQueue->ReceiveTimeout(CONST_TEN_MILLIS, receivedFrame);
		
		if (queueError == wxMSGQUEUE_NO_ERROR) {
					
			TwoCanUtils::DecodeCanHeader(&receivedFrame[0], &header);

			memcpy(payload, &receivedFrame[CONST_HEADER_LENGTH], CONST_PAYLOAD_LENGTH);
			
			// Log received frames
			if (logLevel > FLAGS_LOG_NONE) {
				LogReceivedFrames(&header, &receivedFrame[0]);
			}
			
			AssembleFastMessage(header, payload);
		
		}

	} // end while

	wxLogMessage(_T("TwoCan Device, Read Thread Exiting"));

	return TWOCAN_RESULT_SUCCESS;	
}
#endif

#if defined (__WXMSW__) 

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
		wxLogMessage(_T("TwoCan Device, Found driver: %s"),driverPath);
	}
	else {
		// BUG BUG Log Fatal Error
		wxLogError(_T("TwoCan Device, Cannot find driver: %s"), driverPath);
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
			wxLogError(_T("TwoCan Device, Invalid Driver Manufacturer function: %d"), GetLastError());
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
			wxLogError(_T("TwoCan Device, Invalid  Driver Name function: %d"), GetLastError());
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
			wxLogError(_T("TwoCan Device, Invalid Driver Version function: %d"), GetLastError());
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
				wxLogError(_T("TwoCan Device, Error opening driver: %d"), openResult);
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

			// Get and save the pointer to the write function 
			writeFrame = (LPFNDLLWrite)GetProcAddress(dllHandle, "WriteAdapter");

			if (writeFrame == NULL)	{
				// BUG BUG Log non fatal error, the plug-in can still receive data.
				wxLogError(_T("TwoCan Device, Invalid Write function: %d"), GetLastError());
				deviceMode = FALSE;
				enableHeartbeat = FALSE;
				enableGateway = FALSE;
				return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_WRITE_FUNCTION);
			}


			return TWOCAN_RESULT_SUCCESS;

		}
		else {
			// BUG BUG Log Fatal Error
			wxLogError(_T("TwoCan Device, Invalid Open function: %d"), GetLastError());
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

#if defined (__WXMSW__) 

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
			wxLogError(_T("TwoCan Device, Error starting driver read thread: %d"), readResult);
			return readResult;
		}
		
		// Log the fact that the driver has started its read thread successfully
		wxLogMessage(_T("TwoCan Device, Driver read thread started: %d"), readResult);

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
					
					if (logLevel > FLAGS_LOG_NONE) {
						LogReceivedFrames(&header, canFrame);
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
					wxLogError(_T("TwoCan Device, Unexpected Mutex result: %d"), mutexResult);
				}

			}
			// Reset the FrameReceived event
			ResetEvent(eventHandle);

		} // end while TestDestroy

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
		wxLogError(_T("TwoCan Device, Invalid Driver Read function: %d"), GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_READ_FUNCTION);
	}
}

#endif

#if defined (__WXMSW__) 

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
				wxLogError(_T("TwoCan Device, Error freeing lbrary: %d, Error Code: %d"), freeResult, GetLastError());
				return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_UNLOAD_LIBRARY);
			}
			else {
				return TWOCAN_RESULT_SUCCESS;
			}

		}
		else {
			/// BUG BUG Log error
			wxLogError(_T("TwoCan Device, Error closing driver: %d"), closeResult);
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
// 127233 - Man Overboard (MOB)
// 127237 - Heading/Track control
// 127489 - Engine parameters dynamic
// 127496 - Trip Status - Vessel  
// 127506 - DC Detailed status 
// 128275 - Distance log 
// 129029 - GNSS Position Data 
// 129038 - AIS Class A Position Report
// 129039 - AIS Class B Position Report
// 129040 - AIS Class B Extended Position Report
// 129041 - AIS Aid To Navigation (AToN) Report
// 129284 - Navigation info
// 129285 - Waypoint list
// 129540 - GNSS Sats in View 
// 129793 - AIS Date and Time Report
// 129794 - AIS Class A Static data
// 129795 - AIS Addressed Binary Message
// 129797 - AIS Broadcast Binary Message
// 129798 - AIS SAR Message
// 129799 - Radio Transceiver Information
// 129801 - AIS Addressed Safety Related Message
// 129802 - AIS Broadcast Safety Related Message
// 129808 - DSC Call Information
// 129809 - AIS Class B Static Data: Part A
// 129810 - AIS Class B Static Data Part B
// 130065 - Route List
// 130074 - Waypoint List
// 130323 - Meteorological Data
// 130577 - Direction Data
// 130820 - Manufacturer Proprietary Fast Message (Fusion Media Players)
// 130822 - Manufacturer Proprietary
// 130824 - Manufacturer Proprietary Fast Message (B&G Wind or Performance Data ??) 
// 130850 - Manufacturer Proprietary Fast Message (Navico NAC3 Autopilot)

// Checks whether a frame is a single frame message or multiframe Fast Packet message
bool TwoCanDevice::IsFastMessage(const CanHeader header) {
	static const unsigned int nmeafastMessages[] = { 65240, 126208, 126464, 126996, 126998, 127233, 127237, 127489, 127496, 127506, 128275, 129029, 129038, \
	129039, 129040, 129041, 129284, 129285, 129540, 129793, 129794, 129795, 129797, 129798, 129799, 129801, 129802, 129808, 129809, 129810, 130065, 130074, \
	130323, 130577, 130820, 130822, 130824, 130850 };
	for (size_t i = 0; i < sizeof(nmeafastMessages)/sizeof(unsigned int); i++) {
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
		position = MapFindMatchingEntry(header, payload[0]);
		// No existing fast message 
		if (position == NOT_FOUND) {
			// Find a free slot
			position = MapFindFreeEntry();
			// No free slots, exit
			if (position == NOT_FOUND) {
				return;
			}
			// Insert the first frame of the fast message
			else {
				MapInsertEntry(header, payload, position);
			}
		}
		// An existing fast message is present, append the frame
		else {
			MapAppendEntry(header, payload, position);
		}
	}
	// This is a single frame message, parse it
	else {
		ParseMessage(header, payload);
	}
}

// Initialize each entry in the Fast Message Map
void TwoCanDevice::MapInitialize(void) {
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		fastMessages[i].isFree = TRUE;
		fastMessages[i].data = NULL;
	}
}

// Lock a range of entries
// BUG BUG Remove for production, just used for testing
void TwoCanDevice::MapLockRange(const int start, const int end) {
	if (start < end)  {
		for (int i = start; i < end; i++) {
			fastMessages[i].isFree = FALSE;
			fastMessages[i].timeArrived = TwoCanUtils::GetTimeInMicroseconds();
		}
	}

}

// Find first free entry in fastMessages
int TwoCanDevice::MapFindFreeEntry(void) {
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		if (fastMessages[i].isFree == TRUE) {
			return i;
		}
	}
	// Could also run the Garbage Collection routine in a separate thread, would require locking etc.
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

	// Ensure that this is indeed the first frame of a fast message
	if ((data[0] & 0x1F) == 0) {
		int totalDataLength; // will also include padding as we memcpy all of the frame, because I'm lazy
		totalDataLength = (unsigned int)data[1];
		totalDataLength += 7 - ((totalDataLength - 6) % 7);

		fastMessages[position].sid = (unsigned int)data[0];
		fastMessages[position].expectedLength = (unsigned int)data[1];
		fastMessages[position].header = header;
		fastMessages[position].timeArrived = TwoCanUtils::GetTimeInMicroseconds();;
		fastMessages[position].isFree = FALSE;
		// Remember to free after we have processed the final frame
		fastMessages[position].data = (byte *)malloc(totalDataLength);
		memcpy(&fastMessages[position].data[0], &data[2], 6);
		// First frame of a multi-frame Fast Message contains six data bytes, position the cursor ready for next message
		fastMessages[position].cursor = 6;

		// Fucking Fusion, using fast messages to sends frames less than eight bytes
		if (fastMessages[position].expectedLength <= 6) {
			ParseMessage(header, fastMessages[position].data);
			// Clear the entry
			free(fastMessages[position].data);
			fastMessages[position].isFree = true;
			fastMessages[position].data = NULL;
		}
	}
	// No further processing is performed if this is not a start frame. 
	// A start frame may have been dropped and we received a subsequent frame	 
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
		// Subsequent messages contains seven data bytes (last message may be padded with 0xFF)
		fastMessages[position].cursor += 7; 
		// Is this the last message ?
		if (fastMessages[position].cursor >= fastMessages[position].expectedLength) {
			// Send for parsing
			ParseMessage(header, fastMessages[position].data);
			// Clear the entry
			free(fastMessages[position].data);
			fastMessages[position].isFree = TRUE;
			fastMessages[position].data = NULL;
		}
		return TRUE;
	}
	else if ((data[0] & 0x1F) == 0) {
		// We've found a matching entry, however this is a start frame, therefore
		// we've missed an end frame, and now we have a start frame with the same id (top 3 bits). 
		// The id has obviously rolled over. Should really double check that (data[0] & 0xE0) 
		// Clear the entry as we don't want to leak memory, prior to inserting a start frame
		free(fastMessages[position].data);
		fastMessages[position].isFree = TRUE;
		fastMessages[position].data = NULL;
		// And now insert it
		MapInsertEntry(header, data, position);
		// BUG BUG Should update the dropped frame stats
		return TRUE;
	}
	else {
		// This is not the next frame in the sequence and not a start frame
		// We've dropped an intermedite frame, so free the slot and do no further processing
		free(fastMessages[position].data);
		fastMessages[position].isFree = TRUE;
		fastMessages[position].data = NULL;
		// Dropped Frame Statistics
		if (droppedFrames == 0) {
			droppedFrameTime = wxDateTime::Now();
			droppedFrames += 1;
		}
		else {
			droppedFrames += 1;
		}
		if ((droppedFrames > CONST_DROPPEDFRAME_THRESHOLD) && (wxDateTime::Now() < (droppedFrameTime + wxTimeSpan::Seconds(CONST_DROPPEDFRAME_PERIOD) ) ) ) {
			wxLogError(_T("TwoCan Device, Dropped Frames rate exceeded"));
			wxLogError(wxString::Format(_T("Frame: Source: %d Destination: %d Priority: %d PGN: %d"),header.source, header.destination, header.priority, header.pgn));
			droppedFrames = 0;
		}
		return FALSE;

	}
}

// Determine whether an entry with a matching header & sequence ID exists. 
// If not, then assume this is the first frame of a multi-frame Fast Message
int TwoCanDevice::MapFindMatchingEntry(const CanHeader header, const byte sid) {
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		if (((sid & 0xE0) == (fastMessages[i].sid & 0xE0)) && (fastMessages[i].isFree == FALSE) && (fastMessages[i].header.pgn == header.pgn) && 
			(fastMessages[i].header.source == header.source) && (fastMessages[i].header.destination == header.destination)) {
			return i;
		}
	}
	return NOT_FOUND;
}

// BUG BUG if this gets run in a separate thread, need to lock the fastMessages 
int TwoCanDevice::MapGarbageCollector(void) {
	int staleEntries;
	staleEntries = 0;
	for (int i = 0; i < CONST_MAX_MESSAGES; i++) {
		if ((fastMessages[i].isFree == FALSE) && (TwoCanUtils::GetTimeInMicroseconds() - fastMessages[i].timeArrived > CONST_TIME_EXCEEDED)) {
			staleEntries++;
			free(fastMessages[i].data);
			fastMessages[i].isFree = TRUE;
		}
	}
	return staleEntries;
}

void TwoCanDevice::LogReceivedFrames(const CanHeader *header, const byte *frame) {
	// TwoCan Raw format 0x01,0x01,0xF8,0x09,0x64,0xD9,0xDF,0x19,0xC7,0xB9,0x0A,0x04
	if (logLevel == FLAGS_LOG_RAW) {
		if (rawLogFile.IsOpened()) {
			for (int j = 0; j < CONST_FRAME_LENGTH; j++) {
				rawLogFile.Write(wxString::Format("0x%02X", frame[j]));
				if (j < CONST_FRAME_LENGTH - 1) {
					rawLogFile.Write(",");
				}
			}
		}
		rawLogFile.Write("\r\n");
	}

	// Kees (Canboat) format 2009-06-18Z09:46:01.129,2,127251,1,255,8,ff,e0,6c,fd,ff,ff,ff,ff
	if (logLevel == FLAGS_LOG_CANBOAT) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Write(wxDateTime::UNow().Format("%Y-%m-%dZ%H:%M:%S.%l,"));
			rawLogFile.Write(wxString::Format("%lu,%lu,%lu,%lu,8,", header->source, header->pgn, header->priority, header->destination));
			for (int j = CONST_HEADER_LENGTH; j < CONST_FRAME_LENGTH; j++) {
				rawLogFile.Write(wxString::Format("%02X", frame[j]));
				if (j < CONST_FRAME_LENGTH - 1) {
					rawLogFile.Write(",");
				}
			}
		}
		rawLogFile.Write("\r\n");
	}

	// Candump format (1542794024.886119) can0 09F50303#030000FFFF00FFFF (use candump -l canx where x is 0,1 etc.)
	if (logLevel == FLAGS_LOG_CANDUMP) {
		if (rawLogFile.IsOpened()) {
#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
			timeval currentTime;
			gettimeofday(&currentTime, NULL);   
			rawLogFile.Write(wxString::Format("(%010ld.%06ld)",currentTime.tv_sec,currentTime.tv_usec));
#endif
#if defined (__WXMSW__) 
			FILETIME currentTime;
			GetSystemTimeAsFileTime(&currentTime);

			unsigned long long totalTime = currentTime.dwHighDateTime;
			totalTime <<= 32;
			totalTime |= currentTime.dwLowDateTime;
			totalTime /= 10; // Windows file time is expressed in tenths of microseconds (or 100 nanoseconds)
			totalTime -= 11644473600000000ULL; // convert from Windows epoch (1/1/1601) to Posix Epoch 1/1/1970
			
			unsigned long seconds;
			unsigned long microseconds;
			
			seconds = totalTime / 1000000L;
			microseconds = totalTime - (seconds * 1000000UL);
			
			rawLogFile.Write(wxString::Format("(%010ld.%06ld)",seconds,microseconds));
#endif
			// BUG BUG For linux, should use the actual CAN port on which we are receiving data 
			rawLogFile.Write(" can0 ");
			// Note CanId must be written LSB
			// BUG BUG What about the Extended Frame bit 0x80000000 ??
			rawLogFile.Write(wxString::Format("%02X%02X%02X%02X#", frame[3] ,frame[2],frame[1],frame[0]));
			for (int j = CONST_HEADER_LENGTH; j < CONST_FRAME_LENGTH; j++) {
				rawLogFile.Write(wxString::Format("%02X", frame[j]));
			}
		}
		rawLogFile.Write("\r\n");
	}

	// Yacht Devices format 9:06:35.596 R 09F80203 FF FC 88 CF 0A 00 FF FF
	if (logLevel == FLAGS_LOG_YACHTDEVICES) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Write(wxDateTime::UNow().Format("%H:%M:%S.%l R "));
			// Also LSB
			rawLogFile.Write(wxString::Format("%02X%02X%02X%02X", frame[3] ^ 0x80 ,frame[2],frame[1],frame[0]));
			rawLogFile.Write(wxString::Format(" "));
			for (int j = CONST_HEADER_LENGTH; j < CONST_FRAME_LENGTH; j++) {
				rawLogFile.Write(wxString::Format("%02X", frame[j]));
				if (j < CONST_FRAME_LENGTH - 1) {
					rawLogFile.Write(" ");
				}
			}
		}
		rawLogFile.Write("\r\n");
	}

	// Comma Separated Variable format 
	if (logLevel == FLAGS_LOG_CSV) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Write(wxString::Format("%lu,%lu,%lu,%lu,", header->source, header->destination,header->pgn,header->priority));
			for (int j = CONST_HEADER_LENGTH; j < CONST_FRAME_LENGTH; j++) {
				rawLogFile.Write(wxString::Format("0x%02X", frame[j]));
				if (j < CONST_FRAME_LENGTH - 1) {
					rawLogFile.Write(",");
				}
			}
		}
		rawLogFile.Write("\r\n");
	}

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
				// BUG BUG The bastards are using an address claim as a heartbeat !!
				//wxLogMessage("TwoCan Device, ISO Request for Address Claim");
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendAddressClaim(networkAddress);
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Address Claim: %d", returnCode);
					}
				}
				break;
		
			case 126464: // Supported PGN
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendSupportedPGN();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Supported PGN: %d", returnCode);
					}
				}
				break;
		
			case 126993: // Heartbeat
				// BUG BUG I don't think sn ISO Request is allowed to request a heartbeat ??
				break;
		
			case 126996: // Product Information 
				wxLogMessage("TwoCan Device, ISO Request for Product Information");
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendProductInformation();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Product Information: %d", returnCode);
					}
				}
				break;
		
			case 126998: // Configuration Information
				wxLogMessage("TwoCan Device, ISO Request for Configuration Information");
				if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
					int returnCode;
					returnCode = SendConfigurationInformation();
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Device, Error Sending Configuration Information: %d", returnCode);
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
			wxLogMessage(_T("TwoCan Network, Unique ID: %d"), deviceInformation.uniqueId);
			wxLogMessage(_T("TwoCan Network, Class: %d"), deviceInformation.deviceClass);
			wxLogMessage(_T("TwoCan Network, Function: %d"), deviceInformation.deviceFunction);
			wxLogMessage(_T("TwoCan Network, Industry: %d"), deviceInformation.industryGroup);
			
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
					wxLogMessage(_T("TwoCan Device, Reclaimed network address: %d"), networkAddress);
				}
				else {
					wxLogMessage("TwoCan Device, Error reclaiming network address %d: %d", networkAddress, returnCode);
				}
			}
			// Our uniqueId is larger so increment our network address and see if we can claim the new address
			else if (deviceName > deviceInformation.deviceName) {
				if (networkAddress + 1 <= CONST_MAX_DEVICES) {
					networkAddress += 1;
					int returnCode;
					returnCode = SendAddressClaim(networkAddress);
					if (returnCode == TWOCAN_RESULT_SUCCESS) {
						wxLogMessage(_T("TwoCan Device, Claimed new network address: %d"), networkAddress);
					}
					else {
						wxLogMessage("TwoCan Device, Error claiming new network address %d: %d", networkAddress, returnCode);
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
						wxLogMessage(_T("TwoCan Device, Claimed NULL network address: %d"), networkAddress);
					}
					else {
						wxLogMessage("TwoCan Device, Error claiming NULL network address %d: %d", networkAddress, returnCode);
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
			if (deviceInformation.networkAddress < CONST_MAX_DEVICES) {
			// Update our network address to the commanded address and send an address claim
			networkAddress = deviceInformation.networkAddress;
			int returnCode;
			returnCode = SendAddressClaim(networkAddress);
			if (returnCode == TWOCAN_RESULT_SUCCESS) {
				wxLogMessage(_T("TwoCan Device, Claimed commanded network address: %d"), networkAddress);
			}
			else {
				wxLogMessage(_T("TwoCan Device, Error claiming commanded network address %d: %d"), networkAddress, returnCode);
			}
		}
		else {
				wxLogMessage(_T("TwoCan Device, Error, commanded to use invalid address %d by %d"), deviceInformation.networkAddress, header.source);
			}
		}
		// No NMEA 0183 sentences to pass onto OpenCPN
		result = FALSE;
		break;

	case 65305:
		result = DecodePGN65305(payload);
		break;

	case 65345: // Manufacturer Proprietary
		result = DecodePGN65345(payload);
		break;

	case 65359: // Manufacturer Proprietary
		result = DecodePGN65359(payload);
		break;

	case 65360: // Manufacturer Proprietary
		result = DecodePGN65360(payload);
		break;

	case 65379: // Manufacturer Proprietary
		result = DecodePGN65379(payload);
		break;

	case 65380: // Manufacturer Proprietary
		result = DecodePGN65380(payload);
		break;

	case 126208: // NMEA Group Function 
		result = DecodePGN126208(header.destination, payload);
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

	case 126998: // NMEA Configuration Information
		result = DecodePGN126998(payload);
		// No messages to pass onto OpenCPN
		result = FALSE;
		break;

	case 127233: // Man Overboard
		if (supportedPGN & FLAGS_MOB) {
			result = DecodePGN127233(payload, &nmeaSentences);
		}
		break;

	case 127237: // Heading/Track control
		if (supportedPGN & FLAGS_NAV) {
			result = DecodePGN127237(payload, &nmeaSentences);
		}
		break;

	case 127245: // Rudder
		if (supportedPGN & FLAGS_RSA) {
			result = DecodePGN127245(payload, &nmeaSentences);
		}
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
		
	case 127257: // Attitude
		if (supportedPGN & FLAGS_XDR) {
			result = DecodePGN127257(payload, &nmeaSentences);
		}
		break;
		
	case 127258: // Magnetic Variation
		// BUG BUG needs flags 
		// BUG BUG Not actually used anywhere
		result = DecodePGN127258(payload, &nmeaSentences);
		break;

	case 127488: // Engine Parameters, Rapid Update
		if (supportedPGN & FLAGS_ENG) {
			result = DecodePGN127488(payload, &nmeaSentences);
		}
		break;

	case 127489: // Engine Parameters, Dynamic
		if (supportedPGN & FLAGS_ENG) {
			result = DecodePGN127489(payload, &nmeaSentences);
		}
		break;

	case 127505: // Fluid Levels
		if (supportedPGN & FLAGS_TNK) {
			result = DecodePGN127505(payload, &nmeaSentences);
		}
		break;
		
	case 127508: // Battery Status
		if (supportedPGN & FLAGS_BAT) {
			result = DecodePGN127508(payload, &nmeaSentences);
		}
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
		
		case 128275: // Distance Log
		if (supportedPGN & FLAGS_LOG) {
			result = DecodePGN128275(payload, &nmeaSentences);
		}
		break;

	case 129025: // Position - Rapid Update
		if (supportedPGN & FLAGS_GLL) {
			result = DecodePGN129025(payload, &nmeaSentences, header.source);
		}
		break;
	
	case 129026: // COG, SOG - Rapid Update
		if (supportedPGN & FLAGS_VTG) {
			result = DecodePGN129026(payload, &nmeaSentences, header.source);
		}
		break;
	
	case 129029: // GNSS Position
		if (supportedPGN & FLAGS_GGA) {
			result = DecodePGN129029(payload, &nmeaSentences, header.source);
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
		if (supportedPGN & FLAGS_XTE) {
			result = DecodePGN129283(payload, &nmeaSentences);
		}
		break;
		
	case 129284: // Navigation Information
		if (supportedPGN & FLAGS_NAV) {
			result = DecodePGN129284(payload, &nmeaSentences);
		}
		break;
		
	case 129285: // Route & Waypoint Information
		if (supportedPGN & FLAGS_RTE) {
			result = DecodePGN129285(payload, &nmeaSentences);
		}
		break;

	case 129539: // GNSS DOP's
		if (supportedPGN & FLAGS_GGA) {
			result = DecodePGN129539(payload, &nmeaSentences);
		}
		break;

	case 129540: // GNSS Satellites in view
		if (supportedPGN & FLAGS_GGA) {
			result = DecodePGN129540(payload, &nmeaSentences);
		}
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
	
	case 129795: // AIS Addressed Binary Message
		// BUG BUG to implement
		//if (supportedPGN & FLAGS_AIS) {
		//	result = DecodePGN129795(payload, &nmeaSentences);
		//}
		result = FALSE;
		break;

	case 129797: // AIS Binary Broadcast Message
		// BUG BUG To implement
		//if (supportedPGN & FLAGS_AIS) {
		//	result = DecodePGN129795(payload, &nmeaSentences);
		//}
		result = FALSE;
		break;

	case 129798: // AIS Search and Rescue (SAR) Position Report
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129798(payload, &nmeaSentences);
		}
		break;

	case 129799: // Radio Transceiver Information
		if (supportedPGN & FLAGS_DSC) {
			result = DecodePGN129799(payload, &nmeaSentences);
		}
		break;
		
	case 129801: // AIS Addressed Safety Related Message
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129801(payload, &nmeaSentences);
		}
		break;
	
	case 129802: // AIS Broadcast Safety Related Message
		if (supportedPGN & FLAGS_AIS) {
			result = DecodePGN129802(payload, &nmeaSentences);
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
	
	case 130065: // Route & Waypoint Service - Route List
		if (supportedPGN & FLAGS_RTE) {
			result = DecodePGN130065(payload, &nmeaSentences);
		}
		break;

	case 130074: // Route & Waypoint service - Waypoint List
		if (supportedPGN & FLAGS_RTE) {
			result = DecodePGN130074(payload, &nmeaSentences);
		}
		break;
	
	case 130306: // Wind data
		if (supportedPGN & FLAGS_MWV) {
			result = DecodePGN130306(payload, &nmeaSentences);
		}
		break;
	
	case 130310: // Environmental Parameters
		if (supportedPGN & FLAGS_MTW) {
			result = DecodePGN130310(payload, &nmeaSentences);
		}
		break;
		
	case 130311: // Environmental Parameters (supercedes 130310)
		if (supportedPGN & FLAGS_MTW) {
			result = DecodePGN130311(payload, &nmeaSentences);
		}
		break;
	
	case 130312: // Temperature
		if ((supportedPGN & FLAGS_MTW) || (supportedPGN & FLAGS_ENG)) {
			result = DecodePGN130312(payload, &nmeaSentences);
		}
		break;
		
	case 130316: // Temperature Extended Range
		if (supportedPGN & FLAGS_MTW) {
			result = DecodePGN130316(payload, &nmeaSentences);
		}
		break;

	case 130323: // Meteorological Data
		if (supportedPGN & FLAGS_MET) {
			DecodePGN130323(payload, &nmeaSentences);
		}
		break;

	case 130820: // Manufacturer Proprietary Fast Frame - only interested for Fusion Media Player integration
		if (enableMusic) {
			result = DecodePGN130820(payload, &nmeaSentences);
		}
		break;

	case 130850: // Manufacturer Proprietary Fast Frame - only interested for Navico NAC-3 Autopilot integration
		if (autoPilotModel != AUTOPILOT_MODEL::NONE) {
			result = DecodePGN130850(payload, &nmeaSentences);
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

// Decode PGN 65280 Manufacturer Proprietary
// BUG BUG ToDo
bool TwoCanDevice::DecodePGN65280(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: // Raymarine
				break;
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case 382: // B&G
				break;
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}

		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}

// Decode PGN65305 Manufacturer Proprietary - Navico Autopilot Status
bool TwoCanDevice::DecodePGN65305(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: // Fusion
				break;
			case 1857: //Simrad
			{
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					if (twoCanAutopilot->DecodeNAC3Status(payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
			}
				break;
		}
		
		return FALSE; // Nothing to return
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65309 Manufacturer Proprietary
// B&G WS320 Battery Status
bool TwoCanDevice::DecodePGN65309(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: // Raymarine
				break;
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case 382: { // B&G WS320 Battery Status
				byte status = payload[2];
				byte batteryStatus = payload[3];
				byte batteryCharge = payload[4];
				unsigned short reservedA = payload[5] | (payload[6] << 8);
				byte reservedB = payload[7];
				// BUG BUG Do something
				break;
			}
			case 419: // Fusion
				  break;
			case 1857: // Simrad
				break;
		}
		
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65312 Manufacturer Proprietary
// B&G WS320 Wireless Status
bool TwoCanDevice::DecodePGN65312(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: // Raymarine
				break;
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case 382: { // B&G WS320 Wireless Signal Strenghth
				byte status = payload[2];
				byte wireless = payload[3];
				int reservedA = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);
				// BUG BUG Do Something
				break;
			}
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65345 Manufacturer Proprietary Single Frame Message
// Seatalk Wind Reference
bool TwoCanDevice::DecodePGN65345(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: { // Raymarine
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					if (twoCanAutopilot->DecodeRaymarineAutopilotWind(65345, payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
				break;
			}
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}


// Decode PGN 65359 Manufacturer Proprietary Single Frame Message
// Seatalk Autopilot Heading
bool TwoCanDevice::DecodePGN65359(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: { // Raymarine
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					if (twoCanAutopilot->DecodeRaymarineAutopilotHeading(65359, payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
				break;
			}
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case 382: //B & G
				break;
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}



// Decode PGN 65360 Manufacturer Proprietary Single Frame Message
// Seatalk Autopilot Locked Heading
bool TwoCanDevice::DecodePGN65360(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: { // Raymarine
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					if (twoCanAutopilot->DecodeRaymarineAutopilotHeading(65360, payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
				break;
			}
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65379 Manufacturer Proprietary Single Frame Message
// Seatalk Autopilot Mode
bool TwoCanDevice::DecodePGN65379(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 1851: { // Raymarine
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					if (twoCanAutopilot->DecodeRaymarineAutopilotMode(payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
				break;
			}
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65380 Manufacturer Proprietary Single Frame Message
// Simrad Autopilot
bool TwoCanDevice::DecodePGN65380(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
		case 1851: // Raymarine
			break;
		case 1855: // Furuno
			break;
		case 229: // Garmin
			break;
		case 275: // Navico
			break;
		case  382: //B & G
			break;
		case 419: // Fusion
			break;
		case 1857: {// Simrad
			if (autopilotModel != AUTOPILOT_MODEL::NONE) {
				wxString jsonResponse;
				if (twoCanAutopilot->DecodeAC12Autopilot(payload, &jsonResponse)) {
					if (jsonResponse.Length() > 0) {
						SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
					}
				}
			}
			break;
		}
		} // end switch
		return FALSE;  // Nothing to return
	}
	else {
		return FALSE;
	}
}

// BUG BUG Not fully implemented, just a toy.
// Decode PGN 126208 NMEA Group Function Command
bool TwoCanDevice::DecodePGN126208(const int destination, const byte *payload) {
	if (payload != nullptr) {
		// We're only interested in commands sent to us
		// We can return PGN 126996 Product Information and PGN 126998 Configuration Information details
		// Requests & Reads. We do not wupport Commands or Writes
		if (destination == networkAddress) {

			byte functionCode;
			functionCode = payload[0];

			unsigned int requestedPGN;
			requestedPGN = payload[1] | (payload[2] << 8) | (payload[3] << 9);

			switch (functionCode) {
			case 0: // Request
				switch (requestedPGN) {
				case 126996:
					break;
				case 126998:
					break;
				default:
					break;
				}
				break;
			case 1: // Command
				// Send a Nack
				break;
			case 2: // Acknowledge
				break;
			case 3: // Read Fields
				switch (requestedPGN) {
				case 126996:
					break;
				case 126998:
					break;
				default:
					break;
				}
				break;
			case 4: // Read Fields Reply
				// Not interested in these
				break;
			case 5: // Write Fields
				// Send a Nack
				break;
			case 6: // Write Fields Reply
				// Not interested in these
				break;
			}
		}

	}

	return FALSE;
}


// Decode PGN 126720 Manufacturer Proprietary
bool TwoCanDevice::DecodePGN126720(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5; //Should match 4 - Marine

		switch (manufacturerId) {
			case 1851: { // Raymarine
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					// Raymarine encodes seatalk datagrams inside PGN 126720. Perhaps this is what the Seatalk <-> SeatalkNG gateway does ?
					if (twoCanAutopilot->DecodeRaymarineSeatalk(payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
				break;
			}
			case 1855: // Furuno
				break;
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case 382: // B&G
				break;
			case 419: 
				// Fusion - Note Fusion uses PGN 126720 to transmit commands from the remote control to the media player.
				// We don't decode them, as we generate them in twocanmedia.cpp to control a media player		
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;  // Nothing to return
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

		byte timeSource;
		timeSource = (payload[1] & 0xF) >> 4;

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[2] | (payload[3] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);
		
		if ((TwoCanUtils::IsDataValid(daysSinceEpoch)) && (TwoCanUtils::IsDataValid(secondsSinceMidnight))) {
			
			wxDateTime epoch((time_t)0);
			epoch += wxDateSpan::Days(daysSinceEpoch);
			epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);
			
			// Calculate the local timezone offfset in hours & minutes
			wxDateTime::TimeZone tz(wxDateTime::Local);
    		long seconds = tz.GetOffset();
			int hours = seconds / (3600);
			seconds %= 3600;
			int minutes = seconds / 60;

			if (hours > 0) {
				nmeaSentences->push_back(wxString::Format("$IIZDA,%s,%02d,%02d", epoch.Format("%H%M%S.00,%d,%m,%Y"), hours, minutes));
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIZDA,%s,%03d,%03d", epoch.Format("%H%M%S.00,%d,%m,%Y"), hours, minutes));
			}
			
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

// Decode PGN 126993 NMEA Heartbeat
bool TwoCanDevice::DecodePGN126993(const int source, const byte *payload) {
	if (payload != NULL) {

		unsigned short timeOffset; // milliseconds
		timeOffset = payload[0] | (payload[1] << 8);
		
		byte counter;
		counter = payload[2];

		// BUG BUG following may not be correct

		byte class1CanState;
		class1CanState = payload[3] & 0x03;

		byte class2CanState;
		class2CanState = (payload[3] & 0x0C) >> 2;
		
		byte equipmentState;
		equipmentState = (payload[3] & 0x30) >> 4;

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

// Decode PGN 126998 NMEA Configuration Information
int TwoCanDevice::DecodePGN126998(const byte *payload) {
	if (payload != NULL) {
		unsigned int index = 0;

		wxString installationInformation1;
		unsigned int length = payload[index];
		index++;
		if (payload[index] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			index++;
			for (size_t i = 0; i < length - 2; i++) {
				installationInformation1.append(1, payload[index]);
				index++;
			}
		}

		wxString installationInformation2;
		length = payload[index];
		index ++;
		if (payload[index] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			index ++;
			for (size_t i = 0; i < length - 2; i++) {
				installationInformation2.append(1, payload[index]);
				index++;
			}
		}

		wxString installationInformation3;
		length = payload[index];
		index ++;
		if (payload[index] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			index ++;
			for (size_t i = 0; i < length - 2; i++) {
				installationInformation3.append(1, payload[index]);
				index++;
			}
		}
		
		wxLogMessage("TwoCan Device, Device Configuration Details");
		wxLogMessage("TwoCan Device, Installation Information 1: %s\n", installationInformation1.data());
		wxLogMessage("Installation Information 2: %s\n", installationInformation2.data());
		wxLogMessage("Installation Information 3: %s\n", installationInformation3.data());
		
		if (enableMusic == TRUE) {
			if ((installationInformation3.CmpNoCase("FUSION Entertainment") == 0)  && 
				(installationInformation1.CmpNoCase("info 1") == 0)) {
				// BUG BUG, Is the device name guaranteed to be populated in this field ?
				wxJSONValue root;
				wxJSONWriter writer;
				wxString jsonResponse;
				root["entertainment"]["device"]["name"] = installationInformation2;
				writer.Write(root, jsonResponse);
				SendPluginMessage(_T("TWOCAN_MEDIA_RESPONSE"), jsonResponse);
			}
		}

	}
	return FALSE;
}

// Decode PGN 127233 NMEA Man Overboard
//$--MOB, hhhhh, a, hhmmss.ss, x, xxxxxx, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, x.x, xxxxxxxxx, x*hh	
bool TwoCanDevice::DecodePGN127233(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned int emitterId;
		emitterId = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		byte mobStatus;
		mobStatus = payload[5] & 0x03;

		byte reservedA;
		reservedA = (payload[5] & 0xF8) >> 3;

		unsigned int timeOfDay;
		timeOfDay = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		wxDateTime epoch((time_t)0);
		epoch += wxTimeSpan::Seconds((wxLongLong)timeOfDay / 10000);
		wxString activationTime = epoch.Format("%H%M%S");

		byte positionSource;
		positionSource = payload[10] & 0x03;

		byte reservedB;
		reservedB = (payload[10] & 0xF8) >> 3;

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[11] | (payload[12] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[13] | (payload[14] << 8) | (payload[15] << 16) | (payload[16] << 24);

		epoch = (time_t)0;
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		int latitude;
		latitude = (int)payload[17] | ((int)payload[18] << 8) | ((int)payload[19] << 16) | ((int)payload[20] << 24);

		double latitudeDouble = ((double)latitude * 1e-7);
		int latitudeDegrees = trunc(latitudeDouble);
		double latitudeMinutes = (latitudeDouble - latitudeDegrees) * 60;

		int longitude;
		longitude = (int)payload[21] | ((int)payload[22] << 8) | ((int)payload[23] << 16) | ((int)payload[24] << 24);

		double longitudeDouble = ((double)longitude * 1e-7);
		int longitudeDegrees = trunc(longitudeDouble);
		double longitudeMinutes = (longitudeDouble - longitudeDegrees) * 60;

		byte cogReference;
		cogReference = payload[25] & 0x02;

		unsigned short courseOverGround;
		courseOverGround = payload[26] | (payload[27] << 8);

		unsigned short speedOverGround;
		speedOverGround = payload[28] | (payload[29] << 8);

		unsigned int mmsiNumber;
		mmsiNumber = payload[30] | (payload[31] << 8) | (payload[32] << 16) | (payload[33] << 24);

		byte batteryStatus;
		batteryStatus = payload[34] & 0x03;

		nmeaSentences->push_back(wxString::Format("$IIMOB,%05X,%c,%s,%d,%s,%s,%02d%07.4f,%c,%03d%07.4f,%c,%.0f,%.0f,%d,%d",
			emitterId, // 5 hex digits
			mobStatus == 0 ? 'A' : mobStatus == 1 ? 'M' : mobStatus == 2 ? 'T' : 'V',
			activationTime.ToAscii().data(), positionSource,
			epoch.Format("%d%m%y").ToAscii().data(), epoch.Format("%H%M%S").ToAscii().data(),
			abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
			abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
			speedOverGround * CONVERT_MS_KNOTS / 100, RADIANS_TO_DEGREES((float)courseOverGround / 10000),
			mmsiNumber, batteryStatus));

		// As OpenCPN does not support this sentence, just drop a waypoint
		// When it does, remove this code
		PlugIn_Waypoint waypoint;
		waypoint.m_IsVisible = true;
		waypoint.m_MarkName = wxString::Format("Man Overboard at: %s", activationTime);
		waypoint.m_IconName = _T("Mob");
		waypoint.m_GUID = GetNewGUID();
		waypoint.m_lat = latitudeDouble;
		waypoint.m_lon = longitudeDouble;
		AddSingleWaypoint(&waypoint, true);

		return TRUE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 127237 NMEA Heading/Track Control
// $--APB,A,A,x.x,a,N,A,A,x.x,a,c--c,x.x,a,x.x,a*hh<CR><LF>
// 

//Status A OK, V reliable fix is not available

//Status V = Loran-C Cycle Lock warning flag A = OK or not used

//Cross Track Error Magnitude

//Direction to steer, L or R

//Cross Track Units, N = Nautical Miles

//Status A = Arrival Circle Entered

//Status A = Perpendicular passed at waypoint

//Bearing origin to destination

//M = Magnetic, T = True

//Destination Waypoint ID

//Bearing, present position to Destination

//M = Magnetic, T = True

//Heading to steer to destination waypoint

//M = Magnetic, T = True


bool TwoCanDevice::DecodePGN127237(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte rudderLimitExceeded; // 0 - No, 1 - Yes, 2 - Error, 3 - Unavailable
		rudderLimitExceeded = (payload[0] & 0xC0) >> 6;

		byte offHeadingLimitExceeded; // 0 - No, 1 - Yes, 2 - Error, 3 - Unavailable
		offHeadingLimitExceeded = (payload[0] & 0x30) >> 4; 
        
		byte offTrackLimitExceeded;
		offTrackLimitExceeded = (payload[0] & 0x0C) >> 2;
        
		byte overRide;
		overRide = payload[0] & 0x03;
		
		byte steeringMode;
		steeringMode = (payload[1] & 0xE0) >> 5;
		// 0 - Main Steering
		// 1 - Non-Follow-up Device
		// 2 - Follow-up Device
		// 3 - Heading Control Standalone
		// 4 - Heading Control
		// 5 - Track Control
		
		byte turnMode;
		turnMode = (payload[1] & 0x1C) >> 2;
        // 0 - Rudder Limit controlled
        // 1 - turn rate controlled
        // 2 - radius controlled
		
		byte headingReference;
		headingReference = payload[1] & 0x03;
		// 0 - True
		// 1 - Magnetic
		// 2 - Error
		// 3 - Null
		
		byte reserved;
		reserved = (payload[2] & 0xF8) >> 3;
          
		byte commandedRudderDirection;
		commandedRudderDirection = payload[2] & 0x03;
        // 0 - No Order
		// 1 - Move to starboard
		// 2 - Move to port
		short commandedRudderAngle; //0.0001 radians
		commandedRudderAngle = payload[3] | (payload[4] << 8);
		
		unsigned short headingToSteer; //0.0001 radians
		headingToSteer = payload[5] | (payload[6] << 8);
        
		unsigned short track; //0.0001 radians
		track = payload[7] | (payload[8] << 8);

		unsigned short rudderLimit; //0.0001 radians
		rudderLimit = payload[9] | (payload[10] << 8);

		unsigned short offHeadingLimit; // 0.0001 radians
		offHeadingLimit = payload[11] | (payload[12] << 8);
		  
		short radiusOfTurn; // 0.0001 radians
		radiusOfTurn = payload[13] | (payload[14] << 8);
		  
		short rateOfTurn; // 3.125e-05
		rateOfTurn = payload[15] | (payload[16] << 8); 

        short offTrackLimit; //in metres (or is it 0.01 m)??
		offTrackLimit = payload[17] | (payload[18] << 8); 
		
		unsigned short vesselHeading; // 0.0001 radians
		vesselHeading = payload[19] | (payload[20] << 8); 

		nmeaSentences->push_back(wxString::Format("$IIAPB,A,A,%0.2f,%c,N,,,%02f,%c ", \
		 fabs(CONVERT_METRES_NAUTICAL_MILES * offTrackLimit),offTrackLimit < 0? 'L':'R', \
		 RADIANS_TO_DEGREES((float)headingToSteer / 10000), \
		 headingReference == 0 ? 'T' : headingReference == 1 ? 'M' : char(0) ));
	}
	return TRUE;
}

// Decode PGN 127245 NMEA Rudder
// $--RSA, x.x, A, x.x, A*hh<CR><LF>
bool TwoCanDevice::DecodePGN127245(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte instance;
		instance = payload[0];

		byte directionOrder;
		directionOrder = payload[1] & 0x03;

		short angleOrder; // 0.0001 radians
		angleOrder = payload[2] | (payload[3] << 8);

		short position; // 0.0001 radians
		position = payload[4] | (payload[5] << 8);

		if (TwoCanUtils::IsDataValid(position)) {
			// Main (or Starboard Rudder
			if (instance == 0) { 

				if ((autopilotModel != AUTOPILOT_MODEL::NONE) && (twoCanAutopilot != nullptr)) {
					wxString jsonResponse;
					twoCanAutopilot->EncodeRudderAngle(RADIANS_TO_DEGREES((float)position / 10000), &jsonResponse);
					SendPluginMessage("TWOCAN_AUTOPILOT_RESPONSE", jsonResponse);
				}

				nmeaSentences->push_back(wxString::Format("$IIRSA,%.2f,A,0.0,V", RADIANS_TO_DEGREES((float)position / 10000)));
				return TRUE;
			}
			// Port Rudder
			else if (instance == 1) {
				nmeaSentences->push_back(wxString::Format("$IIRSA,0.0,V,%.2f,A", RADIANS_TO_DEGREES((float)position / 10000)));
				return TRUE;
			}
			return FALSE;
		}
		else {
			return FALSE;
		}
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
		
		// Sign of variation and deviation corresponds to East (E) or West (W)
		
		if (headingReference == HEADING_MAGNETIC) {
		
			if (TwoCanUtils::IsDataValid(heading)) {

				nmeaSentences->push_back(wxString::Format("$IIHDM,%.2f,M", RADIANS_TO_DEGREES((float)heading / 10000)));
			
				if (TwoCanUtils::IsDataValid(deviation)) {
				
					if (TwoCanUtils::IsDataValid(variation)) {
						// heading, deviation and variation all valid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,%.2f,%c,%.2f,%c", RADIANS_TO_DEGREES((float)heading / 10000), \
							RADIANS_TO_DEGREES((float)deviation / 10000), deviation >= 0 ? 'E' : 'W', \
							RADIANS_TO_DEGREES((float)variation / 10000), variation >= 0 ? 'E' : 'W'));
						return TRUE;
					}
				
					else {
						// heading, deviation are valid, variation invalid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,%.2f,%c,,", RADIANS_TO_DEGREES((float)heading / 10000), \
							RADIANS_TO_DEGREES((float)deviation / 10000), deviation >= 0 ? 'E' : 'W'));
						return TRUE;
					}
				}
				
				else {
					if (TwoCanUtils::IsDataValid(variation)) {
						// heading and variation valid, deviation invalid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,,,%.2f,%c", RADIANS_TO_DEGREES((float)heading / 10000), \
							RADIANS_TO_DEGREES((float)variation / 10000), variation >= 0 ? 'E' : 'W'));
						return TRUE;
					}
					else {
						// heading valid, deviation and variation both invalid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,,,,", RADIANS_TO_DEGREES((float)heading / 10000)));
						return TRUE;
		
					}	
					
				}
			
			}
			else {
				return FALSE;
			}
		}
		else if (headingReference == HEADING_TRUE) {
			if (TwoCanUtils::IsDataValid(heading)) {
				nmeaSentences->push_back(wxString::Format("$IIHDT,%.2f", RADIANS_TO_DEGREES((float)heading / 10000)));
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

		int rateOfTurn;
		rateOfTurn = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		// convert radians per second to degress per minute
		// -ve sign means turning to port
		
		if (TwoCanUtils::IsDataValid(rateOfTurn)) {
			nmeaSentences->push_back(wxString::Format("$IIROT,%.2f,A", RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-8 * 60)));
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

// Decode PGN 127257 NMEA Attitude
// $--XDR,a,x.x,a,c--c,...…………...a,x.x,a,c--c*hh<CR><LF>
//        |  |  |   |      |        |
//        |  |  |   |      |      Transducer 'n'1
//        |  |  |   |   Data for variable # of transducers
//        |  |  | Transducer #1 ID
//        |  | Units of measure, Transducer #12
//        | Measurement data, Transducer #1
//     Transducer type, Transducer #1
// Yaw, Pitch & Roll - Transducer type is A (Angular displacement), Units of measure is D (degrees)

bool TwoCanDevice::DecodePGN127257(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		short yaw;
		yaw = payload[1] | (payload[2] << 8);

		short pitch;
		pitch = payload[3] | (payload[4] << 8);

		short roll;
		roll = payload[5] | (payload[6] << 8);

		wxString xdrString;

		// BUG BUG Not sure if Dashboard supports yaw and whether roll should be ROLL or HEEL
		// BUG BUG NMEA 183 v4.11 standard defines Pitch, Yaw & Roll, however don't want to break the existing dashboard
		if (TwoCanUtils::IsDataValid(yaw)) {
			xdrString.Append(wxString::Format("A,%0.2f,D,YAW,", RADIANS_TO_DEGREES((float)yaw / 10000)));
		}

		if (TwoCanUtils::IsDataValid(pitch)) {
			xdrString.Append(wxString::Format("A,%0.2f,D,PITCH,", RADIANS_TO_DEGREES((float)pitch / 10000)));
		}

		if (TwoCanUtils::IsDataValid(roll)) {
			xdrString.Append(wxString::Format("A,%0.2f,D,ROLL,", RADIANS_TO_DEGREES((float)roll / 10000)));
		}

		if (xdrString.length() > 0) {
			xdrString.Prepend("$IIXDR,");
			nmeaSentences->push_back(xdrString);
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


// Decode PGN 127258 NMEA Magnetic Variation
bool TwoCanDevice::DecodePGN127258(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte variationSource; //4 bits
		variationSource = payload[1] & 0x0F;

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[2] | (payload[3] << 8);

		short variation;
		variation = payload[4] | (payload[5] << 8);

		// Persist variation for use by other constructed sentences such as RMC
		magneticVariation = variation;

		variation = RADIANS_TO_DEGREES((float)variation / 10000);

		// BUG BUG Needs to be added to other sentences such as HDG and RMC conversions
		// As there is no direct NMEA 0183 sentence just for variation
		return FALSE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127488 NMEA Engine Parameters, Rapid Update
bool TwoCanDevice::DecodePGN127488(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte engineInstance;
		engineInstance = payload[0];

		unsigned short engineSpeed;
		engineSpeed = payload[1] | (payload[2] << 8);

		unsigned short engineBoostPressure;
		engineBoostPressure = payload[3] | (payload[4] << 8);

		// BUG BUG Need to clarify units & resolution, although unlikely to use this anywhere
		short engineTrim;
		engineTrim = payload[5];

		// Note that until we receive data from engine instance 1, we will always assume it is a single engine vessel
		if (engineInstance > 0) {
			IsMultiEngineVessel = TRUE;
		}

		if (TwoCanUtils::IsDataValid(engineSpeed)) {
			// BUGB BUG Note, Now using NMEA 183 v4.11 standard XDR names
			nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,Engine#%1d", engineSpeed * 0.25f, engineInstance));
			/*
			switch (engineInstance) {
				// Note use of flag to identify whether single engine or dual engine as
				// engineInstance 0 in a dual engine configuration is the port engine
				// BUG BUG Should I use XDR or RPM sentence ?? Depends on how I code the Engine Dashboard !!
				case 0:
					if (IsMultiEngineVessel) {
						nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,PORT", engineSpeed * 0.25f));
						// nmeaSentences->push_back(wxString::Format("$IIRPM,E,2,%.2f,,A", engineSpeed * 0.25f));
					}
					else {
						nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,MAIN", engineSpeed * 0.25f));
						// nmeaSentences->push_back(wxString::Format("$IIRPM,E,0,%.2f,,A", engineSpeed * 0.25f));
					}
					break;
				case 1:
					nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,STBD", engineSpeed * 0.25f));
					// nmeaSentences->push_back(wxString::Format("$IIRPM,E,1,%.2f,,A", engineSpeed * 0.25f));
					break;
				default:
					nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,MAIN", engineSpeed * 0.25f));
					// nmeaSentences->push_back(wxString::Format("$IIRPM,E,0,%.2f,,A", engineSpeed * 0.25f));
					break;
			}
			*/
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

// Decode PGN 127489 NMEA Engine Parameters, Dynamic
bool TwoCanDevice::DecodePGN127489(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte engineInstance;
		engineInstance = payload[0];

		unsigned short oilPressure; // hPa (1hPa = 100Pa)
		oilPressure = payload[1] | (payload[2] << 8);

		unsigned short oilTemperature; // 0.01 degree resolution, in Kelvin
		oilTemperature = payload[3] | (payload[4] << 8);

		unsigned short engineTemperature; // 0.01 degree resolution, in Kelvin
		engineTemperature = payload[5] | (payload[6] << 8);

		unsigned short alternatorPotential; // 0.01 Volts
		alternatorPotential = payload[7] | (payload[8] << 8);

		unsigned short fuelRate; // 0.1 Litres/hour
		fuelRate = payload[9] | (payload[10] << 8);

		unsigned short totalEngineHours;  // seconds
		totalEngineHours = payload[11] | (payload[12] << 8) | (payload[13] << 16) | (payload[14] << 24);

		unsigned short coolantPressure; // hPA
		coolantPressure = payload[15] | (payload[16] << 8);

		unsigned short fuelPressure; // hPa
		fuelPressure = payload[17] | (payload[18] << 8);

		unsigned short reserved;
		reserved = payload[19];

		short statusOne;
		statusOne = payload[20] | (payload[21] << 8);
		// BUG BUG Think of using XDR switch status with meaningful naming
		// XDR parameters, "S", No units, "1" = On, "0" = Off
		// Eg. "$IIXDR,S,1,,S100,S,1,,S203" to indicate Status One - Check Engine, Status 2 - Maintenance Needed
		// BUG BUG Would need either icons or text messages to display the status in the dashboard
		// {"0": "Check Engine"},
		// { "1": "Over Temperature" },
		// { "2": "Low Oil Pressure" },
		// { "3": "Low Oil Level" },
		// { "4": "Low Fuel Pressure" },
		// { "5": "Low System Voltage" },
		// { "6": "Low Coolant Level" },
		// { "7": "Water Flow" },
		// { "8": "Water In Fuel" },
		// { "9": "Charge Indicator" },
		// { "10": "Preheat Indicator" },
		// { "11": "High Boost Pressure" },
		// { "12": "Rev Limit Exceeded" },
		// { "13": "EGR System" },
		// { "14": "Throttle Position Sensor" },
		// { "15": "Emergency Stop" }]

		short statusTwo;
		statusTwo = payload[22] | (payload[23] << 8);

		// {"0": "Warning Level 1"},
		// { "1": "Warning Level 2" },
		// { "2": "Power Reduction" },
		// { "3": "Maintenance Needed" },
		// { "4": "Engine Comm Error" },
		// { "5": "Sub or Secondary Throttle" },
		// { "6": "Neutral Start Protect" },
		// { "7": "Engine Shutting Down" }]

		byte engineLoad;  // percentage
		engineLoad = payload[24];

		byte engineTorque; // percentage
		engineTorque = payload[25];

		// As above, until data is received from engine instance 1 we always assume a single engine vessel
		if (engineInstance > 0) {
			IsMultiEngineVessel = TRUE;
		}

		// BUG BUG Instead of using logical and, separate into separate sentences so if invalid value for one or two sensors, we still send something
		if ((TwoCanUtils::IsDataValid(oilPressure)) && (TwoCanUtils::IsDataValid(engineTemperature)) && (TwoCanUtils::IsDataValid(alternatorPotential))) {
			// BUG BUG Note, Now using NMEA 183 v4.11 standard XDR names
			nmeaSentences->push_back(wxString::Format("$IIXDR,P,%.2f,P,EngineOil#%1d,C,%.2f,C,Engine#%1d,U,%.2f,V,Alternator#%1d", 
				(float)(oilPressure * 100.0f), engineInstance,
				(float)(engineTemperature * 0.01f) - CONST_KELVIN, engineInstance,
				(float)(alternatorPotential * 0.01f), engineInstance));
			// Type G = Generic, For deprecated TwoCan naming I defined units as H to indicate hours
			// NMEA 183 v4.11 does not stipulate a field for the units. Until identified otherwise, leave blank
			nmeaSentences->push_back(wxString::Format("$IIXDR,G,%.2f,,Engine#%1d", (float)totalEngineHours / 3600, engineInstance));
			
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

// Decode PGN 127505 NMEA Fluid Levels
bool TwoCanDevice::DecodePGN127505(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte instance;
		instance = payload[0] & 0x0F;

		byte tankType;
		tankType = (payload[0] & 0xF0) >> 4;

		unsigned short tankLevel; // percentage in 0.025 increments
		tankLevel = payload[1] | (payload[2] << 8);

		unsigned int tankCapacity; // 0.1 L
		tankCapacity = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);
		// BUG BUG Note, Now using NMEA 4.11 standard XDR names
		if (TwoCanUtils::IsDataValid(tankLevel)) {
			switch (tankType) {
				case TANK_FUEL:
					nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,Fuel#%1d", (float)tankLevel / QUARTER_PERCENT, instance));
					break;
				case TANK_FRESHWATER:
					nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,FreshWater#%1d", (float)tankLevel / QUARTER_PERCENT, instance));
					break;
				case TANK_WASTEWATER:
					nmeaSentences->push_back(wxString::Format("$IIXDR,v,%.2f,P,WasteWater#%1d", (float)tankLevel / QUARTER_PERCENT, instance));
					break;
				case TANK_LIVEWELL:
					nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,LiveWellWater#%1d", (float)tankLevel / QUARTER_PERCENT, instance));
					break;
				case TANK_OIL:
					nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,Oil#%1d", (float)tankLevel / QUARTER_PERCENT, instance));
					break;
				case TANK_BLACKWATER:
					nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,BlackWater#%1d", (float)tankLevel / QUARTER_PERCENT, instance));
					break;
			}
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

// Decode PGN 127508 NMEA Battery Status
bool TwoCanDevice::DecodePGN127508(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte batteryInstance;
		batteryInstance = payload[0] & 0xF;

		unsigned short batteryVoltage; // 0.01 volts
		batteryVoltage = payload[1] | (payload[2] << 8);

		short batteryCurrent; // 0.1 amps	
		batteryCurrent = payload[3] | (payload[4] << 8);
		
		unsigned short batteryTemperature; // 0.01 degree resolution, in Kelvin
		batteryTemperature = payload[5] | (payload[6] << 8);
		
		byte sid;
		sid = payload[7];
		
		// BUG BUG Note, Now using NMEA 183 v4.11 standard XDR names
		if ((TwoCanUtils::IsDataValid(batteryVoltage)) && (TwoCanUtils::IsDataValid(batteryCurrent))) {
			nmeaSentences->push_back(wxString::Format("$IIXDR,U,%.2f,V,Battery#%1d,I,%.2f,A,Battery#%1d,C,%.2f,C,Battery#%1d", 
				(float)(batteryVoltage * 0.01f), batteryInstance, 
				(float)(batteryCurrent * 0.1f), batteryInstance, 
				(float)(batteryTemperature * 0.01f) - CONST_KELVIN, batteryInstance));			
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
		
		byte referenceType;
		referenceType = payload[5] & 0x07;

		unsigned short direction;
		direction = payload[6] | (payload[7] << 8);

		if (TwoCanUtils::IsDataValid(speedWaterReferenced)) {

			// BUG BUG Maintain heading globally from other sources to insert corresponding values into sentence	
			nmeaSentences->push_back(wxString::Format("$IIVHW,,T,,M,%.2f,N,%.2f,K", (float)speedWaterReferenced * CONVERT_MS_KNOTS / 100, \
				(float)speedWaterReferenced * CONVERT_MS_KMH / 100));
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

// Decode PGN 128267 NMEA Depth
// $--DPT,x.x,x.x,x.x*hh<CR><LF>
// $--DBT,x.x,f,x.x,M,x.x,F*hh<CR><LF>
bool TwoCanDevice::DecodePGN128267(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned int depth; // /100
		depth = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		short offset; // /1000
		offset = payload[5] | (payload[6] << 8);

		byte maxRange; // * 10
		maxRange = payload[7];

		if (TwoCanUtils::IsDataValid(depth)) {
			
			// OpenCPN Dashboard now accepts NMEA 183 DPT sentences. (at least noticed in 5.6.x) 
			wxString depthSentence;

			depthSentence = wxString::Format("$IIDPT,%.2f,%.2f", (float)depth / 100, (float)offset / 1000);
			if (maxRange != 0xFF) {
				depthSentence.Append(wxString::Format(",%d", maxRange * 10));
			}
			else {
				depthSentence.Append(",");
			}
			
			nmeaSentences->push_back(depthSentence);
			
			// Deprecated
			// OpenCPN Dashboard only accepts DBT sentence
			//nmeaSentences->push_back(wxString::Format("$IIDBT,%.2f,f,%.2f,M,%.2f,F", CONVERT_METRES_FEET * (double)depth / 100, \
			//	(double)depth / 100, CONVERT_METRES_FATHOMS * (double)depth / 100));
			
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

// Decode PGN 128275 NMEA Distance Log
// $--VLW, x.x, N, x.x, N, x.x, N, x.x, N*hh<CR><LF>
//          |       |       |       Total cumulative water distance, Nm
//          |       |       Water distance since reset, Nm
//          |      Total cumulative ground distance, Nm
//          Ground distance since reset, Nm

bool TwoCanDevice::DecodePGN128275(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[0] | (payload[1] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		unsigned int cumulativeDistance;
		cumulativeDistance = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		unsigned int tripDistance;
		tripDistance = payload[10] | (payload[11] << 8) | (payload[12] << 16) | (payload[13] << 24);

		if (TwoCanUtils::IsDataValid(cumulativeDistance)) {
			if (TwoCanUtils::IsDataValid(tripDistance)) {
				nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,%.2f,N,%.2f,N", CONVERT_METRES_NAUTICAL_MILES * tripDistance, CONVERT_METRES_NAUTICAL_MILES * cumulativeDistance));
				return TRUE;
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,,N,%.2f,N", CONVERT_METRES_NAUTICAL_MILES * cumulativeDistance));
				return TRUE;
			}
		}
		else {
			if (TwoCanUtils::IsDataValid(tripDistance)) {
				nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,%.2f,N,,N", CONVERT_METRES_NAUTICAL_MILES * tripDistance));
				return TRUE;
			}
			else {
				return FALSE;
			}
			
		}
					
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129025 NMEA Position Rapid Update
// $--GLL, llll.ll, a, yyyyy.yy, a, hhmmss.ss, A, a*hh<CR><LF>
//                                           Status A valid, V invalid
//                                               mode - note Status = A if Mode is A (autonomous) or D (differential)
bool TwoCanDevice::DecodePGN129025(const byte *payload, std::vector<wxString> *nmeaSentences, byte address) {
	if (payload != NULL) { 

		// Initial reception
		if (preferredGPS.sourceAddress == CONST_GLOBAL_ADDRESS) {
			preferredGPS.sourceAddress = address;
		}

		if (preferredGPS.sourceAddress == address) {

			int latitude;
			latitude = (int)payload[0] | ((int)payload[1] << 8) | ((int)payload[2] << 16) | ((int)payload[3] << 24);

			int longitude;
			longitude = (int)payload[4] | ((int)payload[5] << 8) | ((int)payload[6] << 16) | ((int)payload[7] << 24);

			if (TwoCanUtils::IsDataValid(latitude) && TwoCanUtils::IsDataValid(longitude)) {

				double latitudeDouble = ((double)latitude * 1e-7);
				int latitudeDegrees = trunc(latitudeDouble);
				double latitudeMinutes = (latitudeDouble - latitudeDegrees) * 60;

				double longitudeDouble = ((double)longitude * 1e-7);
				int longitudeDegrees = trunc(longitudeDouble);
				double longitudeMinutes = (longitudeDouble - longitudeDegrees) * 60;

				char gpsMode;
				gpsMode = 'A';

				// BUG BUG Verify S & W values are indeed negative
				// BUG BUG Mode & Status are not available in PGN 129025
				// BUG BUG UTC Time is not available in PGN 129025

				wxDateTime now = wxDateTime::Now();
				wxDateTime tm = now - gpsTimeOffset;

				nmeaSentences->push_back(wxString::Format("$IIGLL,%02d%07.4f,%c,%03d%07.4f,%c,%s,%c,%c", abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', \
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', tm.Format("%H%M%S.00", wxDateTime::UTC).ToAscii(), gpsMode, ((gpsMode == 'A') || (gpsMode == 'D')) ? 'A' : 'V'));
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
	else {
		return FALSE;
	}
}

// Decode PGN 129026 NMEA COG SOG Rapid Update
// $--VTG,x.x,T,x.x,M,x.x,N,x.x,K,a*hh<CR><LF>
bool TwoCanDevice::DecodePGN129026(const byte *payload, std::vector<wxString> *nmeaSentences, byte address) {
	if ((payload != NULL) && (address == preferredGPS.sourceAddress)) {

		byte sid;
		sid = payload[0];

		// True = 0, Magnetic = 1
		byte headingReference;
		headingReference = (payload[1] & 0x03);

		unsigned short courseOverGround;
		courseOverGround = (payload[2] | (payload[3] << 8));

		unsigned short speedOverGround;
		speedOverGround = (payload[4] | (payload[5] << 8));

		// Persist SOG & COG for other constructed sentences
		vesselCOG = courseOverGround;
		vesselSOG = speedOverGround;

		// BUG BUG if Heading Ref = True (0), then ignore %.2f,M and vice versa if Heading Ref = Magnetic (1), ignore %.2f,T
		// BUG BUG GPS Mode should be obtained rather than assumed
		
		if (headingReference == HEADING_TRUE) {
			if (TwoCanUtils::IsDataValid(courseOverGround)) {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
				else {
					nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,,M,,N,,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
			}
			else {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,,M,%.2f,N,%.2f,K,%c", \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;
				}
				else {
					return FALSE;
				}
				
			}
			
		}
		
		else if (headingReference == HEADING_MAGNETIC) {
			if (TwoCanUtils::IsDataValid(courseOverGround)) {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,%.2f,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
				else {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,%.2f,M,,N,,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
			}
			else {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,,M,%.2f,N,%.2f,K,%c", \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;
				}
				else {
					return FALSE;
				}
				
			}
			
			
		}
		else {
			return FALSE;
		}
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

// $--RMC,hhmmss.ss,A,ddmm.mm,a,dddmm.mm,a,x.x,x.x,xxxx,x.x,a,m,s*hh<CR><LF>
//                  |                       |   |        |  | status
//                Validity                 SOG COG Variation FAA Mode

bool TwoCanDevice::DecodePGN129029(const byte *payload, std::vector<wxString> *nmeaSentences, byte address) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[1] | (payload[2] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		long long latitude;
		latitude = (((long long)payload[7] | ((long long)payload[8] << 8) | ((long long)payload[9] << 16) | ((long long)payload[10] << 24) \
			| ((long long)payload[11] << 32) | ((long long)payload[12] << 40) | ((long long)payload[13] << 48) | ((long long)payload[14] << 56)));


		long long longitude;
		longitude = (((long long)payload[15] | ((long long)payload[16] << 8) | ((long long)payload[17] << 16) | ((long long)payload[18] << 24) \
			| ((long long)payload[19] << 32) | ((long long)payload[20] << 40) | ((long long)payload[21] << 48) | ((long long)payload[22] << 56)));

		if (TwoCanUtils::IsDataValid(latitude) && TwoCanUtils::IsDataValid(longitude)) {

			double latitudeDouble = ((double)latitude * 1e-16);
			double latitudeDegrees = trunc(latitudeDouble);
			double latitudeMinutes = (latitudeDouble - latitudeDegrees) * 60;

			double longitudeDouble = ((double)longitude * 1e-16);
			double longitudeDegrees = trunc(longitudeDouble);
			double longitudeMinutes = (longitudeDouble - longitudeDegrees) * 60;

			double altitude;
			altitude = 1e-6 * (((long long)payload[23] | ((long long)payload[24] << 8) | ((long long)payload[25] << 16) | ((long long)payload[26] << 24) \
				| ((long long)payload[27] << 32) | ((long long)payload[28] << 40) | ((long long)payload[29] << 48) | ((long long)payload[30] << 56)));


			byte fixType;
			byte fixMethod;

			fixMethod = payload[31] & 0x0F;
			fixType = (payload[31] & 0xF0) >> 4;

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

			// Automagic preference if multiple GPS sources present
			// Initial reception
			if (preferredGPS.sourceAddress == CONST_GLOBAL_ADDRESS) {
				preferredGPS.sourceAddress = address;
				preferredGPS.lastUpdate = wxDateTime::Now();
				preferredGPS.hdop = hDOP;
				preferredGPS.hdopRetry = 0;
			}
			else {
				// Current GPS source
				if (address == preferredGPS.sourceAddress) {
					preferredGPS.lastUpdate = wxDateTime::Now();
					preferredGPS.hdop = hDOP;
				}
				// An alternative GPS source
				else {
					// Current source has not been updated in the last 30 seconds, failover
					if (wxDateTime::Now() > preferredGPS.lastUpdate + wxTimeSpan::Seconds(30)) {
						preferredGPS.sourceAddress = address;
						preferredGPS.lastUpdate = wxDateTime::Now();
						preferredGPS.hdop = hDOP;
						preferredGPS.hdopRetry = 0;
					}
					// The current source has a greater HDOP than the alternative
					else if (preferredGPS.hdop > hDOP) {
						// And has more than ten successive better hdop values, failover
						if (preferredGPS.hdopRetry > 10) {
							preferredGPS.hdopRetry = 0;
							preferredGPS.sourceAddress = address;
							preferredGPS.lastUpdate = wxDateTime::Now();
							preferredGPS.hdop = hDOP;
						}
						else {
							preferredGPS.hdopRetry++;
							// The current source is to be kept until the alternative has more than ten successive better hdop values
							return FALSE;
						}
					}
					else {
						// The current source is to be kept
						return FALSE;
					}

				}
				
			}

			nmeaSentences->push_back(wxString::Format("$IIGGA,%s,%02.0f%07.4f,%c,%03.0f%07.4f,%c,%d,%d,%.2f,%.1f,M,%.1f,M,,", \
				epoch.Format("%H%M%S").ToAscii(), fabs(latitudeDegrees), fabs(latitudeMinutes), latitudeDegrees >= 0 ? 'N' : 'S', \
				fabs(longitudeDegrees), fabs(longitudeMinutes), longitudeDegrees >= 0 ? 'E' : 'W', \
				fixType, numberOfSatellites, (double)hDOP * 0.01f, (double)altitude * 1e-6, \
				(double)geoidalSeparation * 0.01f));

			OutputDebugStringA(nmeaSentences->at(1).ToAscii().data());

			// Construct a NMEA 183 RMC sentence
			/*
			if ((TwoCanUtils::IsDataValid(vesselCOG)) && (TwoCanUtils::IsDataValid(vesselSOG)) && (TwoCanUtils::IsDataValid(magneticVariation))) {
				nmeaSentences->push_back(wxString::Format("$IIRMC,%s,%c,%02.0f%07.4f,%c,%03.0f%07.4f,%c,%.2f,%.2f,%s,%.2f,%c,%c,", \
					tm.Format("%H%M%S").ToAscii(), GPS_STATUS_VALID, fabs(latitudeDegrees), fabs(latitudeMinutes), latitudeDegrees >= 0 ? 'N' : 'S', \
					fabs(longitudeDegrees), fabs(longitudeMinutes), longitudeDegrees >= 0 ? 'E' : 'W', \
					(float)vesselSOG * CONVERT_MS_KNOTS / 100, RADIANS_TO_DEGREES((float)vesselCOG) * 0.0001f, tm.Format("%d%m%y").ToAscii(), \
					RADIANS_TO_DEGREES((float)magneticVariation) * 0.0001f, FAA_MODE_AUTONOMOUS, GPS_MODE_AUTONOMOUS));

			}
			*/

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
	else {
		return FALSE;
	}
}

// Decode PGN 129033 NMEA Date & Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
bool TwoCanDevice::DecodePGN129033(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {
		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[0] | (payload[1] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		short localOffset;
		localOffset = payload[6] | (payload[7] << 8);

		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		// save the time offset between the computer time & the received gps time for use in constructed sentences such as RMC
		gpsTimeOffset = wxDateTime::Now() - epoch; 

		if ((TwoCanUtils::IsDataValid(daysSinceEpoch))  && (TwoCanUtils::IsDataValid(secondsSinceMidnight))) {
			nmeaSentences->push_back(wxString::Format("$IIZDA,%s,%d,%d", epoch.Format("%H%M%S,%d,%m,%Y"), (int)localOffset / 60, localOffset % 60));
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

// Template for NMEA183 AIS VDM messages
// !--VDM,x,x,x,a,s--s,x*hh
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

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) * 1e-7;
		
		double latitude;
		latitude = (payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24)) * 1e-7;

		byte positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		byte raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		byte timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		unsigned short courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		unsigned short speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		unsigned int communicationState;
		communicationState = payload[18] | (payload[19] << 8) | ((payload[20] & 0x07) << 16);

		byte transceiverInformation; 
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		char aisChannel; 
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		unsigned short trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		short rateOfTurn;
		rateOfTurn = payload[23] | (payload[24] << 8);

		byte navigationalStatus;
		navigationalStatus = payload[25] & 0x0F;

		byte manoeuverIndicator;
		manoeuverIndicator = payload[25] & 0x30 >> 4;

		byte reserved;
		reserved = (payload[25] & 0xC0) >> 6;

		byte spare;
		spare = (payload[26] & 0x07);

		byte reservedForRegionalApplications;
		reservedForRegionalApplications = (payload[26] & 0xF8) >> 3;

		byte sequenceID;
		sequenceID = payload[27];

		// Encode correct AIS rate of turn from sensor data as per ITU M.1371 standard
		// BUG BUG fix this up to remove multiple calculations. 
		int AISRateOfTurn;

		// Undefined/not available
		if (!TwoCanUtils::IsDataValid(rateOfTurn)) {
			AISRateOfTurn = -128;
		}
		else {
			// Greater or less than 708 degrees/min
			if ((RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-5) * 60) > 708) {
				AISRateOfTurn = 127;
			}

			else if ((RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-5) * 60) < -708) {
				AISRateOfTurn = -127;
			}

			else {
				AISRateOfTurn = 4.733 * sqrt(RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-5) * 60);
			}
		}
			
		// Encode VDM message using 6 bit ASCII 

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 4, navigationalStatus);
		AISInsertInteger(binaryData, 42, 8, AISRateOfTurn);
		AISInsertInteger(binaryData, 50, 10, TwoCanUtils::IsDataValid(speedOverGround) ? CONVERT_MS_KNOTS * speedOverGround * 0.1f : 1023);
		AISInsertInteger(binaryData, 60, 1, positionAccuracy);
		AISInsertInteger(binaryData, 61, 28, (int)(longitude * 600000));
		AISInsertInteger(binaryData, 89, 27, (int)(latitude * 600000));
		AISInsertInteger(binaryData, 116, 12, TwoCanUtils::IsDataValid(courseOverGround) ? RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f : 3600);
		AISInsertInteger(binaryData, 128, 9, TwoCanUtils::IsDataValid(trueHeading) ? RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f : 511);
		AISInsertInteger(binaryData, 137, 6, timeStamp);
		AISInsertInteger(binaryData, 143, 2, manoeuverIndicator);
		AISInsertInteger(binaryData, 145, 3, spare);
		AISInsertInteger(binaryData, 148, 1, raimFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);

		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		} 
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}

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

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) * 1e-7;

		double latitude;
		latitude = (payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24)) * 1e-7;
		
		byte positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		byte raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		byte timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		unsigned short courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		unsigned short  speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		unsigned int communicationState;
		communicationState = (payload[18] | (payload[19] << 8) | (payload[20] << 16)) & 0x7FFFF;

		byte transceiverInformation;
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		unsigned short trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		byte regionalReservedA;
		regionalReservedA = payload[23];

		byte regionalReservedB;
		regionalReservedB = payload[24] & 0x03;

		byte unitFlag;
		unitFlag = (payload[24] & 0x04) >> 2;

		byte displayFlag;
		displayFlag = (payload[24] & 0x08) >> 3;

		byte dscFlag;
		dscFlag = (payload[24] & 0x10) >> 4;

		byte bandFlag;
		bandFlag = (payload[24] & 0x20) >> 5;

		byte msg22Flag;
		msg22Flag = (payload[24] & 0x40) >> 6;

		byte assignedModeFlag;
		assignedModeFlag = (payload[24] & 0x80) >> 7;
		
		byte sotdmaFlag;
		sotdmaFlag = payload[25] & 0x01;
		
		// Encode VDM Message using 6bit ASCII
				
		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 8, 0xFF); // spare
		AISInsertInteger(binaryData, 46, 10, TwoCanUtils::IsDataValid(speedOverGround) ? CONVERT_MS_KNOTS * speedOverGround * 0.1f : 1023);
		AISInsertInteger(binaryData, 56, 1, positionAccuracy);
		AISInsertInteger(binaryData, 57, 28, (int)(longitude * 600000));
		AISInsertInteger(binaryData, 85, 27, (int)(latitude * 600000));
		AISInsertInteger(binaryData, 112, 12, TwoCanUtils::IsDataValid(courseOverGround) ? RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f : 3600);
		AISInsertInteger(binaryData, 124, 9, TwoCanUtils::IsDataValid(trueHeading) ? RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f : 511);
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
		
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		
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

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) * 1e-7;

		double latitude;
		latitude = (payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24)) * 1e-7;

		byte positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		byte raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		byte timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		unsigned short courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		unsigned short speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		byte regionalReservedA;
		regionalReservedA = payload[18];

		byte regionalReservedB;
		regionalReservedB = payload[19] & 0x0F;

		byte reservedA;
		reservedA = (payload[19] & 0xF0) >> 4;

		byte shipType;
		shipType = payload[20];

		unsigned short trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		byte reservedB;
		reservedB = payload[23] & 0x0F;

		byte gnssType;
		gnssType = (payload[23] & 0xF0) >> 4;

		unsigned short shipLength;
		shipLength = payload[24] | (payload[25] << 8);
		
		unsigned short shipBeam;
		shipBeam = payload[26] | (payload[27] << 8);

		unsigned short refStarboard;
		refStarboard = payload[28] | (payload[29] << 8);

		unsigned short refBow;
		refBow = payload[30] | (payload[31] << 8);
		
		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName.append(1, (char)payload[32 + i]);
		}
		
		byte dteFlag;
		dteFlag = payload[52] & 0x01;

		byte assignedModeFlag;
		assignedModeFlag = (payload[52] & 0x02) >> 1;

		byte spare;
		spare = (payload[52] & 0x3C) >> 2;

		byte transceiverInformation;
		transceiverInformation = ((payload[52] & 0xC0) >> 6) | ((payload[53] & 0x07) << 2);

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		// Encode VDM Message using 6bit ASCII
			
		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 8, regionalReservedA);
		AISInsertInteger(binaryData, 46, 10, TwoCanUtils::IsDataValid(speedOverGround) ? CONVERT_MS_KNOTS * speedOverGround * 0.1f : 1023);
		AISInsertInteger(binaryData, 56, 1, positionAccuracy);
		AISInsertInteger(binaryData, 57, 28, (int)(longitude * 600000));
		AISInsertInteger(binaryData, 85, 27, (int)(latitude * 600000));
		AISInsertInteger(binaryData, 112, 12, TwoCanUtils::IsDataValid(courseOverGround) ? RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f : 3600);
		AISInsertInteger(binaryData, 124, 9, TwoCanUtils::IsDataValid(trueHeading) ? RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f : 511);
		AISInsertInteger(binaryData, 133, 6, timeStamp);
		AISInsertInteger(binaryData, 139, 4, regionalReservedB);
		AISInsertString(binaryData, 143, 120, shipName);
		AISInsertInteger(binaryData, 263, 8, shipType);
		AISInsertInteger(binaryData, 271, 9, refBow / 10);
		AISInsertInteger(binaryData, 280, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 289, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 295, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 301, 4, gnssType);
		AISInsertInteger(binaryData, 305, 1, raimFlag);
		AISInsertInteger(binaryData, 306, 1, dteFlag);
		AISInsertInteger(binaryData, 307, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 308, 4, spare);

		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send the VDM message, Note no fillbits
		// One day I'll remember why I chose 28 as the length of a multisentence VDM message
		// BUG BUG Or just send two messages, 26 bytes long
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);

		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages -1) { // This is the last message
				if (transceiverInformation & 0x04) {
					nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28))));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28))));
				}
			}
			else {
				if (transceiverInformation & 0x04) {
					nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
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

// Decode PGN 129041 AIS Aids To Navigation (AToN) Report
// AIS Message Type 21
bool TwoCanDevice::DecodePGN129041(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(358);

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) * 1e-7;
		
		double latitude;
		latitude = (payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24)) * 1e-7;
		
		byte positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		byte raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		byte timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		unsigned short shipLength;
		shipLength = payload[14] | (payload[15] << 8);

		unsigned short shipBeam;
		shipBeam = payload[16] | (payload[17] << 8);

		unsigned short refStarboard;
		refStarboard = payload[18] | (payload[19] << 8);

		unsigned short refBow;
		refBow = payload[20] | (payload[21] << 8);
		
		byte AToNType;
		AToNType = payload[22] & 0x1F;

		byte offPositionFlag;
		offPositionFlag = (payload[22] & 0x20) >> 5;

		byte virtualAToN;
		virtualAToN = (payload[22] & 0x40) >> 6;;

		byte assignedModeFlag;
		assignedModeFlag = (payload[22] & 0x80) >> 7;

		byte spare;
		spare = payload[23] & 0x01;

		byte gnssType;
		gnssType = (payload[23] & 0x1E) >> 1;

		byte reserved;
		reserved = payload[23] & 0xE0 >> 5;

		byte AToNStatus;
		AToNStatus = payload[24];

		byte transceiverInformation;
		transceiverInformation = payload[25] & 0x1F;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		byte reservedB;
		reservedB = (payload[25] & 0xE0) >> 5;

		// BUG BUG This is variable up to 20 + 14 (34) characters
		std::string AToNName;
		size_t AToNNameLength = payload[26];
		if (payload[27] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			for (size_t i = 0; i < AToNNameLength - 2; i++) {
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
		AISInsertInteger(binaryData, 164, 28, (int)(longitude * 600000));
		AISInsertInteger(binaryData, 192, 27, (int)(latitude * 600000));
		AISInsertInteger(binaryData, 219, 9, refBow / 10);
		AISInsertInteger(binaryData, 228, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 237, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 243, 6, (shipBeam / 10) - (refStarboard / 10));
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
			fillBits = 6 - ((272 + ((AToNName.length() - 20) * 6)) % 6);
			// Add padding to align on 6 bit boundary
			if (fillBits > 0) {
				AISInsertInteger(binaryData, 272 + (AToNName.length() - 20) * 6, fillBits, 0);
			}
		}
		else {
			// Add padding to align on 6 bit boundary
			fillBits = 6 - (272 % 6);
			binaryData.resize(272 + fillBits);
			if (fillBits > 0) {
				AISInsertInteger(binaryData, 272, fillBits, 0);
			}
		}
		
		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,,%c,%s,%d", 1, 1, aisChannel, encodedVDMMessage, fillBits));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,,%c,%s,%d", 1, 1, aisChannel, encodedVDMMessage, fillBits));
		}

		// Send the VDM message
		/*
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);

		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages -1) { // This is the last message
				if (transceiverInformation & 0x04) {
					nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28))));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28))));
				}
			}
			else {
				if (transceiverInformation & 0x04) {
					nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
				}
			}
		}
		
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}
		*/
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

		byte xteMode;
		xteMode = payload[1] & 0x0F;

		byte navigationTerminated;
		navigationTerminated = (payload[1] & 0xC0) >> 6;

		int crossTrackError;
		crossTrackError = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);
		
		if (TwoCanUtils::IsDataValid(crossTrackError)) {

			nmeaSentences->push_back(wxString::Format("$IIXTE,A,A,%.2f,%c,N", fabs(CONVERT_METRES_NAUTICAL_MILES * crossTrackError * 0.01f), crossTrackError < 0 ? 'L' : 'R'));
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

		unsigned int distance;
		distance = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		byte bearingRef; // 0 = Magnetic, 1 = True
		bearingRef = payload[5] & 0x03;

		byte perpendicularCrossed; // 0 = No, 1 = Yes
		perpendicularCrossed = (payload[5] & 0x0C) >> 2;

		byte circleEntered; // 0 = No, 1 = Yes
		circleEntered = (payload[5] & 0x30) >> 4;

		byte calculationType; // 0 = Great Circle, 1 = Rhumb Line
		calculationType = (payload[5] & 0xC0) >> 6;

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[10] | (payload[11] << 8);

		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		unsigned short bearingOrigin;
		bearingOrigin = (payload[12] | (payload[13] << 8)) * 0.001;

		unsigned short bearingPosition;
		bearingPosition = (payload[14] | (payload[15] << 8)) * 0.001;

		int originWaypointId;
		originWaypointId = payload[16] | (payload[17] << 8) | (payload[18] << 16) | (payload[19] << 24);

		int destinationWaypointId;
		destinationWaypointId = payload[20] | (payload[21] << 8) | (payload[22] << 16) | (payload[23] << 24);

		double latitude;
		latitude = ((payload[24] | (payload[25] << 8) | (payload[26] << 16) | (payload[27] << 24))) * 1e-7;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = (fabs(latitude) - abs(latitudeDegrees)) * 60;

		double longitude;
		longitude = ((payload[28] | (payload[29] << 8) | (payload[30] << 16) | (payload[31] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = (fabs(longitude) - abs(longitudeDegrees)) * 60;

		int waypointClosingVelocity;
		waypointClosingVelocity = (payload[32] | (payload[33] << 8)) * 0.01;

		wxDateTime timeNow;
		timeNow = wxDateTime::Now();

		if (calculationType == GREAT_CIRCLE) { 
			if (bearingRef == HEADING_TRUE) {
				nmeaSentences->push_back(wxString::Format("$IIBWC,%s,%02d%05.2f,%c,%03d%05.2f,%c,%.2f,T,,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"), 
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', 
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', 
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), 
					CONVERT_METRES_NAUTICAL_MILES * distance, destinationWaypointId));
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIBWC,%s,%02d%05.2f,%c,%03d%05.2f,%c,,T,%.2f,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"), 
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', 
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', 
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), \
					CONVERT_METRES_NAUTICAL_MILES * distance, destinationWaypointId));
			}

		}
		else { 
			if (bearingRef == HEADING_TRUE) {
				nmeaSentences->push_back(wxString::Format("$IIBWR,%s,%02d%05.2f,%c,%03d%05.2f,%c,%.2f,T,,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"),
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), \
					CONVERT_METRES_NAUTICAL_MILES * distance, destinationWaypointId));
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIBWR,%s,%02d%05.2f,%c,%03d%05.2f,%c,,T,%.2f,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"),
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
					RADIANS_TO_DEGREES((float)bearingPosition / 10000), \
					CONVERT_METRES_NAUTICAL_MILES * distance, destinationWaypointId));
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
// $--WPL,llll.ll,a,yyyyy.yy,a,c--c
bool TwoCanDevice::DecodePGN129285(const byte * payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {
		// Prepend the NMEA 183 Route sentence
		wxString routeSentence = "$IIRTE,1,1,c";

		unsigned short rps;
		rps = payload[0] | (payload[1] << 8);

		unsigned short nItems;
		nItems = payload[2] | (payload[3] << 8);

		unsigned short databaseVersion;
		databaseVersion = payload[4] | (payload[5] << 8);

		unsigned short routeID;
		routeID = payload[6] | (payload[7] << 8);

		// I presume forward/reverse
		byte direction; 
		direction = (payload[8] & 0xE0) >> 5; 

		byte supplementaryInfo;
		supplementaryInfo = (payload[8] & 0x18) >> 3;

		// NMEA reserved
		byte reservedA = payload[8] & 0x07;

		// As we need to iterate repeated fields with variable length strings
		// can't use hardcoded indexes into the payload
		int index = 9;

		std::string routeName;
		int routeNameLength = payload[index];
		index++;
		if (payload[index] == 1) {
			index++;
			// first byte of Route name indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < routeNameLength - 2; i++) {
				routeName += static_cast<char>(payload[index]);
				index++;
			}
		}

		// NMEA reserved 
		byte reservedB = payload[index];
		index++;

		// repeated fields
		for (unsigned int i = 0; i < nItems; i++) {
			unsigned short waypointID;
			waypointID = payload[index] | (payload[index + 1] << 8);

			routeSentence.append(wxString::Format(",%d", waypointID));

			index += 2;

			std::string waypointName;
			int waypointNameLength = payload[index];
			index++;
			if (payload[index] == 1) {
				// first byte of Waypoint Name indicates encoding; 0 for Unicode, 1 for ASCII
				index++;
				for (int i = 0; i < waypointNameLength - 2; i++) {
					waypointName += (static_cast<char>(payload[index]));
					index++;
				}
			}
					
			double latitude = (payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24)) * 1e-7;
			int latitudeDegrees = trunc(latitude);
			double latitudeMinutes = fabs(latitude - latitudeDegrees);

			double longitude = (payload[index + 4] | (payload[index + 5] << 8) | (payload[index + 6] << 16) | (payload[index + 7] << 24)) * 1e-7;
			int longitudeDegrees = trunc(longitude);
			double longitudeMinutes = fabs(longitude - longitudeDegrees);

			index += 8;

			// BUG BUG Do we use WaypointID or Waypoint Name ?? 
			// Use the same (WaypointID) as used in the Route Sentence
			nmeaSentences->push_back(wxString::Format("$IIWPL,%02d%05.2f,%c,%03d%05.2f,%c,%d",
				abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
				abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
				waypointID));

		}

		nmeaSentences->push_back(routeSentence);

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129539 GNSS DOP's
//        1 2 3                        14 15  16  17  18
//        | | |                         |  |   |   |   |
//$--GSA,a,a,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x.x,x.x,x.x*hh<CR><LF>
// 1. Selection mode: M=Manual, forced to operate in 2D or 3D, A=Automatic, 2D/3D
// 2. Mode (1 = no fix, 2 = 2D fix, 3 = 3D fix)
// 3..14 Satellite ID's
// 15, 16, 17 PDOP, HDOP, VDOP
bool TwoCanDevice::DecodePGN129539(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte desiredMode;
		desiredMode = payload[1] & 0x07;

		byte actualMode;
		actualMode = (payload[1] & 0x38) >> 3;
		// 0 = 1D, 1 =2D, 2 = 3D, 3 =Auto

		byte reserved;
		reserved = (payload[1] & 0xC0) >> 6;

		short hDOP; // * 0.01
		hDOP = payload[2] | (payload[3] << 8);

		short vDOP;
		vDOP = payload[4] | (payload[5] << 8);

		short tDOP;
		tDOP = payload[6] | (payload[7] << 8);

		//nmeaSentences->push_back(wxString::Format("$IIGSA,GSA,A,,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x.x,x.x,x.x));
		// Looks like we'll need to construct this sentence

		return FALSE;
	}
	else {
		return FALSE;
	}

}

// Decode PGN 129540 NMEA Satellites in View
// $--GSV,x,x,x,x,x,x,x,...*hh<CR><LF>
//        | | | | | | snr
//        | | | | | azimuth
//        | | | | elevation
//        | | | satellite id
//    total | satellites in view
//          sentence number
bool TwoCanDevice::DecodePGN129540(const byte * payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte mode;
		mode = payload[1] & 0x03;

		byte reserved;
		reserved = (payload[1] & 0xFC) >> 2;

		byte satsInView;
		satsInView = payload[2];

		unsigned int index;
		index = 3;

		wxString gsvSentence;
		int totalSentences;
		totalSentences = trunc(satsInView / 4) + ((satsInView % 4) == 0 ? 0 : 1);
		int sentenceNumber;
		sentenceNumber = 1;

		for (size_t i = 0;i < satsInView; i++) {
			
			byte prn;
			prn = payload[index];
			index += 1;

			unsigned short elevation; // radians * 0.0001
			elevation = payload[index] | (payload[index+1] << 8);
			index += 2;

			unsigned short azimuth; // radians * 0.0001
			azimuth = payload[index] | (payload[index+1] << 8);
			index += 2;

			unsigned short snr; //db * 0.01
			snr = payload[index] | (payload[index+1] << 8);
			index += 2;

			int rangeResiduals;
			rangeResiduals = payload[index] | (payload[index+1] << 8) | (payload[index+2] << 16) | (payload[index+3] << 24);
			index += 4;

			// From canboat,
			// 0 Not tracked
			// 1 Tracked
			// 2 Used
			// 3 Not tracked+Diff
			// 4 Tracked+Diff
			// 5 Used+Diff
			byte status; 
			status = payload[index] & 0x0F;
			index += 1;

			// BUG BUG Leading zeroes ??
			// BUG BUG Only generate GSV sentence for satellites used for the position fix
			if ((status == 2) || (status == 5)) {
				gsvSentence += wxString::Format(",%02d,%02d,%03d,%02d", prn, 
				(unsigned int)RADIANS_TO_DEGREES((float)elevation / 10000),
				(unsigned int)RADIANS_TO_DEGREES((float)azimuth/10000),(unsigned int)snr/100);
			}

			// Send one NMEA sentence for each quadruple (4) of satellites
			// and another for the remainder (if the number of satellites is not a multipe of 4)
			if ((((i+1)%4) == 0) || (((((i+1)%4) != 0)) && ( i == (satsInView - 1)))) {
				gsvSentence.Prepend(wxString::Format("$GPGSV,%d,%d,%d", totalSentences, sentenceNumber, satsInView));
				nmeaSentences->push_back(gsvSentence);
				gsvSentence.Empty();
				sentenceNumber++;
			}
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
		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) * 1e-7;
		
		double latitude;
		latitude = (payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24)) * 1e-7;

		byte positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		byte raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		byte reservedA;
		reservedA = (payload[13] & 0xFC) >> 2;

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[14] | (payload[15] << 8) | (payload[16] << 16) | (payload[17] << 24);

		unsigned int communicationState;
		communicationState = payload[18] | (payload[19] << 8) | ((payload[20] & 0x07) << 16);

		byte transceiverInformation;
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[21] | (payload[22] << 8);

		byte reservedB;
		reservedB = payload[23] & 0x0F;

		byte gnssType;
		gnssType = (payload[23] & 0xF0) >> 4;

		byte spare;
		spare = payload[24];

		byte longRangeFlag = 0;

		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		// Encode VDM message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 14, epoch.GetYear());
		AISInsertInteger(binaryData, 52, 4, epoch.GetMonth() + 1);
		AISInsertInteger(binaryData, 56, 5, epoch.GetDay());
		AISInsertInteger(binaryData, 61, 5, epoch.GetHour());
		AISInsertInteger(binaryData, 66, 6, epoch.GetMinute());
		AISInsertInteger(binaryData, 72, 6, epoch.GetSecond());
		AISInsertInteger(binaryData, 78, 1, positionAccuracy);
		AISInsertInteger(binaryData, 79, 28, (int)(longitude * 600000));
		AISInsertInteger(binaryData, 107, 27, (int)(latitude * 600000));
		AISInsertInteger(binaryData, 134, 4, gnssType);
		AISInsertInteger(binaryData, 138, 1, longRangeFlag); // Long Range flag doesn't appear to be set anywhere
		AISInsertInteger(binaryData, 139, 9, spare);
		AISInsertInteger(binaryData, 148, 1, raimFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		
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

		std::vector<bool> binaryData(426,0);
	
		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
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

		byte shipType;
		shipType = payload[36];

		unsigned short shipLength;
		shipLength = payload[37] | (payload[38] << 8);

		unsigned short shipBeam;
		shipBeam = payload[39] | (payload[40] << 8);

		unsigned short refStarboard;
		refStarboard = payload[41] | (payload[42] << 8);

		unsigned short refBow;
		refBow = payload[43] | (payload[44] << 8);

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[45] | (payload[46] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[47] | (payload[48] << 8) | (payload[49] << 16) | (payload[50] << 24);

		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);


		unsigned short draft;
		draft = payload[51] | (payload[52] << 8);

		std::string destination;
		for (int i = 0; i < 20; i++) {
			destination.append(1, (char)payload[53 + i]);
		}
		
		byte aisVersion;
		aisVersion = (payload[73] & 0x03);

		byte gnssType;
		gnssType = (payload[73] & 0x3C) >> 2;

		byte dteFlag;
		dteFlag = (payload[73] & 0x40) >> 6;

		byte transceiverInformation;
		transceiverInformation = payload[74] & 0x1F;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, aisVersion );
		AISInsertInteger(binaryData, 40, 30, imoNumber);
		AISInsertString(binaryData, 70, 42, callSign);
		AISInsertString(binaryData, 112, 120, shipName);
		AISInsertInteger(binaryData, 232, 8, shipType);
		AISInsertInteger(binaryData, 240, 9, refBow / 10);
		AISInsertInteger(binaryData, 249, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 258, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 264, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 270, 4, gnssType);
		AISInsertInteger(binaryData, 274, 4, epoch.GetMonth() + 1);
		AISInsertInteger(binaryData, 278, 5, epoch.GetDay());
		AISInsertInteger(binaryData, 283, 5, epoch.GetHour());
		AISInsertInteger(binaryData, 288, 6, epoch.GetMinute());
		AISInsertInteger(binaryData, 294, 8, draft / 10);
		AISInsertString(binaryData, 302, 120, destination);
		AISInsertInteger(binaryData, 422, 1, dteFlag);
		AISInsertInteger(binaryData, 423, 1, 0xFF); //spare
		
		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send VDM message in two NMEA183 sentences
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,2,1,%d,%c,%s,0", AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(0,35).c_str()));
			nmeaSentences->push_back(wxString::Format("!AIVDO,2,2,%d,%c,%s,2", AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(35,36).c_str()));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,2,1,%d,%c,%s,0", AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(0,35).c_str()));
			nmeaSentences->push_back(wxString::Format("!AIVDM,2,2,%d,%c,%s,2", AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(35,36).c_str()));
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
		
		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) * 1e-7;
		
		double latitude;
		latitude = (payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24)) * 1e-7;
		
		byte positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		byte raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		byte timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		unsigned short courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		unsigned short speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		unsigned int communicationState;
		communicationState = (payload[18] | (payload[19] << 8) | (payload[20] << 16)) & 0x7FFFF;

		byte transceiverInformation; 
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		double altitude;
		altitude = 1e-6 * (((long long)payload[21] | ((long long)payload[22] << 8) | ((long long)payload[23] << 16) | ((long long)payload[24] << 24) \
			| ((long long)payload[25] << 32) | ((long long)payload[26] << 40) | ((long long)payload[27] << 48) | ((long long)payload[28] << 56)));
		
		byte reservedForRegionalApplications;
		reservedForRegionalApplications = payload[29];

		byte dteFlag; 
		dteFlag = payload[30] & 0x01;

		// BUG BUG Just guessing these to match NMEA2000 payload with ITU AIS fields

		byte assignedModeFlag;
		assignedModeFlag = (payload[30] & 0x02) >> 1;

		byte sotdmaFlag;
		sotdmaFlag = (payload[30] & 0x04) >> 2;

		byte altitudeSensor;
		altitudeSensor = (payload[30] & 0x08) >> 3;

		byte spare;
		spare = (payload[30] & 0xF0) >> 4;

		byte reserved;
		reserved = payload[31];
		
		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 12, altitude);
		AISInsertInteger(binaryData, 50, 10, TwoCanUtils::IsDataValid(speedOverGround) ? CONVERT_MS_KNOTS * speedOverGround * 0.1f : 1023);
		AISInsertInteger(binaryData, 60, 1, positionAccuracy);
		AISInsertInteger(binaryData, 61, 28, (int)(longitude * 600000));
		AISInsertInteger(binaryData, 89, 27, (int)(latitude * 600000));
		AISInsertInteger(binaryData, 116, 12, TwoCanUtils::IsDataValid(courseOverGround) ? RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f : 3600);
		AISInsertInteger(binaryData, 128, 6, timeStamp);
		AISInsertInteger(binaryData, 134, 1, altitudeSensor);
		AISInsertInteger(binaryData, 135, 7, reservedForRegionalApplications);
		AISInsertInteger(binaryData, 142, 1, dteFlag);
		AISInsertInteger(binaryData, 143, 3, spare);
		AISInsertInteger(binaryData, 146, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 147, 1, raimFlag);
		AISInsertInteger(binaryData, 148, 1, sotdmaFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// BUG BUG Fix
// Decode PGN 129799 Radio Transceiver Information
// $--FSI,xxxxxx,xxxxxx,c,x*hh<CR><LF>
// Example for VHF
// $CDFSI,900016,,d,9*08<CR><LF> 
// Set VHF transmit and receive channel 16, F3E / G3E, simplex, telephone, high power
bool TwoCanDevice::DecodePGN129799(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		unsigned int rxFrequency;
		rxFrequency = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);

		unsigned int txFrequency;
		txFrequency = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);

		int channel;
		channel = payload[8];

		int power;
		power = payload[9];
		// BUG BUG How to convert from the power value to one of
		// // Power Level(0 = standby, 1 = lowest, 9 = highest)

		int mode;
		mode = payload[10];
		// BUG BUG How to map mode to one of. Is it similar to the DSC usage, 
		// d = F3E / G3E simplex, telephone 
		// e = F3E / G3E duplex, telephone
		// m = J3E, telephone 
		// o = H3E, telephone
		// q = F1B / J2B FEC NBDP, Telex / teleprinter
		// s = F1B / J2B ARQ NBDP, Telex / teleprinter
		// t = F1B / J2B receive only, teleprinter / DSC
		// w = F1B/J2B, teleprinter/DSC
		// x = A1A Morse, tape recorder
		// { = A1A Morse, Morse key/head set
		// | = F1C/F2C/F3C, FAX-machine 
		// null for no information

		int bandwidth;
		bandwidth = payload[11];
		// Not fully implemented, and no idea why we would want to convert this in anycase.
		// nmeaSentences->push_back(wxString::Format("$IIFSI,%d,%d,%d,%d ", txFrequency, rxFrequency, power, mode));
		// Could always send it to a compatible radio to set the frequency & power !!
#ifndef NDEBUG
		wxLogMessage(_T("TwoCan Device, PGN 129799, Tx Frequency: %d"), txFrequency);
		wxLogMessage(_T("TwoCan Device, PGN 129799, RX Frequency: %d"), rxFrequency);
		wxLogMessage(_T("TwoCan Device, PGN 129799, Channel: %d"), channel);
		wxLogMessage(_T("TwoCan Device, PGN 129799, Power: %d"), power);
		wxLogMessage(_T("TwoCan Device, PGN 129799, Mode: %d"), mode);
		wxLogMessage(_T("TwoCan Device, PGN 129799, Bandwidth: %d"), bandwidth);
#endif
		return FALSE;
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

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int sourceID;
		sourceID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		byte reservedA;
		reservedA = payload[4] & 0x01;

		byte transceiverInformation;
		transceiverInformation = (payload[5] & 0x3E) >> 1;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		byte sequenceNumber;
		sequenceNumber = (payload[5] & 0xC0) >> 6;

		unsigned int destinationId;
		destinationId = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		byte reservedB;
		reservedB = payload[10] & 0x3F;

		byte retransmitFlag;
		retransmitFlag = (payload[10] & 0x40) >> 6;

		byte reservedC;
		reservedC = (payload[10] & 0x80) >> 7;

		std::string safetyMessage;
		int safetyMessageLength = payload[11];
		if (payload[12] == 1) {
			// first byte of safety message indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < safetyMessageLength - 2; i++) {
				safetyMessage += (static_cast<char>(payload[13 + i]));
			}
		}

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
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);

		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages -1) { // This is the last message
				if (transceiverInformation & 0x04) {
					nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28)),fillBits));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28)),fillBits));
				}
			}
			else {
				if (transceiverInformation & 0x04) {
					nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
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

// Decode PGN 129802 AIS Safety Related Broadcast Message 
// AIS Message Type 14
bool TwoCanDevice::DecodePGN129802(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		std::vector<bool> binaryData(1008);

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int sourceID;
		sourceID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | ((payload[4] & 0x3F) << 24);

		byte reservedA;
		reservedA = (payload[4] & 0xC0) >> 6;

		byte transceiverInformation;
		transceiverInformation = payload[5] & 0x1F;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		byte reservedB;
		reservedB = (payload[5] & 0xE0) >> 5;

		std::string safetyMessage;
		int safetyMessageLength = payload[6];
		if (payload[7] == 1) { 
			// first byte of safety message indicates encoding; 0 for Unicode, 1 for ASCII
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
		AISInsertString(binaryData, 40, l * 6, safetyMessage);

		// Calculate fill bits as safetyMessage is variable in length
		// According to ITU, maximum length of safetyMessage is 966 6bit characters
		int fillBits = (40 + (l * 6)) % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 40 + (l * 6), fillBits, 0);
		}

		// BUG BUG Should check whether the binary message is smaller than 1008 bytes otherwise
		// we just need a substring from the binaryData
		std::vector<bool>::const_iterator first = binaryData.begin();
		std::vector<bool>::const_iterator last = binaryData.begin() + 40 + (l * 6) + fillBits;
		std::vector<bool> newVec(first, last);

		// Encode the VDM Message using 6bit ASCII
		wxString encodedVDMMessage = AISEncodePayload(newVec);

		// Send the VDM message, use 28 characters as an arbitary number for multiple NMEA 183 sentences
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);
		if (numberOfVDMMessages == 1) {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,%d", encodedVDMMessage, fillBits));
		}
		else {
			for (int i = 0; i < numberOfVDMMessages; i++) {
				if (i == numberOfVDMMessages - 1) { // Is this the last message, if so append number of fillbits as appropriate
					if (transceiverInformation & 0x04) {
						nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28), fillBits));
					}
					else {
						nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28), fillBits));
					}
				}
				else {
					if (transceiverInformation & 0x04) {
						nmeaSentences->push_back(wxString::Format("!AIVDO,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
					}
					else {
						nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,%c,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, aisChannel, encodedVDMMessage.Mid(i * 28, 28)));
					}
					
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

		wxString mmsiAddress = wxEmptyString;
		// BUG BUG Note MMSI addresses are 9 digits but the spec for both NMEA 183 & 2000 append zero as the field could
		// also be encoded as a geographic location, encoded using 10 digits.
		if (payload[6] != 0xFF) {
			mmsiAddress = wxString::Format("%02x%02x%02x%02x%02x", payload[2], payload[3], payload[4], payload[5], payload[6]);
		}

		byte firstTelecommand; // or Nature of Distress
		firstTelecommand = payload[7];

		byte secondTelecommand; // or Communication Mode
		secondTelecommand = payload[8];

		wxString receiveFrequency = wxEmptyString;
		if (payload[9] != 0xFF) {
			receiveFrequency = wxString::Format("%02d%02d%02d%02d%02d%02d", payload[9], payload[10], payload[11], payload[12], payload[13], payload[14]);
		}

		wxString transmitFrequency = wxEmptyString;
		if (payload[20] != 0xFF) {
			transmitFrequency = wxString::Format("%02d%02d%02d%02d%02d%02d", payload[15], payload[16], payload[17], payload[18], payload[19], payload[20]);
		}

		wxString telephoneNumber;
		size_t telephoneNumberLength = payload[21];
		if (payload[22] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			for (size_t i = 0; i < telephoneNumberLength - 2; i++) {
				telephoneNumber.append(1, (char)payload[23 + i]);
			}
		}

		size_t index = 21 + telephoneNumberLength;

		double latitude;
		latitude = 1e-7 * (payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24));

		index += 4;

		double longitude;
		longitude = 1e-7 * (payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24));

		index += 4;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		wxString position;
		position = wxString::Format("%02d%02d%03d%02d", abs(latitudeDegrees), (int)trunc(latitudeMinutes),
			abs(longitudeDegrees), (int)trunc(longitudeMinutes));

		// quadrant 0 = North East, 1 North West, 2 South East, 3 South West,
		if (latitude >= 0) {
			if (longitude >= 0) {
				position.insert(0, "0");
			}
			else {
				position.insert(0, "1");
			}
		}
		else {
			if (longitude >= 0) {
				position.insert(0, "2");
			}
			else {
				position.insert(0, "3");
			}
		}

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = (unsigned int)payload[index] | ((unsigned int)payload[index + 1] << 8) | ((unsigned int)payload[index + 2] << 16) | ((unsigned int)payload[index + 3] << 24);

		wxDateTime epoch((time_t)0);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		wxString timeOfPosition = epoch.Format("%H%M");

		index += 4;

		wxString vesselInDistress = wxEmptyString;

		if (payload[index + 4] != 0xFF) { // If there is no MMSI address, the value should be all 0xFF
			vesselInDistress = wxString::Format("%02d%02d%02d%02d%02d", payload[index], payload[index + 1], payload[index + 2], payload[index + 3], payload[index + 4]);
		}

		index += 5;

		byte endOfSequence;
		endOfSequence = payload[index]; // 1 byte

		index += 1;

		byte dscExpansionEnabled; // Encoded over two bits
		dscExpansionEnabled = payload[index] & 0x03;

		index += 1;

		wxString callingRx = wxEmptyString;
		if (payload[index + 5] != 0xFF) {
			callingRx = wxString::Format("%02d%02d%02d%02d%02d%02d", payload[index], payload[index + 1], payload[index + 2], payload[index + 3], payload[index + 4], payload[index + 5]);
		}

		index += 6;

		wxString callingTx = wxEmptyString;
		if (payload[index + 5] != 0xFF) {
			callingTx = wxString::Format("%02d%02d%02d%02d%02d%02d", payload[index], payload[index + 1], payload[index + 2], payload[index + 3], payload[index + 4], payload[index + 5]);
		}

		index += 6;

		unsigned int timeOfTransmission; // Not used in DSC sentence
		timeOfTransmission = payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24);

		index += 4;

		unsigned short dayOfTransmission; // Not used in DSC Sentence
		dayOfTransmission = payload[index] | (payload[index + 1] << 8);

		index += 2;

		unsigned short messageId; // Not used in DSC Sentence
		messageId = payload[index] | (payload[index + 1] << 8);

		index += 2;

		wxString dscSentence;

		dscSentence = wxString::Format("$CDDSC,%02d,%s", formatSpecifier - 100, mmsiAddress);

		if (formatSpecifier == 112) { // If Format Specifier is Distress, DSC Category is NULL
			dscSentence += wxString::Format(",,%02d,%02d,%s,%s,,,%c", firstTelecommand - 100, secondTelecommand - 100, position,
				timeOfPosition, endOfSequence == 117 ? 'R' : endOfSequence == 122 ? 'B' : 'S');
		}
		else { // Format Specifier is All Ships, Group or Individual
			//"$CDDSC,16,0112345670,12,12,09,1474712219,1234,9991212120,00,S,,", _
			//"$CDDSC,16,0112345670,08,09,26,041250,,,,S,,*C9",
			if (dscCategory == 112) { // Either a Distress Ack, Distres Relay or Distress Relay Ack
				dscSentence += wxString::Format(",%02d,%02d,%02d,%s,%s,%s,%02d,%c", dscCategory - 100, firstTelecommand - 100, secondTelecommand - 100,
					position, timeOfPosition, vesselInDistress, secondTelecommand - 100,
					endOfSequence == 117 ? 'R' : endOfSequence == 122 ? 'B' : 'S');
			}
			else { // Urgency of Safety.Eg, A position update
				dscSentence += wxString::Format(",%02d,%02d, %02d,%s,%s,,,%c", dscCategory - 100, firstTelecommand - 100, secondTelecommand - 100,
					position, timeOfPosition, endOfSequence == 117 ? 'R' : endOfSequence == 122 ? 'B' : 'S');
			}
		}

		if ((dscExpansionEnabled & 0x01)== 0x01) {
			dscSentence += ",E";
		}
		else {
			dscSentence += ",";
		}

		nmeaSentences->push_back(dscSentence);

		// If there is DSE Expansion Data, the following pairs are repeated

		if ((dscExpansionEnabled & 0x01) == 0x01) {

			byte dscExpansionSymbol;
			std::vector<byte> dseExpansionData;
			wxString dseSentence;

			dseSentence = wxString::Format("$CDDSE,1,1,A,%s", mmsiAddress);
			
			for (size_t j = 0; j < 2; j++) {

				dscExpansionSymbol = payload[index];
				if (dscExpansionSymbol != 0xFF) {
					dseSentence += wxString::Format(",%02d,", dscExpansionSymbol - 100);
					index += 1;

					size_t  dseExpansionDataLength = payload[index];
					index += 1;

					byte dscEncoding = payload[index]; // Should really check it is 0x01  to denote ASCII
					index += 1;

					if (dseExpansionDataLength > 2) {

						for (size_t k = 0; k < dseExpansionDataLength - 2; k++) {
							dseSentence += std::to_string(payload[index]);
							index += 1;
						}
					} 
				}
				else {
					// Assuming they have correctly encoded a blank value with the correct length & encoding byte
					index += 2;
				}
			}
#ifndef NDEBUG
			wxLogMessage(wxString::Format("Debug DSE: %s", dseSentence));
#endif
			nmeaSentences->push_back(dseSentence);
		} 

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode table from ITU-R M.825 // Not actually used here, but.....
wxString TwoCanDevice::DecodeDSEExpansionCharacters(std::vector<byte> dseData) {
	wxString result;
	char lookupTable[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '\'',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
		'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', '.', ',', '-', '/', ' ' };

	for (size_t i = 0; i < dseData.size(); i += 2) {
		result.append(1, lookupTable[dseData[i]]);
	}
	return result;
}

// Decode PGN 129809 AIS Class B Static Data Report, Part A 
// AIS Message Type 24, Part A
bool TwoCanDevice::DecodePGN129809(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {
		
		std::vector<bool> binaryData(164);

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName.append(1, (char)payload[5 + i]);
		}

		byte transceiverInformation;
		transceiverInformation = payload[25] & 0x1F;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

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
		if (transceiverInformation & 0x04) {		
			nmeaSentences->push_back(wxString::Format("!AIVDO,1,1,,%c,%s,%d", aisChannel, AISEncodePayload(binaryData), fillBits));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,%c,%s,%d", aisChannel, AISEncodePayload(binaryData), fillBits));
		}
		
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

		byte messageID;
		messageID = payload[0] & 0x3F;

		byte repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		byte shipType;
		shipType = payload[5];

		std::string vendorId;
		for (int i = 0; i < 7; i++) {
			vendorId.append(1, (char)payload[6 + i]);
		}

		std::string callSign;
		for (int i = 0; i < 7; i++) {
			callSign.append(1, (char)payload[13 + i]);
		}
		
		unsigned short shipLength;
		shipLength = payload[20] | (payload[21] << 8);

		unsigned short shipBeam;
		shipBeam = payload[22] | (payload[23] << 8);

		unsigned short refStarboard;
		refStarboard = payload[24] | (payload[25] << 8);

		unsigned short refBow;
		refBow = payload[26] | (payload[27] << 8);

		unsigned int motherShipID; // aka mother ship MMSI
		motherShipID = payload[28] | (payload[29] << 8) | (payload[30] << 16) | (payload[31] << 24);

		byte reserved;
		reserved = (payload[32] & 0x03);

		byte spare;
		spare = (payload[32] & 0xFC) >> 2;

		byte transceiverInformation;
		transceiverInformation = payload[33] & 0x1F;

		char aisChannel;
		aisChannel = (transceiverInformation & 0x01) ? 'B' : 'A';

		byte sid;
		sid = payload[34];

		byte gpsFixingDevice;
		gpsFixingDevice = payload[33] & 0x1F;

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, 0x01); // Part B = 1
		AISInsertInteger(binaryData, 40, 8, shipType);
		AISInsertString(binaryData, 48, 42, vendorId);
		AISInsertString(binaryData, 90, 42, callSign);
		AISInsertInteger(binaryData, 132, 9, refBow / 10);
		AISInsertInteger(binaryData, 141, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 150, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 156, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 162, 4, gpsFixingDevice);
		AISInsertInteger(binaryData, 166 ,2 , 0); //spare
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		if (transceiverInformation & 0x04) {
			nmeaSentences->push_back(wxString::Format("!AIVDO,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		else {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,%c,%s,0", aisChannel, AISEncodePayload(binaryData)));
		}
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// decode PGN 130065 NMEA Route & Waypoint Service - Route List
bool TwoCanDevice::DecodePGN130065(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {
		
		byte startRouteId;
		startRouteId = payload[0];
		
		byte nItems;
		nItems = payload[1];

        byte nRoutes;
		nRoutes = payload[2];

        byte databaseId;
		databaseId = payload[3];
		
		unsigned int index;
		index = 4;

		for (unsigned int i = 0; i < nItems; i++) {
			
			byte routeId;
			routeId = payload[index];
			index += 1;

			// BUG BUG Are these null terminated ??
			char routeName[8];
			memcpy(routeName, &payload[index], 8);
			index += 8;

			byte wpIdMethod;
			wpIdMethod = (payload[index] & 0x30) >> 4;

			byte routeStatus;
			routeStatus = (payload[index] & 0xC0 ) >> 6;
			index += 1;

			if (enableWaypoint == TRUE) {
				PlugIn_Route route;
				route.m_NameString = routeName;
				route.m_GUID = GetNewGUID();
				// What to do with
				// route.m_StartString;
    			// route.m_EndString;
				AddPlugInRoute(&route, true);
			}

		}
          
        
		return TRUE;
	}
	else {
		return FALSE;
	}

}

// Decode PGN 130074 NMEA Route & Waypoint Service - Waypoint List
bool TwoCanDevice::DecodePGN130074(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		unsigned short startingWaypointId;
		startingWaypointId = payload[0] | (payload[1] << 8);

		unsigned short items;
		items = payload[2] | (payload[3] << 8);

		unsigned short validItems;
		validItems = payload[4] | (payload[5] << 8);

		unsigned short databaseId;
		databaseId = payload[6] | (payload[7] << 8);

		unsigned short reserved;
		reserved = payload[8] | (payload[9] << 8);

		unsigned int index;
		index = 10;

		// BUG BUG This is potentially broken, however I have never seen more than
		// one waypoint sent, and I have never seen Unicode characters used.
		for (size_t i = 0; i < validItems; i++) {
			unsigned short waypointId;
			waypointId = payload[index] | (payload[index + 1] << 8);
			index += 2;

			// Text with length & control byte
			unsigned int wptNameLength;
			wxString waypointName;

			wptNameLength = payload[index];
			index += 1;
			if (payload[index] == 0x01) { // first byte of Waypoint Name indicates encoding; 0 for Unicode, 1 for ASCII
				index += 1;
				waypointName.clear();
				for (size_t i = 0; i < wptNameLength - 2; i++) {
					waypointName.append(1, (char)payload[index]);
					index++;
				}

			}

			double latitude;
			latitude = (payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24)) * 1e-7;
			int latitudeDegrees = trunc(latitude);
			double latitudeMinutes = fabs(latitude - latitudeDegrees);
			index += 4;

			double longitude;
			longitude = (payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24)) * 1e-7;
			int longitudeDegrees = trunc(longitude);
			double longitudeMinutes = fabs(longitude - longitudeDegrees);
			index += 4;

			// Generate the NMEA 183 WPL sentence, even though it is not used by OpenCPN.
			nmeaSentences->push_back(wxString::Format("$IIWPL,%02d%05.2f,%c,%03d%05.2f,%c,%s",
				abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
				abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
				waypointName.ToAscii()));

			// Insert the waypoint directly into OpenCPN as it does not parse WPL sentences.
			// we don't retrieve waypoints to compare, so no way to avoid duplication......
			if (enableWaypoint) {
				PlugIn_Waypoint waypoint;
				waypoint.m_IsVisible = true;
				waypoint.m_MarkName = waypointName;
				// BUG BUG Should have a UI thingy to specify the default symbol
				waypoint.m_IconName = "Symbol_Triangle";
				waypoint.m_GUID = GetNewGUID();
				waypoint.m_lat = latitude;
				waypoint.m_lon = longitude;
				AddSingleWaypoint(&waypoint, true);
			}
		}
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

		unsigned short windAngle;
		windAngle = payload[3] | (payload[4] << 8);

		byte windReference;
		windReference = (payload[5] & 0x07);

		if (TwoCanUtils::IsDataValid(windSpeed)) {
			if (TwoCanUtils::IsDataValid(windAngle)) {

				if ((autopilotModel != AUTOPILOT_MODEL::NONE) && (twoCanAutopilot != nullptr)) {
					wxString jsonResponse;
					twoCanAutopilot->EncodeWindAngle(RADIANS_TO_DEGREES((float)windAngle / 10000), &jsonResponse);
					SendPluginMessage("TWOCAN_AUTOPILOT_RESPONSE", jsonResponse);
				}


				nmeaSentences->push_back(wxString::Format("$IIMWV,%.2f,%c,%.2f,N,A", RADIANS_TO_DEGREES((float)windAngle/10000), \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T', (double)windSpeed * CONVERT_MS_KNOTS / 100));
				return TRUE;

			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIMWV,,%c,%.2f,N,A", \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T', (double)windSpeed * CONVERT_MS_KNOTS / 100));
				return TRUE;	
			}
		}
		else {
			if (TwoCanUtils::IsDataValid(windAngle)) {
				nmeaSentences->push_back(wxString::Format("$IIMWV,%.2f,%c,,N,A", RADIANS_TO_DEGREES((float)windAngle/10000), \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T'));
				return TRUE;
			}
			else {
				return FALSE;
			}
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

		unsigned short waterTemperature;
		waterTemperature = payload[1] | (payload[2] << 8);

		unsigned short airTemperature;
		airTemperature = payload[3] | (payload[4] << 8);

		unsigned short airPressure;
		airPressure = payload[5] | (payload[6] << 8);
		
		if (TwoCanUtils::IsDataValid(waterTemperature)) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)waterTemperature * 0.01f) - CONST_KELVIN));
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
		
		unsigned short temperature;
		temperature = payload[2] | (payload[3] << 8);
			
		unsigned short humidity;
		humidity = payload[4] | (payload[5] << 8);
		//	Resolution 0.004
			
		unsigned short pressure;
		pressure = payload[6] | (payload[7] << 8);
		
		if ((temperatureSource == TEMPERATURE_SEA) && (TwoCanUtils::IsDataValid(temperature))) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)temperature * 0.01f) - CONST_KELVIN));
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
// $--XDR,C,x.x,C,c-c*hh<<CR?<:F>
bool TwoCanDevice::DecodePGN130312(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte sid;
		sid = payload[0];

		byte instance;
		instance = payload[1];

		byte source;
		source = payload[2];

		unsigned short actualTemperature;
		actualTemperature = payload[3] | (payload[4] << 8);

		unsigned short setTemperature;
		setTemperature = payload[5] | (payload[6] << 8);

		// BUG BUG Perhaps switch statement ??
		if ((source == TEMPERATURE_SEA) && (TwoCanUtils::IsDataValid(actualTemperature))) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)actualTemperature * 0.01f) - CONST_KELVIN));
			return TRUE;
		}
		else if ((source == TEMPERATURE_EXHAUST) && (TwoCanUtils::IsDataValid(actualTemperature))) {
			nmeaSentences->push_back(wxString::Format("$ERXDR,C,%.1f,C,ENGINEEXHAUST#%1d", 
				((float)actualTemperature * 0.01f) - CONST_KELVIN, instance));
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

		unsigned int actualTemperature; // A three byte value, guess I need to special case the validity check. Bloody NMEA!!
		actualTemperature = payload[3] | (payload[4] << 8) | (payload[5] << 16);

		unsigned short setTemperature;
		setTemperature = payload[6] | (payload[7] << 8);

		if ((source == TEMPERATURE_SEA) && (actualTemperature < 0xFFFFFD)) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)actualTemperature * 0.001f) - CONST_KELVIN));
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

// Decode PGN 130323 Meteorological Station Data
//         1   2  3    4  5  6 7 8  9 10 11 12 13 14 15 16 17 18 19 20 21
//         |   |  |    |  |  | | |  |  |  |  |  |  |  |  |  |  |  |  |  |
// $--MDA,n.nn,I,n.nnn,B,n.n,C,n.C,n.n,n,n.n,C,n.n,T,n.n,M,n.n,N,n.n,M*hh<CR><LF>

// Field Number:
// 1. Barometric pressure, inches of mercury, to the nearest 0.01 inch
// 2. I = inches of mercury
// 3.. Barometric pressure, bars, to the nearest .001 bar
// 4. B = bars
// 5. Air temperature, degrees C, to the nearest 0.1 degree C
// 6. C = degrees C
// 7. Water temperature, degrees C (this field left blank by WeatherStation)
// 8. C = degrees C
// 9. Relative humidity, percent, to the nearest 0.1 percent
// 10. Absolute humidity, percent
// 11. Dew point, degrees C, to the nearest 0.1 degree C
// 12. C = degrees C
// 13. Wind direction, degrees True, to the nearest 0.1 degree
// 14. T = true
// 15. Wind direction, degrees Magnetic, to the nearest 0.1 degree
// 16. M = magnetic
// 17. Wind speed, knots, to the nearest 0.1 knot
// 18. N = knots
// 19. Wind speed, meters per second, to the nearest 0.1 m/s
// 20. M = meters per second
// 21. Checksum

bool TwoCanDevice::DecodePGN130323(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		byte dataMode;
		dataMode = payload[0] & 0x0F;

		int daysSinceEpoch;
		daysSinceEpoch = payload[1] | (payload[2] << 8);

		int secondsSinceMidnight;
		secondsSinceMidnight = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		double latitude;
		latitude = (payload[7] | (payload[8] << 8) | (payload[9] << 16) | (payload[10] << 24)) * 1e-7;

		int latitudeDegrees;
		latitudeDegrees = trunc(latitude);

		double latitudeMinutes;
		latitudeMinutes = fabs(latitude - latitudeDegrees);

		double longitude;
		longitude = (payload[11] | (payload[12] << 8) | (payload[13] << 16) | (payload[14] << 24)) * 1e-7;

		int longitudeDegrees;
		longitudeDegrees = trunc(longitude);

		double longitudeMinutes;
		longitudeMinutes = fabs(longitude - longitudeDegrees);

		int windSpeed;
		windSpeed = payload[15] | (payload[16] << 8);

		int windAngle;
		windAngle = payload[17] | (payload[18] << 8);

		byte windReference;
		windReference = payload[19] & 0x07;

		int windGusts;
		windGusts = payload[20] | (payload[21] << 8);

		int atmosphericPressure;
		atmosphericPressure = payload[22] | (payload[23] << 8);

		float ambientTemperature;
		ambientTemperature = ((payload[24] | (payload[25] << 8)) * 0.01f) - 273.15;

		std::string stationId;;
		int index = 26;
		int stringLength = payload[index];
		index++;
		if (payload[index] == 1) {
			index++;
			// first byte of Station ID indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < stringLength - 2; i++) {
				stationId += (static_cast<char>(payload[index]));
				index++;
			}
		}

		std::string stationName;
		stringLength = payload[index];
		index++;
		if (payload[index] == 1) {
			index++;
			// first byte of Station Name indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < stringLength - 2; i++) {
				stationName += (static_cast<char>(payload[index]));
				index++;
			}
		}


		wxDateTime epoch((time_t)0);
		epoch += wxDateSpan::Days(daysSinceEpoch);
		epoch += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		nmeaSentences->push_back(wxString::Format("$IIMDA,,I,%.2f,B,%.1f,C,,C,,,,C,%.2f,T,,M,%.2f,N,%.2f,M", atmosphericPressure, \
		((float)ambientTemperature * 0.01f) - CONST_KELVIN, RADIANS_TO_DEGREES((float)windAngle / 10000 ), \
		(double)windSpeed * CONVERT_MS_KNOTS / 100, (double)windSpeed / 100));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130577 NMEA Direction Data
bool TwoCanDevice::DecodePGN130577(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != NULL) {

		// 0 - Autonomous, 1 - Differential enhanced, 2 - Estimated, 3 - Simulated, 4 - Manual
		byte dataMode;
		dataMode = payload[0] & 0x0F;

		// True = 0, Magnetic = 1
		byte cogReference;
		cogReference = (payload[0] & 0x30);

		byte sid;
		sid = payload[1];

		unsigned short courseOverGround;
		courseOverGround = (payload[2] | (payload[3] << 8));

		unsigned short speedOverGround;
		speedOverGround = (payload[4] | (payload[5] << 8));

		unsigned short heading;
		heading = (payload[6] | (payload[7] << 8));

		unsigned short speedThroughWater;
		speedThroughWater = (payload[8] | (payload[9] << 8));

		unsigned short set;
		set = (payload[10] | (payload[11] << 8));

		unsigned short drift;
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

// Decode PGN 130820 Manufacturer Proprietary Message
// At present return FALSE, because there are no NMEA 183 sentences to generate/send
// Initially implemented to support TwoCan Media, A Remote Control for Fusion Media Players
bool TwoCanDevice::DecodePGN130820(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: {// Fusion
				if (enableMusic) {
						wxString jsonResponse;
						if (twoCanMedia->DecodeMediaResponse(payload, &jsonResponse)) {
							if (jsonResponse.Length() > 0) {
								SendPluginMessage(_T("TWOCAN_MEDIA_RESPONSE"), jsonResponse);
							}
						}
					}
				}
				break;
			case 1857: // Simrad
				break;
		}
	}
	return FALSE;
}

bool TwoCanDevice::DecodePGN130822(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
		case 229: // Garmin
			break;
		case 275: // Navico
			break;
		case  382: //B & G
			break;
		case 419: // Fusion
			break;
		case 1857: // Simrad
			break;
		}
		return FALSE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 130824 Manufacturer Proprietary Message
// B&G Wind or Performance Data ??
bool TwoCanDevice::DecodePGN130824(const byte *payload) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: // Fusion
				break;
			case 1857: // Simrad
				break;
		}
		return FALSE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130850 Manufacturer Proprietary Message
// At present return FALSE, because there are no NMEA 183 sentences to generate/send
// Initially implemented to support TwoCanAutopilot control of Navic NAC3 Autopilots
bool TwoCanDevice::DecodePGN130850(const byte *payload, std::vector<wxString> *nmeaSentences) {
	if (payload != nullptr) {

		unsigned int manufacturerId;
		manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

		byte industryCode;
		industryCode = (payload[1] & 0xE0) >> 5;

		switch (manufacturerId) {
			case 229: // Garmin
				break;
			case 275: // Navico
				break;
			case  382: //B & G
				break;
			case 419: // Fusion
				 break;
			case 1857: // Simrad
			{
				if (autopilotModel != AUTOPILOT_MODEL::NONE) {
					wxString jsonResponse;
					if (twoCanAutopilot->DecodeNAC3Alarm(payload, &jsonResponse)) {
						if (jsonResponse.Length() > 0) {
							SendPluginMessage(_T("TWOCAN_AUTOPILOT_RESPONSE"), jsonResponse);
						}
					}
				}
			}
				break;
		}
		return FALSE;
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
	
return TransmitFrame(id, &payload[0]);
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
	
	return TransmitFrame(id, &payload[0]);
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
	if (!TwoCanUtils::IsDataValid(heartbeatCounter)) {  
		// From observation of B&G devices, appears to rollover after 252 ??
		heartbeatCounter = 0;
	}
	
	return TransmitFrame(id, &payload[0]);

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
	char const *hwVersion = CONST_MODEL_ID; //PLUGIN_COMMON_NAME;
	memcpy(&payload[4], hwVersion,strlen(hwVersion));
	
	// Software Version Bytes [36] - [67]
	// BUG BUG Should derive from PLUGIN_VERSION_MAJOR and PLUGIN_VERSION_MINOR
	memset(&payload[36],0,32);
	//char const *swVersion = ;  
	std::string tmpStr = std::to_string(PLUGIN_VERSION_MAJOR);
	tmpStr += ".";
	tmpStr += std::to_string(PLUGIN_VERSION_MINOR);
	memcpy(&payload[36], tmpStr.c_str(), tmpStr.length());
	
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

	// Update my own Model ID information in the network map
	wxString *mid;
	mid = new wxString(CONST_MODEL_ID);
		
	strcpy(networkMap[header.source].productInformation.modelId,mid->c_str());

	delete mid;

	return FragmentFastMessage(&header,sizeof(payload),&payload[0]);

}

// BUG BUG REMOVE
// Transmit NMEA 2000 Configuration Information
int TwoCanDevice::SendConfigurationInformation() {
	CanHeader header;
	header.pgn = 126998;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	// 3 messages each 32 bytes in length (and size & encoding byte)
	// Others have mentioned max len is 70 characters ??
	// From observation, they are variable length, and perhaps up to 32 bytes
	std::vector<byte> payload;
		
	std::string message;
	
	message = "TwoCan Plugin 2.0";
	
	payload.push_back(message.length());
	payload.push_back(0x01);
	for (auto it : message) {
		payload.push_back(it);
	}
	
	message = "OpenCPN";

	payload.push_back(message.length());
	payload.push_back(0x01);
	for (auto it : message) {
		payload.push_back(it);
	}
	
	message = "twocanplugin@hotmail.com";
	
	payload.push_back(message.length());
	payload.push_back(0x01);
	for (auto it : message) {
		payload.push_back(it);
	}	
	
	return FragmentFastMessage(&header,payload.size(),payload.data());
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
		129038, 129039, 129040, 129041, 129283, 129793, 129794, 129798, 129801, 129802, 
		129808,	129809, 129810, 130306, 130310, 130312, 130577, 130820 };
	unsigned int transmittedPGN[] = {59904, 59392, 60928, 65240, 126464, 126992, 126993, 126996, 
		127250,	127251, 127258, 128259, 128267, 128275, 129025, 129026, 129029, 129033, 
		129038, 129039, 129040, 129041, 129283, 129793, 129794, 129798, 129801, 129802, 
		129808,	129809, 129810, 130306, 130310, 130312, 130577 };
	
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
	
	return TransmitFrame(id, &payload[0]);

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

// Fragment a Fast Packet Message into 8 byte payload chunks and send it
// BUG BUG Investigate refactoring, deduplicating and using function in twocanutils
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
	
	returnCode = TransmitFrame(id, &data[0]);

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
		
		returnCode = TransmitFrame(id, &data[0]);

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
			
		returnCode = TransmitFrame(id, &data[0]);
		
		if (returnCode != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("TwoCan Device, Error sending fast message frame"));
			// BUG BUG Should we log the frame ??
			return returnCode;
		}
	}
	
	return TWOCAN_RESULT_SUCCESS;
}

// Transmit a NMEA 2000 message
// Called by the plugin for the gateway & autopilot functions.
int TwoCanDevice::TransmitFrame(unsigned int id, byte *data) {
	int returnCode;
	const std::lock_guard<std::mutex> lock(writeMutex);

#if defined (__WXMSW__)
	if (writeFrame != NULL) {
		returnCode = writeFrame(id, CONST_PAYLOAD_LENGTH, &data[0]);
	}
	else {
		returnCode = SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_INVALID_WRITE_FUNCTION);
	}
#endif
	
#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
	returnCode = adapterInterface->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif

	return returnCode;

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
