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
// 2.2.2 07/07/2024 - Cleanup Autopilot Code, Support Raymarine Evolution autopilots

#include "twocanautopilot.h"

// List of Raymarine Error Messages, used by PGN 65288
std::vector<std::string> RaymarineAlarmMessages = {
			"No Alarm",
			"Shallow Depth",
			"Deep Depth",
			"Shallow Anchor",
			"Deep Anchor",
			"Off Course",
			"AWA High",
			"AWA Low",
			"AWS High",
			"AWS Low",
			"TWA High",
			"TWA Low",
			"TWS High",
			"TWS Low",
			"WP Arrival",
			"Boat Speed High",
			"Boat Speed Low",
			"Sea Temperature High",
			"Sea Temperature Low",
			"Pilot Watch",
			"Pilot Off Course",
			"Pilot Wind Shift",
			"Pilot Low Battery",
			"Pilot Last Minute Of Watch",
			"Pilot No NMEA Data",
			"Pilot Large XTE",
			"Pilot NMEA DataError",
			"Pilot CU Disconnected",
			"Pilot Auto Release",
			"Pilot Way Point Advance",
			"Pilot Drive Stopped",
			"Pilot Type Unspecified",
			"Pilot Calibration Required",
			"Pilot Last Heading",
			"Pilot No Pilot",
			"Pilot Route Complete",
			"Pilot Variable Text",
			"GPS Failure",
			"MOB",
			"Seatalk1 Anchor",
			"Pilot Swapped Motor Power",
			"Pilot Standby Too Fast To Fish",
			"Pilot No GPS Fix",
			"Pilot No GPS COG",
			"Pilot Start Up",
			"Pilot Too Slow",
			"Pilot No Compass",
			"Pilot Rate Gyro Fault",
			"Pilot Current Limit",
			"Pilot Way Point Advance Port",
			"Pilot Way Point Advance Stbd",
			"Pilot No Wind Data",
			"Pilot No Speed Data",
			"Pilot Seatalk Fail1",
			"Pilot Seatalk Fail2",
			"Pilot Warning Too Fast To Fish",
			"Pilot Auto Dockside Fail",
			"Pilot Turn Too Fast",
			"Pilot No Nav Data",
			"Pilot Lost Waypoint Data",
			"Pilot EEPROM Corrupt",
			"Pilot Rudder Feedback Fail",
			"Pilot Autolearn Fail1",
			"Pilot Autolearn Fail2",
			"Pilot Autolearn Fail3",
			"Pilot Autolearn Fail4",
			"Pilot Autolearn Fail5",
			"Pilot Autolearn Fail6",
			"Pilot Warning Cal Required",
			"Pilot Warning OffCourse",
			"Pilot Warning XTE",
			"Pilot Warning Wind Shift",
			"Pilot Warning Drive Short",
			"Pilot Warning Clutch Short",
			"Pilot Warning Solenoid Short",
			"Pilot Joystick Fault",
			"Pilot No Joystick Data",
			"Pilot Invalid Command",
			"AIS TX Malfunction",
			"AIS Antenna VSWR fault",
			"AIS Rx channel 1 malfunction",
			"AIS Rx channel 2 malfunction",
			"AIS No sensor position in use",
			"AIS No valid SOG information",
			"AIS No valid COG information",
			"AIS 12V alarm",
			"AIS 6V alarm",
			"AIS Noise threshold exceeded channel A",
			"AIS Noise threshold exceeded channel B",
			"AIS Transmitter PA fault",
			"AIS 3V3 alarm",
			"AIS Rx channel 70 malfunction",
			"AIS Heading lost/invalid",
			"AIS internal GPS lost",
			"AIS No sensor position",
			"AIS Lock failure",
			"AIS Internal GGA timeout",
			"AIS Protocol stack restart",
			"Pilot No IPS communications",
			"Pilot Power-On or Sleep-Switch Reset While Engaged",
			"Pilot Unexpected Reset While Engaged",
			"AIS Dangerous Target",
			"AIS Lost Target",
			"AIS Safety Related Message (used to silence)",
			"AIS Connection Lost",
			"No Fix" };


