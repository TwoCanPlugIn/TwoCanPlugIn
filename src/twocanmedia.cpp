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
// NMEA2000® is a registered Trademark of the National Marine Electronics Association
// Fusion® is a registered Trademark of Fusion Entertainment

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCanMedia - Encodes/Decodes PGN's to control Fusion media players
// Owner: twocanplugin@hotmail.com
// Date: 10/10/2021
// Version History: 
// 1.0 Initial Release of Fusion Media controller
 
#include "twocanmedia.h"

TwoCanMedia::TwoCanMedia() {
	sessionId = FUSION_MEDIA_PORT::unknown;
	sourceId = FUSION_MEDIA_PORT::unknown;
	usbMapping = FUSION_MEDIA_PORT::unknown;

	// BUG BUG DEBUG REMOVE
	// Initialize UDP socket for debug spew
	addrLocal.Hostname();
	addrPeer.Hostname("127.0.0.1");
	addrPeer.Service(3002);

	debugSocket = new wxDatagramSocket(addrLocal, wxSOCKET_NONE);
}

TwoCanMedia::~TwoCanMedia(void) {
	// BUG BUG DEBUG REMOVE
	debugSocket->Close();
}

// Decode Media Player responses
// We don't reply to most of these, the data is used to populate fields on the media player 
// control dialog. Eg. Unit & Zone names, AM/FM frequency & station name, track names & length.
// The only responses are for the handshaking when enumerating media folders & tracks.

