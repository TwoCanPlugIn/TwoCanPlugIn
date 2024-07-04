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

#ifndef TWOCAN_AUTOPILOT_H
#define TWOCAN_AUTOPILOT_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
      #include <wx/wx.h>
#endif

#include "twocanutils.h"

// STL 
#include <vector>

// JSON stuff
#include <wx/json_defs.h>
#include <wx/jsonval.h>
#include <wx/jsonreader.h>
#include <wx/jsonwriter.h>

// If we can control an Autopilot, what model. 
// 0 - None, 1 Garmin, 2 Raymarine, 3 Simrad AC-12, 4 Navico NAC-3, 5 Furuno
extern AUTOPILOT_MODEL autopilotModel;

// Map of the devices on the network. Iterated to fimd the address of an autopilot controller
extern NetworkInformation networkMap[CONST_MAX_DEVICES];

// A 1 byte CAN bus network address for this device if it is an Active device (0-253)
extern int networkAddress;

// Autopilot Modes
// These must match the those used in TwoCan Autopilot plugin
typedef enum _AUTOPILOT_MODE {
	STANDBY,
	COMPASS,
	NAV,
	WIND,
	NODRIFT
} AUTOPILOT_MODE;

// Autopilot commands 
typedef enum _AUTOPILOT_COMMAND {
	CHANGE_MODE,
	CHANGE_HEADING,
	CHANGE_WIND,
	KEEP_ALIVE
} AUTOPILOT_COMMAND;

// These are also defined in twocansettings as a hashmap
// BUG BUG Not sure why they are here, do I actually use them ??
#define RAYMARINE_MANUFACTURER_CODE 1851
#define SIMRAD_MANUFACTURER_CODE 1857
#define GARMIN_MANUFACTURER_CODE 229
#define NAVICO_MANUFACTURER_CODE 275
#define BANDG_MANUFACTURER_CODE 381
#define FURUNO_MANUFACTURER_CODE 1855

#define MARINE_INDUSTRY_CODE 4;

#define NAC3_DIRECTION_PORT 2
#define NAC3_DIRECTION_STBD 3
#define NAC3_DIRECTION_UNUSED 255;



// The TwoCan Autopilot
class TwoCanAutoPilot {

public:
	// The constructor
	TwoCanAutoPilot(AUTOPILOT_MODEL model);

	// and destructor
	~TwoCanAutoPilot(void);

	// Iterate the network Map to find the autopilots network address
	bool FindAutopilot(void);

	// Generates NMEA 2000 autopilot messages depending on the manufacturer
	// Commands are issued by TwoCanAutopilot plugin or anything else for that matter
	// that sends TWOCAN_AUTOPILOT_REQUEST messages
	bool EncodeAutopilotCommand(wxString message_body, std::vector<CanMessage> *nmeaMessages);

	// Raymarine Evolution Autopilot Heading (PGN 65359 & 65360)
	bool DecodeRaymarineAutopilotHeading(const int pgn, const byte *payload, wxString *jsonResponse);

	// Raymarine Evolution Autopilot Wind (PGN 65435)
	bool DecodeRaymarineAutopilotWind(const int pgn, const byte *payload, wxString *jsonResponse);

	// Raymarine Evolution Autopilot Mode (PGN 65379)
	bool DecodeRaymarineAutopilotMode(const byte *payload, wxString *jsonResponse);

    // Raymarine Alarms (PGN 65288)
    bool DecodeRaymarineAutopilotAlarm(const byte* payload, wxString* jsonResponse);

	// Raymarine Seatalk Datagrams (PGN 126720)
	bool DecodeRaymarineSeatalk(const byte *payload, wxString *jsonResponse);

	// Navico Autopilot Alarm Message (NAC-3) PGN 130856
	bool DecodeNAC3AlarmMessage(const byte *payload, wxString *jsonResponse);

	// Navico Autopilot Mode and Status (NAC-3 and AC12) PGN 65305
	bool DecodeNAC3Status(const byte *payload, wxString *jsonResponse);

    // Navico Autopilot Command (NAC-3 and AC12) PGN 130850/851
    bool DecodeNAC3Command(const byte* payload, wxString* jsonResponse);

	// Simrad Autopilot Alarm (AC12) PGN 65380
	bool DecodeAC12Autopilot(const byte *payload, wxString *jsonResponse);

	// Garmin Reactor
	// BUG BUG To Do.
	bool DecodeGarminAutopilot(const byte *payload, wxString *jsonResponse);

    // BUG BUG Furuno To Do 

	// Encode some of the values to be displayed on the Autopilot Plugin as JSON 
	bool EncodeRudderAngle(const int rudderangle, wxString *jsonResponse);
	bool EncodeHeading(const unsigned int heading, wxString *jsonResponse);
	bool EncodeWindAngle(const int windangle, wxString *jsonResponse);

protected:

private:
	// The network address of the autopilot controller
	byte autopilotControllerAddress;

	// Seems like Navico NAC3 Autopilots have a keep alive that toggles one bit in the payload.
	bool navicoKeepAliveToggle = false;

};
#endif