// TwoCan Autopilot
TwoCanAutoPilot::TwoCanAutoPilot(AUTOPILOT_MODEL model) {
	// BUG BUG Not sure why I am passing the autopilot model.
	
	// BUG BUG Do I need to persist and/or detect
	// BUG BUG Remove for production
	autopilotControllerAddress = 3;
}

TwoCanAutoPilot::~TwoCanAutoPilot(void) {
}

// Some models of autopilots require the autopilot network address to be included
// in the data. Obtain the network address by iterating through the network map.
// The list of product numbers however needs to be updated.
// Another way would be to use the device & function class values from PGN 60928 (Address Claim)
// Generally the Autopilot has Device Class 40, Function Class 140

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

// Parse the PGN, convert to JSON and send via OCPN messaging so that the UI dialog can be synchronized with the A/P state

// Raymarine Evolution Autopilot Heading
// PGN's 65359 (Heading) and 65360 (Locked Heading)
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
		root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE_EVOLUTION;
		root["autopilot"]["heading"]["trueheading"] = RADIANS_TO_DEGREES((float)headingTrue / 10000);
		root["autopilot"]["heading"]["heading"] = RADIANS_TO_DEGREES((float)headingMagnetic / 10000);
	}
	if (pgn == 65360) {
		root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE_EVOLUTION;
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

	root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE_EVOLUTION;
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

	root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE_EVOLUTION;
	root["autopilot"]["mode"] = mode;

	if (root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}

	return FALSE;
}

// Raymarine Alarm
// PGN 65288
bool TwoCanAutoPilot::DecodeRaymarineAutopilotAlarm(const byte* payload, wxString* jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned short manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);;

	byte reserved;
	reserved = (payload[1] & 0x18) >> 3;

	byte industryGroup;
	industryGroup = (payload[1] & 0xE0) >> 5;

	byte sid;
	sid = payload[2];

	byte alarmStatus;
	alarmStatus = payload[3];

	byte alarmCode;
	alarmCode = payload[4];

	byte alarmGroup;
	alarmGroup = payload[5];

	unsigned short alarmPriority;
	alarmPriority = payload[6] | (payload[7] << 8);

	//  Instrument = 0, Autopilot = 1, Radar = 2, Chart Plotter = 3, AIS = 4
	if (alarmGroup == 1) {
		root["autopilot"]["model"] = AUTOPILOT_MODEL::RAYMARINE_EVOLUTION;
		root["autopilot"]["alarm"] = RaymarineAlarmMessages.at(alarmCode);

		if (root.Size() > 0) {
			writer.Write(root, *jsonResponse);
			return TRUE;
		}
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
	// 0xF0 - Seatalk Datagram, 0x81 indicates the Source, Autopilot Controller
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
				// Lower 2 bits of the high nibble of U * 90
				int heading = (((payload[5] & 0x30) >> 4) * 90);
				// Lower 6 bits of VW
				heading += ((payload[6] & 0x3F) * 2);
				// Number of bits set in the two highest bits of the low nibble of U
				heading += (payload[5] & 0x0C) > 8 ? 2 : (payload[5] & 0x0C) == 8 ? 1 : 0;

				// MSB in the high nibble of U
				bool direction = (payload[5] & 0x80) == 0x80;
				// Rudder position, two's complement
				char rudderPosition = (~payload[7]) + 1;
				}
				break;
		} 
		
	}

	return FALSE;

}

// BUG BUG To Do
// Simrad AC12
// PGN 65380
bool TwoCanAutoPilot::DecodeAC12Autopilot(const byte *payload, wxString *jsonResponse) {

	return TRUE;
}


