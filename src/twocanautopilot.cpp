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
// Date: 30/06/2022
// Version History: 
// 1.0 Initial Release of Autopilot Control.

#include "twocanautopilot.h"

// TwoCan Autopilot
TwoCanAutoPilot::TwoCanAutoPilot(AUTOPILOT_MODEL model) {
	//BUG BUG Not sure why I am passing the autopilot model.
	// BUG BUG Need to persist and/or detect
	autopilotControllerAddress = 3;
}

TwoCanAutoPilot::~TwoCanAutoPilot(void) {
}

// Some models of autopilots require the autopilot network address to be included
// in the data. Obtain the network address by iterating through the network map.
// The list of product numbers however needs to be updated.
bool TwoCanAutoPilot::FindAutopilot(void) {
	bool result = false;
	autopilotControllerAddress = 254; // In valid Address
	for (int i = 0; i < CONST_MAX_DEVICES; i++) {
		switch (networkMap[i].productInformation.productCode) {
			case 18846: // Simrad AC12
				result = true;
				break;
			case 25576: // Simrad NAC3
				result = true;
				break;
			case 67890: // Raymarine Evo
				result = true;
				break;
			case 2545: // Garmin Reactor
				result = true;
				break;
			default:
				result = false;
				break;
		}

		if (result) {
			autopilotControllerAddress = i;
			break;
		}
	}
	return result;
}


// Raymarine Evolution Autopilot Heading, PGN's 65359 (Heading) and 65360 Locked Heading
// Parse the PGN, convert to JSON and send via OCPN messaging so that the UI dialog can be synchronized with the A/P state
bool TwoCanAutoPilot::DecodeRaymarineAutopilotHeading(const int pgn, const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	byte sid;
	sid = payload[2];

	unsigned short headingTrue;
	headingTrue = payload[3] | (payload[4] << 8);

	unsigned short headingMagnetic;
	headingMagnetic = payload[5] | (payload[6] << 8);

	// BUG BUG Do I need to worry if it is a heading or locked heading ??
	if (pgn == 65359) {
		root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE;
		root["autopilot"]["heading"]["trueheading"] = RADIANS_TO_DEGREES((float)headingTrue / 10000);
		root["autopilot"]["heading"]["heading"] = RADIANS_TO_DEGREES((float)headingMagnetic / 10000);
	}
	if (pgn == 65360) {
		root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE;
		root["autopilot"]["targetheading"]["trueheading"] = RADIANS_TO_DEGREES((float)headingTrue / 10000);
		root["autopilot"]["targetheading"]["heading"] = RADIANS_TO_DEGREES((float)headingMagnetic / 10000);
	}

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}

	return FALSE;
}

// Raymarine Evolution Autopilot Wind
// PGN 65345
bool TwoCanAutoPilot::DecodeRaymarineAutopilotWind(const int pgn, const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	short windAngle;
	windAngle = payload[2] | (payload[3] << 8);

	root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE;
	root["autopilot"]["windangle"] = RADIANS_TO_DEGREES(windAngle * 1e-4);

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}
	
	return FALSE;

}

// Raymarine Evolution Autopilot Mode
// PGN 65379
bool TwoCanAutoPilot::DecodeRaymarineAutopilotMode(const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;
	
	unsigned short pilotMode;
	pilotMode = payload[2] | (payload[3] << 8);

	unsigned short pilotSubMode;
	pilotSubMode = payload[4] | (payload[5] << 8);

	int mode;
	switch (pilotMode) {
		case 0x100:
			mode = AUTOPILOT_MODE::WIND;
		break;
		case 0x40:
			mode = AUTOPILOT_MODE::COMPASS;
		break;
		case 0x181:
			mode = AUTOPILOT_MODE::NODRIFT; // Not implemented in UI
		break;
		case 0x180:
			mode = AUTOPILOT_MODE::NAV;
		break;
		case 0x0:
		default:
			mode = AUTOPILOT_MODE::STANDBY;
		break;
	}

	root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE;
	root["autopilot"]["mode"] = mode;

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}

	return FALSE;
}

