// Copyright(C) 2021 by Steven Adler
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
// NMEA2000� is a registered Trademark of the National Marine Electronics Association

#ifndef TWOCAN_MEDIA_H
#define TWOCAN_MEDIA_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif


// wxWidgets
// String Format, Comparisons etc.
#include <wx/string.h>
// For converting NMEA 2000 date & time data
#include <wx/datetime.h>
// Raise events to the plugin
#include <wx/event.h>
// Logging (Info & Errors)
#include <wx/log.h>
// BUG BUG DEBUGGING REMOVE
#include <wx/socket.h>

// JSON stuff
#include <wx/json_defs.h>
#include <wx/jsonval.h>
#include <wx/jsonreader.h>
#include <wx/jsonwriter.h>

#include <twocanutils.h>

#include <unordered_map>

extern int networkAddress;

// From observation
typedef enum fusionMediaType {
	type_am = 0,
	type_type_fm = 1,
	type_sirius = 2,
	type_aux = 3,
	type_ipod = 4,
	type_usb = 5,
	type_mtp = 9,
	type_bt = 10,
	type_dab = 14
} FUSION_MEDIA_TYPE;


typedef enum fusionMediaPorts {
	am = 0,
	fm = 1,
	sxm = 2,
	aux = 3,
	aux2 = 4,
	usb = 5,
	ipod = 6,
	mtp = 7,
	bt = 8,
	dab = 9,
	unknown = 99
} FUSION_MEDIA_PORT;

typedef enum mediaStatus {
	not_installed = 0,
	not_present = 4,
	present = 5,
	// Note 
	// 15 = present & connected (iPod & USB)
	// 25 = present & paired (BT)
} FUSION_MEDIA_STATUS;

typedef enum folderType {
	VIRTUALFOLDER = 0x49,
	DEVICEFOLDER = 0x41,
	MTPFOLDER = 0x47,
	PHYSICALFOLDER = 0x4F,
	MUSICTRACK = 0x17
} MEDIA_FILE_TYPE;

class TwoCanMedia {

	public:
		TwoCanMedia();
		~TwoCanMedia(void);

		// Encode Media Player commands
		// Encode JSON messages from the Media Player Controller into
		// NMEA PGN's for transmission over the network
		bool EncodeMediaRequest(wxString jsonText, std::vector<CanMessage> *canMessages);

		// Decode Media Player responses
		// Decode the Fusion PGN's into JSON messages to pass onto the Media Player Controller
		bool DecodeMediaResponse(const byte * payload, wxString *jsonResponse);

	private:
		// Most operations (next, previous, play, pause) use the media source id
		const int GetMediaSourceByName(const wxString sourceName);
		const wxString GetMediaSourceById(const int sourceId);

		// Each media player is named
		wxString deviceName;

		// The current session we are listening to.
		FUSION_MEDIA_PORT sessionId;
		FUSION_MEDIA_PORT usbMapping; // USB device may be either USB, MTP, IPOD
		FUSION_MEDIA_PORT sourceId;
		int totalSources;
		
		// Per zone values
		int zone0Volume;
		int zone1Volume;
		int zone2Volume;
		int zone0SubWoofer;
		int zone1SubWoofer;
		int zone2SubWoofer;
		int bass;
		int midrange;
		int treble;
		int balance;
		wxString zone0Name;
		wxString zone1Name;
		wxString zone2Name;

		// Radio Station
		wxString radioStationName;
		double radioFrequency;

		// File & Folder information
		int folderId;
		wxString folderName;
		int folderType;
		int folderSessionId = 0;
		
		// Track information
		int trackId;
		wxString trackName;
		int trackStatus;
		int totalTracks;
		int trackLength;
		int elapsedTime;

		// Not sure where I use these.....
		std::unordered_map<int, wxString> mediaSources;
		std::unordered_map<int, wxString> zoneNames;
		std::unordered_map<int, wxString> trackNames;

		// BUG BUG DEBUG REMOVE
		wxDatagramSocket *debugSocket;
		wxIPV4address addrLocal;
		wxIPV4address addrPeer;
};

#endif