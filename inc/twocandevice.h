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

#ifndef TWOCAN_DEVICE_H
#define TWOCAN_DEVICE_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

// Error constants and macros
#include "twocanerror.h"

// Constants, typedefs and utility functions for bit twiddling and array manipulation for NMEA 2000 messages
#include "twocanutils.h"

#ifdef __LINUX__
// For Linux , "baked in" classes for the Log File reader and SocketCAN interface
#include "twocanlogreader.h"
#include "twocansocket.h"
#endif

// STL
// used for AIS stuff
#include <vector>
#include <algorithm>
#include <bitset>
#include <iostream>

// wxWidgets
// BUG BUG work out which ones we really need
#include <wx/defs.h>
// String Format, Comparisons etc.
#include <wx/string.h>
// For converting NMEA 2000 date & time data
#include <wx/datetime.h>
// Raise events to the plugin
#include <wx/event.h>
// Perform read operations in their own thread
#include <wx/thread.h>
// Logging (Info & Errors)
#include <wx/log.h>
// Logging (raw NMEA 2000 messages)
#include <wx/file.h>
// User's paths/documents folder
#include <wx/stdpaths.h>

#ifdef __LINUX__
	// redefine Windows safe snprintf function to an equivalent
	#define _snprintf_s(a,b,c,...) snprintf(a,b,__VA_ARGS__)
#endif

#ifdef  __WXMSW__ 
// Based on an old 'c' code base
// Events and Mutexes that Windows DLL's use
#define CONST_DATARX_EVENT L"Global\\DataReceived"
#define CONST_DATATX_EVENT L"Global\\DataTransmit"
#define CONST_MUTEX_NAME L"Global\\DataMutex"
#endif

// Events passed up to the plugin
const int SENTENCE_RECEIVED_EVENT = wxID_HIGHEST + 1;
extern const wxEventType wxEVT_SENTENCE_RECEIVED_EVENT;

// NMEA 2000 Raw frame log file
// BUG BUG Should enable user to select file location and name. File name is generated automatically.
// Location is set to user's document folder (which is also used by Log File Readers)
#ifdef  __WXMSW__ 
// BUG BUG not used as input log file name as each is hardcoded in each of the Windows log file readers
#define CONST_LOGFILE_NAME L"twocan.log"
#endif

#ifdef __LINUX__
#define CONST_LOGFILE_NAME _T("twocan.log")
#endif

// Globally defined variables

// Name of currently selected CAN Interface
extern wxString canAdapter;

// Flag of bit values indicating what PGN's the plug-in converts
extern int supportedPGN;

// Whether to enable the realtime display of NMEA 2000 messages 
extern bool debugWindowActive;

// Whether the plug-in passively listens to the NMEA 2000 network or is an active device
extern bool deviceMode;

// If we are in active mode, whether to periodically send PGN 126993 heartbeats
extern bool enableHeartbeat;

// Whether to Log raw NMEA 2000 messages
extern int logLevel;

// List of devices dicovered on the NMEA 2000 network
extern NetworkInformation networkMap[CONST_MAX_DEVICES];

// The uniqueID of this device (also used as the serial number)
extern unsigned long uniqueId;

// The current NMEA 2000 network address of this device
extern int networkAddress;

#ifdef  __WXMSW__ 
// NMEA 2000 imported driver function prototypes
// Note to self, cast to wxChar for OpenCPN/wxWidgets wxString stuff
// BUG BUG Add an IsInstalled function to the drivers so that they can be automagically detected
typedef wxChar *(*LPFNDLLManufacturerName)(void);
typedef wxChar *(*LPFNDLLDriverName)(void);
typedef wxChar *(*LPFNDLLDriverVersion)(void);
typedef int(*LPFNDLLOpen)(void);
typedef int(*LPFNDLLClose)(void);
typedef int(*LPFNDLLRead)(byte *frame);
typedef int(*LPFNDLLWrite)(const unsigned int id, const int length, const byte *payload);
#endif

// Buffer used to re-assemble sequences of multi frame Fast Packet messages
typedef struct FastMessageEntry {
	byte IsFree; // indicate whether this entry is free
	time_t timeArrived; // time of last message. garbage collector will remove stale entries
	CanHeader header; // the header of the message. Used to "map" the incoming fast message fragments
	unsigned int sid; // message sequence identifier, used to check if a received message is the next message in the sequence
	unsigned int expectedLength; // total data length obtained from first frame
	unsigned int cursor; // cursor into the current position in the below data
	byte *data; // pointer to memory allocated for the data. Note: must be freed when IsFree is set to TRUE.
} FastMessageEntry;

// Implements a NMEA 2000 Network device
class TwoCanDevice : public wxThread {

public:
	// Constructor and destructor
	TwoCanDevice(wxEvtHandler *handler);
	~TwoCanDevice(void);

