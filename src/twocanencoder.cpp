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

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCanEncoder - converts NMEA 183 sentences to NMEA 2000 messages
// Owner: twocanplugin@hotmail.com
// Date: 01/12/2020
// Version History: 
// 1.0 Initial Release of Bi-directional Gateway
 

#include "twocanencoder.h"


TwoCanEncoder::TwoCanEncoder(void) {
}

TwoCanEncoder::~TwoCanEncoder(void) {
}

// Used to validate XDR Transducer Names and retrieve the instance number (as per NMEA 0183 v4.11 XDR standard)
// Eg. BATTERY#n, ENGINE#n, FUEL#n etc. where n is a digit from 0-9
int TwoCanEncoder::GetInstanceNumber(wxString transducerName) {
	int instanceNumber;
    // Check if second last character is '#'
    if (transducerName.Mid(transducerName.size() - 2,1).Cmp(_T("#")) == 0) {
        // Check if last character is a digit and between 0 - 9.
        if (transducerName.Mid(transducerName.size() - 1,1).IsNumber()) {
            instanceNumber = wxAtoi(transducerName.Mid(transducerName.size() - 1,1));
			if ((instanceNumber > -1) && (instanceNumber < 10)) {
				return instanceNumber;
			}

        }
    }
    return -1;
}

// BUG BUG Duplicated code from twocandevice.cpp. Should refactor & deduplicate
void TwoCanEncoder::FragmentFastMessage(CanHeader *header, std::vector<byte> *payload, std::vector<CanMessage> *canMessages) {
	size_t payloadLength = payload->size();
	std::vector<byte> data;
	CanMessage message;
	message.header = *header;

	// Fragment a fast message into a sequence of singleframe messages
	if (payloadLength > 8) {
	
		// The first frame
		// BUG BUG should maintain a map of sequential ID's for each PGN
		byte sid = 0;
		data.push_back(sid);
		data.push_back(payloadLength);
		for (std::vector<byte>::iterator it = payload->begin(); it != payload->begin() +6; ++it) {
    		data.push_back(*it);
		}
	
		message.payload = data;
		canMessages->push_back(message);

		sid += 1;
	
		// Intermediate frames
		int iterations;
		iterations = (int)((payloadLength - 6) / 7);	
		for (int i = 0; i < iterations; i++) {
			data.clear();

			data.push_back(sid);

			for (std::vector<unsigned char>::iterator it = payload->begin() + 6 + (i *7); it != payload->begin() + 13 + (i *7); ++it) {
        		data.push_back(*it);
    		}

			message.payload = data;
			canMessages->push_back(message);
		
			sid += 1;
		}
	
		// Any remaining frames ?
		int remainingBytes;
		remainingBytes = (payloadLength - 6) % 7;
		if (remainingBytes > 0) {
			data.clear();

			data.push_back(sid);
				
    		for (std::vector<unsigned char>::iterator it = payload->end() - remainingBytes; it != payload->end(); ++it) {
        		data.push_back(*it);
    		}

			// fill any unused bytes with 0xFF
	    	for (int i = 0; i < (7-remainingBytes); i++) {
        		data.push_back(0xFF);
    		}
	
			message.payload = data;
			canMessages->push_back(message);
		}
	}
	// Not a fast message, just a single frame message
	else {
		message.payload = *payload;
		canMessages->push_back(message);
	}
	
}


