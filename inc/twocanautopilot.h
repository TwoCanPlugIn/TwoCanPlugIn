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

// STL 
#include <vector>

#include "twocanautopilotdialog.h"

// If we are in Active Mode, whether we can control an Autpilot. 0 - None, 1, Garmin, 2 Navico, 3 Raymarine
extern int autopilotMode; 

// A 1 byte CAN bus network address for this device if it is an Active device (0-253)
extern int networkAddress;

// The TwoCan Autopilot
class TwoCanAutopilot {

public:
	// The constructor
	TwoCanAutopilot(int mode);
	
	// and destructor
	~TwoCanAutopilot(void);

	// when we have an actve route
	void ActivateRoute(wxString name);
	void DeactivateRoute(void);


	// Generates NMEA 2000 autopilot messages
	bool ParseCommand(int commandId, int commandValue, std::vector<CanMessage> *nmeaMessages);

protected:
private:
	bool isRouteActive;
	wxString routeName;
	wxString waypointName;

};

#endif 