// Rayarine encodes seatalk datagrams inside PGN 126720
bool TwoCanAutoPilot::DecodeRaymarineSeatalk(const byte *payload, wxString *jsonResponse) {
	// Only decode a small subset of the Seatalk Datagrams
	// Not sure why we're doing this as the values are also sent in the three above PGN's
	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	// BUG BUG Would be nice to know what these values represent
	// Perhaps indicates Seatalk <-> SeatalkNG conversion ??
	if ((payload[2] == 0xF0) && (payload[3] == 0x81)) {

		byte seatalkCommand;
		seatalkCommand = payload[4];

		// refer to Thomas Knauff Seatalk documentation
		switch (seatalkCommand) {
			case 0x84: // Compass Heading, Autopilot Course and Rudder Position
				{
				char u, vw, xy, z, m, rr, ss, tt;

				u = (payload[5] & 0xF0) >> 4;
				vw = payload[6];
				xy = payload[7];
				z = payload[8] & 0x0F;
				m = payload[9] & 0x0F;
				rr = payload[10];
				ss = payload[11];
				tt = payload[12];

				int heading = ((u & 0x03) * 90) + ((vw & 0x3f) * 2) + ((u & 0x0c) ? (((u & 0x0c) == 0x0c) ? 2 : 1) : 0);
				bool direction = (u & 0x08) == 0x08;
				int target_heading = (((vw & 0xc0) >> 6) * 90) + (xy / 2);
				int mode = z;
				int rudder_position = rr; // two's complement ??
				int alarms = 0;
				bool off_course = (m & 0x04) == 0x04;
				bool wind_shift = (m & 0x08) == 0x08;
				int display_flags = ss;

				// Construct the JSON response
				}
				break;
			case 0x90: // Device Identification
				{
				// payload[5] seems to be always zero
				int deviceID = payload[6];
				}
				break;
			case 0x9C: // Compass Heading and Rudder Position
				{
				int u, vw, rr;
				u = (payload[5] & 0xF0) >> 4; // Nibble 
				vw = payload[6];
				rr = payload[7];
				int heading = (((u & 0x3) * 90) + ((vw & 0x3f) * 2) + (u & 0xc ? ((u & 0xc) == 0xc ? 2 : 1) : 0)) % 360;
				bool direction = (u & 0x08) == 0x08;
				int rudderPosition = rr;  // Two's complement ??
				}
				break;
		} 
		
	}

	return FALSE;

}

// Simrad AC12
bool TwoCanAutoPilot::DecodeAC12Autopilot(const byte *payload, wxString *jsonResponse) {

	return TRUE;
}



// Decode PGN 130850 as sent from the autopilot. Seems to be an alarm ??
bool TwoCanAutoPilot::DecodeNAC3Alarm(const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;
	int mode;

	// Source 3: 41 9F FF FF 01 FF 3A 00 10 00 03 00 FF
	// Source 3: 41 9F FF FF 01 FF 39 00 10 00 03 00 FF
	
	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	byte statusCommand;
	statusCommand = payload[4];

	byte status;
	status = payload[6]; // 39 = OK ?? // 3A = Rudder Limit Exceeded

	autopilotControllerAddress = payload[10];

	unsigned short reservedA;
	reservedA = payload[3] | (payload[4] << 8);

	byte command;
	command = payload[5];

	byte subCommand;
	subCommand = payload[6];

	byte reservedB;
	reservedB = payload[7];

	byte direction;
	direction = payload[8];

	short angle;
	angle = payload[9] | (payload[10] << 8);

	byte reservedC;
	reservedC = payload[11];


	std::printf("A/P Address %d \n", autopilotControllerAddress);
	std::printf("A %d\n", reservedA);
	std::printf("Command %d \n", command);
	std::printf("Sub Command: %d \n", subCommand);
	std::printf("B %d\n", reservedB);
	std::printf("Direction: %d \n", direction);
	std::printf("Angle: %f \n", RADIANS_TO_DEGREES((float)angle / 10000));
	std::printf("C %d\n", reservedC);

	switch (command) {
		case 0x100:
			mode = AUTOPILOT_MODE::WIND;
			break;
		case 0x40:
			mode = AUTOPILOT_MODE::COMPASS;
			break;
		case 0x181:
			mode = AUTOPILOT_MODE::NODRIFT; // Not implemented in UI
			break;
		case 0x180:
			mode = AUTOPILOT_MODE::NAV;
			break;
		case 0x0:
		default:
			mode = AUTOPILOT_MODE::STANDBY;
			break;
	}

	//root["autopilot"]["model"] = AUTOPILOT_MODEL_NAC3;
	//root["autopilot"]["mode"] = mode;

	//if (root.Size() > 0) {
	//	writer.Write(root, *jsonResponse);
	//	return TRUE;
	//}

	return FALSE;
}