bool TwoCanMedia::DecodeMediaResponse(const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	// Each media source has a name & corresponding number
	wxString sourceName;
	FUSION_MEDIA_TYPE sourceType;
	FUSION_MEDIA_STATUS sourceStatus;

	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	// Could double check that manaufacturerId = 419 (Fusion) but we wouldn't be here if it didn't match

	byte messageId;
	messageId = payload[2];

	// payload[3] is usually seen as 0x80
	// must indicate a Fusion Media Command ??

	switch (messageId) {
	case 1: // Not yet determined
		//A3 99 01 80 01 00 19 00 02 00 C0 0D

		break;
	case 2: // Media sources
		// A3 99 02 80 08 01 0A 25 02 42 54 00

		sourceId = (FUSION_MEDIA_PORT)payload[4];
		sessionId = (FUSION_MEDIA_PORT)payload[5];
		sourceType = (FUSION_MEDIA_TYPE)payload[6];
		sourceStatus = (FUSION_MEDIA_STATUS)payload[7];

		sourceName.Clear();
		for (size_t i = 0; i < payload[8]; i++) {
			sourceName.append(1, payload[9 + i]);
		}

		root["entertainment"]["device"]["input"]["name"] = sourceName;
		root["entertainment"]["device"]["input"]["sourceid"] = sourceId;

		if ((sourceStatus & FUSION_MEDIA_STATUS::present) == FUSION_MEDIA_STATUS::present) {
			mediaSources.emplace(sourceId, sourceName);
			root["entertainment"]["device"]["input"]["inserted"] = true;
		}
		// Give the client an array of all the devices present
		if (mediaSources.size() == totalSources) {
			wxJSONValue v;
			for (auto it : mediaSources) {
				v.Clear();
				v["name"] = it.second;
				v["id"] = it.first;
				root["entertainment"]["device"]["sources"].Append(v);
			}

		}

		// if a usb device, set the mapping
		// provide the client with the name of the current device
		if ((sessionId == FUSION_MEDIA_PORT::ipod) || (sessionId == FUSION_MEDIA_PORT::mtp) || (sessionId == FUSION_MEDIA_PORT::usb)) {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(FUSION_MEDIA_PORT::usb);
				usbMapping = sourceId;
		}
		else {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(sessionId);;
		}

		// and finally give the client the session id to use for all subsequent requests

		root["entertainment"]["device"]["source"]["sessionid"] = sessionId;
		
		break;

	case 3: // Total number of media sources
		// A3 99 03 80 0A

		totalSources = payload[4];

		// BUG BUG Is this necessary
		root["entertainment"]["device"]["input"]["count"] = totalSources;
		
		break;
	case 4: // Track Information
		sessionId = (FUSION_MEDIA_PORT)payload[4];

		if ((sessionId == FUSION_MEDIA_PORT::ipod) || (sessionId == FUSION_MEDIA_PORT::mtp) || (sessionId == FUSION_MEDIA_PORT::usb)) {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(FUSION_MEDIA_PORT::usb);
			usbMapping = sourceId;
		}
		else {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(sessionId);;
		}
		root["entertainment"]["device"]["source"]["sessionid"] = sessionId;

		trackStatus = payload[5];

		// BUG BUG Do we need to raise a power event 
		if (trackStatus == 0x00) {
			root["entertainment"]["device"]["power"] = false;
			break;
		}
		else {
			root["entertainment"]["device"]["power"] = true;
			if ((trackStatus & 0x01) == 0x01) {
				root["entertainment"]["device"]["playing"] = true;
			}
			if ((trackStatus & 0x02) == 0x02) {
				root["entertainment"]["device"]["playing"] = false;
			}
			if ((trackStatus & 0x20) == 0x20) {
				root["entertainment"]["device"]["repeat"] = true;
			}
			else {
				root["entertainment"]["device"]["repeat"] = false;
			}
			if ((trackStatus & 0x40) == 0x40) {
				root["entertainment"]["device"]["shuffle"] = true;
			}
			else {
				root["entertainment"]["device"]["shuffle"] = false;
			}

			// unsure about payload[6]

			trackId = payload[7] | (payload[8] << 8) | (payload[9] << 16) | (payload[10] << 24);
			totalTracks = payload[11] | (payload[12] << 8) | (payload[13] << 16) | (payload[14] << 24);
			trackLength = 1e-3 * (payload[15] | (payload[16] << 8) | (payload[17] << 16) | (payload[18] << 24));

			root["entertainment"]["device"]["track"]["number"] = trackId;
			root["entertainment"]["device"]["track"]["tracks"] = totalTracks;
			root["entertainment"]["device"]["track"]["length"] = trackLength;
		}

		break;

	case 5: // Track Name
		// A3 99 05 80 05 02 00 00 
		// 00 0C 30 32 5F 4A 75 64
		// 67 65 6D 65 6E 74 00

		// When paused not playing on BT
		// A3 99 05 80 08 FF FF FF
		// FF 01 20 00 track name is a space, track number is data unavailable
		sessionId = (FUSION_MEDIA_PORT)payload[4];

		if ((sessionId == FUSION_MEDIA_PORT::ipod) || (sessionId == FUSION_MEDIA_PORT::mtp) || (sessionId == FUSION_MEDIA_PORT::usb)) {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(FUSION_MEDIA_PORT::usb);
			usbMapping = sourceId;
		}
		else {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(sessionId);;
		}
		root["entertainment"]["device"]["source"]["sessionid"] = sessionId;

		if ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24)) == trackId) {
			trackName.Clear();
			for (size_t i = 0; i < payload[9]; i++) {
				trackName.append(1, payload[10 + i]);
			}

			trackNames.emplace(trackId, trackName);

			root["entertainment"]["device"]["source"]["sessionid"] = sessionId;
			root["entertainment"]["device"]["track"]["name"] = trackName;
		}
		
		break;

	case 6:
		 // A3 99 06 80 08 FF FF FF FF 00 00
		// payload[4] obviously source
		// data unavailable ....
		break;

	case 9: // Elapsed time
		// A3 99 09 80 05 8D B1 00 00

		// I presume only valid for USB & iPod devices
		// BUG BUG Better way of determing what kind of device

		sessionId = (FUSION_MEDIA_PORT)payload[4];

		// By definition, if the time is elapsing, we're playing....
		root["entertainment"]["device"]["playing"] = true;

		if ((sessionId == FUSION_MEDIA_PORT::ipod) || (sessionId == FUSION_MEDIA_PORT::mtp) || (sessionId == FUSION_MEDIA_PORT::usb)) {
			root["entertainment"]["device"]["source"]["name"] = _T("usb");// GetMediaSourceById(FUSION_MEDIA_PORT::usb);
			usbMapping = sessionId;
			elapsedTime = 1e-3 * (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24));
			root["entertainment"]["device"]["track"]["elapsedtime"] = elapsedTime;
		}
		// If we are not a USB Media Player, then just display the elapsed time as 100%
		// however see below for radio as we overload these two values
		else {
			root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(sessionId);
			root["entertainment"]["device"]["track"]["elapsedtime"] = 100;
			root["entertainment"]["device"]["track"]["length"] = 100;
		}
		root["entertainment"]["device"]["source"]["sessionid"] = sessionId;

		break;

	case 11: // Radio Station Frequency & Name
		
		// A3 99 0B 80 01 02 20 9C 52 05 BE 00 00
		// A3 99 0B 80 01 02 20 9C 52 05 9B 00 00

		sessionId = (FUSION_MEDIA_PORT)payload[4];
		root["entertainment"]["device"]["source"]["name"] = GetMediaSourceById(sessionId);
		root["entertainment"]["device"]["source"]["sessionid"] = sessionId;
				
		radioFrequency = (payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24)) * (sessionId == FUSION_MEDIA_PORT::am ? 1e-3 : 1e-6);

		// payload 10 unknown, perhaps signal quality

		radioStationName.Clear();
		for (size_t i = 0; i < payload[11]; i++) {
			radioStationName.append(1, payload[12 + i]);
		}

		root["entertainment"]["device"]["radio"]["name"] = radioStationName + (sessionId == FUSION_MEDIA_PORT::am ? " (AM)" : " (FM)");
		root["entertainment"]["device"]["radio"]["frequency"] = radioFrequency;
		
		// overload these two values so the progress bar displays the frequency
		root["entertainment"]["device"]["track"]["elapsedtime"] = sessionId == FUSION_MEDIA_PORT::am ? round(radioFrequency - 520) : round(radioFrequency - 87);
		root["entertainment"]["device"]["track"]["length"] = sessionId == FUSION_MEDIA_PORT::am ? 1100 : 22;
		break;

	case 13:

		break;

	case 15: // Selected Directory item
		// A3 99 0F 80 05 00 00 00 
		// 00 02 06

		// A3 99 0F 80 05 00 00 00 
		// 00 02 06

		if (sessionId == payload[4]) {
			folderId = payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24);
			folderType = (MEDIA_FILE_TYPE)payload[9];
			folderSessionId = payload[10];

			if (folderType == 0x01) { // Root Folder
				root["entertainment"]["device"]["media"]["rootfolder"] = true;
			}
			if (folderType == 0x02) { // normal folder
				root["entertainment"]["device"]["media"]["rootfolder"] = false;
			}
			root["entertainment"]["device"]["media"]["folderid"] = folderId;
			root["entertainment"]["device"]["media"]["foldersessionid"] = folderSessionId;
		}

		// We need some way to synchronize/wait

		break;

	case 16: // Number of items in folder
		// A3 99 10 80 05 03 00 00 00 06
		if (sessionId == payload[4]) { // && (folderSessionId == payload[9])) {
			int folderCount = payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24);
			root["entertainment"]["device"]["media"]["count"] = folderCount;
			root["entertainment"]["device"]["media"]["foldersessionid"] = payload[9];

			//BUG BUG DEBUG
			wxString debugMessage = wxString::Format("Folder Items: %d Folder Session Id: %d", folderCount, folderSessionId);
			debugSocket->SendTo(addrPeer, debugMessage.data(), debugMessage.Length());

			
		}
		break;

	case 17: // file/folder Names
		// A3 99 11 80 05 00 00 00
		// 00 4F 06 05 4D 55 53 49
		// 43 00

		// A3 99 11 80 05 00 00 00 
		// 00 47 06 06 49 41 55 44
		// 49 4F 00

		// A3 99 11 80 05 02 00 00 
		// 00 4F 06 06 53 59 53 54
		// 45 4D 00
		
		// A3 99 11 80 05 08 00 00
		// 00 1F 06 24 30 39 5F 42
		// 6C 61 63 6B 5F 61 6E 64
		// 5F 42 6C 75 65 5F 41 6C
		// 62 75 6D 5F 56 65 72 73
		// 69 6F 6E 5F 2E 6D 70 33
		// 00

		if ((sessionId == payload[4]) && (folderSessionId == payload[10])) {

			folderId = (payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24));

			folderType = payload[9];

			int length = payload[11];

			folderName.clear();
			for (int i = 0; i < length; i++) {
				folderName.append(1, payload[12 + i]);
			}
			
			root["entertainment"]["device"]["media"]["foldername"] = folderName;
			root["entertainment"]["device"]["media"]["foldertype"] = folderType;
			root["entertainment"]["device"]["media"]["folderid"] = folderId;

			wxString debugMessage = wxString::Format("Folder Name: %s Folder Id: %d", folderName, folderId);
			debugSocket->SendTo(addrPeer, debugMessage.data(), debugMessage.Length());


		}		
		break;

	case 18: // Not yet determined
		// A3 99 12 80 6B 0E 00 00 01

		break;

	case 19: //
		// A3 99 13 80 04 00
		// Auxilliary Media Source Gain
		// BUG BUG, aux 0 may just be 'aux' ??
		root["entertainment"]["device"]["source"]["aux" + std::to_string(payload[4])]["gain"] = payload[5];

		break;

	case 21: // Sirius stuff
		/*
		//A3
		99 15 80 1A 00 00 00 07
			00 00 00 01 00 00 00 08
			00 00 00 00 00 00 00 06
			00 00 00 00 00 00 00 00
			00 00 00 04 00 00 00 01
			00 00 00 00 00 00 00 0C
			00 00 00 02 00 00 00 0D
			00 00 00 00 00 00 00 0B
			00 00 00 00 00 00 00 02
			00 00 00 00 00 00 00 03
			00 00 00 01 00 00 00 04
			00 00 00 00 00 00 00 05
			00 00 00 00 00 00 00 09
			00 00 00 01 00 00 00 0A
			00 00 00 00 00 00 00 0E
			00 00 00 00 00 00 00 0F
			00 00 00 00 00 00 00 10
			00 00 00 00 00 00 00 11
			00 00 00 00 00 00 00 12
			00 00 00 00 00 00 00 13
			00 00 00 00 00 00 00 14
			00 00 00 00 00 00 00 15
			00 00 00 00 00 00 00 16
			00 00 00 00 00 00 00 17
			00 00 00 00 00 00 00 18
			00 00 00 00 00 00 00 19
			00 00 00 00 00 00 00
			*/
			/*
			A3
	99 15 80 1A 00 00 00 1A
	00 00 00 00 00 00 00 1B
	00 00 00 00 00 00 00 1C
	00 00 00 00 00 00 00 1D
	00 00 00 00 00 00 00 1E
	00 00 00 00 00 00 00 1F
	00 00 00 00 00 00 00 20
	00 00 00 00 00 00 00 21
	00 00 00 00 00 00 00 22
	00 00 00 00 00 00 00 23
	00 00 00 00 00 00 00 24
	00 00 00 00 00 00 00 25
	00 00 00 00 00 00 00 26
	00 00 00 00 00 00 00 27
	00 00 00 00 00 00 00 28
	00 00 00 00 00 00 00 29
	00 00 00 00 00 00 00 2A
	00 00 00 00 00 00 00 2B
	00 00 00 00 00 00 00 2C
	00 00 00 00 00 00 00 2D
	00 00 00 00 00 00 00 2E
	00 00 00 00 00 00 00 2F
	00 00 00 00 00 00 00 30
	00 00 00 00 00 00 00 31
	00 00 00 00 00 00 00 32
	00 00 00 00 00 00 00 33
	00 00 00 00 00 00 00
			*/
		break;
	case 23: // Mute/Unmute
		// A3 99 17 80 01 Mute
		// A3 99 17 80 02 Unmute
		if (payload[4] == 0x01) {
			root["entertainment"]["device"]["mute"] = true;
		}
		else if (payload[4] == 0x02) {
			root["entertainment"]["device"]["mute"] = false;
		}

	case 24: // balance
		// A3 99 18 80 00 FE
		// Signed, +ve Right Channel, -ve Left Channel
		balance = payload[5];

		root["entertainment"]["device"]["zone" + std::to_string(payload[4])]["balance"] = balance;

		break;

	case 26: // Sub Woofer
		// A3 99 1A 80 0C 0C 0C 00

		zone0SubWoofer = payload[4];
		zone1SubWoofer = payload[5];
		zone2SubWoofer = payload[6];
		
		root["entertainment"]["device"]["zone0"]["subwoofer"] = zone0SubWoofer;
		root["entertainment"]["device"]["zone1"]["subwoofer"] = zone1SubWoofer;
		root["entertainment"]["device"]["zone2"]["subwoofer"] = zone2SubWoofer;

		break;
	
	case 27: // Tone Controls
		// A3 99 1B 80 03 08 05 03
		// Note values range from -15 to 15, so ensure all clients used a signed int
		// For Two's Complement bass = (payload[5] & 0x80) == 0x80 ? payload[5] - 256 : payload[5];
		bass = payload[5]; 
		midrange = payload[6];
		treble = payload[7];
		
		root["entertainment"]["device"]["tone"]["bass"] = bass;
		root["entertainment"]["device"]["tone"]["midrange"] = midrange;
		root["entertainment"]["device"]["tone"]["treble"] = treble;

	break;
	case 28: // Not yet determined
		/*
		A3 99 1C 80 10 13 18 00
		*/
		break;
	case 29: // Volume
		// A3 99 1D 80 01 0A 00 00
		// A3 99 1D 80 01 09 00 00
				
		zone0Volume = payload[4];
		
		zone1Volume = payload[5];
		
		zone2Volume = payload[6];
		
		root["entertainment"]["device"]["zone0"]["volume"] = zone0Volume;
		root["entertainment"]["device"]["zone1"]["volume"] = zone1Volume;
		root["entertainment"]["device"]["zone2"]["volume"] = zone2Volume;

		break;
	case 30: // Not yet determined
		// A3 99 1E 80 07 00 07 00 1F 02 00 00 C0 01
		break;

	case 32: // Power State
	// A3 99 20 80 01 On
	// A3 99 20 80 02 Off

		if (payload[4] == 0x01) {
			root["entertainment"]["device"]["power"] = true;
		}
		if (payload[4] == 0x02) {
			root["entertainment"]["device"]["power"] = false;
		}
		break;

	case 33: // Device Name
		deviceName.Clear();
		for (size_t i = 0; i < payload[4]; i++) {
			deviceName.append(1, payload[5 + i]);
		}
		
		root["entertainment"]["device"]["name"] = deviceName;

		break;

	case 34: // Not yet determined
		//A3 99 22 80 02 01 00 FF FF 01
		break;

	case 45: {// Zone Names
		// A3 99 2D 80 00 06 53 41 4C 4F 4F 4E 00
		// A3 99 2D 80 01 07 43 4F 43 4B 50 49 54 00
		// A3 99 2D 80 02 06 5A 6F 6E 65 20 33 00;

		byte zoneId;
		zoneId = payload[4];

		wxString zoneName;
		unsigned int zoneNameLength = payload[5];
		for (size_t i = 0; i < zoneNameLength; i++) {
			zoneName.append(1, payload[6 + i]);
		}

		zoneNames.emplace(zoneId, zoneName);
		
		root["entertainment"]["device"][wxString::Format("zone%d",zoneId)]["name"] = zoneName;

	}
		break;

	case 54: // Not yet determined
		// A3 99 36 80 06 00 88 13 01
		// A3 99 36 80 07 00 88 13 01
		// A3 99 36 80 08 00 DC 05 01

		break;

	} // end switch

	// Check if we have something to return, need to fix or remove the case statements that are unknown
	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}

	return FALSE;
}

