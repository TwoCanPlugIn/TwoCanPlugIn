// Copyright(C) 2018-2022 by Steven Adler
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
// Unit: Autoplot Control
// Owner: twocanplugin@hotmail.com
// Date: 20/05/2022
// Version History: 
// 1.0 Initial Release of Autopilot Control - Not actually exposed or used yet....

#include "twocanautopilot.h"

// Raymarine PGN 126208, priority high
// These must be used to set the autopilot mode & heading


//    "route":   "%s,3,126208,%s,%s,17,01,63,ff,00,f8,04,01,3b,07,03,04,04,80,01,05,ff,ff",
//    "standby": "%s,3,126208,%s,%s,17,01,63,ff,00,f8,04,01,3b,07,03,04,04,00,00,05,ff,ff"

/*
const keys_code = {
    "+1":      "07,f8",
    "+10":     "08,f7",
    "-1":      "05,fa",
    "-10":     "06,f9",
    "-1-10":   "21,de",
    "+1+10":   "22,dd"
}
*/

//const key_command = "%s,7,126720,%s,%s,22,3b,9f,f0,81,86,21,%s,ff,ff,ff,ff,ff,c1,c2,cd,66,80,d3,42,b1,c8"
//const heading_command = "%s,3,126208,%s,%s,14,01,50,ff,00,f8,03,01,3b,07,03,04,06,%s,%s"
//const wind_direction_command = "%s,3,126208,%s,%s,14,01,41,ff,00,f8,03,01,3b,07,03,04,04,%s,%s"
//const raymarine_ttw_Mode = "%s,3,126208,%s,%s,17,01,63,ff,00,f8,04,01,3b,07,03,04,04,81,01,05,ff,ff"
//const raymarine_ttw = "%s,3,126208,%s,%s,21,00,00,ef,01,ff,ff,ff,ff,ff,ff,04,01,3b,07,03,04,04,6c,05,1a,50"
//const confirm_tack = "%s,2,126720,%s,%s,7,3b,9f,f0,81,90,00,03"

//const keep_alive = "%s,7,65384,%s,255,8,3b,9f,00,00,00,00,00,00"
//const keep_alive2 = "%s,7,126720,%s,255,7,3b,9f,f0,81,90,00,03"


const std::vector<byte> raymarineStandby = {0x17, 0x01, 0x63, 0xff, 0x00, 0xf8, 0x04, 0x01, 0x3b, 0x07, 0x03, 0x04, 0x04, 0x00, 0x00, 0x05, 0xff, 0xff}; 
const std::vector<byte> raymarineAuto = {0x17, 0x01, 0x63, 0xff, 0x00, 0xf8, 0x04, 0x01, 0x3b, 0x07, 0x03, 0x04, 0x04, 0x40, 0x00, 0x05, 0xff, 0xff}; 
const std::vector<byte> raymarineTrack = {0x0d, 0x3b, 0x9f, 0xf0, 0x81, 0x84, 0x46, 0x27, 0x9d, 0x4a, 0x00, 0x00, 0x02, 0x08, 0x4e}; 

// Note last two bytes represent heading as radians * 0.0001
const std::vector<byte>raymarineHeading = {0x14, 0x01, 0x50, 0xff, 0x00, 0xf8, 0x03, 0x01, 0x3b, 0x07, 0x03, 0x04, 0x06, 0x00, 0x00};

// TwoCan Autopilot
TwoCanAutopilot::TwoCanAutopilot(AUTOPILOT_MODEL manufacturer) {
    // Save the autopilot model, 0 - None, 1 - Garmin, 2 - Navico, 3 - Raymarine
    autopilotModel = manufacturer;
}

TwoCanAutopilot::~TwoCanAutopilot(void) {
}