// Decode PGN 65305
bool TwoCanAutoPilot::DecodeNAC3Status(const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;
	int mode;

	unsigned short manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte reserved;
	reserved = (payload[1] & 0x18) >> 3;

	byte industryGroup;
	industryGroup = (payload[1] & 0xE0) >> 5;

	// Autopilot State, Engaged or Standby
	// 0x41 0x9f 0x00 0x02 0x10 0x00 0x00 0x00
	// 0x41 0x9f 0x00 0x02 0x02 0x00 0x00 0x00
	// 0x41 0x9F 0x64 0x02 0x10 0x00 0x00 0x00
	//            |     |   |
	//      0x64 = NAC3 | 0x10 = Engaged
	//      0x00 = AC12 | 0x02 = Standby
	//               State
	
	// NAC 3 Autopilot Mode 
	// 0x41 0x9F 0x64 0x0A 0x40 0x00 0x00 0x40 - NAV 
	// 0x41 0x9F 0x64 0x0A 0x00 0x04 0x00 0x04 - Wind
	// 0x41 0x9F 0x64 0x0A 0x10 0x00 0x00 0x00 - Heading
	// 0x41 0x9F 0x64 0x0A 0x00 0x01 0x00 0x00 - No Drift
	// 0x41 0x9F 0x64 0x0A 0x08 0x00 0x00 0x00 - Standby

	// AC12 Autopilot Mode
	// BUG BUG Needs confirmation
	// 0x41 0x9f 0x00 0x0a 0x06 0x04 0x00 0x00 - Wind
	// 0x41 0x9f 0x00 0x0a 0x1e 0x00 0x00 0x00 - Wind ??
	// 0x41 0x9f 0x00 0x0a 0x0a 0x00 0x00 0x00 - Heading
	// 0x41 0x9f 0x00 0x0a 0x16 0x00 0x00 0x00 - Heading ??
	// 0x41 0x9f 0x00 0x0a 0xf0 0x00 0x80 0x00 - Nav
	// 0x41 0x9f 0x00 0x0a 0x16 0x01 0x00 0x00 - No Drift ??
	// 0x41 0x9f 0x00 0x0a 0x0c 0x00 0x80 0x00 - No Drift ??
	// Standby ??


	byte autopilotModel;
	autopilotModel = payload[2];

	//wxLogMessage(_T("TwoCan Autopilot, Detected Navico Autopilot %s"), autopilotModel == 00 ? "AC-12" : 
	//	autopilotModel == 0x64 ? "NAC-3" : std::to_string(autopilotModel));

	byte operation;
	operation = payload[3];

	switch (operation) {

	case 0x02:
		if (payload[4] == 0x10) {
			//std::printf("Autopilot Engaged\n");
		}
		else if (payload[4] == 0x02) {
			//std::printf("Autopilot in standby\n");
		}
		break; // end case operation 0x02;

	case 0x0A:
		if (autopilotModel == 0x64) {
			// NAC 3
			unsigned short command;
			command = payload[4] | (payload[5] << 8);

			switch (command) {
				case 0x40:
					mode = AUTOPILOT_MODE::NAV;
					break;
				case 0x0400:
					mode = AUTOPILOT_MODE::WIND;
					break;
				case 0x10:
					mode = AUTOPILOT_MODE::COMPASS;
					break;
				case 0x100:
					mode = AUTOPILOT_MODE::NODRIFT;
					break;
				case 0x08:
					mode = AUTOPILOT_MODE::STANDBY;
					break;
			} // end switch command

		} // end if autopilot model NAC-3

		if (autopilotModel == 0x00) {
			// AC12
			unsigned short command;
			command = payload[4] | (payload[5] << 8);

			switch (command) {
				case 0xf0:
					mode = AUTOPILOT_MODE::NAV;
					break;
				case 0x1e:
					mode = AUTOPILOT_MODE::WIND;
					break;
				case 0x0a:
					mode = AUTOPILOT_MODE::COMPASS;
					break;
				case 0x0c:
					mode = AUTOPILOT_MODE::NODRIFT;
					break;
				case 0x00:
					mode = AUTOPILOT_MODE::STANDBY;
					break;
			} // end switch command

		} // end if autopilot model AC 12
		
		root["autopilot"]["mode"] = mode;

		break; // end case operation 0x0A

	} // end switch operation

	
	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}

	return FALSE;
}

// Garmin Reactor
bool TwoCanAutoPilot::DecodeGarminAutopilot(const byte *payload, wxString *jsonResponse) {
	return TRUE;
}

bool TwoCanAutoPilot::EncodeRudderAngle(const int rudderangle, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	root["autopilot"]["rudderangle"] = rudderangle;

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}
	return FALSE;
}

bool TwoCanAutoPilot::EncodeHeading(const unsigned int heading, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	root["autopilot"]["heading"] = heading;

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}
	return FALSE;
}


bool TwoCanAutoPilot::EncodeWindAngle(const int windangle, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	root["autopilot"]["windangle"] = windangle;

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}
	return FALSE;
}

