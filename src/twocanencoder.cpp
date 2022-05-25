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
// 1.1 - 04/07/2021 Add AIS conversion
// 1.2 - 20/05/2022 Add DSC & MOB conversion, Fix incorrect GGA PGN (caused random depth values), Fix APB flags
//       Use NMEA 0183 v4.11 XDR standard transducer names

#include "twocanencoder.h"


TwoCanEncoder::TwoCanEncoder(wxEvtHandler *handler) {
	eventHandlerAddress = handler;
	aisDecoder = new TwoCanAis();
	dseTimer = new wxTimer();
	dseTimer->Bind(wxEVT_TIMER, &TwoCanEncoder::OnDseTimerExpired, this);
}

TwoCanEncoder::~TwoCanEncoder(void) {
	delete aisDecoder;
}

void TwoCanEncoder::RaiseEvent(int pgn, std::vector<byte> *data) {
	wxCommandEvent *event = new wxCommandEvent(wxEVT_SENTENCE_RECEIVED_EVENT, DSE_EXPIRED_EVENT);
	event->SetString(std::to_string(pgn));
	event->SetClientData(data);
	wxQueueEvent(eventHandlerAddress, event);
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
			if ((instanceNumber >= 0) && (instanceNumber <= 9)) {
				return instanceNumber;
			}

        }
    }
    return -1;
}

