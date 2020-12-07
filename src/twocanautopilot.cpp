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
// Unit: Autoplot Control
// Owner: twocanplugin@hotmail.com
// Date: 01/12/2020
// Version History: 
// 1.0 Initial Release of Autopilot Control

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
TwoCanAutopilot::TwoCanAutopilot(int mode) {
    // Save the autopilot model, 0 - None, 1 - Garmin, 2 - Navico, 3 - Raymarine
    autopilotMode = mode;
}

TwoCanAutopilot::~TwoCanAutopilot(void) {
}

void TwoCanAutopilot::ActivateRoute(wxString name) {
  isRouteActive = TRUE;
  routeName = name;
}

void TwoCanAutopilot::DeactivateRoute(void) {
  isRouteActive = FALSE;
  routeName = wxEmptyString;
}


bool TwoCanAutopilot::ParseCommand(int commandId, int commandValue, std::vector<CanMessage> *nmeaMessages) {
    CanMessage message;
    CanHeader header;
    switch (autopilotMode) {
        case FLAGS_AUTOPILOT_GARMIN:
            switch (commandId) {
                case AUTOPILOT_POWER_EVENT:
                    if (commandValue == 0) {
                        // Turn Off 
                    }
                    else if (commandValue == 1) {
                    }
                    else if (commandValue == 2) {
                        // Turn On
                    }
                break;
                case AUTOPILOT_MODE_EVENT:
                    if (commandValue == 0) {
                        // heading Mode 
                    }
                    else if (commandValue == 1) {
                        // Wind mode
                    }
                    else if (commandValue == 2) {
                        // GPS mode
                    }
                break;
                case AUTOPILOT_HEADING_EVENT:
                    // Adjust Heading
                break;
            }

            break;
        case FLAGS_AUTOPILOT_RAYMARINE:
            switch (commandId) {
                case AUTOPILOT_POWER_EVENT:
                    if (commandValue == 0) {
                        // Turn Off 
                    }
                    else if (commandValue == 1) {
                        // set to Standby
                        header.pgn = 0;
                        header.destination = CONST_GLOBAL_ADDRESS;
                        header.priority = CONST_PRIORITY_VERY_HIGH;
                        header.source =networkAddress;
                        message.header = header;
                        message.payload = raymarineStandby;
                        nmeaMessages->push_back(message);
                        return TRUE;
                    }
                    else if (commandValue == 2) {
                        // set to On
                        header.pgn = 0;
                        header.destination = CONST_GLOBAL_ADDRESS;
                        header.priority = CONST_PRIORITY_VERY_HIGH;
                        header.source =networkAddress;
                        message.header = header;
                        message.payload = raymarineTrack;
                        nmeaMessages->push_back(message);
                        return TRUE;
                    }
                break;
                case AUTOPILOT_MODE_EVENT:
                    if (commandValue == 0) {
                        // set to Compass Heading Mode
                        header.pgn = 0;
                        header.destination = CONST_GLOBAL_ADDRESS;
                        header.priority = CONST_PRIORITY_VERY_HIGH;
                        header.source =networkAddress;
                        message.header = header;
                        message.payload = raymarineHeading;
                        nmeaMessages->push_back(message);
                        return TRUE;
                    }
                    else if (commandValue == 1) {
                        // set to Wind Mode
                        
                    }
                    else if (commandValue == 2) {
                        // set to GPS mode
                        header.pgn = 0;
                        header.destination = CONST_GLOBAL_ADDRESS;
                        header.priority = CONST_PRIORITY_VERY_HIGH;
                        header.source =networkAddress;
                        message.header = header;
                        message.payload = raymarineTrack;
                        nmeaMessages->push_back(message);
                        return TRUE;
                    }
                break;
                case AUTOPILOT_HEADING_EVENT:
                    // Adjust Heading
                break;
            }

            break;

        case FLAGS_AUTOPILOT_NAVICO:
            switch (commandId) {
                case AUTOPILOT_POWER_EVENT:
                    if (commandValue == 0) {
                        // Turn Off 
                    }
                    else if (commandValue == 1) {
                        // set to Standby
                    }
                    else if (commandValue == 2) {
                        // Turn On
                    }
                break;
                case AUTOPILOT_MODE_EVENT:
                    if (commandValue == 0) {
                        // heading Mode 
                    }
                    else if (commandValue == 1) {
                        // Wind mode
                    }
                    else if (commandValue == 2) {
                        // GPS mode
                    }
                break;
                case AUTOPILOT_HEADING_EVENT:
                    // Adjust Heading
                break;
            }

            break;


            break;
        default:
            return FALSE;
            break;
    }


return TRUE;    
}

