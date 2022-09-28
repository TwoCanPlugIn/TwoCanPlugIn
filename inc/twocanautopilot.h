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

// If we are in Active Mode, whether we can control an Autpilot. 0 - None, 1, Garmin, 2 Navico, 3 Raymarine, 4 Furuno
extern int autopilotManufacturer; 

// Map of the devices on the network. Iterated to fimd the address of an autopilot controller
extern NetworkInformation networkMap[CONST_MAX_DEVICES];

// A 1 byte CAN bus network address for this device if it is an Active device (0-253)
extern int networkAddress;

// Autopilot commands
#define AUTOPILOT_CHANGE_STATUS 0
#define AUTOPILOT_CHANGE_COURSE 1
#define AUTOPILOT_CHANGE_WIND 2
#define AUTOPILOT_CHANGE_MANUFACTURER 3
#define AUTOPILOT_KEEP_ALIVE 4

// These must match the UI radio box values
// Autopilot Modes
#define AUTOPILOT_POWER_STANDBY 0
#define AUTOPILOT_MODE_COMPASS 1
#define AUTOPILOT_MODE_NAV 2
#define AUTOPILOT_MODE_WIND 3
#define AUTOPILOT_MODE_NODRIFT 4

typedef enum _AUTOPILOT_MODE {
	STANDBY,
	COMPASS,
	NAV,
	WIND,
	NODRIFT
} AUTOPILOT_MODE;


// These are also defined in twocansettings as a hashmap
#define RAYMARINE_MANUFACTURER_CODE 1851
#define SIMRAD_MANUFACTURER_CODE 1857
#define GARMIN_MANUFACTURER_CODE 229
#define NAVICO_MANUFACTURER_CODE 275
#define BANDG_MANUFACTURER_CODE 381
#define FURUNO_MANUFACTURER_CODE 1851

#define MARINE_INDUSTRY_CODE 4;

typedef enum _AUTOPILOT_MANUFACTURER {
	RAYMARINE = 1851,
	SIMRAD = 1857,
	GARMIN = 229,
	NAVICO = 275,
	BANDG = 381,
	FURUNO = 1851
} AUTOPILOT_MANUFACTURER;


#define NAC3_DIRECTION_PORT 2
#define NAC3_DIRECTION_STBD 3
#define NAC3_DIRECTION_UNUSED 255;

// The TwoCan Autopilot
class TwoCanAutoPilot {

public:
	// The constructor
	TwoCanAutoPilot(int mode);

	// and destructor
	~TwoCanAutoPilot(void);

	bool FindAutopilot(unsigned int autopilotAddress);

	// Generates NMEA 2000 autopilot messages depending on the manufacturer
	// Commands are issued by TwoCanAutopilot plugin or anything else
	// that sends TWOCAN_AUTOPILOT_REQUEST messages
	bool EncodeAutopilotCommand(wxString message_body, std::vector<CanMessage> *nmeaMessages);

	// Raymarine Evolution Autopilot Heading
	bool DecodeRaymarineAutopilotHeading(const int pgn, const byte *payload, wxString *jsonResponse);

	// Raymarine Evolution Autopilot Wind
	bool DecodeRaymarineAutopilotWind(const int pgn, const byte *payload, wxString *jsonResponse);

	// Raymarine Evolution Autopilot Mode
	bool DecodeRaymarineAutopilotMode(const byte *payload, wxString *jsonResponse);

	// Raymarine Seatalk Datagrams
	bool DecodeRaymarineSeatalk(const byte *payload, wxString *jsonResponse);

	// Navico NAC-2/3
	bool DecodeNAC3Autopilot(const byte *payload, wxString *jsonResponse);

	// Simrad AC12
	bool DecodeAC12Autopilot(const byte *payload, wxString *jsonResponse);

	// Garmin Reactor
	bool DecodeGarminAutopilot(const byte *payload, wxString *jsonResponse);

	// Rudder Angle - Decoded in twocandevice.cpp, but encode the autopilot json here
	bool DecodeRudderAngle(const int rudderanglw, wxString *jsonResponse);

protected:

private:
	// The network address of the autopilot controller
	byte autopilotAddress;

};
#endif