bool TwoCanEncoder::EncodeMessage(wxString sentence, std::vector<CanMessage> *canMessages) {
	CanHeader header;
	std::vector<byte> payload;
	
	// BUG BUG REMOVE
	wxLogMessage(_T("TwoCan Encoder, Debug Info. Sentence received: %s"), sentence);

	// Parse the NMEA 183 sentence
	nmeaParser << sentence;

	// BUG BUG ToDo (if ever......)
	// DSC and AIS
	if (nmeaParser.PreParse()) {

		// BUG BUG Should use a different priority based on the PGN
		// PGN is initialized further in the switch statement
		header.source = networkAddress;
		header.destination = CONST_GLOBAL_ADDRESS;
		header.priority = CONST_PRIORITY_MEDIUM;
		
		// BUG BUG Sequence Id is a monotonically increasing number
		// however it should indicate related readings from sensors 
		// Eg. each successive depth value should have successive SID's
		// The sequence id, an unsigned char, rolls over after 253. 
		sequenceId ++;
		if (!TwoCanUtils::IsDataValid(sequenceId)) {
			sequenceId = 0;
		}

		// APB Heading Track Controller(Autopilot) Sentence "B"
		if (nmeaParser.LastSentenceIDReceived == _T("APB")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_RTE)) {
					// if (EncodePGN127237(&nmeaParser, &payload)) { // Heading/Track Control
					//	header.pgn = 127237;
					//	FragmentFastMessage(&header, &payload, canMessages);
					// }
					if (EncodePGN129283(&nmeaParser, &payload)) {
						header.pgn = 129283;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129284(&nmeaParser, &payload)) {
						header.pgn = 129284;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				return TRUE;
				}
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// BOD Bearing - Origin to Destination
		else if (nmeaParser.LastSentenceIDReceived == _T("BOD")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_RTE)) {
				// IGNORE
				return FALSE;
				}
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// BWC Bearing & Distance to Waypoint Great Circle
		else if (nmeaParser.LastSentenceIDReceived == _T("BWC")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ZDA)) {
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 129033;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				if (!(supportedPGN & FLAGS_XTE)) {
					if (EncodePGN129283(&nmeaParser, &payload)) {
						header.pgn = 129283;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				if (!(supportedPGN & FLAGS_NAV)) {
					if (EncodePGN129284(&nmeaParser, &payload)) {
						header.pgn = 129284;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// BWR Bearing & Distance to Waypoint Rhumb Line
		else if (nmeaParser.LastSentenceIDReceived == _T("BWR")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ZDA)) {
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}
	
					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 129033;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
	
				if (!(supportedPGN & FLAGS_XTE)) {
					if (EncodePGN129283(&nmeaParser, &payload)) {
						header.pgn = 129283;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
	
				if (!(supportedPGN & FLAGS_NAV)) {
					if (EncodePGN129284(&nmeaParser, &payload)) {
						header.pgn = 129284;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
	
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// BWW Bearing Waypoint to Waypoint
		else if (nmeaParser.LastSentenceIDReceived == _T("BWW")) {
			if (nmeaParser.Parse()) {
				//IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// DBT Depth below transducer
		else if (nmeaParser.LastSentenceIDReceived == _T("DBT")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_DPT)) {
					if (EncodePGN128267(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// DPT Depth
		else if (nmeaParser.LastSentenceIDReceived == _T("DPT")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_DPT)) {
					if (EncodePGN128267(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// BUG BUG ToDo, Not actually implemented
		// DSC Digital Selective Calling Information
		else if (nmeaParser.LastSentenceIDReceived == _T("DSC")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_DSC)) {
					if (EncodePGN129808(&nmeaParser, &payload)) {
						header.pgn = 129808;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// BUG BUG ToDo, Not actully implemented
		// DSE Expanded Digital Selective Calling
		else if (nmeaParser.LastSentenceIDReceived == _T("DSE")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_DSC)) {
					if (EncodePGN129808(&nmeaParser, &payload)) {
						header.pgn = 129808;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// DTM Datum Reference
		else if (nmeaParser.LastSentenceIDReceived == _T("DTM")) {
			if (nmeaParser.Parse()) {
				// IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// GGA Global Positioning System Fix Data
		else if (nmeaParser.LastSentenceIDReceived == _T("GGA")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ZDA)) {
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				if (!(supportedPGN & FLAGS_GGA)) {
					if (EncodePGN129025(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129029(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				//EncodePGN129539(&nmeaParser, &payload); GNS DOP
					
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// GLL Geographic Position Latitude / Longitude
		else if (nmeaParser.LastSentenceIDReceived == _T("GLL")) {
			if (nmeaParser.Parse()) {
				// Date & Time
				if (!(supportedPGN & FLAGS_ZDA)) {
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}
					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 129033;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				// Position
				if (!(supportedPGN & FLAGS_GLL)) {
					if (EncodePGN129025(&nmeaParser, &payload)) {
						header.pgn = 129025;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129029(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// GNS GNSS Fix Data
		else if (nmeaParser.LastSentenceIDReceived == _T("GNS")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ZDA)) {
				
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 129033;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				if (!(supportedPGN & FLAGS_GGA)) {
					
					if (EncodePGN129025(&nmeaParser, &payload)) {
						header.pgn = 129025;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129029(&nmeaParser, &payload)) {
						header.pgn = 129029;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// GSA GNSS DOP and Active Satellites
		else if (nmeaParser.LastSentenceIDReceived == _T("GSA")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_GGA)) {
					if (EncodePGN129029(&nmeaParser, &payload)) {
						header.pgn = 129029;
						FragmentFastMessage(&header, &payload, canMessages);
					}
					
					//if (EncodePGN129539(&nmeaParser, &payload)) {
					//	header.pgn = 129539;
					//	FragmentFastMessage(&header, &payload, canMessages);
					//}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// GSV GNSS Satellites In View
		else if (nmeaParser.LastSentenceIDReceived == _T("GSV")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_GGA)) {
					if (EncodePGN129540(&nmeaParser, &payload)) {
						header.pgn = 129540;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// HDG Heading, Deviation & Variation
		else if (nmeaParser.LastSentenceIDReceived == _T("HDG")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_HDG)) {
					if (EncodePGN127250(&nmeaParser, &payload)) {
						header.pgn = 127250;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				
					if (EncodePGN127258(&nmeaParser, &payload)) {
						header.pgn = 127258;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN130577(&nmeaParser, &payload)) {
						header.pgn = 130577;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
					
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// HDM Heading, Magnetic
		else if (nmeaParser.LastSentenceIDReceived == _T("HDM")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_HDG)) {
				
					if (EncodePGN127250(&nmeaParser, &payload)) {
						header.pgn = 127250;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN130577(&nmeaParser, &payload)) {
						header.pgn = 130577;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// HDT Heading, True
		else if (nmeaParser.LastSentenceIDReceived == _T("HDT")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_HDG)) {
			
					if (EncodePGN127250(&nmeaParser, &payload)) {
						header.pgn = 127250;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				
					if (EncodePGN130577(&nmeaParser, &payload)) {
						header.pgn = 130577;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// MTW Water Temperature
		else if (nmeaParser.LastSentenceIDReceived == _T("MTW")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_MTW)) {
	
					if (EncodePGN130310(&nmeaParser, &payload)) {
						header.pgn = 130310;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN130311(&nmeaParser, &payload)) {
						header.pgn = 130311;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// MWD Wind Direction & Speed
		else if (nmeaParser.LastSentenceIDReceived == _T("MWD")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_MWV)) {
				
					if (EncodePGN130306(&nmeaParser, &payload)) {
						header.pgn = 130306;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// MWV Wind Speed & Angle
		else if (nmeaParser.LastSentenceIDReceived == _T("MWV")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_MWV)) {
		
					if (EncodePGN130306(&nmeaParser, &payload)) {
						header.pgn = 130306;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// RMB Recommended Minimum Navigation Information
		else if (nmeaParser.LastSentenceIDReceived == _T("RMB")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_XTE)) {
					if (EncodePGN129283(&nmeaParser, &payload)) {
						header.pgn = 129283;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				if (!(supportedPGN & FLAGS_NAV)) {
					if (EncodePGN129284(&nmeaParser, &payload)) {
						header.pgn = 129284;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// RMC Recommended Minimum Specific GNSS Data
		else if (nmeaParser.LastSentenceIDReceived == _T("RMC")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ZDA)) {
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 129033;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				//if (EncodePGN127250(&nmeaParser, &payload)) {
				//	header.pgn = 127250;
				//	FragmentFastMessage(&header, &payload, canMessages);
				//}

				//if (EncodePGN127258(&nmeaParser, &payload)) {
				//	header.pgn = 127258;
				//	FragmentFastMessage(&header, &payload, canMessages);
				//}

				if (!(supportedPGN & FLAGS_GGA)) {
					if (EncodePGN129025(&nmeaParser, &payload)) {
						header.pgn = 129025;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				
				if (!(supportedPGN & FLAGS_VTG)) {
					if (EncodePGN129026(&nmeaParser, &payload)) {
						header.pgn = 129026;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}

				//if (EncodePGN129029(&nmeaParser, &payload)) {
				//	header.pgn = 129029;
				//	FragmentFastMessage(&header, &payload, canMessages);
				//}


				//if (EncodePGN130577(&nmeaParser, &payload)) {
				//	header.pgn = 130577;
				//	FragmentFastMessage(&header, &payload, canMessages);
				//}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// ROT Rate Of Turn
		else if (nmeaParser.LastSentenceIDReceived == _T("ROT")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ROT)) {
					if (EncodePGN127251(&nmeaParser, &payload)) {
						header.pgn = 127251;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// RPM Revolutions
		else if (nmeaParser.LastSentenceIDReceived == _T("RPM")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ENG)) {
					if (EncodePGN127488(&nmeaParser, &payload)) {
						header.pgn = 127488;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// RSA Rudder Sensor Angle
		else if (nmeaParser.LastSentenceIDReceived == _T("RSA")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_RSA)) {
			
					if (EncodePGN127245(&nmeaParser, &payload)) {
						header.pgn = 128267;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// RTE Routes RTE Routes
		else if (nmeaParser.LastSentenceIDReceived == _T("RTE")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_RTE)) {
				// IGNORE
				}
				return FALSE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// VBW Dual Ground / Water Speed
		else if (nmeaParser.LastSentenceIDReceived == _T("VBW")) {
			if (nmeaParser.Parse()) {
				// IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// VDM AIS VHF Data Link Message
		else if (nmeaParser.LastSentenceIDReceived == _T("VDM")) {
			if (!(supportedPGN & FLAGS_AIS)) {
				if (nmeaParser.Parse()) {
					// IGNORE
				}
				else {
					wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
				}
			}
			return FALSE;
		}

		// VDO AIS VHF Data Link Own Vessel Report
		else if (nmeaParser.LastSentenceIDReceived == _T("VDO")) {
			if (nmeaParser.Parse()) {
				// IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// VDR Set & Drift
		else if (nmeaParser.LastSentenceIDReceived == _T("VDR")) {
			if (nmeaParser.Parse()) {
				// BUG BUG What Flags ??
				if (EncodePGN130577(&nmeaParser, &payload)) {
					header.pgn = 130577;
					FragmentFastMessage(&header, &payload, canMessages);
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// VHW Water Speed and Heading
		else if (nmeaParser.LastSentenceIDReceived == _T("VHW")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_VHW)) {
					if (EncodePGN128259(&nmeaParser, &payload) == TRUE) {
						header.pgn = 128259;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				if (!(supportedPGN & FLAGS_HDG)) {
					if (EncodePGN127250(&nmeaParser, &payload) == TRUE) {
						header.pgn = 127250;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// VLW Dual Ground / Water Distance
		else if (nmeaParser.LastSentenceIDReceived == _T("VLW")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_VHW)) {
					if (EncodePGN128275(&nmeaParser, &payload)) {
						header.pgn = 128275;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// VTG Course Over Ground & Ground Speed
		else if (nmeaParser.LastSentenceIDReceived == _T("VTG")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_VTG)) {
					if (EncodePGN129026(&nmeaParser, &payload)) {
						header.pgn = 129026;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN130577(&nmeaParser, &payload)) {
						header.pgn = 130577;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;				
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// WCV Waypoint Closure Velocity
		else if (nmeaParser.LastSentenceIDReceived == _T("WCV")) {
			if (nmeaParser.Parse()) {
				// IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// WNC Distance Waypoint to Waypoint
		else if (nmeaParser.LastSentenceIDReceived == _T("WNC")) {
			if (nmeaParser.Parse()) {
				//IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// WPL Waypoint Location
		else if (nmeaParser.LastSentenceIDReceived == _T("WPL")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_RTE)) {
					//IGNORE
				}
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// XDR Transducer Measurements
		// A big ugly mess
		else if (nmeaParser.LastSentenceIDReceived == _T("XDR")) {
			if (nmeaParser.Parse()) {
				// Each XDR sentence may have up to four measurement values
				for (int i = 0; i < nmeaParser.Xdr.TransducerCnt; i++) {
					payload.clear();
					// "A" Angular Displacement in "D" Degrees
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("A")) {
						if (!(supportedPGN & FLAGS_XDR)) {
							short yaw = SHRT_MAX;
							short pitch = SHRT_MAX;
							short roll = SHRT_MAX;
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("D")) {
								if (nmeaParser.Xdr.TransducerInfo[i].TransducerName == _T("PITCH"))  {
									pitch = 10000 * DEGREES_TO_RADIANS(nmeaParser.Xdr.TransducerInfo[i].MeasurementData);
								}
								else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName == _T("YAW")) {
									yaw = 10000 * DEGREES_TO_RADIANS(nmeaParser.Xdr.TransducerInfo[i].MeasurementData);
								}
								else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName == _T("ROLL")) {
									roll = 10000 * DEGREES_TO_RADIANS(nmeaParser.Xdr.TransducerInfo[i].MeasurementData);	
								}
								else {
									// Not a transducer measurement we are interested in
									break;
								}
								if (EncodePGN127257(yaw, pitch, roll, &payload)) {
									header.pgn = 127257;
									FragmentFastMessage(&header, &payload, canMessages);
								}
							
							}
						}
					}


					// "C" Temperature in "C" degrees Celsius
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("C")) {
						if (!(supportedPGN & FLAGS_ENG)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("C")) {

								int engineInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;

								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINE#"), &remainingString)) && (engineInstance != -1)) {

									payload.push_back(engineInstance);

									unsigned short oilPressure = USHRT_MAX;
									payload.push_back(oilPressure & 0xFF);
									payload.push_back((oilPressure >> 8) & 0xFF);

									unsigned short oilTemperature = USHRT_MAX;
									payload.push_back(oilTemperature & 0xFF);
									payload.push_back((oilTemperature >> 8) & 0xFF);

									// BUG BUG REMOVE
									wxLogMessage(_T("TwoCan Encoder, Debug Info, Temperature: %f "), nmeaParser.Xdr.TransducerInfo[i].MeasurementData);

									unsigned short engineTemperature = static_cast<unsigned short>((nmeaParser.Xdr.TransducerInfo[i].MeasurementData + CONST_KELVIN) * 100.0f);
									payload.push_back(engineTemperature & 0xFF);
									payload.push_back((engineTemperature >> 8) & 0xFF);

									unsigned short alternatorPotential = USHRT_MAX;
									payload.push_back(alternatorPotential & 0xFF);
									payload.push_back((alternatorPotential >> 8) & 0xFF);

									unsigned short fuelRate = USHRT_MAX; // 0.1 Litres/hour
									payload.push_back(fuelRate & 0xFF);
									payload.push_back((fuelRate >> 8) &0xFF);

									unsigned int totalEngineHours = UINT_MAX;  // seconds
									payload.push_back(totalEngineHours & 0xFF);
									payload.push_back((totalEngineHours >> 8) & 0xFF);
									payload.push_back((totalEngineHours >> 16) & 0xFF);
									payload.push_back((totalEngineHours >> 24) & 0xFF);

									unsigned short coolantPressure = USHRT_MAX; // hPA
									payload.push_back(coolantPressure & 0xFF);
									payload.push_back((coolantPressure >> 8) & 0xFF);

									unsigned short fuelPressure = USHRT_MAX; // hPa
									payload.push_back(fuelPressure & 0xFF);
									payload.push_back((fuelPressure >> 8) & 0xFF);

									byte reserved = UCHAR_MAX;
									payload.push_back(reserved & 0xFF);

									unsigned short statusOne = USHRT_MAX;
									payload.push_back(statusOne & 0xFF);
									payload.push_back((statusOne >> 8) & 0xFF);
		
									unsigned short statusTwo = USHRT_MAX;
									payload.push_back(statusTwo & 0xFF);
									payload.push_back((statusTwo >>8) & 0xFF);

									byte engineLoad = UCHAR_MAX;  // percentage
									payload.push_back(engineLoad & 0xFF);

									byte engineTorque = UCHAR_MAX; // percentage
									payload.push_back(engineTorque & 0xFF);

									header.pgn = 127489;
									FragmentFastMessage(&header, &payload, canMessages);
								}
							
							}
													
						}
					}
					// "T" Tachometer in "R" RPM
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("T")) {
						if (!(supportedPGN & FLAGS_ENG)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("R")) {

								int engineInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;
								
								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINE#"), &remainingString)) && (engineInstance != -1)) {
									
									// BUG BUG duplicating code for PGN 127488
									payload.push_back(engineInstance);

									// BUG BUG REMOVE
									wxLogMessage(_T("TwoCan Encoder, Debug Info, RPM: %d"), static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 4.0f));

									unsigned short engineSpeed = static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 4.0f);
									payload.push_back(engineSpeed & 0xFF);
									payload.push_back((engineSpeed >> 8) & 0xFF);
		
									unsigned short engineBoostPressure = USHRT_MAX;
									payload.push_back(engineBoostPressure & 0xFF);
									payload.push_back((engineBoostPressure >> 8) & 0xFF);

									short engineTrim = SHRT_MAX;
									payload.push_back(engineTrim & 0xFF);
									payload.push_back((engineTrim >> 8) & 0xFF);

									header.pgn = 127488;
									FragmentFastMessage(&header, &payload, canMessages);
								}
															
							}
						}
					}
					
		    
					// "P" Pressure in "P" pascal
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("P")) {
						if (!(supportedPGN & FLAGS_ENG)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("P")) {

								int engineInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;

								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINE#"), &remainingString)) && (engineInstance != -1)) {
									
									// BUG BUG duplicating code for PGN 127488
									payload.push_back(engineInstance);

									unsigned short engineSpeed = USHRT_MAX ;
									payload.push_back(engineSpeed & 0xFF);
									payload.push_back((engineSpeed >> 8) & 0xFF);
		
									// BUG BUG Unsure of units & range
									unsigned short engineBoostPressure = nmeaParser.Xdr.TransducerInfo[i].MeasurementData / 100;
									payload.push_back(engineBoostPressure & 0xFF);
									payload.push_back((engineBoostPressure >> 8) & 0xFF);

									short engineTrim = SHRT_MAX;
									payload.push_back(engineTrim & 0xFF);
									payload.push_back((engineTrim >> 8) & 0xFF);

									header.pgn = 127488;
									FragmentFastMessage(&header, &payload, canMessages);
								}
								
								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINEOIL#"), &remainingString)) && ( engineInstance != -1)) {

									payload.push_back(engineInstance);

									unsigned short oilPressure = nmeaParser.Xdr.TransducerInfo[i].MeasurementData / 100; // hPa (1hPa = 100Pa)
									payload.push_back(oilPressure & 0xFF);
									payload.push_back((oilPressure >> 8) & 0xFF);

									unsigned short oilTemperature = USHRT_MAX;
									payload.push_back(oilTemperature & 0xFF);
									payload.push_back((oilTemperature >> 8) & 0xFF);

									unsigned short engineTemperature = USHRT_MAX;
									payload.push_back(engineTemperature & 0xFF);
									payload.push_back((engineTemperature >> 8) & 0xFF);

									unsigned short alternatorPotential = USHRT_MAX; // 0.01 Volts
									payload.push_back(alternatorPotential & 0xFF);
									payload.push_back((alternatorPotential >> 8) & 0xFF);

									unsigned short fuelRate = USHRT_MAX; // 0.1 Litres/hour
									payload.push_back(fuelRate & 0xFF);
									payload.push_back((fuelRate >> 8) &0xFF);

									unsigned int totalEngineHours = UINT_MAX;  // seconds
									payload.push_back(totalEngineHours & 0xFF);
									payload.push_back((totalEngineHours >> 8) & 0xFF);
									payload.push_back((totalEngineHours >> 16) & 0xFF);
									payload.push_back((totalEngineHours >> 24) & 0xFF);

									unsigned short coolantPressure = USHRT_MAX; // hPA
									payload.push_back(coolantPressure & 0xFF);
									payload.push_back((coolantPressure >> 8) & 0xFF);

									unsigned short fuelPressure = USHRT_MAX; // hPa
									payload.push_back(fuelPressure & 0xFF);
									payload.push_back((fuelPressure >> 8) & 0xFF);

									byte reserved = UCHAR_MAX;
									payload.push_back(reserved & 0xFF);

									unsigned short statusOne = USHRT_MAX;
									payload.push_back(statusOne & 0xFF);
									payload.push_back((statusOne >> 8) & 0xFF);
		
									unsigned short statusTwo = USHRT_MAX;
									payload.push_back(statusTwo & 0xFF);
									payload.push_back((statusTwo >> 8) & 0xFF);

									byte engineLoad = UCHAR_MAX;  // percentage
									payload.push_back(engineLoad & 0xFF);

									byte engineTorque = UCHAR_MAX; // percentage
									payload.push_back(engineTorque & 0xFF);

									header.pgn = 127489;
									FragmentFastMessage(&header, &payload, canMessages);
								}
															
							}
						}

					}

					// "I" Current in "A" amperes
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("I")) {
						if (!(supportedPGN & FLAGS_BAT)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("A")) {
								
								int batteryInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;

								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("BATTERY#"), &remainingString)) && (batteryInstance != -1)) {
									
									payload.push_back(batteryInstance & 0xF);

									unsigned short batteryVoltage = USHRT_MAX;
									payload.push_back(batteryVoltage & 0xFF);
									payload.push_back((batteryVoltage >> 8) & 0xFF);

									short batteryCurrent  = nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 10;
									payload.push_back(batteryCurrent & 0xFF);
									payload.push_back((batteryCurrent >> 8) & 0xFF);
		
									unsigned short batteryTemperature = USHRT_MAX; 
									payload.push_back(batteryTemperature & 0xFF);
									payload.push_back((batteryTemperature >> 8) & 0xFF);
		
									payload.push_back(sequenceId);

									header.pgn = 127508;
									FragmentFastMessage(&header, &payload, canMessages);
								}
							}
						}
					}

					// "U" Voltage in "V" volts
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("U")) {
						if (!(supportedPGN & FLAGS_BAT)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("V")) {
								
								int batteryInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;
								
								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("BATTERY#"), &remainingString)) && (batteryInstance != -1)) {
																	
									payload.push_back(batteryInstance & 0xF);

									// BUG BUG REMOVE
									wxLogMessage(_T("TwoCan Encoder, Debug Info, Volts: %d"), static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 100.0f));

									unsigned short batteryVoltage = static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 100.0f);
									payload.push_back(batteryVoltage & 0xFF);
									payload.push_back((batteryVoltage >> 8) & 0xFF);

									short batteryCurrent  = SHRT_MAX;
									payload.push_back(batteryCurrent & 0xFF);
									payload.push_back((batteryCurrent >> 8) & 0xFF);
		
									unsigned short batteryTemperature = USHRT_MAX; 
									payload.push_back(batteryTemperature & 0xFF);
									payload.push_back((batteryTemperature >> 8) & 0xFF);
		
									payload.push_back(sequenceId);

									header.pgn = 127508;
									FragmentFastMessage(&header, &payload, canMessages);
								}


								if ((nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ALTERNATOR#"), &remainingString)) && (batteryInstance != -1)) {

									payload.push_back(batteryInstance);

									unsigned short oilPressure = USHRT_MAX;
									payload.push_back(oilPressure & 0xFF);
									payload.push_back((oilPressure >> 8) & 0xFF);

									unsigned short oilTemperature = USHRT_MAX;
									payload.push_back(oilTemperature & 0xFF);
									payload.push_back((oilTemperature >> 8) & 0xFF);

									unsigned short engineTemperature = USHRT_MAX;
									payload.push_back(engineTemperature & 0xFF);
									payload.push_back((engineTemperature >> 8) & 0xFF);

									unsigned short alternatorPotential = nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 100; 
									payload.push_back(alternatorPotential & 0xFF);
									payload.push_back((alternatorPotential >> 8) & 0xFF);

									unsigned short fuelRate = USHRT_MAX; // 0.1 Litres/hour
									payload.push_back(fuelRate & 0xFF);
									payload.push_back((fuelRate >> 8) &0xFF);

									unsigned int totalEngineHours = UINT_MAX;  // seconds
									payload.push_back(totalEngineHours & 0xFF);
									payload.push_back((totalEngineHours >> 8) & 0xFF);
									payload.push_back((totalEngineHours >> 16) & 0xFF);
									payload.push_back((totalEngineHours >> 24) & 0xFF);

									unsigned short coolantPressure = USHRT_MAX; // hPA
									payload.push_back(coolantPressure & 0xFF);
									payload.push_back((coolantPressure >> 8) & 0xFF);

									unsigned short fuelPressure = USHRT_MAX; // hPa
									payload.push_back(fuelPressure & 0xFF);
									payload.push_back((fuelPressure >> 8) & 0xFF);

									byte reserved = UCHAR_MAX;
									payload.push_back(reserved & 0xFF);

									unsigned short statusOne = USHRT_MAX;
									payload.push_back(statusOne & 0xFF);
									payload.push_back((statusOne >> 8) & 0xFF);
		
									unsigned short statusTwo = USHRT_MAX;
									payload.push_back(statusTwo & 0xFF);
									payload.push_back((statusTwo >>8) & 0xFF);

									byte engineLoad = UCHAR_MAX;  // percentage
									payload.push_back(engineLoad & 0xFF);

									byte engineTorque = UCHAR_MAX; // percentage
									payload.push_back(engineTorque & 0xFF);

									header.pgn = 127489;
									FragmentFastMessage(&header, &payload, canMessages);
								}
							}
						}
					}	

					// "V" Volume or "E" Volume - "P" as percent capacity
					if ((nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("V")) || (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("E"))) {
						if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("P")) {
							if (!(supportedPGN & FLAGS_TNK)) {

								int tankInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								byte tankType;
								wxString remainingString;

								if (tankInstance != -1) {

									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("FUEL#"), &remainingString)) {
										tankType = 0;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith( _T("FRESHWATER#"), &remainingString)) {
										tankType = 1;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("WASTEWATER#"), &remainingString)) {
										tankType = 2;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("LIVEWELL#"), &remainingString)) {
										tankType = 3;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("OIL#"), &remainingString)) {
										tankType = 4;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("BLACKWATER#"), &remainingString)) {
										tankType = 5;
									}

									else {
										// Not a transducer measurement we are interested in
										break;
									}
		
									payload.push_back((tankInstance & 0x0F) | ((tankType << 4) & 0xF0));

									// BUG BUG REMOVE
									wxLogMessage(_T("TwoCan Encoer, debg Info, Tank: %d %d"), tankInstance, static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 40.0f));

									unsigned short tankLevel = static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 40.0f); // percentage in 0.025 increments
									payload.push_back(tankLevel & 0xFF);
									payload.push_back((tankLevel >> 8) & 0xFF);

									unsigned int tankCapacity = UINT_MAX; 
									payload.push_back(tankCapacity & 0xFF);
									payload.push_back((tankCapacity >> 8) & 0xFF); 
									payload.push_back((tankCapacity >> 16) & 0xFF);
									payload.push_back((tankCapacity >> 24) & 0xFF);

									header.pgn = 127505;
									FragmentFastMessage(&header, &payload, canMessages);

								}
							}
						}
					}
				}
				return TRUE;
			}	
		
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// XTE Cross - Track Error, Measured
		else if (nmeaParser.LastSentenceIDReceived == _T("XTE")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_XTE)) {
					if (EncodePGN129283(&nmeaParser, &payload)) {
						header.pgn = 129283;
						FragmentFastMessage(&header, &payload, canMessages);
					}
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// ZDA Time & Date
		// PGN 126992, 129029, 129033
		else if (nmeaParser.LastSentenceIDReceived == _T("ZDA")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_ZDA)) {
					if (EncodePGN126992(&nmeaParser, &payload)) {
						header.pgn = 126992;
						FragmentFastMessage(&header, &payload, canMessages);
					}

					if (EncodePGN129033(&nmeaParser, &payload)) {
						header.pgn = 129033;
						FragmentFastMessage(&header, &payload, canMessages);
					}
	
				}
				return TRUE;
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// ZTG UTC & Time to Destination Waypoint
		else if (nmeaParser.LastSentenceIDReceived == _T("ZTG")) {
			if (nmeaParser.Parse()) {
				// IGNORE
			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}
	} 
	else {
		wxLogMessage(_T("TwoCan Encoder, Error pre-parsing %s"), sentence);
	}

	return FALSE;
}

// Encoding Routines begin here
// Note to self, PGN's may be encoded by multiple NMEA 183 sentences
// hence the parser->LastSentenceIDParsed palaver.

// Encode PGN 126992 NMEA System Time
bool TwoCanEncoder::EncodePGN126992(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("RMC")) {

		if (parser->Rmc.IsDataValid == NTrue) {

			n2kMessage->push_back(sequenceId);

			wxDateTime epoch;
			epoch.ParseDateTime("00:00:00 01-01-1970");

			wxDateTime now;
		
			// BUG BUG should add the date time parser to NMEA 183....
			// BUG BUG Note year 3000 bug, as NMEA 183 only supports two digits to represent the year
			now.ParseDateTime(wxString::Format(_T("%s/%s/20%s %s:%s:%s"), 
			parser->Rmc.Date.Mid(0,2), parser->Rmc.Date.Mid(2,2), parser->Rmc.Date.Mid(4,2),
	 		parser->Rmc.UTCTime.Mid(0,2), parser->Rmc.UTCTime.Mid(2,2), parser->Rmc.UTCTime.Mid(4,2)));

			wxTimeSpan dateDiff = now - epoch;

			unsigned short daysSinceEpoch = dateDiff.GetDays();
			unsigned int secondsSinceMidnight = ((dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

			n2kMessage->push_back((TIME_SOURCE_GPS & 0x0F) << 4);

			n2kMessage->push_back(daysSinceEpoch & 0xFF);
			n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

			n2kMessage->push_back(secondsSinceMidnight & 0xFF); 
			n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
			n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF); 
			n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

			return TRUE;
		}
	}

	else if (parser->LastSentenceIDParsed == _T("ZDA")) {
		n2kMessage->push_back(sequenceId);

		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now;
		now.ParseDateTime(wxString::Format(_T("%s/%s/20%s %s:%s:%s"), 
		parser->Zda.Day, parser->Zda.Month, parser->Zda.Year,
		parser->Zda.UTCTime.Mid(0,2), parser->Zda.UTCTime.Mid(2,2), parser->Zda.UTCTime.Mid(4,2)));

		wxTimeSpan dateDiff = now - epoch;

		unsigned short daysSinceEpoch = dateDiff.GetDays();
		unsigned int secondsSinceMidnight = ((dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

		n2kMessage->push_back((TIME_SOURCE_GPS & 0x0F) << 4);

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF); 
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF); 
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);
	
		return TRUE;
	}

	else if (parser->LastSentenceIDParsed == _T("GLL")) {
		if (parser->Gll.IsDataValid == NTrue) {
			n2kMessage->push_back(sequenceId);

			wxDateTime epoch;
			epoch.ParseDateTime("00:00:00 01-01-1970");

			wxDateTime now = wxDateTime::Now();
			now.SetHour(std::atoi(parser->Gll.UTCTime.Mid(0,2)));
			now.SetMinute(std::atoi(parser->Gll.UTCTime.Mid(2,2)));
			now.SetSecond(std::atoi(parser->Gll.UTCTime.Mid(4,2)));

			wxTimeSpan dateDiff = now - epoch;

			unsigned short daysSinceEpoch = dateDiff.GetDays();
			unsigned int secondsSinceMidnight = ((dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

			n2kMessage->push_back((TIME_SOURCE_GPS & 0x0F) << 4);

			n2kMessage->push_back(daysSinceEpoch & 0xFF);
			n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

			n2kMessage->push_back(secondsSinceMidnight & 0xFF); 
			n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
			n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF); 
			n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);
	
			return TRUE;
		}
	}

	else if (parser->LastSentenceIDParsed == _T("GGA")) {
		n2kMessage->push_back(sequenceId);

		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		// GGA sentence only has utc time, not date, so assume today
		wxDateTime now = wxDateTime::Now();
		now.SetHour(std::atoi(parser->Gga.UTCTime.Mid(0,2)));
		now.SetMinute(std::atoi(parser->Gga.UTCTime.Mid(2,2)));
		now.SetSecond(std::atoi(parser->Gga.UTCTime.Mid(4,2)));
		
		wxTimeSpan dateDiff = now - epoch;

		unsigned short daysSinceEpoch = dateDiff.GetDays();
		unsigned int secondsSinceMidnight = ((dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;
		n2kMessage->push_back((TIME_SOURCE_GPS & 0x0F) << 4);

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF); 
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF); 
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);
	
		return TRUE;
	}

	return FALSE;
}


// Encode payload for PGN 127245 NMEA Rudder Position
bool TwoCanEncoder::EncodePGN127245(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	if (parser->LastSentenceIDParsed == _T("RSA")) {
		n2kMessage->clear();
		// BUG BUG How to deal with multi rudder configurations
		// Would need to issue two NMEA2000 messages.
		if (parser->Rsa.IsStarboardDataValid) {
			n2kMessage->push_back(0); // Starbord or main rudder
			n2kMessage->push_back(0xFF); // direction order
			n2kMessage->push_back(0xFF); // angleOrder (2 bytes)
			n2kMessage->push_back(0xFF);

			short position = 1000 * DEGREES_TO_RADIANS(parser->Rsa.Starboard);
			n2kMessage->push_back(position & 0xFF);
			n2kMessage->push_back((position >> 8) & 0xFF);

			return TRUE;
		}
		else if (parser->Rsa.IsPortDataValid) {
			n2kMessage->push_back(1); // Portrudder
			n2kMessage->push_back(0xFF); // direction order
			n2kMessage->push_back(0xFF); // angleOrder (2 bytes)
			n2kMessage->push_back(0xFF);

			short position = 1000 * DEGREES_TO_RADIANS(parser->Rsa.Port);
			n2kMessage->push_back(position & 0xFF);
			n2kMessage->push_back((position >> 8) & 0xFF);

			return TRUE;
		}
	}
	return FALSE;
}	

// Encode payload for PGN 127250 NMEA Vessel Heading
bool TwoCanEncoder::EncodePGN127250(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();
	if (parser->LastSentenceIDParsed == _T("HDG")) {
		n2kMessage->push_back(sequenceId);			

		unsigned short heading = DEGREES_TO_RADIANS(parser->Hdg.MagneticSensorHeadingDegrees) * 10000;
		n2kMessage->push_back(heading & 0xFF);
		n2kMessage->push_back((heading >> 8) & 0xFF);

		short deviation = DEGREES_TO_RADIANS(parser->Hdg.MagneticDeviationDegrees) * 10000;
		if (parser->Hdg.MagneticDeviationDirection == EASTWEST::West) {
			deviation = -deviation;
		}
		n2kMessage->push_back(deviation & 0xFF);
		n2kMessage->push_back((deviation >>8) & 0xFF);

		short variation = DEGREES_TO_RADIANS(parser->Hdg.MagneticVariationDegrees) * 10000;
		if (parser->Hdg.MagneticVariationDirection == EASTWEST::West) {
			variation = -variation;
		}
		n2kMessage->push_back(variation & 0xFF);
		n2kMessage->push_back((variation >> 8) & 0xFF);

		byte headingReference = HEADING_MAGNETIC;
		n2kMessage->push_back(headingReference & 0x03);

		return TRUE;
	}

	else if (parser->LastSentenceIDParsed == _T("HDM")) {
		n2kMessage->push_back(sequenceId);			

		unsigned short heading = DEGREES_TO_RADIANS(parser->Hdm.DegreesMagnetic) * 10000;
		n2kMessage->push_back(heading & 0xFF);
		n2kMessage->push_back((heading >> 8) & 0xFF);

		short deviation = SHRT_MAX;
		n2kMessage->push_back(deviation & 0xFF);
		n2kMessage->push_back((deviation >>8) & 0xFF);

		short variation = SHRT_MAX;
		n2kMessage->push_back(variation & 0xFF);
		n2kMessage->push_back((variation >> 8) & 0xFF);

		byte headingReference = HEADING_MAGNETIC;
		n2kMessage->push_back(headingReference & 0x03);

		return TRUE;
	}
	else if (parser->LastSentenceIDParsed == _T("HDT")) {
		n2kMessage->push_back(sequenceId);			

		unsigned short heading = DEGREES_TO_RADIANS(parser->Hdt.DegreesTrue) * 10000;
		n2kMessage->push_back(heading & 0xFF);
		n2kMessage->push_back((heading >> 8) & 0xFF);

		short deviation = SHRT_MAX;
		n2kMessage->push_back(deviation & 0xFF);
		n2kMessage->push_back((deviation >>8) & 0xFF);

		short variation = SHRT_MAX;
		n2kMessage->push_back(variation & 0xFF);
		n2kMessage->push_back((variation >> 8) & 0xFF);

		byte headingReference = HEADING_TRUE;
		n2kMessage->push_back(headingReference & 0x03);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 127251 NMEA Rate of Turn (ROT)
bool TwoCanEncoder::EncodePGN127251(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("ROT")) {

		n2kMessage->push_back(sequenceId);
		// conversion is degress per minute to radians per second
		int rateOfTurn = 600000 * DEGREES_TO_RADIANS(parser->Rot.RateOfTurn);

		n2kMessage->push_back(rateOfTurn & 0xFF);
		n2kMessage->push_back((rateOfTurn >> 8) & 0xFF);
		n2kMessage->push_back((rateOfTurn >> 16) & 0xFF);
		n2kMessage->push_back((rateOfTurn >> 24) & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 127257 NMEA Attitude
bool TwoCanEncoder::EncodePGN127257(const short yaw, const short pitch, const short roll, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	n2kMessage->push_back(sequenceId);

	n2kMessage->push_back(yaw & 0xFF);
	n2kMessage->push_back((yaw >> 8) & 0xFF);

	n2kMessage->push_back(pitch & 0xFF);
	n2kMessage->push_back((pitch >> 8) & 0xFF);

	n2kMessage->push_back(roll & 0xFF);
	n2kMessage->push_back((roll >> 8) & 0xFF);

	return TRUE;
	
}

// Encode payload for PGN 127258 NMEA Magnetic Variation
bool TwoCanEncoder::EncodePGN127258(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("HDG")) {

		n2kMessage->push_back(sequenceId);

		byte variationSource = 1; // 0 = Manual, 1 = Automatic Chart 
		n2kMessage->push_back(variationSource & 0x0F);

		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now = wxDateTime::Now();
		wxTimeSpan dateDiff = now - epoch;

		unsigned short daysSinceEpoch = dateDiff.GetDays();
		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		short variation = 10000 * DEGREES_TO_RADIANS(parser->Hdg.MagneticVariationDegrees);
		if (parser->Hdg.MagneticVariationDirection == EASTWEST::West) {
			variation = -variation;
		}

		n2kMessage->push_back(variation & 0xFF);
		n2kMessage->push_back((variation >> 8) & 0xFF);
			
		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 127488 Engine Rapid Update
bool TwoCanEncoder::EncodePGN127488(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("RPM")) {

		// Differentiate between shaft 'S' & engine 'E' RPM
		if (parser->Rpm.Source.StartsWith('E')) {
			// BUG BUG Engine numbering ??
			byte engineInstance = parser->Rpm.EngineNumber;
			n2kMessage->push_back(engineInstance);

			unsigned short engineSpeed = 4 * parser->Rpm.RevolutionsPerMinute;
			n2kMessage->push_back(engineSpeed & 0xFF);
			n2kMessage->push_back((engineSpeed >> 8) & 0xFF);
		
			unsigned short engineBoostPressure = USHRT_MAX;
			n2kMessage->push_back(engineBoostPressure & 0xFF);
			n2kMessage->push_back((engineBoostPressure >> 8) & 0xFF);

			// BUG BUG Using propeller pitch instead of engine trim ??
			// BUG BUG Unsure of units
			short engineTrim = parser->Rpm.PropellerPitch;
			n2kMessage->push_back(engineTrim & 0xFF);

			return TRUE;	
		}
	}
	return FALSE;
}

// Encode payload for PGN 127489 Engine Static Parameters
// BUG BUG Not all parameters are configured, assumes values are enumerated from NMEA 183 XDR sentence
bool TwoCanEncoder::EncodePGN127250(const byte engineInstance, const unsigned short oilPressure, const unsigned short engineTemperature, const unsigned short alternatorPotential, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	n2kMessage->push_back(engineInstance);
	
	n2kMessage->push_back(oilPressure & 0xFF);
	n2kMessage->push_back((oilPressure >> 8) & 0xFF);

	unsigned short oilTemperature = USHRT_MAX;
	n2kMessage->push_back(oilTemperature & 0xFF);
	n2kMessage->push_back((oilTemperature >> 8) & 0xFF);

	n2kMessage->push_back(engineTemperature & 0xFF);
	n2kMessage->push_back((engineTemperature >> 8) & 0xFF);

	n2kMessage->push_back(alternatorPotential & 0xFF);
	n2kMessage->push_back((alternatorPotential >> 8) & 0xFF);

	unsigned short fuelRate = USHRT_MAX; // 0.1 Litres/hour
	n2kMessage->push_back(fuelRate & 0xFF);
	n2kMessage->push_back((fuelRate >> 8) &0xFF);

	unsigned int totalEngineHours = UINT_MAX;  // seconds
	n2kMessage->push_back(totalEngineHours & 0xFF);
	n2kMessage->push_back((totalEngineHours >> 8) & 0xFF);
	n2kMessage->push_back((totalEngineHours >> 16) & 0xFF);
	n2kMessage->push_back((totalEngineHours >> 24) & 0xFF);

	unsigned short coolantPressure = USHRT_MAX; // hPA
	n2kMessage->push_back(coolantPressure & 0xFF);
	n2kMessage->push_back((coolantPressure >> 8) & 0xFF);

	unsigned short fuelPressure = USHRT_MAX; // hPa
	n2kMessage->push_back(fuelPressure & 0xFF);
	n2kMessage->push_back((fuelPressure >> 8) & 0xFF);

	byte reserved = UCHAR_MAX;
	n2kMessage->push_back(reserved & 0xFF);

	unsigned short statusOne = USHRT_MAX;
	n2kMessage->push_back(statusOne & 0xFF);
	n2kMessage->push_back((statusOne >> 8) & 0xFF);

	unsigned short statusTwo = USHRT_MAX;
	n2kMessage->push_back(statusTwo & 0xFF);
	n2kMessage->push_back((statusTwo >> 8) & 0xFF);

	byte engineLoad = UCHAR_MAX;  // percentage
	n2kMessage->push_back(engineLoad & 0xFF);

	byte engineTorque = UCHAR_MAX; // percentage
	n2kMessage->push_back(engineTorque & 0xFF);

	return TRUE;

}

// Encode payload for PGN 128259 NMEA Speed & Heading
bool TwoCanEncoder::EncodePGN128259(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("VHW")) {

		n2kMessage->push_back(sequenceId);

		unsigned short speedWaterReferenced = (unsigned short) (100 * parser->Vhw.Knots / CONVERT_MS_KNOTS);
		n2kMessage->push_back(speedWaterReferenced & 0xFF);
		n2kMessage->push_back((speedWaterReferenced >> 8) & 0xFF);

		unsigned short speedGroundReferenced = USHRT_MAX;
		n2kMessage->push_back(speedGroundReferenced & 0xFF);
		n2kMessage->push_back((speedGroundReferenced >> 8) & 0xFF);

		n2kMessage->push_back(0); // VHW does not indicate speed source, so assume paddle wheel

		unsigned short heading = (unsigned short)(10000 * DEGREES_TO_RADIANS(parser->Vhw.DegreesMagnetic));
		n2kMessage->push_back(heading & 0xFF);
		n2kMessage->push_back((heading >> 8) & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 128267 NMEA Depth
bool TwoCanEncoder::EncodePGN128267(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("DPT")) {

		n2kMessage->push_back(sequenceId);

		unsigned int depth = (unsigned int)(100 * parser->Dpt.DepthMeters);
		n2kMessage->push_back(depth & 0xFF);
		n2kMessage->push_back((depth >> 8) & 0xFF);
		n2kMessage->push_back((depth >> 16) & 0xFF);
		n2kMessage->push_back((depth >> 24) & 0xFF);
		
		short offset = (short)(1000 * nmeaParser.Dpt.OffsetFromTransducerMeters);
		n2kMessage->push_back(offset & 0xFF);
		n2kMessage->push_back((offset >> 8) & 0xFF);

		byte maxRange = (byte)(0.1 * nmeaParser.Dpt.MaximumRangeMeters);
		n2kMessage->push_back(maxRange & 0xFF);

		return TRUE;
	}
	else if (parser->LastSentenceIDParsed == _T("DBT")) {

		n2kMessage->push_back(sequenceId);
			
		unsigned int depth = (unsigned int)(100 * parser->Dbt.DepthMeters);
		n2kMessage->push_back(depth & 0xFF);
		n2kMessage->push_back((depth >> 8) & 0xFF);
		n2kMessage->push_back((depth >> 16) & 0xFF);
		n2kMessage->push_back((depth >> 24) & 0xFF);
		
		short offset = SHRT_MAX;
		n2kMessage->push_back(offset & 0xFF);
		n2kMessage->push_back((offset >> 8) & 0xFF);

		byte maxRange = UCHAR_MAX;
		n2kMessage->push_back(maxRange & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 128275 NMEA Distance Log
bool TwoCanEncoder::EncodePGN128275(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("VLW")) {
		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now = wxDateTime::Now();
		wxTimeSpan dateDiff = now - epoch;

		unsigned short daysSinceEpoch = dateDiff.GetDays();
		unsigned int secondsSinceMidnight = ((dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

		// BUG Units & Magnitude ??
		unsigned int cumulativeDistance = parser->Vlw.TotalDistanceNauticalMiles / CONVERT_METRES_NAUTICAL_MILES;
		n2kMessage->push_back(cumulativeDistance & 0xFF);
		n2kMessage->push_back((cumulativeDistance >> 8) & 0xFF);
		n2kMessage->push_back((cumulativeDistance >> 16) & 0xFF);
		n2kMessage->push_back((cumulativeDistance >> 24) & 0xFF);

		unsigned int tripDistance = parser->Vlw.DistanceSinceResetNauticalMiles / CONVERT_METRES_NAUTICAL_MILES;
		n2kMessage->push_back(tripDistance & 0xFF);
		n2kMessage->push_back((tripDistance >> 8) & 0xFF);
		n2kMessage->push_back((tripDistance >> 16) & 0xFF);
		n2kMessage->push_back((tripDistance >> 24) & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 129025 NMEA Position Rapid Update
bool TwoCanEncoder::EncodePGN129025(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (nmeaParser.LastSentenceIDParsed == _T("RMC")) {
		if (parser->Rmc.IsDataValid == NTrue) {
						
			int latitude = parser->Rmc.Position.Latitude.Latitude * 1e7;

			if (parser->Rmc.Position.Latitude.Northing == South) {
				latitude = -latitude;
			}
			n2kMessage->push_back(latitude & 0xFF);
			n2kMessage->push_back((latitude >> 8) & 0xFF);
			n2kMessage->push_back((latitude >> 16) & 0xFF);
			n2kMessage->push_back((latitude >> 24) & 0xFF);

			int longitude = parser->Rmc.Position.Longitude.Longitude * 1e7;

			if (parser->Rmc.Position.Longitude.Easting == West) {
				longitude = -longitude;
			}
			n2kMessage->push_back(longitude & 0xFF);
			n2kMessage->push_back((longitude >> 8) & 0xFF);
			n2kMessage->push_back((longitude >> 16) & 0xFF);
			n2kMessage->push_back((longitude >> 24) & 0xFF);

			wxLogMessage(_T("Lat %f, Lon %f"), parser->Rmc.Position.Latitude.Latitude, parser->Rmc.Position.Longitude.Longitude);
			wxLogMessage(_T("Lat %d, Lon %d"), latitude, longitude);

			return TRUE;
		}
	}
	else if (nmeaParser.LastSentenceIDParsed == _T("GLL")) {
		if (parser->Gll.IsDataValid == NTrue) {
	
			int latitude = (int)(parser->Gll.Position.Latitude.Latitude * 1e7);
			if (parser->Gll.Position.Latitude.Northing == South) {
				latitude = -latitude;
			}
			n2kMessage->push_back(latitude & 0xFF);
			n2kMessage->push_back((latitude >> 8) & 0xFF);
			n2kMessage->push_back((latitude >> 16) & 0xFF);
			n2kMessage->push_back((latitude >> 24) & 0xFF);

			int longitude = (int)(parser->Gll.Position.Longitude.Longitude * 1e7);
			if (parser->Gll.Position.Longitude.Easting == West) {
				longitude = -longitude;
			}
			n2kMessage->push_back(longitude & 0xFF);
			n2kMessage->push_back((longitude >> 8) & 0xFF);
			n2kMessage->push_back((longitude >> 16) & 0xFF);
			n2kMessage->push_back((longitude >> 24) & 0xFF);
			return TRUE;
		}
	}

	else if (nmeaParser.LastSentenceIDParsed == _T("GGA")) {
		if (parser->Gga.GPSQuality != 0) { // 0 indicates fix not available
	
			int latitude = (int)(parser->Gga.Position.Latitude.Latitude * 1e7);
			if (parser->Gga.Position.Latitude.Northing == South) {
				latitude = -latitude;
			}
			n2kMessage->push_back(latitude & 0xFF);
			n2kMessage->push_back((latitude >> 8) & 0xFF);
			n2kMessage->push_back((latitude >> 16) & 0xFF);
			n2kMessage->push_back((latitude >> 24) & 0xFF);

			int longitude = (int)(parser->Gga.Position.Longitude.Longitude * 1e7);
			if (parser->Gga.Position.Longitude.Easting == West) {
				longitude = -longitude;
			}
			n2kMessage->push_back(longitude & 0xFF);
			n2kMessage->push_back((longitude >> 8) & 0xFF);
			n2kMessage->push_back((longitude >> 16) & 0xFF);
			n2kMessage->push_back((longitude >> 24) & 0xFF);
			return TRUE;
		}
	}
	return FALSE;	
}

// Encode payload for PGN 129026 NMEA COG SOG Rapid Update
bool TwoCanEncoder::EncodePGN129026(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (nmeaParser.LastSentenceIDParsed == _T("RMC")) {
		if (parser->Rmc.IsDataValid) {
	
			n2kMessage->push_back(sequenceId);

			byte headingReference = HEADING_TRUE;
			n2kMessage->push_back(headingReference & 0x03);

			unsigned short courseOverGround = 10000 * DEGREES_TO_RADIANS(parser->Rmc.TrackMadeGoodDegreesTrue);
			n2kMessage->push_back(courseOverGround & 0xFF);
			n2kMessage->push_back((courseOverGround >> 8) & 0xFF);

			unsigned short speedOverGround = 100 * parser->Rmc.SpeedOverGroundKnots / CONVERT_MS_KNOTS;
			n2kMessage->push_back(speedOverGround & 0xFF);
			n2kMessage->push_back((speedOverGround >> 8) & 0xFF);

			return TRUE;
		}
	}
	return FALSE;
}

bool TwoCanEncoder::EncodePGN129029(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("GGA")) {

		n2kMessage->push_back(sequenceId);

		unsigned short daysSinceEpoch;
		unsigned int secondsSinceMidnight;
	
		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now;
		now.ParseDateTime(parser->Gga.UTCTime);

		wxTimeSpan dateDiff = now - epoch;

		daysSinceEpoch = dateDiff.GetDays();
		secondsSinceMidnight = (dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue();

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);
	
		long long latitude = parser->Gga.Position.Latitude.Latitude;
		if (parser->Gga.Position.Latitude.Northing == South) {
			latitude = -latitude;
		}
		n2kMessage->push_back(latitude & 0xFF);
		n2kMessage->push_back((latitude >> 8) & 0xFF);
		n2kMessage->push_back((latitude >> 16) & 0xFF);
		n2kMessage->push_back((latitude >> 24) & 0xFF);
		n2kMessage->push_back((latitude >> 32) & 0xFF);
		n2kMessage->push_back((latitude >> 40) & 0xFF);
		n2kMessage->push_back((latitude >> 48) & 0xFF);
		n2kMessage->push_back((latitude >> 56) & 0xFF);

		long long longitude = parser->Gga.Position.Longitude.Longitude ;
		if (parser->Gga.Position.Longitude.Easting == West) {
			longitude = -longitude;
		}
		n2kMessage->push_back(longitude & 0xFF);
		n2kMessage->push_back((longitude >> 8) & 0xFF);
		n2kMessage->push_back((longitude >> 16) & 0xFF);
		n2kMessage->push_back((longitude >> 24) & 0xFF);
		n2kMessage->push_back((longitude >> 32) & 0xFF);
		n2kMessage->push_back((longitude >> 40) & 0xFF);
		n2kMessage->push_back((longitude >> 48) & 0xFF);
		n2kMessage->push_back((longitude >> 56) & 0xFF);
	
		long long altitude = parser->Gga.AntennaAltitudeMeters * 1e6;

		n2kMessage->push_back(altitude & 0xFF);
		n2kMessage->push_back((altitude >> 8) & 0xFF);
		n2kMessage->push_back((altitude >> 16) & 0xFF);
		n2kMessage->push_back((altitude >> 24) & 0xFF);
		n2kMessage->push_back((altitude >> 32) & 0xFF);
		n2kMessage->push_back((altitude >> 40) & 0xFF);
		n2kMessage->push_back((altitude >> 48) & 0xFF);
		n2kMessage->push_back((altitude >> 56) & 0xFF);

		n2kMessage->push_back((parser->Gga.GPSQuality << 4) & 0xF0);
	
		//fixIntegrity;
		n2kMessage->push_back(1 & 0x03);

		n2kMessage->push_back(parser->Gga.NumberOfSatellitesInUse);

		unsigned short hDOP = 100 * parser->Gga.HorizontalDilutionOfPrecision;
		n2kMessage->push_back(hDOP & 0xFF);
		n2kMessage->push_back((hDOP >> 8) & 0xFF);

		//PDOP
		n2kMessage->push_back(0xFF);
		n2kMessage->push_back(0xFF);

		unsigned short geoidalSeparation = 100 * parser->Gga.GeoidalSeparationMeters;
		n2kMessage->push_back(geoidalSeparation & 0xFF);
		n2kMessage->push_back((geoidalSeparation >> 8) & 0xFF);

		// BUG BUG How to determine the correct number of reference stations
		// GGA only provides 1 (or perhaps none ?)
		// possibly check if parser->Gga.DifferentialReferenceStationID is NaN
	
		n2kMessage->push_back(1);
	
		unsigned short referenceStationType = 0; //0 = GPS
		unsigned short referenceStationID = parser->Gga.DifferentialReferenceStationID;
		unsigned short referenceStationAge = parser->Gga.AgeOfDifferentialGPSDataSeconds;

		n2kMessage->push_back( ((referenceStationType << 4) & 0xF0) | (referenceStationID & 0x0F) ); 
		n2kMessage->push_back((referenceStationID >> 4) & 0xFF);
		n2kMessage->push_back(referenceStationAge & 0xFF);
		n2kMessage->push_back((referenceStationAge >> 8) & 0xFF);

		return TRUE;		
	}

	else if (parser->LastSentenceIDParsed == _T("GLL")) {

		n2kMessage->push_back(sequenceId);

		unsigned short daysSinceEpoch;
		unsigned int secondsSinceMidnight;
	
		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now;
		now.ParseDateTime(parser->Gga.UTCTime);

		wxTimeSpan dateDiff = now - epoch;

		daysSinceEpoch = dateDiff.GetDays();
		secondsSinceMidnight = (dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue();

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);
	
		long long latitude = parser->Gga.Position.Latitude.Latitude;
		if (parser->Gga.Position.Latitude.Northing == South) {
			latitude = -latitude;
		}
		n2kMessage->push_back(latitude & 0xFF);
		n2kMessage->push_back((latitude >> 8) & 0xFF);
		n2kMessage->push_back((latitude >> 16) & 0xFF);
		n2kMessage->push_back((latitude >> 24) & 0xFF);
		n2kMessage->push_back((latitude >> 32) & 0xFF);
		n2kMessage->push_back((latitude >> 40) & 0xFF);
		n2kMessage->push_back((latitude >> 48) & 0xFF);
		n2kMessage->push_back((latitude >> 56) & 0xFF);

		long long longitude = parser->Gga.Position.Longitude.Longitude ;
		if (parser->Gga.Position.Longitude.Easting == West) {
			longitude = -longitude;
		}
		n2kMessage->push_back(longitude & 0xFF);
		n2kMessage->push_back((longitude >> 8) & 0xFF);
		n2kMessage->push_back((longitude >> 16) & 0xFF);
		n2kMessage->push_back((longitude >> 24) & 0xFF);
		n2kMessage->push_back((longitude >> 32) & 0xFF);
		n2kMessage->push_back((longitude >> 40) & 0xFF);
		n2kMessage->push_back((longitude >> 48) & 0xFF);
		n2kMessage->push_back((longitude >> 56) & 0xFF);
	
		long long altitude = parser->Gga.AntennaAltitudeMeters * 1e6;

		n2kMessage->push_back(altitude & 0xFF);
		n2kMessage->push_back((altitude >> 8) & 0xFF);
		n2kMessage->push_back((altitude >> 16) & 0xFF);
		n2kMessage->push_back((altitude >> 24) & 0xFF);
		n2kMessage->push_back((altitude >> 32) & 0xFF);
		n2kMessage->push_back((altitude >> 40) & 0xFF);
		n2kMessage->push_back((altitude >> 48) & 0xFF);
		n2kMessage->push_back((altitude >> 56) & 0xFF);

		n2kMessage->push_back((parser->Gga.GPSQuality << 4) & 0xF0);
	
		//fixIntegrity;
		n2kMessage->push_back(1 & 0x03);

		n2kMessage->push_back(parser->Gga.NumberOfSatellitesInUse);

		unsigned short hDOP = 100 * parser->Gga.HorizontalDilutionOfPrecision;
		n2kMessage->push_back(hDOP & 0xFF);
		n2kMessage->push_back((hDOP >> 8) & 0xFF);

		//PDOP
		n2kMessage->push_back(0xFF);
		n2kMessage->push_back(0xFF);

		unsigned short geoidalSeparation = 100 * parser->Gga.GeoidalSeparationMeters;
		n2kMessage->push_back(geoidalSeparation & 0xFF);
		n2kMessage->push_back((geoidalSeparation >> 8) & 0xFF);

		// BUG BUG How to determine the correct number of reference stations
		// GGA only provides 1 (or perhaps none ?)
		// possibly check if parser->Gga.DifferentialReferenceStationID is NaN
	
		n2kMessage->push_back(1);
	
		unsigned short referenceStationType = 0; //0 = GPS
		unsigned short referenceStationID = parser->Gga.DifferentialReferenceStationID;
		unsigned short referenceStationAge = parser->Gga.AgeOfDifferentialGPSDataSeconds;

		n2kMessage->push_back( ((referenceStationType << 4) & 0xF0) | (referenceStationID & 0x0F) ); 
		n2kMessage->push_back((referenceStationID >> 4) & 0xFF);
		n2kMessage->push_back(referenceStationAge & 0xFF);
		n2kMessage->push_back((referenceStationAge >> 8) & 0xFF);

		return TRUE;		
	}

	else if (parser->LastSentenceIDParsed == _T("GSA")) {

		n2kMessage->push_back(sequenceId);

		unsigned short daysSinceEpoch;
		unsigned int secondsSinceMidnight;
	
		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now;
		now.ParseDateTime(parser->Gga.UTCTime);

		wxTimeSpan dateDiff = now - epoch;

		daysSinceEpoch = dateDiff.GetDays();
		secondsSinceMidnight = (dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue();

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);
	
		long long latitude = parser->Gga.Position.Latitude.Latitude;
		if (parser->Gga.Position.Latitude.Northing == South) {
			latitude = -latitude;
		}
		n2kMessage->push_back(latitude & 0xFF);
		n2kMessage->push_back((latitude >> 8) & 0xFF);
		n2kMessage->push_back((latitude >> 16) & 0xFF);
		n2kMessage->push_back((latitude >> 24) & 0xFF);
		n2kMessage->push_back((latitude >> 32) & 0xFF);
		n2kMessage->push_back((latitude >> 40) & 0xFF);
		n2kMessage->push_back((latitude >> 48) & 0xFF);
		n2kMessage->push_back((latitude >> 56) & 0xFF);

		long long longitude = parser->Gga.Position.Longitude.Longitude ;
		if (parser->Gga.Position.Longitude.Easting == West) {
			longitude = -longitude;
		}
		n2kMessage->push_back(longitude & 0xFF);
		n2kMessage->push_back((longitude >> 8) & 0xFF);
		n2kMessage->push_back((longitude >> 16) & 0xFF);
		n2kMessage->push_back((longitude >> 24) & 0xFF);
		n2kMessage->push_back((longitude >> 32) & 0xFF);
		n2kMessage->push_back((longitude >> 40) & 0xFF);
		n2kMessage->push_back((longitude >> 48) & 0xFF);
		n2kMessage->push_back((longitude >> 56) & 0xFF);
	
		long long altitude = parser->Gga.AntennaAltitudeMeters * 1e6;

		n2kMessage->push_back(altitude & 0xFF);
		n2kMessage->push_back((altitude >> 8) & 0xFF);
		n2kMessage->push_back((altitude >> 16) & 0xFF);
		n2kMessage->push_back((altitude >> 24) & 0xFF);
		n2kMessage->push_back((altitude >> 32) & 0xFF);
		n2kMessage->push_back((altitude >> 40) & 0xFF);
		n2kMessage->push_back((altitude >> 48) & 0xFF);
		n2kMessage->push_back((altitude >> 56) & 0xFF);

		n2kMessage->push_back((parser->Gga.GPSQuality << 4) & 0xF0);
	
		//fixIntegrity;
		n2kMessage->push_back(1 & 0x03);

		n2kMessage->push_back(parser->Gga.NumberOfSatellitesInUse);

		unsigned short hDOP = 100 * parser->Gga.HorizontalDilutionOfPrecision;
		n2kMessage->push_back(hDOP & 0xFF);
		n2kMessage->push_back((hDOP >> 8) & 0xFF);

		//PDOP
		n2kMessage->push_back(0xFF);
		n2kMessage->push_back(0xFF);

		unsigned short geoidalSeparation = 100 * parser->Gga.GeoidalSeparationMeters;
		n2kMessage->push_back(geoidalSeparation & 0xFF);
		n2kMessage->push_back((geoidalSeparation >> 8) & 0xFF);

		n2kMessage->push_back(1);
	
		unsigned short referenceStationType = 0; //0 = GPS
		unsigned short referenceStationID = parser->Gga.DifferentialReferenceStationID;
		unsigned short referenceStationAge = parser->Gga.AgeOfDifferentialGPSDataSeconds;

		n2kMessage->push_back( ((referenceStationType << 4) & 0xF0) | (referenceStationID & 0x0F) ); 
		n2kMessage->push_back((referenceStationID >> 4) & 0xFF);
		n2kMessage->push_back(referenceStationAge & 0xFF);
		n2kMessage->push_back((referenceStationAge >> 8) & 0xFF);

		return TRUE;		
	}
	return FALSE;
}

// Encode payload for PGN 129033 NMEA Date & Time
bool TwoCanEncoder::EncodePGN129033(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	if (parser->LastSentenceIDParsed == _T("ZDA")) {
		n2kMessage->clear();

		wxDateTime epoch;
		epoch.ParseDateTime("00:00:00 01-01-1970");

		wxDateTime now;

		now.ParseDateTime(wxString::Format(_T("%d/%d/20%d %s:%s:%s"), 
			parser->Zda.Day, parser->Zda.Month, parser->Zda.Year,
	 		parser->Zda.UTCTime.Mid(0,2), parser->Zda.UTCTime.Mid(2,2), parser->Zda.UTCTime.Mid(4,2)));

		wxTimeSpan dateDiff = now - epoch;

		unsigned short daysSinceEpoch = dateDiff.GetDays();
		unsigned int secondsSinceMidnight = ((dateDiff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

		short localOffset = (parser->Zda.LocalHourDeviation * 60) + (parser->Zda.LocalMinutesDeviation);
		n2kMessage->push_back(localOffset & 0xFF);
		n2kMessage->push_back((localOffset >> 8) & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// AIS Messages not supported yet

// Encode payload for PGN 129038 NMEA AIS Class A Position Report
// AIS Message Types 1,2 or 3
bool TwoCanEncoder::EncodePGN129038(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {

/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	double longitude;
	longitude = ((n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

	double latitude;
	latitude = ((n2kMessage[9] | (n2kMessage[10] << 8) | (n2kMessage[11] << 16) | (n2kMessage[12] << 24))) * 1e-7;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

	int positionAccuracy;
	positionAccuracy = n2kMessage[13] & 0x01;

	int raimFlag;
	raimFlag = (n2kMessage[13] & 0x02) >> 1;

	int timeStamp;
	timeStamp = (n2kMessage[13] & 0xFC) >> 2;

	int courseOverGround;
	courseOverGround = n2kMessage[14] | (n2kMessage[15] << 8);

	int speedOverGround;
	speedOverGround = n2kMessage[16] | (n2kMessage[17] << 8);

	int communicationState;
	communicationState = (n2kMessage[18] | (n2kMessage[19] << 8) | (n2kMessage[20] << 16) & 0x7FFFF);

	int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
	transceiverInformation = (n2kMessage[20] & 0xF8) >> 3;

	int trueHeading;
	trueHeading = n2kMessage[21] | (n2kMessage[22] << 8);

	double rateOfTurn;
	rateOfTurn = n2kMessage[23] | (n2kMessage[24] << 8);

	int navigationalStatus;
	navigationalStatus = n2kMessage[25] & 0x0F;

	int reserved;
	reserved = (n2kMessage[25] & 0xF0) >> 4;

	// BUG BUG No idea about the bitlengths for the following, just guessing

	int manoeuverIndicator;
	manoeuverIndicator = n2kMessage[26] & 0x03;

	int spare;
	spare = (n2kMessage[26] & 0x0C) >> 2;

	int reservedForRegionalApplications;
	reservedForRegionalApplications = (n2kMessage[26] & 0x30) >> 4;

	int sequenceID;
	sequenceID = (n2kMessage[26] & 0xC0) >> 6;

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
	*/

	return TRUE;
}

// Encode payload for PGN 129039 NMEA AIS Class B Position Report
// AIS Message Type 18
bool TwoCanEncoder::EncodePGN129039(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	double longitude;
	longitude = ((n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

	double latitude;
	latitude = ((n2kMessage[9] | (n2kMessage[10] << 8) | (n2kMessage[11] << 16) | (n2kMessage[12] << 24))) * 1e-7;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

	int positionAccuracy;
	positionAccuracy = n2kMessage[13] & 0x01;

	int raimFlag;
	raimFlag = (n2kMessage[13] & 0x02) >> 1;

	int timeStamp;
	timeStamp = (n2kMessage[13] & 0xFC) >> 2;

	int courseOverGround;
	courseOverGround = n2kMessage[14] | (n2kMessage[15] << 8);

	int  speedOverGround;
	speedOverGround = n2kMessage[16] | (n2kMessage[17] << 8);

	int communicationState;
	communicationState = n2kMessage[18] | (n2kMessage[19] << 8) | ((n2kMessage[20] & 0x7) << 16);

	int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
	transceiverInformation = (n2kMessage[20] & 0xF8) >> 3;

	int trueHeading;
	trueHeading = RADIANS_TO_DEGREES((n2kMessage[21] | (n2kMessage[22] << 8)));

	int regionalReservedA;
	regionalReservedA = n2kMessage[23];

	int regionalReservedB;
	regionalReservedB = n2kMessage[24] & 0x03;

	int unitFlag;
	unitFlag = (n2kMessage[24] & 0x04) >> 2;

	int displayFlag;
	displayFlag = (n2kMessage[24] & 0x08) >> 3;

	int dscFlag;
	dscFlag = (n2kMessage[24] & 0x10) >> 4;

	int bandFlag;
	bandFlag = (n2kMessage[24] & 0x20) >> 5;

	int msg22Flag;
	msg22Flag = (n2kMessage[24] & 0x40) >> 6;

	int assignedModeFlag;
	assignedModeFlag = (n2kMessage[24] & 0x80) >> 7;

	int	sotdmaFlag;
	sotdmaFlag = n2kMessage[25] & 0x01;

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
	*/
	
	return TRUE;
	
}

// Encode payload for PGN 129040 AIS Class B Extended Position Report
// AIS Message Type 19
bool TwoCanEncoder::EncodePGN129040(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	double longitude;
	longitude = ((n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

	double latitude;
	latitude = ((n2kMessage[9] | (n2kMessage[10] << 8) | (n2kMessage[11] << 16) | (n2kMessage[12] << 24))) * 1e-7;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

	int positionAccuracy;
	positionAccuracy = n2kMessage[13] & 0x01;

	int raimFlag;
	raimFlag = (n2kMessage[13] & 0x02) >> 1;

	int timeStamp;
	timeStamp = (n2kMessage[13] & 0xFC) >> 2;

	int courseOverGround;
	courseOverGround = n2kMessage[14] | (n2kMessage[15] << 8);

	int speedOverGround;
	speedOverGround = n2kMessage[16] | (n2kMessage[17] << 8);

	int regionalReservedA;
	regionalReservedA = n2kMessage[18];

	int regionalReservedB;
	regionalReservedB = n2kMessage[19] & 0x0F;

	int reservedA;
	reservedA = (n2kMessage[19] & 0xF0) >> 4;

	int shipType;
	shipType = n2kMessage[20];

	int trueHeading;
	trueHeading = n2kMessage[21] | (n2kMessage[22] << 8);

	int reservedB;
	reservedB = n2kMessage[23] & 0x0F;

	int gnssType;
	gnssType = (n2kMessage[23] & 0xF0) >> 4;

	int shipLength;
	shipLength = n2kMessage[24] | (n2kMessage[25] << 8);

	int shipBeam;
	shipBeam = n2kMessage[26] | (n2kMessage[27] << 8);

	int refStarboard;
	refStarboard = n2kMessage[28] | (n2kMessage[29] << 8);

	int refBow;
	refBow = n2kMessage[30] | (n2kMessage[31] << 8);

	std::string shipName;
	for (int i = 0; i < 20; i++) {
		shipName.append(1, (char)n2kMessage[32 + i]);
	}

	int dteFlag;
	dteFlag = n2kMessage[52] & 0x01;

	int assignedModeFlag;
	assignedModeFlag = n2kMessage[52] & 0x02 >> 1;

	int spare;
	spare = (n2kMessage[52] & 0x0C) >> 2;

	int AisTransceiverInformation;
	AisTransceiverInformation = (n2kMessage[52] & 0xF0) >> 4;

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
	*/

	return TRUE;	
}

// Encode payload for PGN 129041 AIS Aids To Navigation (AToN) Report
// AIS Message Type 21
bool TwoCanEncoder::EncodePGN129041(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	double longitude;
	longitude = ((n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

	double latitude;
	latitude = ((n2kMessage[9] | (n2kMessage[10] << 8) | (n2kMessage[11] << 16) | (n2kMessage[12] << 24))) * 1e-7;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

	int positionAccuracy;
	positionAccuracy = n2kMessage[13] & 0x01;

	int raimFlag;
	raimFlag = (n2kMessage[13] & 0x02) >> 1;

	int timeStamp;
	timeStamp = (n2kMessage[13] & 0xFC) >> 2;

	int shipLength;
	shipLength = n2kMessage[14] | (n2kMessage[15] << 8);

	int shipBeam;
	shipBeam = n2kMessage[16] | (n2kMessage[17] << 8);

	int refStarboard;
	refStarboard = n2kMessage[18] | (n2kMessage[19] << 8);

	int refBow;
	refBow = n2kMessage[20] | (n2kMessage[21] << 8);

	int AToNType;
	AToNType = (n2kMessage[22] & 0xF8) >> 3;

	int offPositionFlag;
	offPositionFlag = (n2kMessage[22] & 0x04) >> 2;

	int virtualAToN;
	virtualAToN = (n2kMessage[22] & 0x02) >> 1;;

	int assignedModeFlag;
	assignedModeFlag = n2kMessage[22] & 0x01;

	int spare;
	spare = n2kMessage[23] & 0x01;

	int gnssType;
	gnssType = (n2kMessage[23] & 0x1E) >> 1;

	int reserved;
	reserved = n2kMessage[23] & 0xE0 >> 5;

	int AToNStatus;
	AToNStatus = n2kMessage[24];

	int transceiverInformation;
	transceiverInformation = (n2kMessage[25] & 0xF8) >> 3;

	int reservedB;
	reservedB = n2kMessage[25] & 0x07;

	// BUG BUG This is variable up to 20 + 14 (34) characters
	std::string AToNName;
	int AToNNameLength = n2kMessage[26];
	if (n2kMessage[27] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
		for (int i = 0; i < AToNNameLength - 1; i++) {
			AToNName.append(1, (char)n2kMessage[28 + i]);
		}
	}

	// Encode VDM Message using 6bit ASCII
	
	AISInsertInteger(binaryData, 0, 6, messageID);
	AISInsertInteger(binaryData, 6, 2, repeatIndicator);
	AISInsertInteger(binaryData, 8, 30, userID);
	AISInsertInteger(binaryData, 38, 5, AToNType);
	AISInsertString(binaryData, 43, 120, AToNNameLength <= 20 ? AToNName : AToNName.substr(0, 20));
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
		AISInsertString(binaryData, 272, (AToNName.length() - 20) * 6, AToNName.substr(20, AToNName.length() - 20));
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

	wxString encodedVDMMessage = AISEncoden2kMessage(binaryData);

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
	*/

	return TRUE;
	
}

// Encode payload for PGN 129283 NMEA Cross Track Error
// Generated by APB, RMB or XTE sentences
bool TwoCanEncoder::EncodePGN129283(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("XTE")) {

		if (parser->Xte.IsDataValid == NTrue) {
	
			n2kMessage->push_back(sequenceId);
		
			byte xteMode;
			if (parser->Xte.FAAModeIndicator == _T("A")) {
				xteMode = 0; 
			}
			else if (parser->Xte.FAAModeIndicator == _T("D")) {
				xteMode = 1;
			}
			else if (parser->Xte.FAAModeIndicator == _T("E")) {
				xteMode = 2;
			}
			else if (parser->Xte.FAAModeIndicator ==_T("S")) {
				xteMode = 3;
			}
			else if (parser->Xte.FAAModeIndicator == _T("M")) {
				xteMode = 4;
			}
			else { // BUG BUG Data not available??
				xteMode = 0x0F;
			}
			
			byte navigationTerminated = 0; //0 = No

			n2kMessage->push_back( (xteMode & 0x0F) | ((navigationTerminated << 6) & 0xC0));
	
			double rawXTE = parser->Xte.CrossTrackErrorDistance;
			int crossTrackError;

			if (nmeaParser.Xte.CrossTrackUnits == _T("N")) {
				crossTrackError = (int)(100 *  rawXTE / CONVERT_METRES_NAUTICAL_MILES);
			}

			if (nmeaParser.Xte.CrossTrackUnits == _T("K")) {
				crossTrackError = (int)rawXTE * 100000;
			}

			if (parser->Xte.DirectionToSteer == LEFTRIGHT::Left) {
				crossTrackError = -crossTrackError;
			}

			n2kMessage->push_back(crossTrackError & 0xFF);
			n2kMessage->push_back((crossTrackError >> 8) & 0xFF);
			n2kMessage->push_back((crossTrackError >> 16) & 0xFF);
			n2kMessage->push_back((crossTrackError >> 24) & 0xFF);

			// BUG BUG REMOVE
			wxLogMessage(_T("TwoCan Encoder, Debig nfo,  XTE: %d"), crossTrackError);

			return TRUE;
		}
	}

	else if (parser->LastSentenceIDParsed == _T("APB")) {
		n2kMessage->push_back(sequenceId);

		if ((parser->Apb.IsLoranBlinkOK == NTrue) && (parser->Apb.IsLoranCCycleLockOK)) {		
			// BUG BUG Assume GPS
			byte xteMode = 0;
			
			byte navigationTerminated = 0; // 0 = No
			if ((parser->Apb.IsArrivalCircleEntered == NTrue) || (parser->Apb.IsPerpendicular == NTrue)) {
				navigationTerminated = 1;
			}

			n2kMessage->push_back( (xteMode & 0x0F) | ((navigationTerminated << 6) & 0xC0));
	
			double rawXTE = parser->Apb.CrossTrackErrorMagnitude;
			int crossTrackError;

			if (nmeaParser.Apb.CrossTrackUnits == _T("N")) {
				crossTrackError = (int)(100 *  rawXTE / CONVERT_METRES_NAUTICAL_MILES);
			}

			if (nmeaParser.Apb.CrossTrackUnits == _T("K")) {
				crossTrackError = (int)rawXTE * 100000;
			}

			if (parser->Apb.DirectionToSteer == LEFTRIGHT::Left) {
				crossTrackError = -crossTrackError;
			}

			n2kMessage->push_back(crossTrackError & 0xFF);
			n2kMessage->push_back((crossTrackError >> 8) & 0xFF);
			n2kMessage->push_back((crossTrackError >> 16) & 0xFF);
			n2kMessage->push_back((crossTrackError >> 24) & 0xFF);

			// BUG BUG REMOVE
			wxLogMessage(_T("TwoCan Encoder,Debug Info, APB %d"), crossTrackError);

			return TRUE;
		}
	}
	else if (parser->LastSentenceIDParsed == _T("RMB")) {
		
		if (parser->Rmb.IsDataValid == NTrue) {
		
			n2kMessage->push_back(sequenceId);
		
			byte xteMode;
			if (parser->Rmb.FAAModeIndicator == _T("A")) {
				xteMode = 0; 
			}
			else if (parser->Rmb.FAAModeIndicator == _T("D")) {
				xteMode = 1;
			}
			else if (parser->Rmb.FAAModeIndicator == _T("E")) {
				xteMode = 2;
			}
			else if (parser->Rmb.FAAModeIndicator ==_T("S")) {
				xteMode = 3;
			}
			else if (parser->Rmb.FAAModeIndicator == _T("M")) {
				xteMode = 4;
			}
			else { // BUG BUG Data not available??
				xteMode = 0x0F;
			}
			
			byte navigationTerminated = 0; //0 = No
			if (parser->Rmb.IsArrivalCircleEntered == NTrue) {
				navigationTerminated = 1; 
			}
			n2kMessage->push_back( (xteMode & 0x0F) | ((navigationTerminated << 6) & 0xC0));
	
			double rawXTE = parser->Rmb.CrossTrackError;
			int crossTrackError = (int)(100 *  rawXTE / CONVERT_METRES_NAUTICAL_MILES);
			
			if (parser->Rmb.DirectionToSteer == LEFTRIGHT::Left) {
				crossTrackError = -crossTrackError;
			}

			n2kMessage->push_back(crossTrackError & 0xFF);
			n2kMessage->push_back((crossTrackError >> 8) & 0xFF);
			n2kMessage->push_back((crossTrackError >> 16) & 0xFF);
			n2kMessage->push_back((crossTrackError >> 24) & 0xFF);

			return TRUE;
		}
	}
	return FALSE;
}

// Encode payload for PGN 129284 Navigation Data
//$--BWC, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, T, x.x, M, x.x, N, c--c, a*hh<CR><LF>
//$--BWR, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, T, x.x, M, x.x, N, c--c, a*hh<CR><LF>
//$--BOD, x.x, T, x.x, M, c--c, c--c*hh<CR><LF>
//$--WCV, x.x, N, c--c, a*hh<CR><LF>

// Not sure of this use case, as it implies there is already a chartplotter on board
bool TwoCanEncoder::EncodePGN129284(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();
	/*
	if (parser->LastSentenceIDParsed == _T("RMB")) {

		if (parser->Rmb.IsDataValid == NTrue) {
	
			n2kMessage->push_back(sequenceId);

			unsigned short distance = 100 * parser->Rmb.RangeToDestinationNauticalMiles/ CONVERT_METRES_NAUTICAL_MILES;
			n2kMessage.push_back(distance & 0xFF);
			n2kMessage.push_back((distance >> 8) & 0xFF);
		
			byte bearingRef = (HEADING_TRUE << 6) & 0xC;

			byte perpendicularCrossed = (0 << 4) & 0x30; // Yes or No
	
			byte circleEntered = (0 << 2) & 0x0C; // Yes or No
			if (parser->Rmb.IsArrivalCircleEntered == NTrue) {
				circleEntered = (1 << 2) & 0x0C;
			}

			byte calculationType = (0 & 0x03); // Great Circle or Rhumb Line
	
			n2kMessage->push_back(bearingRef | perpendicularCrossed | circleEntered | calculationType;

			unsigned int secondsSinceMidnight;
			secondsSinceMidnight = n2kMessage[4] | (n2kMessage[5] << 8) | (n2kMessage[6] << 16) | (n2kMessage[7] << 24);

			unsigned short daysSinceEpoch;
			daysSinceEpoch = n2kMessage[8] | (n2kMessage[9] << 8);

			unsigned short bearingOrigin = USHRT_MAX; // 10 & 11
			n2kMessage->push_back(bearingOrigin & 0xFF);
			n2kMessage->push_back((bearingOrigin >> 8)) & 0xFF;

			unsigned short bearingPosition = 1000 * DEGREES_TO_RADIANS(parser->Rmb.BearingToDestinationDegreesTrue);
			n2kMessage->push_back(bearingPosition & 0xFF);
			n2kMessage->push_back((bearingPosition >> 8) & 0xFF);

			int originWaypointId = parser->Rmb.From;
			n2kMessage[14] | (n2kMessage[15] << 8) | (n2kMessage[16] << 16) | (n2kMessage[17] << 24);

			int destinationWaypointId = parser->Rmb.To;
			n2kMessage[18] | (n2kMessage[19] << 8) | (n2kMessage[20] << 16) | (n2kMessage[21] << 24);

			double latitude = parser->Rmb.DestinationPosition.Latitude.Latitude;
			if (parser->Rmb.DestinationPosition.Latitude.Northing == NORTHSOUTH::South) {
				latitude = -latitude;
			}

			latitude = ((n2kMessage[22] | (n2kMessage[23] << 8) | (n2kMessage[24] << 16) | (n2kMessage[25] << 24))) * 1e-7;

			double longitude = parser->Rmb.DestinationPosition.Longitude.Longitude;
			if (parser->Rmb.DestinationPosition.Longitude.Easting == EASTWEST::West) {
				longitude = -longitude;
			}

			longitude = ((n2kMessage[26] | (n2kMessage[27] << 8) | (n2kMessage[28] << 16) | (n2kMessage[29] << 24))) * 1e-7;
		
			unsigned short waypointClosingVelocity = 100 * parser->Rmb.DestinationClosingVelocityKnots/CONVERT_MS_KNOTS;
			n2kMessage->push_back(waypointClosingVelocity & 0xFF);
			n2kMessage->push_back((waypointClosingVelocity >> 8) & 0xFF);
		
			return TRUE;
		}
	}
	*/
	return FALSE;
}

// Encode payload for PGN 129285 Route and Waypoint Information
// $--RTE,x.x,x.x,a,c--c,c--c, ..��... c--c*hh<CR><LF>
// and 
// $--WPL,llll.ll,a,yyyyy.yy,a,c--c
bool TwoCanEncoder::EncodePGN129285(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	
	// Need to construct a route/waypoint thingy......
	// This is what we are sent
	//'$ECWPL,4740.899,N,12224.470,W,001*72',
	//'$ECWPL,4741.661,N,12226.537,W,One*0F',
	//'$ECWPL,4742.669,N,12226.395,W,Two*02',
	//'$ECWPL,4745.423,N,12225.527,W,Three*07',
	//'$ECWPL,4741.008,N,12224.334,W,005*70',
	//'$ECRTE,3,1,c,Route One,001,One*6C',
	//'$ECRTE,3,2,c,Route One,Two,Three*18',
	//'$ECRTE,3,3,c,Route One,005*02')

	// Use the Plugin_Route
	/*
	unsigned short rps;
	rps = n2kMessage[0] | (n2kMessage[1] << 8);

	unsigned short nItems;
	nItems = n2kMessage[2] | (n2kMessage[3] << 8);

	unsigned short databaseVersion;
	databaseVersion = n2kMessage[4] | (n2kMessage[5] << 8);

	unsigned short routeID;
	routeID = n2kMessage[6] | (n2kMessage[7] << 8);

	unsigned char direction; // I presume forward/reverse
	direction = n2kMessage[8] & 0xC0;

	unsigned char supplementaryInfo;
	supplementaryInfo = n2kMessage[8] & 0x30;

	// NMEA reserved
	// unsigned short reservedA = n2kMessage[8} & 0x0F;

	std::string routeName;
	// BUG BUG If this is null terminated, just use strcpy
	for (int i = 0; i < 255; i++) {
		if (isprint(n2kMessage[9 + i])) {
			routeName.append(1, n2kMessage[9 + i]);
		}
	}

	// NMEA reserved n2kMessage[264]
	//unsigned int reservedB = n2kMessage[264];

	// repeated fields
	for (unsigned int i = 0; i < nItems; i++) {
		int waypointID;
		waypointID = n2kMessage[265 + (i * 265)] | (n2kMessage[265 + (i * 265) + 1] << 8);

		std::string waypointName;
		for (int j = 0; j < 255; j++) {
			if (isprint(n2kMessage[265 + (i * 265) + 266 + j])) {
				waypointName.append(1, n2kMessage[265 + (i * 265) + 266 + j]);
			}
		}

		double latitude = n2kMessage[265 + (i * 265) + 257] | (n2kMessage[265 + (i * 265) + 258] << 8) | (n2kMessage[265 + (i * 265) + 259] << 16) | (n2kMessage[265 + (i * 265) + 260] << 24);
		int latitudeDegrees = (int)latitude;
		double latitudeMinutes = (latitude - latitudeDegrees) * 60;

		double longitude = n2kMessage[265 + (i * 265) + 261] | (n2kMessage[265 + (i * 265) + 262] << 8) | (n2kMessage[265 + (i * 265) + 263] << 16) | (n2kMessage[265 + (i * 265) + 264] << 24);
		int longitudeDegrees = (int)longitude;
		double longitudeMinutes = (longitude - longitudeDegrees) * 60;

		nmeaSentences->push_back(wxString::Format("$IIWPL,%02d%05.2f,%c,%03d%05.2f,%c,%s",
			abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
			abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
			waypointName.c_str()));
	}
	*/
	return TRUE;

}

// Encode payload for PGN 129540 GNSS Satellites in View
bool TwoCanEncoder::EncodePGN129540(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	int numberOfMessages = parser->Gsv.NumberOfMessages;
	int messageNumber = parser->Gsv.MessageNumber;

	if (parser->Gsv.MessageNumber == 1) {
		// This is the first message so reset everything
		for (int i = 0; i < 12; i++) {
			gpsSatelites[i].AzimuthDegreesTrue = 0;
			gpsSatelites[i].ElevationDegrees = 0;
			gpsSatelites[i].SatNumber = 0;
			gpsSatelites->SignalToNoiseRatio = 0;
		}
		// Save the data
		for (int i = 0; i < ((parser->Gsv.SatsInView > 4) ? 4 : parser->Gsv.SatsInView); i++ ) {
			gpsSatelites[i].AzimuthDegreesTrue = parser->Gsv.SatInfo->AzimuthDegreesTrue;
			gpsSatelites[i].ElevationDegrees = parser->Gsv.SatInfo->ElevationDegrees;
			gpsSatelites[i].SatNumber = parser->Gsv.SatInfo->SatNumber;
			gpsSatelites->SignalToNoiseRatio = parser->Gsv.SatInfo->SignalToNoiseRatio;
		}

	}
	if (parser->Gsv.MessageNumber == 2) {
		for (int i = 4; i < ((parser->Gsv.SatsInView > 8) ? 8 : parser->Gsv.SatsInView - 4); i++ ) {
			gpsSatelites[i].AzimuthDegreesTrue = parser->Gsv.SatInfo->AzimuthDegreesTrue;
			gpsSatelites[i].ElevationDegrees = parser->Gsv.SatInfo->ElevationDegrees;
			gpsSatelites[i].SatNumber = parser->Gsv.SatInfo->SatNumber;
			gpsSatelites->SignalToNoiseRatio = parser->Gsv.SatInfo->SignalToNoiseRatio;
		}
	}

	if (parser->Gsv.MessageNumber == 3) {
		for (int i = 8; i < ((parser->Gsv.SatsInView >= 12) ? 12 : parser->Gsv.SatsInView - 8); i++ ) {
			gpsSatelites[i].AzimuthDegreesTrue = parser->Gsv.SatInfo->AzimuthDegreesTrue;
			gpsSatelites[i].ElevationDegrees = parser->Gsv.SatInfo->ElevationDegrees;
			gpsSatelites[i].SatNumber = parser->Gsv.SatInfo->SatNumber;
			gpsSatelites->SignalToNoiseRatio = parser->Gsv.SatInfo->SignalToNoiseRatio;
		}
	}

	if (messageNumber == numberOfMessages) {
		// we have all the messages
		
		n2kMessage->push_back(sequenceId);

		byte mode = 0; // Mode, 3 = range Residuals used to determine position
		n2kMessage->push_back(mode & 0x03);

		n2kMessage->push_back(parser->Gsv.SatsInView);

		int index = 3;

		for (int i = 0;i < parser->Gsv.SatsInView; i++) {
			
			n2kMessage->push_back(gpsSatelites[i].SatNumber);
			index += 1;

			unsigned short elevation = 10000 * DEGREES_TO_RADIANS(gpsSatelites[i].ElevationDegrees);
			n2kMessage->push_back(elevation & 0xFF);
			n2kMessage->push_back((elevation >> 8) & 0xFF);
			index += 2;

			unsigned short azimuth = 10000 * DEGREES_TO_RADIANS(gpsSatelites[i].AzimuthDegreesTrue);
			n2kMessage->push_back(azimuth & 0xFF);
			n2kMessage->push_back((azimuth >> 8) & 0xFF);
			index += 2;

			unsigned short snr = 100 * gpsSatelites[i].SignalToNoiseRatio;
			n2kMessage->push_back(snr & 0xFF);
			n2kMessage->push_back((snr >> 8) & 0xFF);
			index += 2;

			unsigned int rangeResiduals = UINT_MAX;
			n2kMessage->push_back(rangeResiduals & 0xFF);
			n2kMessage->push_back((rangeResiduals >> 8) & 0xFF);
			n2kMessage->push_back((rangeResiduals >> 16) & 0xFF);
			n2kMessage->push_back((rangeResiduals >> 24) & 0xFF);
			index += 4;

			// from canboat,
			// 0 Not tracked
			// 1 Tracked
			// 2 Used
			// 3 Not tracked+Diff
			// 4 Tracked+Diff
			// 5 Used+Diff
			n2kMessage->push_back(1);
			index += 1;
		}

	}

	return TRUE;
}

// Encode payload for PGN 129793 AIS Date and Time report
// AIS Message Type 4 and if date is present also Message Type 11
bool TwoCanEncoder::EncodePGN129793(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	// Should really check whether this is 4 (Base Station) or 
	// 11 (mobile station, but only in response to a request using message 10)
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	double longitude;
	longitude = ((n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

	double latitude;
	latitude = ((n2kMessage[9] | (n2kMessage[10] << 8) | (n2kMessage[11] << 16) | (n2kMessage[12] << 24))) * 1e-7;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

	int positionAccuracy;
	positionAccuracy = n2kMessage[13] & 0x01;

	int raimFlag;
	raimFlag = (n2kMessage[13] & 0x02) >> 1;

	int reservedA;
	reservedA = (n2kMessage[13] & 0xFC) >> 2;

	int secondsSinceMidnight;
	secondsSinceMidnight = n2kMessage[14] | (n2kMessage[15] << 8) | (n2kMessage[16] << 16) | (n2kMessage[17] << 24);

	int communicationState;
	communicationState = n2kMessage[18] | (n2kMessage[19] << 8) | ((n2kMessage[20] & 0x7) << 16);

	int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
	transceiverInformation = (n2kMessage[20] & 0xF8) >> 3;

	int daysSinceEpoch;
	daysSinceEpoch = n2kMessage[21] | (n2kMessage[21] << 8);

	int reservedB;
	reservedB = n2kMessage[22] & 0x0F;

	int gnssType;
	gnssType = (n2kMessage[22] & 0xF0) >> 4;

	int spare;
	spare = n2kMessage[23];

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
	*/
	return TRUE;

}


// Encode payload for PGN 129794 NMEA AIS Class A Static and Voyage Related Data
// AIS Message Type 5
bool TwoCanEncoder::EncodePGN129794(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	unsigned int messageID;
	messageID = n2kMessage[0] & 0x3F;

	unsigned int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	unsigned int userID; // aka MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	unsigned int imoNumber;
	imoNumber = n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24);

	std::string callSign;
	for (int i = 0; i < 7; i++) {
		callSign.append(1, (char)n2kMessage[9 + i]);
	}

	std::string shipName;
	for (int i = 0; i < 20; i++) {
		shipName.append(1, (char)n2kMessage[16 + i]);
	}

	unsigned int shipType;
	shipType = n2kMessage[36];

	unsigned int shipLength;
	shipLength = n2kMessage[37] | n2kMessage[38] << 8;

	unsigned int shipBeam;
	shipBeam = n2kMessage[39] | n2kMessage[40] << 8;

	unsigned int refStarboard;
	refStarboard = n2kMessage[41] | n2kMessage[42] << 8;

	unsigned int refBow;
	refBow = n2kMessage[43] | n2kMessage[44] << 8;

	// BUG BUG Just guessing that this is correct !!
	unsigned int daysSinceEpoch;
	daysSinceEpoch = n2kMessage[45] | (n2kMessage[46] << 8);

	unsigned int secondsSinceMidnight;
	secondsSinceMidnight = n2kMessage[47] | (n2kMessage[48] << 8) | (n2kMessage[49] << 16) | (n2kMessage[50] << 24);

	wxDateTime eta;
	eta.ParseDateTime("00:00:00 01-01-1970");
	eta += wxDateSpan::Days(daysSinceEpoch);
	eta += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

	unsigned int draft;
	draft = n2kMessage[51] | n2kMessage[52] << 8;

	std::string destination;
	for (int i = 0; i < 20; i++) {
		destination.append(1, (char)n2kMessage[53 + i]);
	}

	// BUG BUG These could be back to front
	unsigned int aisVersion;
	aisVersion = (n2kMessage[73] & 0xC0) >> 6;

	unsigned int gnssType;
	gnssType = (n2kMessage[73] & 0x3C) >> 2;

	unsigned int dteFlag;
	dteFlag = (n2kMessage[73] & 0x02) >> 1;

	unsigned int transceiverInformation;
	transceiverInformation = n2kMessage[74] & 0x1F;

	// Encode VDM Message using 6bit ASCII
	
	AISInsertInteger(binaryData, 0, 6, messageID);
	AISInsertInteger(binaryData, 6, 2, repeatIndicator);
	AISInsertInteger(binaryData, 8, 30, userID);
	AISInsertInteger(binaryData, 38, 2, aisVersion);
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

	wxString encodedVDMMessage = AISEncoden2kMessage(binaryData);

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
	*/
	return TRUE;
}

//	Encode payload for PGN 129798 AIS SAR Aircraft Position Report
// AIS Message Type 9
bool TwoCanEncoder::EncodePGN129798(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	double longitude;
	longitude = ((n2kMessage[5] | (n2kMessage[6] << 8) | (n2kMessage[7] << 16) | (n2kMessage[8] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

	double latitude;
	latitude = ((n2kMessage[9] | (n2kMessage[10] << 8) | (n2kMessage[11] << 16) | (n2kMessage[12] << 24))) * 1e-7;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

	int positionAccuracy;
	positionAccuracy = n2kMessage[13] & 0x01;

	int raimFlag;
	raimFlag = (n2kMessage[13] & 0x02) >> 1;

	int timeStamp;
	timeStamp = (n2kMessage[13] & 0xFC) >> 2;

	int courseOverGround;
	courseOverGround = n2kMessage[14] | (n2kMessage[15] << 8);

	int speedOverGround;
	speedOverGround = n2kMessage[16] | (n2kMessage[17] << 8);

	int communicationState;
	communicationState = (n2kMessage[18] | (n2kMessage[19] << 8) | (n2kMessage[20] << 16)) & 0x7FFFF;

	int transceiverInformation;
	transceiverInformation = (n2kMessage[20] & 0xF8) >> 3;

	double altitude;
	altitude = 1e-6 * (((long long)n2kMessage[21] | ((long long)n2kMessage[22] << 8) | ((long long)n2kMessage[23] << 16) | ((long long)n2kMessage[24] << 24) \
		| ((long long)n2kMessage[25] << 32) | ((long long)n2kMessage[26] << 40) | ((long long)n2kMessage[27] << 48) | ((long long)n2kMessage[28] << 56)));

	int reservedForRegionalApplications;
	reservedForRegionalApplications = n2kMessage[29];

	int dteFlag;
	dteFlag = n2kMessage[30] & 0x01;

	// BUG BUG Just guessing these to match NMEA2000 n2kMessage with ITU AIS fields

	int assignedModeFlag;
	assignedModeFlag = (n2kMessage[30] & 0x02) >> 1;

	int sotdmaFlag;
	sotdmaFlag = (n2kMessage[30] & 0x04) >> 2;

	int altitudeSensor;
	altitudeSensor = (n2kMessage[30] & 0x08) >> 3;

	int spare;
	spare = (n2kMessage[30] & 0xF0) >> 4;

	int reserved;
	reserved = n2kMessage[31];

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

	*/
	return TRUE;

}

//	Encode payload for PGN 129801 AIS Addressed Safety Related Message
// AIS Message Type 12
bool TwoCanEncoder::EncodePGN129801(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int sourceID;
	sourceID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	int reservedA;
	reservedA = n2kMessage[4] & 0x01;

	int transceiverInfo;
	transceiverInfo = (n2kMessage[5] & 0x3E) >> 1;

	int sequenceNumber;
	sequenceNumber = (n2kMessage[5] & 0xC0) >> 6;

	int destinationId;
	destinationId = n2kMessage[6] | (n2kMessage[7] << 8) | (n2kMessage[8] << 16) | (n2kMessage[9] << 24);

	int reservedB;
	reservedB = n2kMessage[10] & 0x3F;

	int retransmitFlag;
	retransmitFlag = (n2kMessage[10] & 0x40) >> 6;

	int reservedC;
	reservedC = (n2kMessage[10] & 0x80) >> 7;

	std::string safetyMessage;
	for (int i = 0; i < 156; i++) {
		safetyMessage.append(1, (char)n2kMessage[11 + i]);
	}
	// BUG BUG Not sure if ths is encoded same as Addressed Safety Message
	//std::string safetyMessage;
	//int safetyMessageLength = n2kMessage[6];
	//if (n2kMessage[7] == 1) {
	// first byte of safetmessage indicates encoding; 0 for Unicode, 1 for ASCII
	//for (int i = 0; i < safetyMessageLength - 2; i++) {
	//	safetyMessage += (static_cast<char>(n2kMessage[8 + i]));
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

	wxString encodedVDMMessage = AISEncoden2kMessage(binaryData);

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
	*/
	return TRUE;
}

// Encode payload for PGN 129802 AIS Safety Related Broadcast Message 
// AIS Message Type 14
bool TwoCanEncoder::EncodePGN129802(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int sourceID;
	sourceID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | ((n2kMessage[4] & 0x3F) << 24);

	int reservedA;
	reservedA = (n2kMessage[4] & 0xC0) >> 6;

	int transceiverInfo;
	transceiverInfo = n2kMessage[5] & 0x1F;

	int reservedB;
	reservedB = (n2kMessage[5] & 0xE0) >> 5;

	std::string safetyMessage;
	int safetyMessageLength = n2kMessage[6];
	if (n2kMessage[7] == 1) {
		// first byte of safetmessage indicates encoding; 0 for Unicode, 1 for ASCII
		for (int i = 0; i < safetyMessageLength - 2; i++) {
			safetyMessage += (static_cast<char>(n2kMessage[8 + i]));
		}
	}


	AISInsertInteger(binaryData, 0, 6, messageID);
	AISInsertInteger(binaryData, 6, 2, repeatIndicator);
	AISInsertInteger(binaryData, 8, 30, sourceID);
	AISInsertInteger(binaryData, 38, 2, 0); //spare
	int l = safetyMessage.size(&nmeaParser, &payload);
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
	std::vector<bool>::const_iterator first = binaryData.begin(&nmeaParser, &payload);
	std::vector<bool>::const_iterator last = binaryData.begin() + 40 + (l * 6) + fillBits;
	std::vector<bool> newVec(first, last);

	// Encode the VDM Message using 6bit ASCII
	wxString encodedVDMMessage = AISEncoden2kMessage(newVec);

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
	*/
	return TRUE;
}


// Encode payload for PGN 129808 NMEA DSC Call
// A mega confusing combination !!
// $--DSC, xx,xxxxxxxxxx,xx,xx,xx,x.x,x.x,xxxxxxxxxx,xx,a,a
//          |     |       |  |  |  |   |  MMSI        | | Expansion Specifier
//          |   MMSI     Category  Position           | Acknowledgement        
//          Format Specifer  |  |      |Time          Nature of Distress
//                           |  Type of Communication or Second telecommand
//                           Nature of Distress or First Telecommand

// and
// $--DSE

bool TwoCanEncoder::EncodePGN129808(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*	
	byte formatSpecifier;
	formatSpecifier = n2kMessage[0];

	byte dscCategory;
	dscCategory = n2kMessage[1];

	char mmsiAddress[5];
	sprintf(mmsiAddress, "%02d%02d%02d%02d%02d", n2kMessage[2], n2kMessage[3], n2kMessage[4], n2kMessage[5], n2kMessage[6]);

	byte firstTeleCommand; // or Nature of Distress
	firstTeleCommand = n2kMessage[7];

	byte secondTeleCommand; // or Communication Mode
	secondTeleCommand = n2kMessage[8];

	char receiveFrequency;
	receiveFrequency = n2kMessage[9]; // Encoded of 9, 10, 11, 12, 13, 14

	char transmitFrequency;
	transmitFrequency = n2kMessage[15]; // Encoded of 15, 16, 17, 18, 19, 20

	char telephoneNumber;
	telephoneNumber = n2kMessage[21]; // encoded over 8 or 16 bytes

	int index = 0;

	double latitude;
	latitude = ((n2kMessage[index + 1] | (n2kMessage[index + 2] << 8) | (n2kMessage[index + 3] << 16) | (n2kMessage[index + 4] << 24))) * 1e-7;

	index += 4;

	int latitudeDegrees = (int)latitude;
	double latitudeMinutes = (latitude - latitudeDegrees) * 60;

	double longitude;
	longitude = ((n2kMessage[index + 1] | (n2kMessage[index + 2] << 8) | (n2kMessage[index + 3] << 16) | (n2kMessage[index + 4] << 24))) * 1e-7;

	int longitudeDegrees = (int)longitude;
	double longitudeMinutes = (longitude - longitudeDegrees) * 60;

	unsigned int secondsSinceMidnight;
	secondsSinceMidnight = n2kMessage[2] | (n2kMessage[3] << 8) | (n2kMessage[4] << 16) | (n2kMessage[5] << 24);

	// note n2kMessage index.....
	char vesselInDistress[5];
	sprintf(vesselInDistress, "%02d%02d%02d%02d%02d", n2kMessage[2], n2kMessage[3], n2kMessage[4], n2kMessage[5], n2kMessage[6]);

	byte endOfSequence;
	endOfSequence = n2kMessage[101]; // 1 byte

	byte dscExpansionEnabled; // Encoded over two bits
	dscExpansionEnabled = (n2kMessage[102] & 0xC0) >> 6;

	byte reserved; // 6 bits
	reserved = n2kMessage[102] & 0x3F;

	byte callingRx; // 6 bytes
	callingRx = n2kMessage[103];

	byte callingTx; // 6 bytes
	callingTx = n2kMessage[104];

	unsigned int timeOfTransmission;
	timeOfTransmission = n2kMessage[105] | (n2kMessage[106] << 8) | (n2kMessage[107] << 16) | (n2kMessage[108] << 24);

	unsigned int dayOfTransmission;
	dayOfTransmission = n2kMessage[109] | (n2kMessage[110] << 8);

	unsigned int messageId;
	messageId = n2kMessage[111] | (n2kMessage[112] << 8);

	// The following pairs are repeated

	byte dscExpansionSymbol;
	dscExpansionSymbol = n2kMessage[113];

	// Now iterate through the DSE Expansion data

	for (size_t i = 120; i < sizeof(n2kMessage);) {
		switch (n2kMessage[i]) {
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
	*/
	return FALSE;

}

// Encode payload for PGN 129809 AIS Class B Static Data Report, Part A 
// AIS Message Type 24, Part A
bool TwoCanEncoder::EncodePGN129809(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	std::string shipName;
	for (int i = 0; i < 20; i++) {
		shipName.append(1, (char)n2kMessage[5 + i]);
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

	*/
	return TRUE;
}

// Encode payload for PGN 129810 AIS Class B Static Data Report, Part B 
// AIS Message Type 24, Part B
bool TwoCanEncoder::EncodePGN129810(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
/*
	int messageID;
	messageID = n2kMessage[0] & 0x3F;

	int repeatIndicator;
	repeatIndicator = (n2kMessage[0] & 0xC0) >> 6;

	int userID; // aka sender's MMSI
	userID = n2kMessage[1] | (n2kMessage[2] << 8) | (n2kMessage[3] << 16) | (n2kMessage[4] << 24);

	int shipType;
	shipType = n2kMessage[5];

	std::string vendorId;
	for (int i = 0; i < 7; i++) {
		vendorId.append(1, (char)n2kMessage[6 + i]);
	}

	std::string callSign;
	for (int i = 0; i < 7; i++) {
		callSign.append(1, (char)n2kMessage[12 + i]);
	}

	unsigned int shipLength;
	shipLength = n2kMessage[19] | n2kMessage[20] << 8;

	unsigned int shipBeam;
	shipBeam = n2kMessage[21] | n2kMessage[22] << 8;

	unsigned int refStarboard;
	refStarboard = n2kMessage[23] | n2kMessage[24] << 8;

	unsigned int refBow;
	refBow = n2kMessage[25] | n2kMessage[26] << 8;

	unsigned int motherShipID; // aka mother ship MMSI
	motherShipID = n2kMessage[27] | (n2kMessage[28] << 8) | (n2kMessage[29] << 16) | (n2kMessage[30] << 24);

	int reserved;
	reserved = (n2kMessage[31] & 0x03);

	int spare;
	spare = (n2kMessage[31] & 0xFC) >> 2;

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
	AISInsertInteger(binaryData, 162, 6, 0); //spare
	*/

	return TRUE;

}

// Encode payload for PGN 130306 NMEA Wind
bool TwoCanEncoder::EncodePGN130306(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("MWV")) {

		n2kMessage->push_back(sequenceId);

		unsigned short windSpeed = (unsigned short)(100 * parser->Mwv.WindSpeed / CONVERT_MS_KNOTS);
		n2kMessage->push_back(windSpeed & 0xFF);
		n2kMessage->push_back((windSpeed >> 8) & 0xFF);

		unsigned short windAngle = (unsigned short)(10000 * DEGREES_TO_RADIANS(parser->Mwv.WindAngle));
		n2kMessage->push_back(windAngle & 0xFF);
		n2kMessage->push_back((windAngle >> 8) & 0xFF);

		byte windReference = parser->Mwv.Reference == "T" ? WIND_REFERENCE_TRUE : parser->Mwv.Reference == "R" ? WIND_REFERENCE_APPARENT : UCHAR_MAX;
		n2kMessage->push_back(windReference & 0x07);

		return TRUE;
	}
	return FALSE;	
}

// Encode payload for PGN 130310 NMEA Water & Air Temperature and Pressure
bool TwoCanEncoder::EncodePGN130310(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("MTW")) {

		n2kMessage->push_back(sequenceId);

		unsigned short waterTemperature = static_cast<unsigned short>(100 * (parser->Mtw.Temperature + CONST_KELVIN));

		wxLogMessage(_T("TwoCan Encoder, debug Info, Temperature: %d"), waterTemperature);

		n2kMessage->push_back(waterTemperature & 0xFF);
		n2kMessage->push_back((waterTemperature >> 8) & 0xFF);

		short airTemperature = SHRT_MAX;
		n2kMessage->push_back(airTemperature & 0xFF);
		n2kMessage->push_back((airTemperature >> 8) & 0xFF);

		short airPressure = SHRT_MAX;
		n2kMessage->push_back(airPressure & 0xFF);
		n2kMessage->push_back((airPressure >> 8) & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 130311 NMEA Environment
bool TwoCanEncoder::EncodePGN130311(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();
	
	if (parser->LastSentenceIDParsed == _T("MTW")) {
		n2kMessage->push_back(sequenceId);

		byte temperatureSource = TEMPERATURE_SEA;
		byte humiditySource = UCHAR_MAX;
		n2kMessage->push_back((temperatureSource & 0x3F) | ((humiditySource << 6) & 0xC0));

		unsigned short temperature = static_cast<unsigned short>(100 * (parser->Mtw.Temperature + CONST_KELVIN));
		n2kMessage->push_back(temperature & 0xFF);
		n2kMessage->push_back((temperature >> 8) & 0xFF);

		unsigned short humidity = USHRT_MAX;
		n2kMessage->push_back(humidity & 0xFF);
		n2kMessage->push_back((humidity >> 8) & 0xFF);
	
		unsigned short pressure = USHRT_MAX;
		n2kMessage->push_back(pressure & 0xFF);
		n2kMessage->push_back((pressure >> 8) & 0xFF);

		return TRUE;
	}
	return FALSE;
}


// Encode payload for PGN 130312 NMEA Temperature
bool TwoCanEncoder::EncodePGN130312(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("MTW")) {
	
		n2kMessage->push_back(sequenceId);

		byte instance = 0;
		n2kMessage->push_back(instance);

		byte source = TEMPERATURE_SEA;
		n2kMessage->push_back(source);

		// FIND OUT parser->Mtw.TemperatureUnits
		unsigned short actualTemperature = static_cast<unsigned short>(100 * (parser->Mtw.Temperature + CONST_KELVIN));
		n2kMessage->push_back(actualTemperature & 0xFF);
		n2kMessage->push_back((actualTemperature >> 8) & 0xFF);

		unsigned short setTemperature = USHRT_MAX;
		n2kMessage->push_back(setTemperature & 0xFF);
		n2kMessage->push_back((setTemperature >> 8) & 0xFF);

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 130316 NMEA Temperature Extended Range
bool TwoCanEncoder::EncodePGN130316(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("MTW")) {
	
		n2kMessage->push_back(sequenceId);

		byte instance = 0; 
		n2kMessage->push_back(instance);

		n2kMessage->push_back(TEMPERATURE_SEA);

		unsigned int actualTemperature = static_cast<unsigned short>(100 * (parser->Mtw.Temperature + CONST_KELVIN));
		n2kMessage->push_back(actualTemperature & 0xFF);
		n2kMessage->push_back((actualTemperature >> 8) & 0xFF);
		n2kMessage->push_back((actualTemperature >> 16) & 0xFF);

		return TRUE;
	}
	return FALSE;
		
}


// Encode payload for PGN 130577 NMEA Direction Data
// BUG BUG Work out what to convert this to
bool TwoCanEncoder::EncodePGN130577(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	n2kMessage->clear();

	if (parser->LastSentenceIDParsed == _T("VDR")) {
		// 0 - Autonomous, 1 - Differential enhanced, 2 - Estimated, 3 - Simulated, 4 - Manual
		byte dataMode = 0;

		// True = 0, Magnetic = 1
		byte cogReference = 0;
		
		n2kMessage->push_back( (dataMode & 0x0F) | ((cogReference << 4) & 0x30) );

		n2kMessage->push_back(sequenceId);

		unsigned short courseOverGround = static_cast<unsigned short>(10000 * DEGREES_TO_RADIANS(parser->Vdr.DegreesTrue));
		n2kMessage->push_back(courseOverGround & 0xFF);
		n2kMessage->push_back((courseOverGround >> 8) & 0xFF);

		unsigned short speedOverGround = static_cast<unsigned short>(100 * (parser->Vdr.Knots/ CONVERT_MS_KNOTS));
		n2kMessage->push_back(speedOverGround & 0xFF);
		n2kMessage->push_back((speedOverGround >> 8) & 0xFF);

		unsigned short heading = USHRT_MAX; // 1000 * DEGREES_TO_RADIANS(parser->Vdr.DegreesMagnetic);
		n2kMessage->push_back(heading & 0xFF);
		n2kMessage->push_back((heading >> 8) & 0xFF);

		unsigned short speedThroughWater = USHRT_MAX;
		n2kMessage->push_back(speedThroughWater & 0xFF);
		n2kMessage->push_back((speedThroughWater >> 8) & 0xFF);

		unsigned short set = USHRT_MAX;
		n2kMessage->push_back(set & 0xFF);
		n2kMessage->push_back((set >> 8) & 0xFF);

		unsigned short drift = USHRT_MAX;
		n2kMessage->push_back(drift & 0xFF);
		n2kMessage->push_back((drift >> 8) & 0xFF);

		return TRUE;
	}
	else {
		return FALSE;
	}

}

int TwoCanEncoder::AISDecodeInteger(std::vector<bool> &binaryData, int start, int length) {
	int result = 0;
	for (int i = 0; i < length; i++) {
		result += binaryData[i + start] << (length - 1 - i);
	}
	return result;
}

std::string TwoCanEncoder::AISDecodeString(std::vector<bool> &binaryData, int start, int length) {
	int n = 0;
	int b = 1;
	std::string result;
	for (int i = length - 1; i > 0; i--) {
		n = n + (b * binaryData[i]);
		b += b;
		if ((i % 6) == 0) { // gnaw every 6 bits
			if (n < 32) {
				n += 64;
			} 
			result.append(1, (char)n);
			n = 0;
			b = 1;
		} // end gnawing
	}  // end for
	reverse(result.begin(), result.end());
	return result;
}

// Decode a NMEA 0183 6 bit encoded character to an 8 bit ASCII character
char TwoCanEncoder::AISDecodeCharacter(char value) {
	char result = value - 48;
	result = result > 40 ? result - 8 : result;
	return result;
}

// Decode the NMEA 0183 ASCII values, derived from 6 bit encoded data to an array of bits
// so that we can gnaw through the bits to retrieve each AIS data field 
std::vector<bool> TwoCanEncoder::AISDecoden2kMessage(wxString SixBitData) {
	std::vector<bool> decodedData(168);
	for (size_t i = 0; i < SixBitData.length(); i++) {
		char testByte = AISDecodeCharacter((char)SixBitData[i]);
		// Perform in reverse order so that we store in LSB order
		for (int j = 5; j >= 0; j--) {
			decodedData.push_back(testByte & (1 << j)); // sets each bit value in the array
		}
	}
	return decodedData;
}