// Simrad Event Command 
// Usually sent from MFD to control the Autopilot, 
// however alarm codes are also sent from the Autopilot using this PGN
// PGN 130850
bool TwoCanAutoPilot::DecodeNAC3Command(const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned int manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte industryCode;
	industryCode = (payload[1] & 0xE0) >> 5;

	byte autopilotControllerAddress;
	autopilotControllerAddress = payload[2];

	byte autopilotModel;
	autopilotModel = payload[4];

	byte request;
	request = payload[5];

	switch (request) {

		case 0x0A: {
			unsigned short autoPilotCommand;
			autoPilotCommand = payload[6] | (payload[7] << 8);

			byte direction;
			direction = payload[8];

			short angle;
			angle = payload[9] | (payload[10] << 8);

			byte reservedB;
			reservedB = payload[11];

			switch (autoPilotCommand) {
				case 0x06:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::STANDBY;
					break;
				case 0x09:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::COMPASS;
					break;
				case 0x0F:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::WIND;
					break;
				case 0x0A:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::NAV;
					break;
				case 0x0C:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::NODRIFT;
					break;
				case 0x1A:
					// BUG BUG Really don't need to parse this, not relevant to UI
					if (TwoCanUtils::IsDataValid(angle)) {
						root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
						root["autopilot"]["rudderAngle"] = RADIANS_TO_DEGREES((float)angle * 1e-4);
					}
					break;
				default:
					// Could log a message but without knowing what exactly sent this...
					break;
			} // end switch autopilot command
		} // end case 0x00
			break;
		case 0xFF:
			root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
			root["autopilot"]["alarm"] = wxEmptyString; // This PGN only provides an alarm code
			break;
		default:
			break;
	} // end switch request

	// Leave the status processing to PGN 65305
	//if (root.Size() > 0) {
	//	writer.Write(root, *jsonResponse);
	//	return TRUE;
	//}


	return FALSE;
}

// Sent by Simrad Autopilot in response to receiving a command from a MFD
// Simply echoes what was sent in PGN 130850
// PGN 130851 Simrad Event Reply
bool DecodeNAC3Reply(const byte* payload, wxString* jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned short manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte reserved;
	reserved = (payload[1] & 0x18) >> 3;

	byte industryGroup;
	industryGroup = (payload[1] & 0xE0) >> 5;

	byte controllerAddress;
	controllerAddress = payload[2];

	unsigned short reservedA;
	reservedA = payload[3] | (payload[4] << 8);

	byte reply;
	reply = payload[5];

	unsigned short autoPilotCommand;
	autoPilotCommand = payload[6] | (payload[7] << 8);

	byte direction;
	direction = payload[8];

	short angle;
	angle = payload[9] | (payload[10] << 8);

	byte reservedB;
	reservedB = payload[11];

	switch (reply) {

		case 0x0A:

			switch (autoPilotCommand) {
				case 0x06:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::STANDBY;
					break;
				case 0x09:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::COMPASS;
					break;
				case 0x0F:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::WIND;
					break;
				case 0x0A:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::NAV;
					break;
				case 0x0C:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					root["autopilot"]["mode"] = AUTOPILOT_MODE::NODRIFT;
					break;
				case 0x1A:
					root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
					if (TwoCanUtils::IsDataValid(angle)) {
						root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
						root["autopilot"]["mode"] = RADIANS_TO_DEGREES((float)angle * 1e-4);
					}
					break;
				default:
					break;
			} // end case autopilot command
			break;
		default:
			break;
	}// end switch reply

	// Leave the status processing to PGN 65305
	//if (root.Size() > 0) {
	//	writer.Write(root, *jsonResponse);
	//	return TRUE;
	//}
	
	return FALSE;
}