// TwoCan Autopilot Plugin sends requests via OCPN Messaging to command the Autopilot
// Eg. Engage/Disengage the autopilot, change modes, change heading and wind angle.
bool TwoCanAutoPilot::EncodeAutopilotCommand(wxString message_body, std::vector<CanMessage> *canMessages) {
	CanHeader header;
	std::vector<byte>payload;
    
	wxJSONValue root;
	wxJSONReader reader;
	if (reader.Parse(message_body, &root) > 0) {
		// BUG BUG should log errors
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
		
		if (root["autopilot"].HasMember("mode")) {
				commandId = AUTOPILOT_COMMAND::CHANGE_MODE;
				commandValue = root["autopilot"]["mode"].AsInt();
		}

		if (root["autopilot"].HasMember("heading")) {
				commandId = AUTOPILOT_COMMAND::CHANGE_HEADING;
				commandValue = root["autopilot"]["heading"].AsInt();

		}

		if (root["autopilot"].HasMember("windangle")) {
				commandId = AUTOPILOT_COMMAND::CHANGE_WIND;
				commandValue = root["autopilot"]["windangle"].AsInt();
		}

		if (root["autopilot"].HasMember("keepalive")) {
				commandId = AUTOPILOT_COMMAND::KEEP_ALIVE;
		}

		//// The following are generated when we are in GPS Mode following a route and navigating to a waypoint
		//// Generate PGN 129283
		//// BUG BUG 129284 Navigation , should come from ????
		//if (root["autopilot"].HasMember("xte")) {
		//	// BUG BUG Do we need to check the units ??
		//	// BUG BUG Enforce the units from the autopilot plugin !!
		//	// Convert xte from NM into SI  units
		//	double rawCrossTrackError = root["autopilot"]["xte"].AsDouble();
		//	int crossTrackError = (int)(100 * rawCrossTrackError / CONVERT_METRES_NAUTICAL_MILES);

		//	// BUG BUG Refactor TwoCanEncoder ??
		//	header.pgn = 129283;
		//	header.destination = CONST_GLOBAL_ADDRESS;
		//	header.source = networkAddress;
		//	header.priority = CONST_PRIORITY_HIGH;

		//	payload.clear();

		//	// Sequence Identifier
		//	payload.push_back(0xA0);

		//	byte xteMode = 0;
		//	byte navigationTerminated = 0; //0 = No

		//	payload.push_back((xteMode & 0x0F) |  0x30 | ((navigationTerminated << 6) & 0xC0));

		//	payload.push_back(crossTrackError & 0xFF);
		//	payload.push_back((crossTrackError >> 8) & 0xFF);
		//	payload.push_back((crossTrackError >> 16) & 0xFF);
		//	payload.push_back((crossTrackError >> 24) & 0xFF);

		//	goto exit;
		//	
		//}

		//// Generate PGN 129284
		//if (root["autopilot"].HasMember("bearing")) {

		//	goto exit;
		//}

		
		// Now parse the commands and generate the approprite PGN's
		switch (autopilotModel) {
		case AUTOPILOT_MODEL::GARMIN:
				switch (commandId) {
				case AUTOPILOT_COMMAND::KEEP_ALIVE:

						break;
				case AUTOPILOT_COMMAND::CHANGE_MODE:
						if (commandValue == AUTOPILOT_MODE::STANDBY) {
						// Standby
						}
						else if (commandValue == AUTOPILOT_MODE::COMPASS) {
						// Heading Mode 
						}
						else if (commandValue == AUTOPILOT_MODE::WIND) {
						// Wind mode
						}
						else if (commandValue == AUTOPILOT_MODE::NAV) {
						// GPS mode
						}
						break;
				case AUTOPILOT_COMMAND::CHANGE_HEADING:
						// Adjust Heading
						break;
				case AUTOPILOT_COMMAND::CHANGE_WIND:
						// Adjust Wind Angle
						break;
				} 

				break; // end garmin
		
		case AUTOPILOT_MODEL::RAYMARINE:

				// Changes are performed via PGN 126208 Group Function to write the 
				// specific fields of proprietary PGN's

				switch (commandId) {
				case AUTOPILOT_COMMAND::KEEP_ALIVE:
						// PGN 65384
						payload.clear();
						header.pgn = 65384;
						header.destination = autopilotControllerAddress;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;
						// 3b, 9f is Raymarine Manufacturer Code, Reserved bits and Industry Code
						payload.push_back(0x3b);
						payload.push_back(0x9f);
						payload.push_back(0x00);
						payload.push_back(0x00);
						payload.push_back(0x00);
						payload.push_back(0x00);
						payload.push_back(0x00);
						payload.push_back(0x00);

						break;

				case AUTOPILOT_COMMAND::CHANGE_MODE:
						header.pgn = 126208;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;

						// PGN 126208 Group Function Command
						payload.push_back(0x01);

						// Commanded PGN
						// PGN 65379 (0x00FF63) Seatalk Pilot Mode
						payload.push_back(0x63);
						payload.push_back(0xFF);
						payload.push_back(0x00);

						// Reserved bits 0xF0 | 0x08 = Priority unchanged
						payload.push_back(0xF8);
						
						// Number of Parameter Pairs
						// BUG BUG Perhaps a variadic or iterator list function ??
						payload.push_back(0x04); 
						
						// First Pair, Field 1 of PGN 65379 
						payload.push_back(0x01);
						// Manufacturer Code 0x073B == 1851
						payload.push_back(0x3b);
						payload.push_back(0x07);

						// Second Pair, Field 3 of PGN 65379
						payload.push_back(0x03);
						// Industry Code, 4 = Marine
						payload.push_back(0x04);

						// Third Pair, Field 4 of PGN 65379
						payload.push_back(0x04);
						// Pilot Mode
						
						if (commandValue == AUTOPILOT_MODE::STANDBY) {
							payload.push_back(0x00);
							payload.push_back(0x00);
						}
						if (commandValue == AUTOPILOT_MODE::COMPASS) {
							payload.push_back(0x40);
							payload.push_back(0x00);
						}
						if (commandValue == AUTOPILOT_MODE::NAV) {
							payload.push_back(0x80);
							payload.push_back(0x01);
						}
						if (commandValue == AUTOPILOT_MODE::WIND) {
							payload.push_back(0x00);
							payload.push_back(0x01);
						}
						// 0x0181 - No Drift, not implemented yet...

						// Fourth Pair, Field 5 of PGN 65379
						payload.push_back(0x05);
						// Pilot Sub Mode 0xFFFF undefined
						payload.push_back(0xFF);
						payload.push_back(0xFF);

						break;

				case AUTOPILOT_COMMAND::CHANGE_HEADING: {
						header.pgn = 126208;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;

						// PGN 126208 Group Function Command
						payload.push_back(0x01);

						// Commanded PGN
						// PGN 65360 (00FF50) Seatalk Heading
						payload.push_back(0x50);
						payload.push_back(0xFF);
						payload.push_back(0x00);

						// Reserved bits 0xF0 | 0x08 = Priority unchanged
						payload.push_back(0xF8);

						// Number of Parameter Pairs
						payload.push_back(0x03);

						// First Pair, Field 1 of PGN 65360 
						payload.push_back(0x01);
						// Manufacturer Code 0x073B == 1851
						payload.push_back(0x3b);
						payload.push_back(0x07);

						// Second Pair, Field 3 of PGN 65360
						payload.push_back(0x03);
						// Industry Code, 4 = Marine
						payload.push_back(0x04);

						// Third Pair, Field 4 of PGN 65360
						payload.push_back(0x04);
						// Heading, Convert to radians * 1e4
						unsigned short heading = DEGREES_TO_RADIANS(commandValue) * 10000;
						payload.push_back(heading & 0xFF);
						payload.push_back((heading >> 8) & 0xFF);

						break;
					}

				case AUTOPILOT_COMMAND::CHANGE_WIND: {
						header.pgn = 126208;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;

						// PGN 126208 Group Function Command
						payload.push_back(0x01);

						// Commanded PGN
						// PGN 65345 (00FF41) Seatalk Wind
						payload.push_back(0x41);
						payload.push_back(0xFF);
						payload.push_back(0x00);

						// Reserved bits 0xF0 | 0x08 = Priority unchanged
						payload.push_back(0xF8);

						// Number of Parameter Pairs
						payload.push_back(0x03);

						// First Pair, Field 1 of PGN 65345 
						payload.push_back(0x01);
						// Manufacturer Code 0x073B == 1851
						payload.push_back(0x3b);
						payload.push_back(0x07);

						// Second Pair, Field 3 of PGN 65345
						payload.push_back(0x03);
						// Industry Code, 4 = Marine
						payload.push_back(0x04);

						// Third Pair, Field 4 of PGN 65345
						//BUG BUG is this signed, meaning relative to the boat +/- port/starboard
						// or unsigned !!
						// Wind Angle, Convert to radians * 1e4
						short windAngle = DEGREES_TO_RADIANS(commandValue) * 1000;
						payload.push_back(windAngle & 0xFF);
						payload.push_back((windAngle >> 8) & 0xFF);

						break;
					}
				} 

				break; // end raymarine

			case AUTOPILOT_MODEL::SIMRAD: // AC12

				switch (commandId) {
				case AUTOPILOT_COMMAND::KEEP_ALIVE:
						header.pgn = 65341;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;

						// The usual manufacturer/industry code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);
						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x0D);
						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x7F);
						
						break;
				case AUTOPILOT_COMMAND::CHANGE_MODE:
						header.pgn = 65341;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;

						// The usual manufacturer/industry code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);
						
						if (commandValue == AUTOPILOT_MODE::STANDBY) {

							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0xFF);

						}
						
						if (commandValue == AUTOPILOT_MODE::COMPASS) {
							
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0x15);
							payload.push_back(0x9A);

						}
						else if (commandValue == AUTOPILOT_MODE::WIND) {
							
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0x00);
							payload.push_back(0x00);

						}
						else if (commandValue == AUTOPILOT_MODE::NAV) {
							
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0x12);
							payload.push_back(0x00);
						}

						break;

				case AUTOPILOT_COMMAND::CHANGE_HEADING: {
						header.pgn = 65431;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;
						// This is the usual manufacturer code/industry code
						payload.push_back(0x41);
						payload.push_back(0x9F);

						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x03); // command to change heading
						payload.push_back(0xFF);
						short heading = DEGREES_TO_RADIANS(commandValue) * 10000;
						payload.push_back(heading & 0xFF);
						payload.push_back((heading >> 8) & 0xFF);

						break;
					}

				case AUTOPILOT_COMMAND::CHANGE_WIND: {
						header.pgn = 65431;
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;
						// This is the usual manufacturer code/industry code
						payload.push_back(0x41);
						payload.push_back(0x9F);

						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x03); // command to change wind angle
						payload.push_back(0xFF);
						short windAngle = DEGREES_TO_RADIANS(commandValue) * 10000;
						payload.push_back(windAngle & 0xFF);
						payload.push_back((windAngle >> 8) & 0xFF);

						break;
					}
				} 

				break; // end AC-12

				case AUTOPILOT_MODEL::NAVICO:
				switch (commandId) {
				case AUTOPILOT_COMMAND::KEEP_ALIVE:
					
					header.destination = CONST_GLOBAL_ADDRESS;
					header.source = networkAddress;
					header.priority = 3;
					header.pgn = 65305;
					
					payload.push_back(0x41);
					payload.push_back(0x9F);
					payload.push_back(0x01);
					payload.push_back(navicoKeepAliveToggle ? 0x03 : 0x0B); //This toggles between 3 & B. Is 3 the A/P address ??
					payload.push_back(0x00);
					payload.push_back(0x00);
					payload.push_back(0x00);
					payload.push_back(0x00);

					navicoKeepAliveToggle = !navicoKeepAliveToggle;

					break;
				case AUTOPILOT_COMMAND::CHANGE_MODE:
					if (commandValue == AUTOPILOT_MODE::STANDBY) {

						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = 3;
						header.pgn = 130850;

						// Manufacturer Code & Industry Code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);

						// The network address of the autopilot controller
						payload.push_back(autopilotControllerAddress);

						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x0A); // NAC-3 Command
						payload.push_back(0x06); // Standby command
						payload.push_back(0x00); // Reserved
						payload.push_back(0xFF); // Direction
						payload.push_back(0xFF); // Angle
						payload.push_back(0xFF);
						payload.push_back(0xFF); // Unused
						payload.push_back(0xFF);
					}
					else if (commandValue == AUTOPILOT_MODE::COMPASS) {

						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = 3;
						header.pgn = 130850;


						// Manufacturer Code & Industry Code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);

						// The network address of the autopilot controller
						// BUG BUG Change for production
						payload.push_back(autopilotControllerAddress);

						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x0A); // NAC-3 Command
						payload.push_back(0x09); // Heading Hold
						payload.push_back(0x00); // Reserved
						payload.push_back(0xFF); // Direction
						payload.push_back(0xFF); // Angle
						payload.push_back(0xFF);
						payload.push_back(0xFF); // Unused
						payload.push_back(0xFF);
					}
					else if (commandValue == AUTOPILOT_MODE::WIND) {

						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = 3;
						header.pgn = 130850;


						// Manufacturer Code & Industry Code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);

						// The network address of the autopilot controller
						// BUG BUG Change for production
						payload.push_back(autopilotControllerAddress);

						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x0A); // NAC-3 Command
						payload.push_back(0x0F); // Wind Mode
						payload.push_back(0x00); // Reserved
						payload.push_back(0xFF); // Direction
						payload.push_back(0xFF); // Angle
						payload.push_back(0xFF);
						payload.push_back(0xFF); // Unused
						payload.push_back(0xFF);
					}
					else if (commandValue == AUTOPILOT_MODE::NAV) {
						header.destination = CONST_GLOBAL_ADDRESS;
						header.source = networkAddress;
						header.priority = 3;
						header.pgn = 130850;


						// Manufacturer Code & Industry Code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);

						// The network address of the autopilot controller
						// BUG BUG Change for production
						payload.push_back(autopilotControllerAddress);

						payload.push_back(0xFF);
						payload.push_back(0xFF);
						payload.push_back(0x0A); // NAC-3 Command
						payload.push_back(0x0A); // Navigation Mode
						payload.push_back(0x00); // Reserved
						payload.push_back(0xFF); // Direction
						payload.push_back(0xFF); // Angle
						payload.push_back(0xFF);
						payload.push_back(0xFF); // Unused
						payload.push_back(0xFF);
 					}
					break;
				case AUTOPILOT_COMMAND::CHANGE_HEADING:
				case AUTOPILOT_COMMAND::CHANGE_WIND:
					header.destination = CONST_GLOBAL_ADDRESS;
					header.source = networkAddress;
					header.priority = 3;
					header.pgn = 130850;

					// Manufacturer Code & Industry Code palaver
					payload.push_back(0x41);
					payload.push_back(0x9F);

					// The network address of the autopilot controller
					// BUG BUG Change for prodction
					payload.push_back(autopilotControllerAddress);

					payload.push_back(0xFF);
					payload.push_back(0xFF);
					payload.push_back(0x0A); // NAC3 Command
					payload.push_back(0x1A); // Alter Heading
					payload.push_back(0x00); // Reserved

					byte direction = commandValue < 0 ? NAC3_DIRECTION_PORT : NAC3_DIRECTION_STBD;
					unsigned short heading = DEGREES_TO_RADIANS(abs(commandValue)) * 10000;

					payload.push_back(direction);
					payload.push_back(heading & 0xFF);
					payload.push_back((heading >> 8) & 0xFF);

					payload.push_back(0xFF);
					payload.push_back(0xFF);

					break;
				} // end switch command
				break; // end NAC3

			case AUTOPILOT_MODEL::FURUNO:

				switch (commandId) {
				case AUTOPILOT_COMMAND::CHANGE_MODE:
					if (commandValue == AUTOPILOT_MODE::STANDBY) {
						// set to Standby
					}
					else if (commandValue == AUTOPILOT_MODE::COMPASS) {
						// Heading Mode 
					}
					else if (commandValue == AUTOPILOT_MODE::WIND) {
						// Wind mode
					}
					else if (commandValue == AUTOPILOT_MODE::NAV) {
						// GPS mode
					}
					break;
				case AUTOPILOT_COMMAND::CHANGE_HEADING:
					// Adjust Heading
					break;
				case AUTOPILOT_COMMAND::CHANGE_WIND:
					break;
				} // end switch command
				break; // end furuno

			default:
				return FALSE;
				break;
		} // end switch manufacturer

		if (payload.size() > 0) {
			TwoCanUtils::FragmentFastMessage(header, payload, canMessages);
			return TRUE;
		}
	}
	return FALSE;
}