// Encode Media Player commands
bool TwoCanMedia::EncodeMediaCommand(wxString text, std::vector<CanMessage> *canMessages) {
	CanMessage message;
	
	// Even though these commands are all less than 8 bytes (except tone controls)
	// Fusion decided to send them as a fast packet !! Go figure....
	message.header.pgn = 126720;
	message.header.source = networkAddress;
	message.header.priority = 7;
	message.header.destination = CONST_GLOBAL_ADDRESS;

	wxJSONReader reader;
	wxJSONValue root;

	if (reader.Parse(text, &root) > 0) {
		// BUG BUG should log errors, but as I'm generating the JSON, there shouldn't be any
		wxLogMessage("TwoCan Media, JSON Error in following");
		wxLogMessage("%s", text);
		wxArrayString jsonErrors = reader.GetErrors();
		for (auto it : jsonErrors) {
			wxLogMessage(it);
		}
		return FALSE;
	}

	// Power On/Off
	if (root["entertainment"]["device"].HasMember("power")) {
		bool power = root["entertainment"]["device"]["power"].AsBool();
		if (power == TRUE) { 
			// Power on
			// A3 99 01 00 
			message.payload.push_back(0xA0); 
			message.payload.push_back(0x04); 
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x01);
			message.payload.push_back(0x00);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);
			
		}
		else {
			// Power Off
			// A3 99 1C 00 02 
			message.payload.push_back(0xA0); 
			message.payload.push_back(0x05);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x1C);
			message.payload.push_back(0x00);
			message.payload.push_back(0x02);
			message.payload.push_back(0xFF);
		}

		canMessages->push_back(message);

		return TRUE;
	}
	// Mute/Unmute
	if (root["entertainment"]["device"].HasMember("mute")) {
		bool mute = root["entertainment"]["device"]["mute"].AsBool();
		if (mute == TRUE) {
			// Mute
			// A3 99 11 00 01
			message.payload.push_back(0xA0); 
			message.payload.push_back(0x05);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x11);
			message.payload.push_back(0x00);
			message.payload.push_back(0x01);
			message.payload.push_back(0xFF);
		}
		else {
			// Unmute
			// A3 99 11 00 02
			message.payload.push_back(0xA0); 
			message.payload.push_back(0x05);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x11);
			message.payload.push_back(0x00);
			message.payload.push_back(0x02);
			message.payload.push_back(0xFF);
		}
		canMessages->push_back(message);
		return TRUE;
	}

	if (root["entertainment"]["device"].HasMember("repeat")) {
		bool repeat = root["entertainment"]["device"]["repeat"].AsBool();

		if (repeat == TRUE) {
			// Toggle Repeat
			// A3 99 0F 00 09 00 
			// 00 00 00 00 00 00

			message.payload.push_back(0xA0); 
			message.payload.push_back(0x0C);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x0F);
			message.payload.push_back(0x00);
			message.payload.push_back(0x09);
			message.payload.push_back(0x00);

			canMessages->push_back(message);

			message.payload.clear();

			message.payload.push_back(0xA1); 
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);

			canMessages->push_back(message);

			return TRUE;
		}
	}

	if (root["entertainment"]["device"].HasMember("shuffle")) {
		bool shuffle = root["entertainment"]["device"]["shuffle"].AsBool();
		if (shuffle == TRUE) {
			// Toggle Shuffle
			// A3 99 0F 00 0A 00 
			// 00 00 00 00 00 00

			message.payload.push_back(0xA0); 
			message.payload.push_back(0x0C);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x0F);
			message.payload.push_back(0x00);
			message.payload.push_back(0x0A);
			message.payload.push_back(0x00);

			canMessages->push_back(message);

			message.payload.clear();

			message.payload.push_back(0xA1); 
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0x00);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);

			canMessages->push_back(message);
		
			return TRUE;
		}

	}

	if (root["entertainment"]["device"].HasMember("play")) {
		bool play = root["entertainment"]["device"]["play"].AsBool();
		
		if (play == TRUE) {

			// Play
			// A3 99 03 00 05 01
			message.payload.push_back(0xA0); 
			message.payload.push_back(0x06);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x03);
			message.payload.push_back(0x00);
			message.payload.push_back(sessionId);
			message.payload.push_back(0x01);

			canMessages->push_back(message);

		}
		else {
			// Pause
			// A3 99 03 00 05 02
			message.payload.push_back(0xA0); 
			message.payload.push_back(0x06);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x03);
			message.payload.push_back(0x00);
			message.payload.push_back(sessionId);
			message.payload.push_back(0x02);

			canMessages->push_back(message);

		}
		return TRUE;
	}

	if (root["entertainment"]["device"].HasMember("next")) {
		bool next = root["entertainment"]["device"]["next"].AsBool();
		
		if (next == TRUE) {
			// Different commands for am/fm vs usb
			// For radios, there are three modes, manual, auto & preset. 
			// presets seem to be stored in the controller
			// manual increments by 9KHz steps for am and 5MHz steps for am
			// No idea how or what the auto mode determines where to tune next.
			if ((sessionId == FUSION_MEDIA_PORT::am) || (sessionId == FUSION_MEDIA_PORT::fm)) {
				int frequency = root["entertainment"]["device"]["radio"]["frequency"].AsDouble() * 1e6;
				// A3 99 05 00 01 02 80 04 39 06
				message.payload.push_back(0xA0); 
				message.payload.push_back(0x0A);
				message.payload.push_back(0xA3);
				message.payload.push_back(0x99);
				message.payload.push_back(0x05);
				message.payload.push_back(0x00);
				message.payload.push_back(sessionId);
				message.payload.push_back(0x01); // Forward - Auto

				canMessages->push_back(message);

				message.payload.clear();

				message.payload.push_back(0xA1); 
				message.payload.push_back(frequency & 0xFF);
				message.payload.push_back((frequency >> 8) & 0xFF);
				message.payload.push_back((frequency >> 16) & 0xFF);
				message.payload.push_back((frequency >> 24) & 0xFF);
				message.payload.push_back(0xFF);
				message.payload.push_back(0xFF);
				message.payload.push_back(0xFF);

				canMessages->push_back(message);
			}
			
			else if ((sessionId == FUSION_MEDIA_PORT::usb) || (sessionId == FUSION_MEDIA_PORT::mtp) || (sessionId == FUSION_MEDIA_PORT::ipod)) {
				// Next
				// A3 99 03 00 05 04
				message.payload.push_back(0xA0); 
				message.payload.push_back(0x06);
				message.payload.push_back(0xA3);
				message.payload.push_back(0x99);
				message.payload.push_back(0x03);
				message.payload.push_back(0x00);
				message.payload.push_back(sessionId);
				message.payload.push_back(0x04);

				canMessages->push_back(message);
			}

			return TRUE;
		}
	}

	if (root["entertainment"]["device"].HasMember("previous")) {
		bool previous = root["entertainment"]["device"]["previous"].AsBool();
		
		if (previous == TRUE) {

			if ((sessionId == FUSION_MEDIA_PORT::am) || (sessionId == FUSION_MEDIA_PORT::fm)) {
				int frequency = root["entertainment"]["device"]["radio"]["frequency"].AsDouble() * 1e6;
				// A3 99 05 00 01 02 80 04 39 06
				message.payload.push_back(0xA0); 
				message.payload.push_back(0x0A);
				message.payload.push_back(0xA3);
				message.payload.push_back(0x99);
				message.payload.push_back(0x05);
				message.payload.push_back(0x00);
				message.payload.push_back(sessionId);
				message.payload.push_back(0x03); // Reverse - Auto

				canMessages->push_back(message);

				message.payload.clear();

				message.payload.push_back(0xA1); 
				message.payload.push_back(frequency & 0xFF);
				message.payload.push_back((frequency >> 8) & 0xFF);
				message.payload.push_back((frequency >> 16) & 0xFF);
				message.payload.push_back((frequency >> 24) & 0xFF);
				message.payload.push_back(0xFF);
				message.payload.push_back(0xFF);
				message.payload.push_back(0xFF);

				canMessages->push_back(message);

			}

			else if ((sessionId == FUSION_MEDIA_PORT::usb) || (sessionId == FUSION_MEDIA_PORT::mtp) || (sessionId == FUSION_MEDIA_PORT::ipod)) {
				// Previous
				// A3 99 09 00 05 00 00 00 00 03 01
				// A3 99 09 00 05 01 00 00 00 04 05
				message.payload.push_back(0xA0); 
				message.payload.push_back(0x0B);
				message.payload.push_back(0xA3);
				message.payload.push_back(0x99);
				message.payload.push_back(0x09);
				message.payload.push_back(0x00);
				message.payload.push_back(sessionId);
				message.payload.push_back(0x00);	

				message.payload.clear();

				message.payload.push_back(0xA1);
				message.payload.push_back(0x00);
				message.payload.push_back(0x00);
				message.payload.push_back(0x00);
				message.payload.push_back(0x03);
				message.payload.push_back(folderSessionId);
				message.payload.push_back(0xFF);
				message.payload.push_back(0xFF);
				
				canMessages->push_back(message);
				
			}

			return TRUE;
		}
	}

	// BUG BUG Note implemented yet in UI, needs monotonically increasing preset number.
	// Save preset radio station
	// A3 99 0F 00 1F 00 00 00 E0 2D 74 05
	if (root["entertainment"]["device"].HasMember("preset")) {
		bool preset = root["entertainment"]["device"]["preset"].AsBool();
		int frequency = root["entertainment"]["device"]["frequency"].AsInt();
		int presetValue = 0x1E;
		if (preset == TRUE) {
			message.payload.push_back(0xA0);
			message.payload.push_back(0x0C);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x0F);
			message.payload.push_back(0x00);
			message.payload.push_back(presetValue); // BUG BUG Monotonically increasing number
			message.payload.push_back((presetValue >> 8) & 0xFF);

			canMessages->push_back(message);

			message.payload.clear();

			message.payload.push_back(0xA1);
			message.payload.push_back((presetValue >> 16) & 0xFF);
			message.payload.push_back((presetValue >> 24) & 0xFF);
			message.payload.push_back(frequency & 0xFF);
			message.payload.push_back((frequency >> 8) & 0xFF);
			message.payload.push_back((frequency >> 16) & 0xFF);
			message.payload.push_back((frequency >> 24) & 0xFF);
			message.payload.push_back(0xFF);

			canMessages->push_back(message);
		}
		return TRUE;
	}

	// Source Selection
	if (root["entertainment"]["device"].HasMember("source")) {
		wxString sourceName = root["entertainment"]["device"]["source"].AsString().Lower();
		int source;
		if ((sourceName == "usb") && (usbMapping != FUSION_MEDIA_PORT::unknown)) {
			source = usbMapping; // UI just has 'usb', but it may map to any usb device; usb, mtp, ipod
		}
		else if ((sourceName == "usb") && (usbMapping == FUSION_MEDIA_PORT::unknown)) {
			source = FUSION_MEDIA_PORT::usb;
		}
		else {
			source = GetMediaSourceByName(sourceName);
		}

		if (source == -1) {
			return FALSE;
		}
		//A3 99 02 00 03
		
		message.payload.push_back(0xA0); 
		message.payload.push_back(0x05);
		message.payload.push_back(0xA3);
		message.payload.push_back(0x99);
		message.payload.push_back(0x02);
		message.payload.push_back(0x00);
		message.payload.push_back(source);
		message.payload.push_back(0xFF);

		canMessages->push_back(message);
		return TRUE;
	}
	// Volume (Note zero based, zone1 = 0)
	if (root["entertainment"]["device"]["zone0"].HasMember("volume")) {
		int volume = root["entertainment"]["device"]["zone0"]["volume"].AsInt();
		//A3 99 18 00 01 09
		message.payload.push_back(0xA0); 
		message.payload.push_back(0x06);
		message.payload.push_back(0xA3);
		message.payload.push_back(0x99);
		message.payload.push_back(0x18);
		message.payload.push_back(0x00);
		message.payload.push_back(0x00);
		message.payload.push_back(volume);

		canMessages->push_back(message);

		return TRUE;
	}
	if (root["entertainment"]["device"]["zone1"].HasMember("volume")) {
		int volume = root["entertainment"]["device"]["zone1"]["volume"].AsInt();
		message.payload.push_back(0xA0); 
		message.payload.push_back(0x06);
		message.payload.push_back(0xA3);
		message.payload.push_back(0x99);
		message.payload.push_back(0x18);
		message.payload.push_back(0x00);
		message.payload.push_back(0x01);
		message.payload.push_back(volume);

		canMessages->push_back(message);
		return TRUE;

	}

	if (root["entertainment"]["device"].HasMember("tone")) {
		int bass = root["entertainment"]["device"]["tone"]["bass"].AsInt();
		int midrange = root["entertainment"]["device"]["tone"]["midrange"].AsInt();
		int treble = root["entertainment"]["device"]["tone"]["treble"].AsInt();

		// tone - last 3 bytes are bass, mid, treble
		// A3 99 16 00 03 08 04 05 
		// Bugger, this will need to be fragmented

		message.payload.push_back(0xA0); 
		message.payload.push_back(0x08);
		message.payload.push_back(0xA3);
		message.payload.push_back(0x99);
		message.payload.push_back(0x16);
		message.payload.push_back(0x00);
		message.payload.push_back(0x03); // Unsure
		message.payload.push_back(bass);

		canMessages->push_back(message);

		message.payload.clear();

		message.payload.push_back(0xA1); 
		message.payload.push_back(midrange);
		message.payload.push_back(treble);
		message.payload.push_back(0xFF);
		message.payload.push_back(0xFF);
		message.payload.push_back(0xFF);
		message.payload.push_back(0xFF);
		message.payload.push_back(0xFF);

		canMessages->push_back(message);
		return TRUE;

	}

	if (root["entertainment"]["device"]["media"].HasMember("request")) {
		int request = root["entertainment"]["device"]["media"]["request"].AsInt();
		int folderId = root["entertainment"]["device"]["media"]["folderid"].AsInt();
		
		// BUG BUG Remove
		wxString debugMessage = wxString::Format("Send folder request: FolderId: %d, Folder Session Id: %d RQST: %d", folderId, folderSessionId, request);
		debugSocket->SendTo(addrPeer, debugMessage.data(), debugMessage.Length());

		// A3 99 09 00 05 00 00 00 00 01 02

		// 0 just gets message 16, ?
		// 1 is get current folder,
		// 2 is next
		// 3 is back

		message.payload.push_back(0xA0);
		message.payload.push_back(0x0B);
		message.payload.push_back(0xA3);
		message.payload.push_back(0x99);
		message.payload.push_back(0x09);
		message.payload.push_back(0x00);
		message.payload.push_back(sessionId);
		message.payload.push_back(folderId & 0xFF);

		canMessages->push_back(message);
		message.payload.clear();

		message.payload.push_back(0xA1);
		message.payload.push_back((folderId >> 8) & 0xFF);
		message.payload.push_back((folderId >> 16) & 0xFF);
		message.payload.push_back((folderId >> 24) & 0xFF);
		message.payload.push_back(request);
		message.payload.push_back(folderSessionId);
		message.payload.push_back(0xFF);
		message.payload.push_back(0xFF);

		canMessages->push_back(message);

		return TRUE;

	}

	if (root["entertainment"]["device"]["media"].HasMember("ack")) {
		if (root["entertainment"]["device"]["media"]["ack"].AsBool() == true) {
			//A3 99 0A 00 05 02
			message.payload.push_back(0xA0);
			message.payload.push_back(0x06);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x0A);
			message.payload.push_back(0x00);
			message.payload.push_back(sessionId);
			message.payload.push_back(folderSessionId);

			// BUG BUG Remove
			wxString debugMessage = wxString::Format("Sent Ack, Session Id: %d, Folder Session Id: %d", sessionId, folderSessionId);
			debugSocket->SendTo(addrPeer, debugMessage.data(), debugMessage.Length());

			canMessages->push_back(message);
			return TRUE;
		}
	}

	if (root["entertainment"]["device"]["media"].HasMember("confirm")) {
		if (root["entertainment"]["device"]["media"]["confirm"].AsBool() == true) {
			int folderId = root["entertainment"]["device"]["media"]["folderid"].AsInt();
			int recordsReceived = root["entertainment"]["device"]["media"]["recordsreceived"].AsInt();
			// Send Confirmation
			// A3 99 0B 00 05 00 00 00 00 01 00 00 00 02

			message.payload.push_back(0xA0);
			message.payload.push_back(0x0E);
			message.payload.push_back(0xA3);
			message.payload.push_back(0x99);
			message.payload.push_back(0x0B);
			message.payload.push_back(0x00);
			message.payload.push_back(sessionId);
			message.payload.push_back(folderId & 0xFF);

			canMessages->push_back(message);
			message.payload.clear();

			message.payload.push_back(0xA1);
			message.payload.push_back((folderId >> 8) & 0xFF);
			message.payload.push_back((folderId >> 16) & 0xFF);
			message.payload.push_back((folderId >> 24) & 0xFF);
			message.payload.push_back(recordsReceived & 0xFF);
			message.payload.push_back((recordsReceived >> 8) & 0xFF);
			message.payload.push_back((recordsReceived >> 16) & 0xFF);
			message.payload.push_back((recordsReceived >> 24) & 0xFF);

			canMessages->push_back(message);
			message.payload.clear();

			message.payload.push_back(0xA2);
			message.payload.push_back(folderSessionId);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);
			message.payload.push_back(0xFF);

			canMessages->push_back(message);

			wxString debugMessage = wxString::Format("Sent Confirm SessionId: %d, Folder Id %d, Records: %d", sessionId, folderId, recordsReceived);
			debugSocket->SendTo(addrPeer, debugMessage.data(), debugMessage.Length());


			return TRUE;
		}

	}

	return FALSE;
}