// Simrad Alarm Message, sent by Autopilot. The alarm code is also sent in PGN 130850
// PGN 130856
bool TwoCanAutoPilot::DecodeNAC3AlarmMessage(const byte* payload, wxString* jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;

	unsigned short manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte reserved;
	reserved = (payload[1] & 0x18) >> 3;

	byte industryGroup;
	industryGroup = (payload[1] & 0xE0) >> 5;

	byte alarmCode;
	alarmCode = payload[2];

	byte alarmState;
	alarmState = payload[3];

	std::string message;
	unsigned int messageLength = payload[4];
	if (payload[5] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
		for (size_t i = 0; i < messageLength - 2; i++) {
			message.append(1, payload[6 + i]);
		}

		root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
		root["autopilot"]["alarm"] = message;

	}

	if(root.Size() > 0) {
		writer.Write(root, *jsonResponse);
		return TRUE;
	}

	return FALSE;
}


// Sent by Simrad Autopilot to advertise its status and mode
// Also sent by MFD's as a Keep Alive
// PGN 65305
bool TwoCanAutoPilot::DecodeNAC3Status(const byte *payload, wxString *jsonResponse) {
	wxJSONValue root;
	wxJSONWriter writer;
	
	unsigned short manufacturerId;
	manufacturerId = payload[0] | ((payload[1] & 0x07) << 8);

	byte reserved;
	reserved = (payload[1] & 0x18) >> 3;

	byte industryGroup;
	industryGroup = (payload[1] & 0xE0) >> 5;

	// Heartbeats sent from the MFD
	// 0x41 0x9F 0x01 0x0B 0x00 0x00 0x00 0x00 
	// 0x41 0x9F 0x01 0x03 0x00 0x00 0x00 0x00

	// Autopilot Model
	// Byte 2 (0x64) indicates NAC3
	// Byte 2 (0x00) indicates AC12
	// Byte 2 (0xFF) Indicates TP32

	// Byte 3 (0x02) is a Status report (Engaged or Standby)
	// Byte 3 (0x0A) is a Mode report

	
	// NAC 3 - Mode reports
	// 0x41 0x9F 0x64 0x0A 0x40 0x00 0x00 0x40 - NAV 
	// 0x41 0x9F 0x64 0x0A 0x00 0x04 0x00 0x04 - Wind
	// 0x41 0x9F 0x64 0x0A 0x10 0x00 0x00 0x00 - Heading
	// 0x41 0x9F 0x64 0x0A 0x00 0x01 0x00 0x00 - No Drift
	// 0x41 0x9F 0x64 0x0A 0x08 0x00 0x00 0x00 - Standby

	// NAC 3 Status reports
	// Byte 4 (0x10) = Engaged
	// Byte 4 (0x02) = Standby
	// Byte 4 0x04 =  NFU (what is 0x06?)
	// 0x41 0x9f 0x64 0x02 0x10 0x00 0x00 0x00 - Engaged
	// 0x41 0x9f 0x64 0x02 0x02 0x00 0x00 0x00 - Standby
	// 0x41 0x9F 0x64 0x02 0x04 0x00 0x00 0x00 - NFU
	// 0x41 0x9F 0x64 0x02 00x6 0x00 0x00 0x00 - Also NFU ?? Puzzling

	// AC-12 - Mode report BUG BUG Confirm
	// 0x41 0x9f 0x00 0x0a 0x06 0x04 0x00 0x00 - Wind
	// 0x41 0x9f 0x00 0x0a 0x1e 0x00 0x00 0x00 - Wind ??
	// 0x41 0x9f 0x00 0x0a 0x0a 0x00 0x00 0x00 - Heading
	// 0x41 0x9f 0x00 0x0a 0x16 0x00 0x00 0x00 - Heading ??
	// 0x41 0x9f 0x00 0x0a 0xf0 0x00 0x80 0x00 - Nav
	// 0x41 0x9f 0x00 0x0a 0x16 0x01 0x00 0x00 - No Drift ??
	// 0x41 0x9f 0x00 0x0a 0x0c 0x00 0x80 0x00 - No Drift ??


	// TP-32
	// Byte 3 (0x0A) - Mode report
	// 0x41 0x9F 0xFF 0x0A 0x14 0x04 0x80 0x05 - 'U-Turn or Wind (last 2 digits wind angle??)

	// Byte 3 (0x02) - Status report
	// 0x41 0x9F 0xFF 0x02 0x10 0x00 0x00 0x00


	// Byte 3 (0x1D) - Unknown
	// 0x41 0x9F 0xFF 0x1D 0x81 0x00 0x00 0x00

	byte autopilotModel;
	autopilotModel = payload[2];

	switch (autopilotModel) {
		case 0x00:
			root["autopilot"]["model"] = AUTOPILOT_MODEL::SIMRAD_AC12;
			break;
		case 0x64:
			root["autopilot"]["model"] = AUTOPILOT_MODEL::NAVICO_NAC3;
			break;
		case 0xFF:
			// BUG BUG Need an enum for TP32
			root["autopilot"]["model"] = AUTOPILOT_MODEL::SIMRAD_AC12;
			break;
		case 0x01:
			// Autopilot Heartbeat
			break;
		default:
			// Autopilot Unknown
			break;
	}

	byte command;
	command = payload[3];

	switch (command) {

		case 0x02: // Status
			if (payload[4] == 0x10) {
				// Engaged
			}
			else if (payload[4] == 0x02) {
				root["autopilot"]["mode"] = AUTOPILOT_MODE::STANDBY;
			}
			else if (payload[4] == 0x04) {
				root["autopilot"]["mode"] = AUTOPILOT_MODE::NFU;
			}
			break; // break 02

		case 0x0A: // Mode
			if (autopilotModel == 0x64) { // Navico NAC-3

				unsigned short mode;
				mode = payload[4] | (payload[5] << 8);

				switch (mode) {
					case 0x40:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::NAV;
						break;
					case 0x0400:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::WIND;
						break;
					case 0x10:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::COMPASS;
						break;
					case 0x100:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::NODRIFT;
						break;
					case 0x08:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::STANDBY;
						break;

					default:
						// Unrecognized mode
						break;

				} // end switch mode

			} // end if autopilot model NAC-3

			else if (autopilotModel == 0x00) { // AC12

				unsigned short mode;
				mode = payload[4] | (payload[5] << 8);

				switch (mode) {
					case 0xF0:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::NAV;
						break;
					case 0x1E:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::WIND;
						break;
					case 0x0A:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::COMPASS;
						break;
					case 0x0C:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::NODRIFT;
						break;
					case 0x14:
						// BUG BUG ToDO U-Turn
						break;
					case 0x00:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::STANDBY;
						break;
					default:
						// "Unrecognized mode
						break;

				} // end switch mode

			} // end if autopilot model AC 12

			else if (autopilotModel == 0xFF) {	// TP-32
				// To verify
				// Standby = 0x00 0x0A
				unsigned short mode;
				mode = payload[4] | (payload[5] << 8);

				switch (mode) {
					case 0x40:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::NAV;
						break;
					case 0x0400:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::WIND;
						break;
					case 0x10:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::COMPASS;
						break;
					case 0x100:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::NODRIFT;
						break;
					case 0x08:
						root["autopilot"]["mode"] = AUTOPILOT_MODE::STANDBY;
						break;
					default:
						// Unrecognized mode
						break;

				} // end switch mode


			} // end if autopilot model TP-32

			else {
				// Unrecognized Autopilot Model
			}

			// BUG BUG Confirm for all autopilot models
			short angle;
			angle = payload[6] | (payload[7] << 8);
			if (TwoCanUtils::IsDataValid(angle)) {
				if (root["autopilot"]["mode"].AsInt() == AUTOPILOT_MODE::WIND) {
					root["autopilot"]["windangle"] = RADIANS_TO_DEGREES((float)angle * 1e-4);
				}
				else if ((root["autopilot"]["mode"].AsInt() == AUTOPILOT_MODE::NAV) || 
					(root["autopilot"]["mode"].AsInt() == AUTOPILOT_MODE::COMPASS)) {
					root["autopilot"]["angle"] = RADIANS_TO_DEGREES((float)angle * 1e-4);
				}

			}
		
			break; // end switch 0A

		// Heartbeat
		case 0x03: 
		case 0x0B:
			break;

		// Unkown command
		default:
			break;

	} // end switch command
	
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

// Furuno Navpilot
bool TwoCanAutoPilot::DecodeFurunoAutopilot(const byte* payload, wxString* jsonResponse) {
	return TRUE;
}

// These three are invoked upon reception of PGN 127245 (Rudder Angle), 127250 (Heading) & 130306 (Wind)
// to generate JSON and send OCPN TWOCAN_AUTOPILOT_RESPONSE to update UI on TwoCan Autopilot plugin
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

		else if (root["autopilot"].HasMember("heading")) {
				commandId = AUTOPILOT_COMMAND::CHANGE_HEADING;
				commandValue = root["autopilot"]["heading"].AsInt();

		}

		else if (root["autopilot"].HasMember("windangle")) {
				commandId = AUTOPILOT_COMMAND::CHANGE_WIND;
				commandValue = root["autopilot"]["windangle"].AsInt();
		}

		else if (root["autopilot"].HasMember("keepalive")) {
				commandId = AUTOPILOT_COMMAND::KEEP_ALIVE;
		}

		else {
			// For some reason an invalid command or value
			return FALSE;
		}
		
		// Now parse the commands and generate the approprite PGN's
		switch (autopilotModel) {
		
			case AUTOPILOT_MODEL::RAYMARINE_EVOLUTION:

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
				} // end switch command id

				break; // end raymarine

			case AUTOPILOT_MODEL::SIMRAD_AC12:

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

				} // end switch command id

				break; // end AC-12

				case AUTOPILOT_MODEL::NAVICO_NAC3:
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

			case AUTOPILOT_MODEL::GARMIN_REACTOR:
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
				} // end swicth command id

				break; // end garmin


			case AUTOPILOT_MODEL::FURUNO_NAVPILOT:

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
						// Adjust Wind Angle
						break;

				} // end switch command if

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
bool TwoCanAutoPilot::EncodePGN130850(int command) {
	std::vector<byte>payload;

	//,41,9f,%s,ff,ff,%s,00,00,00,ff
	payload.push_back(0x41); // This is the usual manufacturer code/industry code
	payload.push_back(0x9F);
	payload.push_back(command);
	payload.push_back(0xFF); //unknown
	payload.push_back(autopilotControllerAddress);
	payload.push_back(command);
	payload.push_back(0xFF); // Mode
	payload.push_back(0xFF); // Mode
	payload.push_back(0xFF); // direction
	payload.push_back(0xFF); // angle
	payload.push_back(0xFF); // angle
	payload.push_back(0xFF); // unknown

	return FALSE;
}

// The nav PGNS's, 129283 (XTE) and 129284 (Navigation) are encoded as JSON
// in the Autopilot Plugin

/*

void EncodePGN129283(void) {
	std::vector<byte> payload;

	byte sid = 0x0A;
	payload.push_back(sid);

	byte xteMode = 0;
	byte navigationTerminated = 0; 0 = No

	payload.push_back((xteMode & 0x0F) |  0x30 | ((navigationTerminated << 6) & 0xC0));

	payload.push_back(crossTrackError & 0xFF);
	payload.push_back((crossTrackError >> 8) & 0xFF);
	payload.push_back((crossTrackError >> 16) & 0xFF);
	payload.push_back((crossTrackError >> 24) & 0xFF);

}


void EncodePGN129284(void) {
std::vector<byte> payload;
	
	byte sequenceId = 0xA0;
	payload.push_back(sequenceId);

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

}

// No idea why I have heading and track control here
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
*/