// BUG BUG To refactor
// Simnet Autopilot Command
void EncodePGN130850(int command) {
	std::vector<byte>payload;
	int controllingDevice; // Is this the network address of the controlling device
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

// PGN 129284
void EncodePGN129284(void) {
std::vector<byte> payload;
	
	byte sequenceId = 0xA0;
	payload.push_back(sequenceId);

	/*

	unsigned short distance = 100 * parser->Rmb.RangeToDestinationNauticalMiles / CONVERT_METRES_NAUTICAL_MILES;
	payload.push_back(distance & 0xFF);
	payload.push_back((distance >> 8) & 0xFF);

	byte bearingRef = HEADING_TRUE;

	byte perpendicularCrossed = 0;

	byte circleEntered = parser->Rmb.IsArrivalCircleEntered == NTrue ? 1 : 0;

	byte calculationType = 0;

	payload.push_back(bearingRef | (perpendicularCrossed << 2) | (circleEntered << 4) | (calculationType << 6));

	wxDateTime epochTime((time_t)0);
	wxDateTime now = wxDateTime::Now();

	wxTimeSpan diff = now - epochTime;

	unsigned short daysSinceEpoch = diff.GetDays();
	unsigned int secondsSinceMidnight = ((diff.GetSeconds() - (daysSinceEpoch * 86400)).GetValue()) * 10000;

	payload.push_back(secondsSinceMidnight & 0xFF);
	payload.push_back((secondsSinceMidnight >> 8) & 0xFF);
	payload.push_back((secondsSinceMidnight >> 16) & 0xFF);
	payload.push_back((secondsSinceMidnight >> 24) & 0xFF);

	payload.push_back(daysSinceEpoch & 0xFF);
	payload.push_back((daysSinceEpoch >> 8) & 0xFF);

	unsigned short bearingOrigin = USHRT_MAX;
	payload.push_back(bearingOrigin & 0xFF);
	payload.push_back((bearingOrigin >> 8) & 0xFF);

	unsigned short bearingPosition = 1000 * DEGREES_TO_RADIANS(parser->Rmb.BearingToDestinationDegreesTrue);
	payload.push_back(bearingPosition & 0xFF);
	payload.push_back((bearingPosition >> 8) & 0xFF);

	wxString originWaypointId = parser->Rmb.From;
	// BUG BUG Need to get the waypointId
	payload.push_back(0xFF);
	payload.push_back(0xFF);
	payload.push_back(0xFF);
	payload.push_back(0xFF);

	wxString destinationWaypointId = parser->Rmb.To;
	payload.push_back(0xFF);
	payload.push_back(0xFF);
	payload.push_back(0xFF);
	payload.push_back(0xFF);

	int latitude = parser->Rmb.DestinationPosition.Latitude.Latitude * 1e7;
	if (parser->Rmb.DestinationPosition.Latitude.Northing == NORTHSOUTH::South) {
		latitude = -latitude;
	}
	payload.push_back(latitude & 0xFF);
	payload.push_back((latitude >> 8) & 0xFF);
	payload.push_back((latitude >> 16) & 0xFF);
	payload.push_back((latitude >> 24) & 0xFF);

	int longitude = parser->Rmb.DestinationPosition.Longitude.Longitude * 1e7;
	if (parser->Rmb.DestinationPosition.Longitude.Easting == EASTWEST::West) {
		longitude = -longitude;
	}

	payload.push_back(longitude & 0xFF);
	payload.push_back((longitude >> 8) & 0xFF);
	payload.push_back((longitude >> 16) & 0xFF);
	payload.push_back((longitude >> 24) & 0xFF);

	unsigned short waypointClosingVelocity = 100 * parser->Rmb.DestinationClosingVelocityKnots / CONVERT_MS_KNOTS;
	payload.push_back(waypointClosingVelocity & 0xFF);
	payload.push_back((waypointClosingVelocity >> 8) & 0xFF);

	*/
}


// PGN 127237
void EncodePGN127237(void) {

	std::vector<byte> payload;

	byte rudderLimitExceeded; // 0 - No, 1 - Yes, 2 - Error, 3 - Unavailable


	byte offHeadingLimitExceeded; // 0 - No, 1 - Yes, 2 - Error, 3 - Unavailable

	byte offTrackLimitExceeded;

	byte overRide;

	payload.push_back(((rudderLimitExceeded << 6) & 0xC0) | ((offHeadingLimitExceeded << 4) & 0x30) |
		((offTrackLimitExceeded << 2) & 0x0C) | (overRide & 0x03));

	byte steeringMode;
	// 0 - Main Steering
	// 1 - Non-Follow-up Device
	// 2 - Follow-up Device
	// 3 - Heading Control Standalone
	// 4 - Heading Control
	// 5 - Track Control

	byte turnMode;
	// 0 - Rudder Limit controlled
	// 1 - turn rate controlled
	// 2 - radius controlled

	byte headingReference;
	// 0 - True
	// 1 - Magnetic
	// 2 - Error
	// 3 - Null

	payload.push_back(((steeringMode << 5) & 0xE0) | ((turnMode << 2) & 0x1C) | (headingReference & 0x03));

	byte commandedRudderDirection;
	// 0 - No Order
	// 1 - Move to starboard
	// 2 - Move to port

	payload.push_back(0xF8 | (commandedRudderDirection & 0x03));

	short commandedRudderAngle; //0.0001 radians
	payload.push_back(commandedRudderAngle & 0xFF);
	payload.push_back((commandedRudderAngle >> 8) & 0xFF);

	unsigned short headingToSteer; //0.0001 radians
	payload.push_back(headingToSteer & 0xFF);
	payload.push_back((headingToSteer >> 8) & 0xFF);

	unsigned short track; //0.0001 radians
	payload.push_back(track & 0xFF);
	payload.push_back((track >> 8) & 0xFF);

	unsigned short rudderLimit; //0.0001 radians
	payload.push_back(rudderLimit & 0xFF);
	payload.push_back((rudderLimit >> 8) & 0xFF);

	unsigned short offHeadingLimit; // 0.0001 radians
	payload.push_back(offHeadingLimit & 0xFF);
	payload.push_back((offHeadingLimit >> 8) & 0xFF);

	short radiusOfTurn; // 0.0001 radians
	payload.push_back(radiusOfTurn & 0xFF);
	payload.push_back((radiusOfTurn >> 8) & 0xFF);

	short rateOfTurn; // 3.125e-05
	payload.push_back(rateOfTurn & 0xFF);
	payload.push_back((rateOfTurn >> 8) & 0xFF);

	short offTrackLimit; //in metres (or is it 0.01 m)??
	payload.push_back(offTrackLimit & 0xFF);
	payload.push_back((offTrackLimit >> 8) & 0xFF);

	unsigned short vesselHeading; // 0.0001 radians
	payload.push_back(vesselHeading & 0xFF);
	payload.push_back((vesselHeading >> 8) & 0xFF);

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


	//0x41 0x9F 0xFF 0xFF 0x03 0xFF 0x18 0x95
	//	17:39 : 25 : 858  PGN : 65341, Source : 3, Destination : 255

	//	Manufacturer Code : 1857
	//	Manufacturer : Simrad

	//	Industry Group : 4
	//	Reserved : 3
	//	Command : 3 (0x03)  Seems to be 2 in Standby
	//	Wind Angle - 156.807087

}
	
/* PGN 127237 is Heading/Track Control
function AC12_PGN127237 () {
  const heading_track_pgn = {
	  "navigation" : "%s,2,127237,%s,%s,15,ff,3f,ff,ff,7f,%s,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s", - course to steer, heading
	  "headinghold": "%s,2,127237,%s,%s,15,ff,7f,ff,ff,7f,%s,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s", locked heading/heading
	  "wind":        "%s,2,127237,%s,%s,15,ff,7f,ff,ff,7f,ff,ff,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s", locked heading, heading
	  "standby":     "%s,2,127237,%s,%s,15,ff,7f,ff,ff,7f,ff,ff,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s" // Magnetic
	  "standby":  "%s,2,127237,%s,%s,15,ff,3f,ff,ff,7f,ff,ff,00,00,ff,ff,ff,ff,ff,7f,ff,ff,ff,ff,%s" // True
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



//const key_command = "%s,7,126720,%s,%s,22,3b,9f,f0,81,86,21,%s,ff,ff,ff,ff,ff,c1,c2,cd,66,80,d3,42,b1,c8"
//const heading_command = "%s,3,126208,%s,%s,14,01,50,ff,00,f8,03,01,3b,07,03,04,06,%s,%s"
//const wind_direction_command = "%s,3,126208,%s,%s,14,01,41,ff,00,f8,03,01,3b,07,03,04,04,%s,%s"
//const raymarine_ttw_Mode = "%s,3,126208,%s,%s,17,01,63,ff,00,f8,04,01,3b,07,03,04,04,81,01,05,ff,ff"
//const raymarine_ttw = "%s,3,126208,%s,%s,21,00,00,ef,01,ff,ff,ff,ff,ff,ff,04,01,3b,07,03,04,04,6c,05,1a,50"
//const confirm_tack = "%s,2,126720,%s,%s,7,3b,9f,f0,81,90,00,03"

//const keep_alive = "%s,7,65384,%s,255,8,3b,9f,00,00,00,00,00,00"

//const keep_alive2 = "%s,7,126720,%s,255,7,3b,9f,f0,81,90,00,03"


//const std::vector<byte> raymarineStandby = {0x17, 0x01, 0x63, 0xff, 0x00, 0xf8, 0x04, 0x01, 0x3b, 0x07, 0x03, 0x04, 0x04, 0x00, 0x00, 0x05, 0xff, 0xff};
//const std::vector<byte> raymarineAuto = {0x17, 0x01, 0x63, 0xff, 0x00, 0xf8, 0x04, 0x01, 0x3b, 0x07, 0x03, 0x04, 0x04, 0x40, 0x00, 0x05, 0xff, 0xff};
//const std::vector<byte> raymarineTrack = {0x0d, 0x3b, 0x9f, 0xf0, 0x81, 0x84, 0x46, 0x27, 0x9d, 0x4a, 0x00, 0x00, 0x02, 0x08, 0x4e};

// Note last two bytes represent heading as radians * 0.0001
//const std::vector<byte>raymarineHeading = {0x14, 0x01, 0x50, 0xff, 0x00, 0xf8, 0x03, 0x01, 0x3b, 0x07, 0x03, 0x04, 0x06, 0x00, 0x00};