bool TwoCanAutopilot::EncodeAutopilotCommand(wxString message_body, std::vector<CanMessage> *nmeaMessages) {
    CanMessage message;
    CanHeader header;

	wxJSONValue root;
	wxJSONReader reader;
	if (reader.Parse(message_body, &root) > 0) {
		// BUG BUG should log errors, but as I'm generating the JSON, there shouldn't be any,
		// however another developer could write a better controller using different steering algorithms
		wxLogMessage("TwoCan Plugin Autopilot, JSON Error in following");
		wxLogMessage("%s", message_body);
		wxArrayString jsonErrors = reader.GetErrors();
		for (auto it : jsonErrors) {
			wxLogMessage(it);
		}
		return FALSE;
	}
	else {
		int commandValue;
		int commandId;
		if (root["autopilot"].HasMember("status")) {
			commandId = AUTOPILOT_CHANGE_STATUS;
			commandValue = root["autopilot"]["state"].AsInt();
			wxMessageBox(wxString::Format("AUTOPILOT STATUS CHANGE: %d", commandValue), "TwoCanPlugin Debug");
		}
		
		else if (root["autopilot"].HasMember("heading")) {
			commandId = AUTOPILOT_CHANGE_COURSE;
			commandValue = root["autopilot"]["heading"].AsInt();
			wxMessageBox(wxString::Format("AUTOPILOT COURSE CHANGE: %d", commandValue), "TwoCanPlugin Debug");
		}

		else if (root["autopilot"].HasMember("manufacturer")) {
			commandId = AUTOPILOT_CHANGE_MANUFACTURER;
			commandValue = root["autopilot"]["manufacturer"].AsInt();
			autopilotModel = (AUTOPILOT_MODEL)commandValue;
			wxMessageBox(wxString::Format("AUTOPILOT MANUFACTURER CHANGE: %d", commandValue), "TwoCanPlugin Debug");
		}

		else {
			// Not a command we are interested in
			wxLogMessage(_T("TwoCan Plugin, Invalid Autopilot Request: %s"), message_body);
			return FALSE;
		}

		// Now parse the commands and generate the approprite PGN's
		switch (autopilotModel) {
		case AUTOPILOT_MODEL::GARMIN:
				switch (commandId) {
					case AUTOPILOT_CHANGE_STATUS:
						if (commandValue == AUTOPILOT_POWER_OFF) {
						// Turn Off 
						}
						else if (commandValue == AUTOPILOT_POWER_STANDBY) {
						// Standby
						}
						if (commandValue == AUTOPILOT_MODE_HEADING) {
						// Heading Mode 
						}
						else if (commandValue == AUTOPILOT_MODE_WIND) {
						// Wind mode
						}
						else if (commandValue == AUTOPILOT_MODE_GPS) {
						// GPS mode
						}
						break;
					case AUTOPILOT_CHANGE_COURSE:
						// Adjust Heading
						break;
				} // end switch command
				break; // end case garmin
		
		case AUTOPILOT_MODEL::RAYMARINE:
				switch (commandId) {
					case AUTOPILOT_CHANGE_STATUS:
						if (commandValue == AUTOPILOT_POWER_OFF) {
							// Turn Off 
						}
						else if (commandValue == AUTOPILOT_POWER_STANDBY) {
							// Set to Standby
							header.pgn = 0;
							header.destination = CONST_GLOBAL_ADDRESS;
							header.priority = CONST_PRIORITY_VERY_HIGH;
							header.source = networkAddress;
							message.header = header;
							message.payload = raymarineStandby;
							nmeaMessages->push_back(message);
						}
						
						else if (commandValue == AUTOPILOT_MODE_HEADING) {
							// set to Compass Heading Mode
							header.pgn = 0;
							header.destination = CONST_GLOBAL_ADDRESS;
							header.priority = CONST_PRIORITY_VERY_HIGH;
							header.source = networkAddress;
							message.header = header;
							message.payload = raymarineHeading;
							nmeaMessages->push_back(message);
						}

						else if (commandValue == AUTOPILOT_MODE_WIND) {
							// set to Wind Mode
						}

						else if (commandValue == AUTOPILOT_MODE_GPS) {
							// set to GPS mode
							header.pgn = 0;
							header.destination = CONST_GLOBAL_ADDRESS;
							header.priority = CONST_PRIORITY_VERY_HIGH;
							header.source = networkAddress;
							message.header = header;
							message.payload = raymarineTrack;
							nmeaMessages->push_back(message);
						}
						break;
					case AUTOPILOT_CHANGE_COURSE:
						// Adjust Heading
						break;
				} // end switch command
				break; // end case raymarine

		case AUTOPILOT_MODEL::NAVICO_NAC3:
				switch (commandId) {
					case AUTOPILOT_CHANGE_STATUS:
						if (commandValue == AUTOPILOT_POWER_OFF) {
							// Turn Off 
						}
						else if (commandValue == AUTOPILOT_POWER_STANDBY) {
							// set to Standby
						}
						else if (commandValue == AUTOPILOT_MODE_HEADING) {
							// Heading Mode 
						}
						else if (commandValue == AUTOPILOT_MODE_WIND) {
							// Wind mode
						}
						else if (commandValue == AUTOPILOT_MODE_GPS) {
							// GPS mode
						}
						break;
					case AUTOPILOT_CHANGE_COURSE:
						// Adjust Heading
						break;
				} // end switch command
				break; // end case navico

		case AUTOPILOT_MODEL::FURUNO:
				switch (commandId) {
					case AUTOPILOT_CHANGE_STATUS:
						if (commandValue == AUTOPILOT_POWER_OFF) {
							// Turn Off 
						}
						else if (commandValue == AUTOPILOT_POWER_STANDBY) {
							// set to Standby
						}
						else if (commandValue == AUTOPILOT_MODE_HEADING) {
							// Heading Mode 
						}
						else if (commandValue == AUTOPILOT_MODE_WIND) {
							// Wind mode
						}
						else if (commandValue == AUTOPILOT_MODE_GPS) {
							// GPS mode
						}
						break;
					case AUTOPILOT_CHANGE_COURSE:
						// Adjust Heading
						break;
				} // end switch command
				break; // end furuno

		default:
			return FALSE;
			break;
		} // end switch manufacturer

	}
return TRUE;    
}