// I guess modern programmers would use std::map !!
const wxString TwoCanMedia::GetMediaSourceById(const int sourceId) {
	wxString sourceName = wxEmptyString;
	switch (FUSION_MEDIA_PORT(sourceId)) {
	case FUSION_MEDIA_PORT::am:
		sourceName = "am";
		break;
	case FUSION_MEDIA_PORT::fm:
		sourceName = "fm";
		break;
	case FUSION_MEDIA_PORT::sxm:
		sourceName = "sxm";
		break;
	case FUSION_MEDIA_PORT::aux:
		sourceName = "aux";
		break;
	case FUSION_MEDIA_PORT::aux2:
		sourceName = "aux2";
		break;
	case FUSION_MEDIA_PORT::usb:
		sourceName = "usb";
		break;
	case FUSION_MEDIA_PORT::ipod:
		sourceName = "ipod";
		break;
	case FUSION_MEDIA_PORT::mtp:
		sourceName = "mtp";
		break;
	case FUSION_MEDIA_PORT::bt:
		sourceName = "bt";
		break;
	case FUSION_MEDIA_PORT::dab:
		sourceName = "dab";
		break;
	}
	return sourceName;
}

const int TwoCanMedia::GetMediaSourceByName(const wxString sourceName) {
	// BUG BUG Guessing that  sxm(Sirius) is 2, ipod is 6, dab is 7 
	// as unable to test/verify
	
	int sourceId;
	sourceId = -1;
	if (sourceName == "am") {
		sourceId = 0;
	}
	else if (sourceName == "fm") {
		sourceId = 1;
	}
	else if (sourceName == "sxm") {
		sourceId = 2;
	}
	else if (sourceName == "aux") {
		sourceId = 3;
	}
	else if (sourceName == "aux2") {
		sourceId = 4;
	}
	else if (sourceName == "usb") {
		sourceId = 5;
	}
	else if (sourceName == "ipod") {
		sourceId = 6;
	}
	else if (sourceName == "mtp") {
		sourceId = 7;
	}
	else if (sourceName == "bt") {
		sourceId = 8;
	}
	else if (sourceName == "dab") {
		sourceId = 9;
	}
	return  sourceId;
	
}