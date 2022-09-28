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
TwoCanAutoPilot::TwoCanAutoPilot(int manufacturer) {
    // Save the autopilot model, 0 - None, 1 - Garmin, 2 - Navico, 3 - Raymarine
    autopilotManufacturer = manufacturer;
}

TwoCanAutoPilot::~TwoCanAutoPilot(void) {
}

bool TwoCanAutoPilot::FindAutopilot(unsigned int autopilotAddress) {
	for (auto it : networkMap) {
		// Find either the product id, name etc to match to an autopilot
		// AC 12 Product Id 18846, Device Function": 150, "Device Class": 40,
		// NAC 3
		// Raymarine Evo

		switch (it.productInformation.productCode) {
		case 1234:
			break;
		default:
			return false;
			break;
		}
	}
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

	if (pgn == 65359) {
		root["autopilot"]["manufacturer"] = "Raymarine";
		root["autopilot"]["heading"]["trueheading"] = RADIANS_TO_DEGREES((float)headingTrue / 10000);
		root["autopilot"]["heading"]["heading"] = RADIANS_TO_DEGREES((float)headingMagnetic / 10000);
	}
	if (pgn == 65360) {
		root["autopilot"]["manufacturer"] = "Raymarine";
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

	root["autopilot"]["manufacturer"] = "Raymarine";
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

	root["autopilot"]["manufacturer"] = "Raymarine";
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



// Navico NAC-2/3
bool TwoCanAutoPilot::DecodeNAC3Autopilot(const byte *payload, wxString *jsonResponse) {
	// Source 3: 41 9F FF FF 01 FF 3A 00 10 00 03 00 FF
	// Source 3: 41 9F FF FF 01 FF 39 00 10 00 03 00 FF

	// PGN 130850

	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	byte statusCommand;
	statusCommand = payload[4];

	byte status;
	status = payload[6]; // 39 = OK ?? // 3A = Rudder Limit Exceeded

	byte autopilotControllerddress;
	autopilotControllerddress = payload[10];

	// PGN 65305



	return TRUE;
}

// Garmin Reactor
bool TwoCanAutoPilot::DecodeGarminAutopilot(const byte *payload, wxString *jsonResponse) {
	return TRUE;
}

bool TwoCanAutoPilot::DecodeRudderAngle(const int rudderangle, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	root["autopilot"]["rudder"] = rudderangle;

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}
	return FALSE;
}


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
		
		if (root["autopilot"].HasMember("manufacturer")) {
			autopilotManufacturer = root["autopilot"]["manufacturer"].AsInt();
			wxMessageBox(wxString::Format("AUTOPILOT MANUFACTURER CHANGE: %d", commandValue), "TwoCanPlugin Debug");

			if (root["autopilot"].HasMember("mode")) {
				commandId = AUTOPILOT_CHANGE_STATUS;
				commandValue = root["autopilot"]["mode"].AsInt();
				wxMessageBox(wxString::Format("AUTOPILOT STATUS CHANGE: %d", commandValue), "TwoCanPlugin Debug");
			}

			else if (root["autopilot"].HasMember("heading")) {
				commandId = AUTOPILOT_CHANGE_COURSE;
				commandValue = root["autopilot"]["heading"].AsInt();
				wxMessageBox(wxString::Format("AUTOPILOT COURSE CHANGE: %d", commandValue), "TwoCanPlugin Debug");
			}

			else if (root["autopilot"].HasMember("windangle")) {
				commandId = AUTOPILOT_CHANGE_WIND;
				commandValue = root["autopilot"]["windangle"].AsInt();
				wxMessageBox(wxString::Format("AUTOPILOT WIND ANGLE CHANGE: %d", commandValue), "TwoCanPlugin Debug");
			}

			else if (root["autopilot"].HasMember("keepalive")) {
				commandId = AUTOPILOT_KEEP_ALIVE;
				wxMessageBox("AUTOPILOT KEEP ALIVE", "TwoCanPlugin Debug");
			}

			else {
				// Not a command we are interested in
				wxLogMessage(_T("TwoCan Plugin, Invalid Autopilot Request: %s"), message_body);
				return FALSE;
			}
			// Fall through to encode the NMEA 2000 message
		}
		else {
			// Does not have a manufacturer 
			wxLogMessage(_T("TwoCan Plugin, Invalid Autopilot Manufacturer: %s"), message_body);
			return FALSE;
		}

		// Now parse the commands and generate the approprite PGN's
		switch (autopilotManufacturer) {
			case FLAGS_AUTOPILOT_GARMIN:
				switch (commandId) {
					case AUTOPILOT_KEEP_ALIVE:

						break;
					case AUTOPILOT_CHANGE_STATUS:
						if (commandValue == AUTOPILOT_POWER_STANDBY) {
						// Standby
						}
						else if (commandValue == AUTOPILOT_MODE_COMPASS) {
						// Heading Mode 
						}
						else if (commandValue == AUTOPILOT_MODE_WIND) {
						// Wind mode
						}
						else if (commandValue == AUTOPILOT_MODE_NAV) {
						// GPS mode
						}
						break;
					case AUTOPILOT_CHANGE_COURSE:
						// Adjust Heading
						break;
					case AUTOPILOT_CHANGE_WIND:
						// Adjust Wind Angle
						break;
				} 

				break; // end garmin
		
			case FLAGS_AUTOPILOT_RAYMARINE:
				// Changes are performed via PGN 126208 Group Function to write the 
				// specific fields of proprietary PGN's

				switch (commandId) {
					case AUTOPILOT_KEEP_ALIVE:
						// PGN 65384
						payload.clear();
						header.pgn = 65384;
						header.destination = 0; // BUG BUG Need autopilot adress
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

					case AUTOPILOT_CHANGE_STATUS:
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

						
						if (commandValue == AUTOPILOT_POWER_STANDBY) {
							payload.push_back(0x00);
							payload.push_back(0x00);
						}
						if (commandValue == AUTOPILOT_MODE_COMPASS) {
							payload.push_back(0x40);
							payload.push_back(0x00);
						}
						if (commandValue == AUTOPILOT_MODE_NAV) {
							payload.push_back(0x80);
							payload.push_back(0x01);
						}
						if (commandValue == AUTOPILOT_MODE_WIND) {
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

					case AUTOPILOT_CHANGE_COURSE: {
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

					case AUTOPILOT_CHANGE_WIND: {
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

			case FLAGS_AUTOPILOT_NAVICO: // AC12
				switch (commandId) {
					case AUTOPILOT_KEEP_ALIVE:
						header.pgn = 65341;
						header.destination = 9;// ??
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
					case AUTOPILOT_CHANGE_STATUS:
						header.pgn = 65341;
						header.destination = 9;// ??
						header.source = networkAddress;
						header.priority = CONST_PRIORITY_HIGH;

						// The usual manufacturer/industry code palaver
						payload.push_back(0x41);
						payload.push_back(0x9F);
						
						if (commandValue == AUTOPILOT_POWER_STANDBY) {

							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0xFF);

						}
						
						if (commandValue == AUTOPILOT_MODE_COMPASS) {
							
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0x15);
							payload.push_back(0x9A);

						}
						else if (commandValue == AUTOPILOT_MODE_WIND) {
							
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0x00);
							payload.push_back(0x00);

						}
						else if (commandValue == AUTOPILOT_MODE_NAV) {
							
							payload.push_back(0xFF);
							payload.push_back(0xFF);
							payload.push_back(0x02);
							payload.push_back(0xFF);
							payload.push_back(0x12);
							payload.push_back(0x00);
						}

						break;

					case AUTOPILOT_CHANGE_COURSE: {
						header.pgn = 65431;
						header.destination = 0; //?
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

					case AUTOPILOT_CHANGE_WIND: {
						header.pgn = 65431;
						header.destination = 0; //?
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

				break; // end navico

			case FLAGS_AUTOPILOT_NAC3:
				header.destination = CONST_GLOBAL_ADDRESS;
				header.source = networkAddress;
				header.priority = 3;
				header.pgn = 130850;

				// Manufacturer Code & Industry Code palaver
				payload.push_back(0x41);
				payload.push_back(0x9F);

				// The network address of the autopilot controller
				payload.push_back(autopilotAddress);

				switch (commandId) {
				case AUTOPILOT_CHANGE_STATUS:
					if (commandValue == AUTOPILOT_POWER_STANDBY) {
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
					else if (commandValue == AUTOPILOT_MODE_COMPASS) {
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
					else if (commandValue == AUTOPILOT_MODE_WIND) {
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
					else if (commandValue == AUTOPILOT_MODE_NAV) {
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
				case AUTOPILOT_CHANGE_COURSE:
				case AUTOPILOT_CHANGE_WIND:
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

			case FLAGS_AUTOPILOT_FURUNO:
				switch (commandId) {
					case AUTOPILOT_CHANGE_STATUS:
						if (commandValue == AUTOPILOT_POWER_STANDBY) {
							// set to Standby
						}
						else if (commandValue == AUTOPILOT_MODE_COMPASS) {
							// Heading Mode 
						}
						else if (commandValue == AUTOPILOT_MODE_WIND) {
							// Wind mode
						}
						else if (commandValue == AUTOPILOT_MODE_NAV) {
							// GPS mode
						}
						break;
					case AUTOPILOT_CHANGE_COURSE:
						// Adjust Heading
						break;
					case AUTOPILOT_CHANGE_WIND:
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


// Product Identifiers (obtained from PGN 126996, 60928)
// Simrad AC-12 Product Code = 18846, Device Function = 150, Device Class = 40,
// Simrad NAC-3
// Raymarine EVO
// Garmin Reactor 40 Product Code = 2545, Device Function = 150, Device Class = 80

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

// Raymarine
/*
const keys_code = {
	"+1":      "07,f8",
	"+10":     "08,f7",
	"-1":      "05,fa",
	"-10":     "06,f9",
	"-1-10":   "21,de",
	"+1+10":   "22,dd"
}


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

*/