	// Reference to event handler address, ie. the TwoCan PlugIn
	wxEvtHandler *eventHandlerAddress;
#ifdef __LINUX__
	// wxMessage Queue to receive CAN Frames from either the Linux LogFile Reader or SocketCAN interface
	wxMessageQueue<std::vector<byte>> *canQueue;
#endif
	// Event raised when a NMEA 2000 message is received and converted to a NMEA 0183 sentence
	void RaiseEvent(wxString sentence);
	
	// Initialize & DeInitialize the device.
	// As we don't throw errors in the constructor, invoke functions that may fail from these functions
	int Init(wxString driverPath);
	int DeInit(void);

protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	byte canFrame[CONST_FRAME_LENGTH];
#ifdef  __WXMSW__ 
	// To reuse existing 'C' CAN Adapter exported functions
	BOOL freeResult = FALSE;
	HINSTANCE dllHandle = NULL;
	WIN32_FIND_DATA findFileData;
	HANDLE fileHandle = NULL;
	HANDLE eventHandle = NULL;
	HANDLE mutexHandle = NULL;
	LPDWORD threadId = NULL;
	LPFNDLLWrite writeFrame = NULL;
#endif

#ifdef __LINUX__
	// BUG BUG implement these as derived classes from an abstract class ??
	TwoCanLogReader *linuxLogReader; 
	TwoCanSocket *linuxSocket;
	// Need to persist the name of the Linux Driver, either "Log File Reader" or can0/slcan0/vcan0
	wxString linuxDriverName;
#endif

#ifdef  __WXMSW__ 
	// Functions to control the Windows CAN Adapter
	int LoadWindowsDriver(wxString driverPath);
	int ReadWindowsDriver(void);
	int UnloadWindowsDriver(void);
#endif

#ifdef __LINUX__	
	// Functions to control the Linux CAN interface
	int ReadLinuxDriver(void);
#endif

	// Heartbeat timer
	wxTimer *heartbeatTimer;
	void OnHeartbeat(wxEvent &event);
	int heartbeatCounter;

	// Statistics
	int receivedFrames;
	int transmittedFrames;
	int droppedFrames;
	wxDateTime droppedFrameTime;

	// File handle for logging raw frame data
	wxFile rawLogFile;

	// All the NMEA 2000 goodness

	// the 8 byte NAME of this device derived from the 8 bytes used in PGN 60928
	// this value is used to uniquely identify a device and to resolve address claim conflicts
	unsigned long long deviceName;

	// ISO Address Claim
	DeviceInformation deviceInformation;

	// NMEA 2000 Product Information
	ProductInformation productInformation;

	// Determine whether frame is a single frame message or multiframe Fast Packet message
	bool IsFastMessage(const CanHeader header);

	// The Fast Packet buffer - used to reassemble Fast packet messages
	FastMessageEntry fastMessages[CONST_MAX_MESSAGES];
	
	// Assemble sequence of Fast Messages int a payload
	void AssembleFastMessage(const CanHeader header, const byte *message);
	
	// And its companion
	int FragmentFastMessage(CanHeader *header, unsigned int payloadLength, byte *payload);

	// Add, Append and Find entries in the FastMessage buffer
	void MapInitialize(void);
	void MapLockRange(const int start, const int end);
	int MapFindFreeEntry(void);
	void MapInsertEntry(const CanHeader header, const byte *data, const int position);
	int MapAppendEntry(const CanHeader header, const byte *data, const int position);
	int MapFindMatchingEntry(const CanHeader header);
	int MapGarbageCollector(void);

	// Big switch statement to determine which function is called to decode each received NMEA 2000 message
	void ParseMessage(const CanHeader header, const byte *payload);
	
	// Decode PGN59392 ISO Acknowledgement
	int DecodePGN59392(const byte *payload);
	
	// Decode PGN 59904 ISO Request
	int DecodePGN59904(const byte *payload, unsigned int *requestedPGN);

	// Decode PGN 60928 ISO Address Claim
	int DecodePGN60928(const byte *payload, DeviceInformation *device_Information);
	
	// Decode PGN 65240 ISO Commanded Address
	int DecodePGN65240(const byte *payload, DeviceInformation *device_Information);

	// Decode PGN 126992 NMEA System Time
	bool DecodePGN126992(const byte *payload, std::vector<wxString> *nmeaSentences);
	
	// Decode PGN 126993 NMEA heartbeat
	bool DecodePGN126993(const int souce, const byte *payload);

	// Decode PGN 126996 NMEA Product Information
	int DecodePGN126996(const byte *payload, ProductInformation *product_Information);
	