// Timer for processing DSE sentences
void TwoCanEncoder::OnDseTimerExpired(wxEvent &event) {
	if (dseMMSINumber != 0) {
		// we have not received the corresponding sentence within the timeout period, so send an incomplete PGN 129808 message
		dseMMSINumber = 0;
		
		// Fill out the remaining bytes for PGN 129808 
		dscPayload.push_back(0xFF);

		// Field 22
		dscPayload.push_back(0x02); // Length of data includes length byte & encoding byte
		dscPayload.push_back(0x01); // 01 = ASCII

		// Field 23
		dscPayload.push_back(0xFF);

		// Field 24
		dscPayload.push_back(0x02); // Length of data includes length byte & encoding byte
		dscPayload.push_back(0x01); // 01 = ASCII

		RaiseEvent(129808, &dscPayload);
	}
	else {
		// If MMSI Number equals zero, then we have already received the corresponding DSE sentence,
		// constructed the remainder of PGN 129808 and transmitted it. The timer should have been stopped in anycase.
	}

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
	
	// Parse the NMEA 183 sentence
	nmeaParser << sentence;

	if (nmeaParser.PreParse()) {

		// BUG BUG Should use a different priority based on the PGN
		// The actual PGN is initialized later in the switch statement
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
				if (!(supportedPGN & FLAGS_NAV)) {
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
				// BUG BUG Not implemented
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
				// IGNORE

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

		// DSE Expanded Digital Selective Calling
		else if (nmeaParser.LastSentenceIDReceived == _T("DSE")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_DSC)) {
					if ((dseTimer->IsRunning()) && (dseMMSINumber == nmeaParser.Dse.mmsiNumber) && (nmeaParser.Dse.sentenceNumber == nmeaParser.Dse.totalSentences)) {
						// We've received a DSE sentence that matches a preceding DSC sentence and within the time limit
						// Add the DSE data pairs to the PGN 129808 payload
						// Not sure if the DSE is limted to two items for NMEA 2000 ?
						for (size_t i = 0; i < nmeaParser.Dse.codeFields.size(), i < 2; i++) {
							dscPayload.push_back(nmeaParser.Dse.codeFields.at(i) + 100); // Code byte 
							dscPayload.push_back(nmeaParser.Dse.dataFields.at(i).size() + 2); // Length byte includes length & control byte
							dscPayload.push_back(0x01); // Control Byte, 0x01 = ASCII
							for (auto it : nmeaParser.Dse.dataFields.at(i)) {
								dscPayload.push_back(it);
							}
						}
						// Reset the MMSI number to indicate that we have processed the accompanying DSE sentence
						dseMMSINumber = 0;
						dseTimer->Stop();
						// Transmit the completed PGN 129808 message
						header.pgn = 129808;
						FragmentFastMessage(&header, &dscPayload, canMessages);
						return TRUE;
					}
				}
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

				// EncodePGN129539(&nmeaParser, &payload); GNS DOP
					
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

		// GNS GNSS Fix Data
		else if (nmeaParser.LastSentenceIDReceived == _T("GNS")) {
			if (nmeaParser.Parse()) {
				// Date and Time
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

		// MOB Man Overboard
		else if (nmeaParser.LastSentenceIDReceived == _T("MOB")) {
			if (nmeaParser.Parse()) {
				if (!(supportedPGN & FLAGS_MOB)) {

					if (EncodePGN127233(&nmeaParser, &payload)) {
						header.pgn = 127233;
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
						header.pgn = 127245;
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
					if (aisDecoder->ParseAisMessage(nmeaParser.Vdm, &payload, &header.pgn)) {
						FragmentFastMessage(&header, &payload, canMessages);
					}
					return TRUE;
				}
				else {
					wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
				}
			}
			return FALSE;
		}

		// VDO AIS VHF Data Link Own Vessel Report
		else if (nmeaParser.LastSentenceIDReceived == _T("VDO")) {
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
				// IGNORE

			}
			else {
				wxLogMessage(_T("TwoCan Encoder Parse Error, %s: %s"), sentence, nmeaParser.ErrorMessage);
			}
			return FALSE;
		}

		// WPL Waypoint Location
		else if (nmeaParser.LastSentenceIDReceived == _T("WPL")) {
			if (nmeaParser.Parse()) {
				if ((!(supportedPGN & FLAGS_RTE)) || (enableWaypoint == TRUE)) {
					if (EncodePGN130074(&nmeaParser, &payload)) {
						header.pgn = 130074;
						FragmentFastMessage(&header, &payload, canMessages);
					}

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

								if (engineInstance != -1) {

									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINE#"), &remainingString)) {

										payload.push_back(engineInstance);

										unsigned short oilPressure = USHRT_MAX;
										payload.push_back(oilPressure & 0xFF);
										payload.push_back((oilPressure >> 8) & 0xFF);

										unsigned short oilTemperature = USHRT_MAX;
										payload.push_back(oilTemperature & 0xFF);
										payload.push_back((oilTemperature >> 8) & 0xFF);

										unsigned short engineTemperature = static_cast<unsigned short>(((nmeaParser.Xdr.TransducerInfo[i].MeasurementData + CONST_KELVIN) * 100));
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
					}
					// "T" Tachometer in "R" RPM
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("T")) {
						if (!(supportedPGN & FLAGS_ENG)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("R")) {

								int engineInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;

								if (engineInstance != -1) {
								
									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINE#"), &remainingString)) {
										
										// BUG BUG duplicating code for PGN 127488
										payload.push_back(engineInstance);

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
					}
					
		    
					// "P" Pressure in "P" pascal
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("P")) {
						if (!(supportedPGN & FLAGS_ENG)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("P")) {

								int engineInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;

								if (engineInstance != -1) {

									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINE#"), &remainingString)) {
										
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
								
									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ENGINEOIL#"), &remainingString)) {

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

					}

					// "I" Current in "A" amperes
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("I")) {
						if (!(supportedPGN & FLAGS_BAT)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("A")) {
								
								int batteryInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								wxString remainingString;

								if (batteryInstance != -1) {

									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("BATTERY#"), &remainingString)) {
										
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
					}

					// "U" Voltage in "V" volts
					if (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("U")) {
						if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("V")) {
								
							int batteryInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
							wxString remainingString;

							if (batteryInstance != -1) {
															
								if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("BATTERY#"), &remainingString)) {
									if (!(supportedPGN & FLAGS_BAT)) {
										payload.push_back(batteryInstance & 0xF);

										unsigned short batteryVoltage = static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * 100.0f);
										payload.push_back(batteryVoltage & 0xFF);
										payload.push_back((batteryVoltage >> 8) & 0xFF);

										short batteryCurrent = SHRT_MAX;
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
								
								if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("ALTERNATOR#"), &remainingString)) {
									if (!(supportedPGN & FLAGS_ENG)) {
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
										payload.push_back((fuelRate >> 8) & 0xFF);

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
					}	

					// "V" Volume or "E" Volume - "P" as percent capacity
					if ((nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("V")) || (nmeaParser.Xdr.TransducerInfo[i].TransducerType == _T("E"))) {
						if (!(supportedPGN & FLAGS_TNK)) {
							if (nmeaParser.Xdr.TransducerInfo[i].UnitOfMeasurement == _T("P")) {

								int tankInstance = GetInstanceNumber(nmeaParser.Xdr.TransducerInfo[i].TransducerName);
								byte tankType;
								wxString remainingString;

								if (tankInstance != -1) {

									if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("FUEL#"), &remainingString)) {
										tankType = TANK_FUEL;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith( _T("FRESHWATER#"), &remainingString)) {
										tankType = TANK_FRESHWATER;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("WASTEWATER#"), &remainingString)) {
										tankType = TANK_WASTEWATER;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("LIVEWELL#"), &remainingString)) {
										tankType = TANK_LIVEWELL;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("OIL#"), &remainingString)) {
										tankType = TANK_OIL;
									}

									else if (nmeaParser.Xdr.TransducerInfo[i].TransducerName.StartsWith(_T("BLACKWATER#"), &remainingString)) {
										tankType = TANK_BLACKWATER;
									}

									else {
										// Not a transducer measurement we are interested in
										return FALSE;
									}
		
									payload.push_back((tankInstance & 0x0F) | ((tankType << 4) & 0xF0));

									unsigned short tankLevel = static_cast<unsigned short>(nmeaParser.Xdr.TransducerInfo[i].MeasurementData * QUARTER_PERCENT); // percentage in 0.25 % increments
									payload.push_back(tankLevel & 0xFF);
									payload.push_back((tankLevel >> 8) & 0xFF);

									unsigned int tankCapacity = UINT_MAX;  // Capacity in tenths of litres
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

			wxDateTime epochTime((time_t)0);
			wxDateTime now;
		
			// BUG BUG should add the date time parser to NMEA 183....
			// BUG BUG Note year 3000 bug, as NMEA 183 only supports two digits to represent the year
			now.ParseDateTime(wxString::Format(_T("%s/%s/20%s %s:%s:%s"), 
			parser->Rmc.Date.Mid(0,2), parser->Rmc.Date.Mid(2,2), parser->Rmc.Date.Mid(4,2),
	 		parser->Rmc.UTCTime.Mid(0,2), parser->Rmc.UTCTime.Mid(2,2), parser->Rmc.UTCTime.Mid(4,2)));

			wxTimeSpan dateDiff = now - epochTime;

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

		wxDateTime epochTime((time_t)0);
		wxDateTime now;

		now.ParseDateTime(wxString::Format(_T("%s/%s/20%s %s:%s:%s"), 
		parser->Zda.Day, parser->Zda.Month, parser->Zda.Year,
		parser->Zda.UTCTime.Mid(0,2), parser->Zda.UTCTime.Mid(2,2), parser->Zda.UTCTime.Mid(4,2)));

		wxTimeSpan dateDiff = now - epochTime;

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

			wxDateTime epoch((time_t)0);
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

		wxDateTime epochTime((time_t)0);

		// GGA sentence only has utc time, not date, so assume today
		wxDateTime now = wxDateTime::Now();
		now.SetHour(std::atoi(parser->Gga.UTCTime.Mid(0,2)));
		now.SetMinute(std::atoi(parser->Gga.UTCTime.Mid(2,2)));
		now.SetSecond(std::atoi(parser->Gga.UTCTime.Mid(4,2)));
		
		wxTimeSpan dateDiff = now - epochTime;

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

// Encode payload for PGN 127233 NMEA Man Overboard (MOB)
bool TwoCanEncoder::EncodePGN127233(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	if (parser->LastSentenceIDParsed == _T("MOB")) {
		n2kMessage->clear();

		n2kMessage->push_back(sequenceId);

		unsigned int emitterId = std::atoi(parser->Mob.EmitterID);
		n2kMessage->push_back(emitterId & 0xFF);
		n2kMessage->push_back((emitterId >> 8) & 0xFF);
		n2kMessage->push_back((emitterId >> 16) & 0xFF);
		n2kMessage->push_back((emitterId >> 24) & 0xFF);

		byte mobStatus = parser->Mob.MobStatus;
		n2kMessage->push_back((mobStatus & 0x07) | 0xF8);

		unsigned int hours = std::atoi(parser->Mob.ActivationTime.Mid(0, 2));
		unsigned int minutes = std::atoi(parser->Mob.ActivationTime.Mid(2, 2));
		unsigned int seconds = std::atoi(parser->Mob.ActivationTime.Mid(4, 2));

		unsigned int timeOfDay = ((hours * 3600) + (minutes * 60) + seconds) * 1e4;
		n2kMessage->push_back(timeOfDay & 0xFF);
		n2kMessage->push_back((timeOfDay >> 8) & 0xFF);
		n2kMessage->push_back((timeOfDay >> 16) & 0xFF);
		n2kMessage->push_back((timeOfDay >> 24) & 0xFF);

		byte positionSource = parser->Mob.PositionReference;
		n2kMessage->push_back((positionSource & 0x07) | 0xFE);

		wxDateTime epochTime((time_t)0);
		wxDateTime now = wxDateTime::Now();
		wxTimeSpan diff = now - epochTime;

		unsigned short daysSinceEpoch = diff.GetDays();
		unsigned int secondsSinceMidnight = (unsigned int)(diff.GetSeconds().ToLong() - (diff.GetDays() * 24 * 60 * 60)) * 10000;

		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

		int latitude = parser->Mob.Position.Latitude.Latitude * 1e7;
		if (parser->Mob.Position.Latitude.Northing == South) {
			latitude = -latitude;
		}
		n2kMessage->push_back(latitude & 0xFF);
		n2kMessage->push_back((latitude >> 8) & 0xFF);
		n2kMessage->push_back((latitude >> 16) & 0xFF);
		n2kMessage->push_back((latitude >> 24) & 0xFF);

		int longitude = parser->Mob.Position.Latitude.Latitude * 1e7;
		if (parser->Mob.Position.Longitude.Easting == West) {
			longitude = -longitude;
		}
		n2kMessage->push_back(longitude & 0xFF);
		n2kMessage->push_back((longitude >> 8) & 0xFF);
		n2kMessage->push_back((longitude >> 16) & 0xFF);
		n2kMessage->push_back((longitude >> 24) & 0xFF);

		byte cogReference = 0;
		n2kMessage->push_back((cogReference & 0x02) | 0xFC);

		unsigned short courseOverGround;
		courseOverGround = DEGREES_TO_RADIANS(parser->Mob.CourseOverGround) * 10000;
		n2kMessage->push_back(courseOverGround & 0xFF);
		n2kMessage->push_back((courseOverGround >> 8) & 0xFF);

		unsigned short speedOverGround;
		speedOverGround = (parser->Mob.SpeedOverGround / CONVERT_MS_KNOTS) * 100;
		n2kMessage->push_back(speedOverGround & 0xFF);
		n2kMessage->push_back((speedOverGround >> 8) & 0xFF);

		unsigned int mmsiNumber = std::atoi(parser->Mob.mmsiNumber);
		n2kMessage->push_back(mmsiNumber & 0xFF);
		n2kMessage->push_back((mmsiNumber >> 8) & 0xFF);
		n2kMessage->push_back((mmsiNumber >> 16) & 0xFF);
		n2kMessage->push_back((mmsiNumber >> 24) & 0xFF);

		byte batteryStatus = parser->Mob.BatteryStatus;
		n2kMessage->push_back((batteryStatus & 0x07) | 0xF8);

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

		wxDateTime epochTime((time_t)0);
		wxDateTime now = wxDateTime::Now();

		wxTimeSpan dateDiff = now - epochTime;

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
		wxDateTime epochTime((time_t)0);
		wxDateTime now = wxDateTime::Now();

		wxTimeSpan dateDiff = now - epochTime;

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
	
		wxDateTime epochTime((time_t)0);
		wxDateTime now;

		now.ParseDateTime(parser->Gga.UTCTime);
		wxTimeSpan dateDiff = now - epochTime;

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
	return FALSE;
}

// Encode payload for PGN 129033 NMEA Date & Time
bool TwoCanEncoder::EncodePGN129033(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {
	if (parser->LastSentenceIDParsed == _T("ZDA")) {
		n2kMessage->clear();

		wxDateTime epochTime((time_t)0);
		wxDateTime now;

		now.ParseDateTime(wxString::Format(_T("%d/%d/20%d %s:%s:%s"), 
			parser->Zda.Day, parser->Zda.Month, parser->Zda.Year,
	 		parser->Zda.UTCTime.Mid(0,2), parser->Zda.UTCTime.Mid(2,2), parser->Zda.UTCTime.Mid(4,2)));

		wxTimeSpan dateDiff = now - epochTime;

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

	if (parser->LastSentenceIDParsed == _T("RMB")) {

		if (parser->Rmb.IsDataValid == NTrue) {
	
			n2kMessage->push_back(sequenceId);

			unsigned short distance = 100 * parser->Rmb.RangeToDestinationNauticalMiles/ CONVERT_METRES_NAUTICAL_MILES;
			n2kMessage->push_back(distance & 0xFF);
			n2kMessage->push_back((distance >> 8) & 0xFF);
		
			byte bearingRef = HEADING_TRUE;

			byte perpendicularCrossed = 0;
	
			byte circleEntered = parser->Rmb.IsArrivalCircleEntered == NTrue ? 1 : 0;

			byte calculationType = 0;
	
			n2kMessage->push_back(bearingRef | (perpendicularCrossed << 2) | (circleEntered << 4) | (calculationType << 6));

			wxDateTime epochTime((time_t)0);
			wxDateTime now = wxDateTime::Now();
			
			wxTimeSpan diff = now - epochTime;

			unsigned short daysSinceEpoch = diff.GetDays();
			unsigned int secondsSinceMidnight = ((diff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

			n2kMessage->push_back(secondsSinceMidnight & 0xFF);
			n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
			n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
			n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

			n2kMessage->push_back(daysSinceEpoch & 0xFF);
			n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

			unsigned short bearingOrigin = USHRT_MAX;
			n2kMessage->push_back(bearingOrigin & 0xFF);
			n2kMessage->push_back((bearingOrigin >> 8) & 0xFF);

			unsigned short bearingPosition = 1000 * DEGREES_TO_RADIANS(parser->Rmb.BearingToDestinationDegreesTrue);
			n2kMessage->push_back(bearingPosition & 0xFF);
			n2kMessage->push_back((bearingPosition >> 8) & 0xFF);

			wxString originWaypointId = parser->Rmb.From;
			// BUG BUG Need to get the waypointId
			n2kMessage->push_back(0xFF);
			n2kMessage->push_back(0xFF);
			n2kMessage->push_back(0xFF);
			n2kMessage->push_back(0xFF);

			wxString destinationWaypointId = parser->Rmb.To;
			n2kMessage->push_back(0xFF);
			n2kMessage->push_back(0xFF);
			n2kMessage->push_back(0xFF);
			n2kMessage->push_back(0xFF);

			int latitude = parser->Rmb.DestinationPosition.Latitude.Latitude * 1e7;
			if (parser->Rmb.DestinationPosition.Latitude.Northing == NORTHSOUTH::South) {
				latitude = -latitude;
			}
			n2kMessage->push_back(latitude & 0xFF);
			n2kMessage->push_back((latitude >> 8) & 0xFF);
			n2kMessage->push_back((latitude >> 16) & 0xFF);
			n2kMessage->push_back((latitude >> 24) & 0xFF);
			
			int longitude = parser->Rmb.DestinationPosition.Longitude.Longitude * 1e7;
			if (parser->Rmb.DestinationPosition.Longitude.Easting == EASTWEST::West) {
				longitude = -longitude;
			}

			n2kMessage->push_back(longitude & 0xFF);
			n2kMessage->push_back((longitude >> 8) & 0xFF);
			n2kMessage->push_back((longitude >> 16) & 0xFF);
			n2kMessage->push_back((longitude >> 24) & 0xFF);

			unsigned short waypointClosingVelocity = 100 * parser->Rmb.DestinationClosingVelocityKnots/CONVERT_MS_KNOTS;
			n2kMessage->push_back(waypointClosingVelocity & 0xFF);
			n2kMessage->push_back((waypointClosingVelocity >> 8) & 0xFF);
		
			return TRUE;
		}
	}
	
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

	if (parser->LastSentenceIDParsed == _T("DSC")) {

		// Field 1. Format Specifier
		// 
		byte formatSpecifier = parser->Dsc.formatSpecifer;
		n2kMessage->push_back(formatSpecifier + 100);

		// Field 2. Categeory
		// If Format Specifier is Distress, DSC Category is 0xFF
		// Otherwise DSC Category is one of: Distress = 112, Routine = 100, Safety = 108  = Safety, Urgency = 110
		byte dscCategory;
		if (formatSpecifier == (byte)DSC_FORMAT_SPECIFIER::DISTRESS) {
			dscCategory = 0xFF;
			n2kMessage->push_back(dscCategory);
		}
		else {
			dscCategory = parser->Dsc.category;
			n2kMessage->push_back(dscCategory + 100);
		}
		
		// Field 3. MMSI (or geographic area), 
		// Note MMSI addresses which are 9 digits, have a trailing zero for both NMEA 183 & NMEA 2000
		// Similarly if Format Specifier is GEOGRAPHIC AREA, MMSI Address is a position (10 digits)
		// If ALLSHIPS, MMSI Address is filled with 0xFF
		std::string sourceAddress = std::to_string(parser->Dsc.mmsiNumber);
		if (formatSpecifier == (byte)DSC_FORMAT_SPECIFIER::ALLSHIPS) {
			for (size_t i = 0; i < 5; i++) {
				n2kMessage->push_back(0xFF);
			}
		}
		else {
			for (size_t i = 0; i < 5; i++) {
				// BUG BUG This displays correctly on B&G but seems to differ from the spec. Nees to be tested against other MFD's
				n2kMessage->push_back(strtol(sourceAddress.substr(i * 2, 2).data(), NULL, 10));
			}
		}

		// Field 4. 
		// If the Format Specifier is a Distress, then this field is nature of distress
		// otherwise it is the first telecommand  

		byte firstTelecommand = parser->Dsc.natureOfDistressOrFirstTelecommand;
		n2kMessage->push_back(firstTelecommand + 100);

		// Field 5. 
		// If the Format Specifier is a Distress, then this field is the 
		// proposed telecommunications Mode
		// If the Format Specifier is All Ships and the  Category is Distress 
		// this is the Nature of Distress
		byte secondTeleCommand;
		if (formatSpecifier == (byte)DSC_FORMAT_SPECIFIER::DISTRESS) {
			secondTeleCommand = parser->Dsc.subsequentCommunicationsOrSecondTelecommand;
		}
		else if (formatSpecifier == (byte)DSC_FORMAT_SPECIFIER::ALLSHIPS) {
			if (dscCategory == (byte)DSC_CATEGORY::CAT_DISTRESS) {
				secondTeleCommand = parser->Dsc.relayNatureOfDistress;
			}
		}
		
		n2kMessage->push_back(secondTeleCommand);


		if (parser->Dsc.positionOrFrequency.Length() == 12) {

			// Field 6 - Proposed Communications Frequencies
			// 1,2,3 MF/HF frequency encoded in 100Hz.
			//9 VHF - second digit simplex, 

			std::string receiveFrequency = parser->Dsc.positionOrFrequency.SubString(0, 6).ToStdString();
			for (std::string::iterator it = receiveFrequency.begin(); it != receiveFrequency.end(); ++it) {
				n2kMessage->push_back(*it);
			}

			std::string transmitFrequency = parser->Dsc.positionOrFrequency.SubString(6, 6).ToStdString();
			for (std::string::iterator it = transmitFrequency.begin(); it != transmitFrequency.end(); ++it) {
				n2kMessage->push_back(*it);
			}

		}
		else {
			for (int i = 0; i < 12; i++) {
				n2kMessage->push_back(0xFF);
			}
		}

		// Field 8 - Telephone Number 8 or 10 digits ??
		if (parser->Dsc.timeOrTelephone.Length() != 4) {
			std::string telephoneNumber = parser->Dsc.timeOrTelephone.ToStdString();
			// Encoded String with Length & Encoding byte
			n2kMessage->push_back(telephoneNumber.length() + 2);
			n2kMessage->push_back(0x01); // indicate ASCII encoding
			for (std::string::iterator it = telephoneNumber.begin(); it != telephoneNumber.end(); ++it) {
				n2kMessage->push_back(*it);
			}
		}
		else {
			n2kMessage->push_back(0x02); // Length & Encoding byte
			n2kMessage->push_back(0x01);
		}

		if (parser->Dsc.positionOrFrequency.Length() == 10) {
			int quadrant = std::atoi(parser->Dsc.positionOrFrequency.SubString(0, 1));

			// Field 9
			int latitudeDegrees = std::atoi(parser->Dsc.positionOrFrequency.Mid(1, 2));
			int latitudeMinutes = std::atoi(parser->Dsc.positionOrFrequency.Mid(3, 2));
			double latitudeDouble = (double)latitudeDegrees + ((double)latitudeMinutes / 60);
			int latitude = ((double)latitudeDegrees + ((double)latitudeMinutes / 60)) * 1e7; //latitudeDouble * 1e7;

			// Field 10
			int longitudeDegrees = std::atoi(parser->Dsc.positionOrFrequency.Mid(5, 3));
			int longitudeMinutes = std::atoi(parser->Dsc.positionOrFrequency.Mid(8, 2));
			double longitudeDouble = (double)longitudeDegrees + ((double)longitudeMinutes / 60);
			int longitude = ((double)longitudeDegrees + ((double)longitudeMinutes / 60)) * 1e7;//longitudeDouble * 1e7;

			switch (quadrant) {
				case 0: // North East
					break;
				case 1: // North West
					longitude = -longitude;
					break;
				case 2: // South East
					latitude = -latitude;
					break;
				case 3: // South West
					latitude = -latitude;
					longitude = -longitude;
					break;
			}

			n2kMessage->push_back(latitude & 0xFF);
			n2kMessage->push_back((latitude >> 8) & 0xFF);
			n2kMessage->push_back((latitude >> 16) & 0xFF);
			n2kMessage->push_back((latitude >> 24) & 0xFF);
						
			n2kMessage->push_back(longitude & 0xFF);
			n2kMessage->push_back((longitude >> 8) & 0xFF);
			n2kMessage->push_back((longitude >> 16) & 0xFF);
			n2kMessage->push_back((longitude >> 24) & 0xFF);

		}
		else {
			for (int i = 0; i < 8; i++) {
				n2kMessage->push_back(0xFF);
			}
		}

		// Field 11 - Time of transmission
		if (parser->Dsc.timeOrTelephone.Length() == 4) {
			if (parser->Dsc.timeOrTelephone != "8888") {

				unsigned int secondsSinceMidnight = ((std::atoi(parser->Dsc.timeOrTelephone.SubString(0, 2)) * 3600) +
					(std::atoi(parser->Dsc.timeOrTelephone.SubString(2, 2)) * 60)) * 10000;

				n2kMessage->push_back(secondsSinceMidnight & 0xFF);
				n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
				n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
				n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

			}
			else {
				for (int i = 0; i < 4; i++) {
					n2kMessage->push_back(0xFF);
				}
			}
		}
		
		// Field 12 _Should always contain the MMSI of the vessel in distress
		// BUG BUG B&G Fails to display if this is set as per the spec ?
		std::string Address = std::to_string(parser->Dsc.mmsiNumber);
		if ((formatSpecifier == (byte)DSC_FORMAT_SPECIFIER::DISTRESS) || (dscCategory == (byte)DSC_CATEGORY::CAT_DISTRESS)) {
			for (size_t i = 0; i < 5; i++) {
				//n2kMessage->push_back(strtol(sourceAddress.substr(i * 2, 2).data(), NULL, 10));
				n2kMessage->push_back(0xFF);
			}

		}
		else {
			for (size_t i = 0; i < 5; i++) {
				n2kMessage->push_back(0xFF);
			}
		}


		// Field 13 - End of Sequence
		byte endOfSequence;
		if (parser->Dsc.ack == 'R') {
			endOfSequence = 117;
		}
		else if (parser->Dsc.ack == 'B') {
			endOfSequence = 122;
		}
		else if (parser->Dsc.ack == 'S') {
			endOfSequence = 127;
		}
		else {
			endOfSequence = 127;
		}

		n2kMessage->push_back(endOfSequence);

		// Field 14 DSE follows & Field 15 (reserved)
		byte dscExpansionEnabled = parser->Dsc.dseExpansion;
		n2kMessage->push_back(dscExpansionEnabled | 0xFC);

		// I guess the following are filled in by the radio.

		// Field 16 - No idea what this is, presumably the DSC VHF Channel 70 or HF Frequencies 2187.5 kHz or 8414.5 
		// "900016";
		std::string callingFrequency = parser->Dsc.positionOrFrequency.ToStdString();
		for (std::string::iterator it = callingFrequency.begin(); it != callingFrequency.end(); ++it) {
			n2kMessage->push_back(0xFF); // Default to Data Unavailable
		}

		// Field 17
		std::string receivingFrequency = parser->Dsc.positionOrFrequency.ToStdString();
		for (std::string::iterator it = receivingFrequency.begin(); it != receivingFrequency.end(); ++it) {
			n2kMessage->push_back(0xFF); // Default to Data Unavailable
		}

		// We'll just use now as the date & time of receipt
		wxDateTime epochTime((time_t)0);
		wxDateTime now = wxDateTime::Now();

		wxTimeSpan diff = now - epochTime;

		unsigned short daysSinceEpoch = diff.GetDays();
		unsigned int secondsSinceMidnight = ((diff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

		// Field 18 - Time of receipt
		n2kMessage->push_back(secondsSinceMidnight & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 8) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 16) & 0xFF);
		n2kMessage->push_back((secondsSinceMidnight >> 24) & 0xFF);

		// Field 19 - Date of receipt
		n2kMessage->push_back(daysSinceEpoch & 0xFF);
		n2kMessage->push_back((daysSinceEpoch >> 8) & 0xFF);

		// Field 20 - DSC Equipment Message ID
		unsigned short messageId = USHRT_MAX;
		n2kMessage->push_back(messageId & 0xFF);
		n2kMessage->push_back((messageId >> 8) & 0xFF);

		// If there is a DSE sentence to follow, we copy this payload and wait for the DSE sentence to arrive,
		// add the remaining bytes and send
		if (parser->Dsc.dseExpansion == NMEA0183_BOOLEAN::NTrue) {
			dseMMSINumber = parser->Dsc.mmsiNumber;
			dseTimer->Start(2 * CONST_ONE_SECOND, wxTIMER_ONE_SHOT);
			// Make a copy of the vector and wait till the corresponding DSE sentence is processed
			dscPayload.clear();
			dscPayload = *n2kMessage;
			return FALSE;
		}
		else {
			// Fill out the following fields with "no data"
			// The following pairs are repeated DSE Expansion fields
			// Field 21
			// DSE Expansion Field Symbol
			byte dseExpansionFieldSymbol = 0xFF;
			n2kMessage->push_back(dseExpansionFieldSymbol);

			// Field 22
			n2kMessage->push_back(0x02); // Length of data includes length byte & encoding byte
			n2kMessage->push_back(0x01); // 01 = ASCII


			// Field 23
			dseExpansionFieldSymbol = 0xFF;
			n2kMessage->push_back(dseExpansionFieldSymbol);

			// Field 24
			n2kMessage->push_back(0x02);
			n2kMessage->push_back(0x01);

			return TRUE;
		}
	}
	return FALSE;

}

// Encode payload for PGN030306 NMEA Waypoint Location
bool TwoCanEncoder::EncodePGN130074(const NMEA0183 *parser, std::vector<byte> *n2kMessage) {

	if (parser->LastSentenceIDParsed == _T("WPL")) {

		unsigned short startingWaypointId = 0;
		n2kMessage->push_back(startingWaypointId & 0xFF);
		n2kMessage->push_back((startingWaypointId >> 8) & 0xFF);

		unsigned short items = 1;
		n2kMessage->push_back(items & 0xFF);
		n2kMessage->push_back((items << 8) & 0xFF);

		unsigned short validItems = 1;
		n2kMessage->push_back(validItems & 0xFF);
		n2kMessage->push_back((validItems << 8) & 0xFF);

		unsigned short databaseId = 0;
		n2kMessage->push_back(databaseId & 0xFF);
		n2kMessage->push_back((databaseId >> 8) & 0xFF);

		// reserved;
		n2kMessage->push_back(0xFF);
		n2kMessage->push_back(0xFF);

		// We'll make an id using the waypoint name
		// Wouldn't it be nice if there was a unique GUID ?
		unsigned short waypointId;
		unsigned short pair1 = 0;
		unsigned short pair2 = 0;
		for (unsigned int i = 0; i < parser->Wpl.To.size(); i++) {
			pair1 = pair1 ^ (unsigned short)parser->Wpl.To[i];
		}
		for (int i = parser->Wpl.To.size() - 1; i >= 0; i--) {
			pair2 = pair2 ^ (unsigned short)parser->Wpl.To[i];
		}
		waypointId = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;

		n2kMessage->push_back(waypointId & 0xFF);
		n2kMessage->push_back((waypointId << 8) & 0xFF);

		// Text with length & control byte
		n2kMessage->push_back(parser->Wpl.To.size() + 2);
		n2kMessage->push_back(0x01); // First byte of Waypoint Name indicates ASCII or UUnicode encoding
		for (auto it = parser->Wpl.To.begin(); it != parser->Wpl.To.end(); ++it) {
			n2kMessage->push_back(*it);
		}

		int latitude = parser->Wpl.Position.Latitude.Latitude * 1e7;

		if (parser->Wpl.Position.Latitude.Northing == South) {
			latitude = -latitude;
		}
		n2kMessage->push_back(latitude & 0xFF);
		n2kMessage->push_back((latitude >> 8) & 0xFF);
		n2kMessage->push_back((latitude >> 16) & 0xFF);
		n2kMessage->push_back((latitude >> 24) & 0xFF);

		int longitude = parser->Wpl.Position.Longitude.Longitude * 1e7;

		if (parser->Wpl.Position.Longitude.Easting == West) {
			longitude = -longitude;
		}
		n2kMessage->push_back(longitude & 0xFF);
		n2kMessage->push_back((longitude >> 8) & 0xFF);
		n2kMessage->push_back((longitude >> 16) & 0xFF);
		n2kMessage->push_back((longitude >> 24) & 0xFF);

		return TRUE;
	}
	return FALSE;
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

		unsigned short waterTemperature = static_cast<unsigned short>((parser->Mtw.Temperature + CONST_KELVIN) * 100);

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