// Simnet Autopilot Command
void EncodePGN130850(int command) {
	std::vector<byte>payload;
	int controllingDevice; //Is this the network address of the controlling device
	int eventId;


	//,41,9f,%s,ff,ff,%s,00,00,00,ff
	payload.push_back(0x41); // This is the usual manufacturer code/industry code
	payload.push_back(0x9F);
	payload.push_back(command);
	payload.push_back(0xFF); //unknown
	payload.push_back(controllingDevice);
	payload.push_back(eventId);
	payload.push_back(0xFF); // Mode
	payload.push_back(0xFF); // Mode
	payload.push_back(0xFF); // direction
	payload.push_back(0xFF); // angle
	payload.push_back(0xFF); // angle
	payload.push_back(0xFF); // unknown
}

// Simnet Autopilot Mode
void EncodePGN65341(void) {

}

// Simnet Wind Angle
void EncodePGN65431(int windAngle) {
	std::vector<byte>payload;

	payload.push_back(0x41); // This is the usual manufacturer code/industry code
	payload.push_back(0x9F);
	payload.push_back(0xFF);
	payload.push_back(0xFF);
	payload.push_back(0x03); // command
	payload.push_back(0xFF);
	payload.push_back(windAngle & 0xFF); // wind angle to be expressed in tenths of radians
	payload.push_back((windAngle >> 8) & 0xFF);

}
	
/* PGN 127237 is Heading/Track Control
function AC12_PGN127237 () {
  const heading_track_pgn = {
	  "navigation" : "%s,2,127237,%s,%s,15,ff,3f,ff,ff,7f,%s,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s", - course to steer, heading
	  "headinghold": "%s,2,127237,%s,%s,15,ff,7f,ff,ff,7f,%s,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s", locked heading/heading
	  "wind":        "%s,2,127237,%s,%s,15,ff,7f,ff,ff,7f,ff,ff,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s", locked heading, heading
	  "standby":     "%s,2,127237,%s,%s,15,ff,7f,ff,ff,7f,ff,ff,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s" // Magnetic
	  // "standby":  "%s,2,127237,%s,%s,15,ff,3f,ff,ff,7f,ff,ff,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s" // True
  }

  // These are possibly heartbeats

  const pgn65340 = {
	  "standby":     "%s,3,65340,%s,255,8,41,9f,00,00,fe,f8,00,80",
	  "headinghold": "%s,3,65340,%s,255,8,41,9f,10,01,fe,fa,00,80", // Heading Hold
	  "followup":    "%s,3,65340,%s,255,8,41,9f,10,03,fe,fa,00,80", // Follow up
	  "wind":        "%s,3,65340,%s,255,8,41,9f,10,03,fe,fa,00,80",
	  "navigation":  "%s,3,65340,%s,255,8,41,9f,10,06,fe,f8,00,80"
  }
  const pgn65302 = {
	  "standby":    "%s,7,65302,%s,255,8,41,9f,0a,6b,00,00,00,ff",
	  "headinghold":"%s,7,65302,%s,255,8,41,9f,0a,69,00,00,28,ff", // Heading Hold
	  "followup":   "%s,7,65302,%s,255,8,41,9f,0a,69,00,00,30,ff", // Follow up
	  "wind":       "%s,7,65302,%s,255,8,41,9f,0a,69,00,00,30,ff",
	  "navigation": "%s,7,65302,%s,255,8,41,9f,0a,6b,00,00,28,ff"  // guessing
  }

*/