	// Decode PGN 127250 NMEA Vessel Heading
	bool DecodePGN127250(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 127251 NMEA Rate of Turn (ROT)
	bool DecodePGN127251(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 127258 NMEA Magnetic Variation
	bool DecodePGN127258(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 128259 NMEA Speed & Heading
	bool DecodePGN128259(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 128267 NMEA Depth
	bool DecodePGN128267(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 128275 Distance Log
	bool DecodePGN128275(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129025 NMEA Position Rapid Update
	bool DecodePGN129025(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129026 NMEA COG SOG Rapid Update
	bool DecodePGN129026(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129029 NMEA GNSS Position
	bool DecodePGN129029(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129033 NMEA Date & Time
	bool DecodePGN129033(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129038 AIS Class A Position Report
	bool DecodePGN129038(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Deocde PGN 129039 AIS Class B Position Report
	bool DecodePGN129039(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129040 AIS Class B Extended Position Report
	bool DecodePGN129040(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129041 AIS Aids To Navigation (AToN) Report
	bool DecodePGN129041(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129283 NMEA Cross Track Error (XTE)
	bool DecodePGN129283(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129284 Navigation Data
	bool DecodePGN129284(const byte * payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129285 Navigation Route/WP Information
	bool DecodePGN129285(const byte * payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129793 AIS Date and Time report
	bool DecodePGN129793(const byte * payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129794 AIS Class A Static Data
	bool DecodePGN129794(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129796 AIS Acknowledge 
	// Decode PGN 129797 AIS Binary Broadcast Message 

	//	Decode PGN 129798 AIS SAR Aircraft Position Report
	bool DecodePGN129798(const byte *payload, std::vector<wxString> *nmeaSentences);

	//	Decode PGN 129801 AIS Addressed Safety Related Message
	bool DecodePGN129801(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129802 AIS Safety Related Broadcast Message 
	bool DecodePGN129802(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129803 AIS Interrogation
	// Decode PGN 129804 AIS Assignment Mode Command 
	// Decode PGN 129805 AIS Data Link Management Message 
	// Decode PGN 129806 AIS Channel Management
	// Decode PGN 129807 AIS Group Assignment

	// Decode PGN 129808 DSC Message
	bool DecodePGN129808(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129809 AIS Class B Static Data Report, Part A 
	bool DecodePGN129809(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 129810 AIS Class B Static Data Report, Part B 
	bool DecodePGN129810(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 130306 NMEA Wind
	bool DecodePGN130306(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 130310 NMEA Water & Air Temperature and Pressure
	bool DecodePGN130310(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 130311 NMEA Environmental Parameters (supercedes 130310)
	bool DecodePGN130311(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 130312 NMEA Temperature
	bool DecodePGN130312(const byte *payload, std::vector<wxString> *nmeaSentences);
	
	// Decode PGN 130316 NMEA Temperature Extended Range
	bool DecodePGN130316(const byte *payload, std::vector<wxString> *nmeaSentences);

	// Decode PGN 130577 NMEA Direction Data
	bool DecodePGN130577(const byte *payload, std::vector<wxString> *nmeaSentences);
	
	// Transmit an ISO Request
	int SendISORequest(const byte destination, const unsigned int pgn);

	// Transmit an ISO Address Claim
	int SendAddressClaim(const unsigned int sourceAddress);

	// Transmit NMEA 2000 Product Information
	int SendProductInformation();
	
	// Transmit NMEA 2000 Supported Parameter Groups
	int SendSupportedPGN(void);

	// Respond to ISO Rqsts
	int SendISOResponse(unsigned int sender, unsigned int pgn);
	
	// Send NMEA 2000 Heartbeat
	int SendHeartbeat(void);

	// Appends '*' and Checksum to NMEA 183 Sentence prior to sending to OpenCPN
	void SendNMEASentence(wxString sentence);

	// Computes the NMEA 0183 XOR checksum
	wxString ComputeChecksum(wxString sentence);

	// Assemnble NMEA 183 VDM & VDO sentences
	// BUG BUG is this used anywhere ??
	std::vector<wxString> AssembleAISMessage(const std::vector<bool> binaryPayload, const int messageType);

	// Insert an integer value into AIS 6 bit encoded binary data, prior to AIS encoding
	void AISInsertInteger(std::vector<bool>& binaryData, int start, int length, int value);

	// Insert a date value into AIS 6 bit encoded binary data, prior to AIS encoding
	void AISInsertDate(std::vector<bool>& binaryData, int start, int length, int day, int month, int hour, int minute);

	// Insert a string value into AIS 6 bit encoded binary data, prior to AIS encoding
	void AISInsertString(std::vector<bool>& binaryData, int start, int length, std::string value);

	// Encode an 8 bit ASCII character using NMEA 0183 6 bit encoding
	char AISEncodeCharacter(char value);

	// Create the NMEA 0183 AIS VDM/VDO payload from the 6 bit encoded binary data
	wxString AISEncodePayload(std::vector<bool>& binaryData);

	// Just for completeness, in case one day we convert NMEA 183 to NMEA 2000 
	// and need to decode NMEA183 VDM/VDO messages to NMEA 2000 PGN's
	// although we may be able to use the OpenCPN WANTS_AIS_SENTENCES setting.

	// Decode an 8 bit ASCII character using NMEA 0183 6 bit encoding
	char AISDecodeCharacter(char value);

	// Decode the NMEA 0183 AIS VDM/VDO payload to a bit array of 6 bit characters
	std::vector<bool> AISDecodePayload(wxString SixBitData);

	// AIS VDM Sequential message ID, 0 - 9 used to distinguish multi-sentence NMEA 183 VDM messages
	int AISsequentialMessageId;

};

#endif
