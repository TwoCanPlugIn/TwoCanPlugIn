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

// If we are in Active Mode, whether we can control an Autopilot. 
extern bool enableAutopilot;
// If we can control an AUtopilot, what model. 
// 0 - None, 1 Garmin, 2 Raymarine, 3 Simrad AC-12, 4 Navico NAC-3, 5 Furuno
extern AUTOPILOT_MODEL autopilotModel; 

// A 1 byte CAN bus network address for this device if it is an Active device (0-253)
extern int networkAddress;

#define AUTOPILOT_CHANGE_STATUS 0
#define AUTOPILOT_CHANGE_COURSE 1
#define AUTOPILOT_CHANGE_MANUFACTURER 2

// These must match the UI radio box values
#define AUTOPILOT_POWER_OFF 0
#define AUTOPILOT_POWER_STANDBY 1
#define AUTOPILOT_MODE_HEADING 2
#define AUTOPILOT_MODE_WIND 3
#define AUTOPILOT_MODE_GPS 4

// These are also defined in twocansettings as a hashmap
#define RAYMARINE_MANUFACTURER_CODE 1851
#define SIMARD_MANUFACTURER_CODE 1857
#define GARMIN_MANUFACTURER_CODE 229

#define MARINE_INDUSTRY_CODE 4;


// The TwoCan Autopilot
class TwoCanAutopilot {

public:
	// The constructor
	TwoCanAutopilot(AUTOPILOT_MODEL model);

	// and destructor
	~TwoCanAutopilot(void);

	// Generates NMEA 2000 autopilot messages
	bool EncodeAutopilotCommand(wxString message_body, std::vector<CanMessage> *nmeaMessages);

protected:
private:

};
#endif