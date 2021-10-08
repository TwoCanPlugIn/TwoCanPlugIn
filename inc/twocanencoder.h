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
// NMEA2000� is a registered Trademark of the National Marine Electronics Association

#ifndef TWOCAN_ENCODER_H
#define TWOCAN_ENCODER_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

// AIS Decoding/Encoding (also includes the usual twocan & nmea183 includes)
#include "twocanais.h"

// NMEA 183 GNSS Satellite information
#include "satinfo.h"

// STL
#include <vector>
#include <algorithm>
#include <bitset>
#include <iostream>
#include <functional>
//#include <bits/stdc++.h> 
//#include <typeinfo>

// wxWidgets
// String Format, Comparisons etc.
#include <wx/string.h>
// For converting NMEA 2000 date & time data
#include <wx/datetime.h>
// Raise events to the plugin
#include <wx/event.h>
// Logging (Info & Errors)
#include <wx/log.h>

// Global variables
// A 1 byte CAN bus network address for this device if it is an Active device (0-253)
extern int networkAddress;

// Bit mask determing what NMEA 2000 PGN's are to be converted to NMEA 183 sentences
// For the gateway, logical not determines if we convert from NMEA 183 to NMEA 2000
extern int supportedPGN;

class TwoCanEncoder {

public:

	TwoCanEncoder(void);

	~TwoCanEncoder(void);

	// Validate XDR Transducer Names
	int GetInstanceNumber(wxString transducerName);

	// Fragment fast messages into sequences of messages
	void FragmentFastMessage(CanHeader *header, std::vector<byte> *payload, std::vector<CanMessage> *canMessages);

	// The big switch statement that determines the conversion of 
	// NMEA 183 sentences to NMEA 2000 messages
	bool EncodeMessage(wxString sentence, std::vector<CanMessage> *canMessages);

	// The following routines convert a NMEA 183 sentence to a NMEA 2000 message

	// Encode PGN 126992 NMEA System Time
	bool EncodePGN126992(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode payload PGN 127233 NMEA Man Overboard (MOB)
	bool EncodePGN127233(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 127245 NMEA Rudder Angle
	bool EncodePGN127245(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 127250 NMEA Vessel Heading
	bool EncodePGN127250(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 127251 NMEA Rate of Turn (ROT)
	bool EncodePGN127251(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 1272571 NMEA Attitude
	bool EncodePGN127257(const short yaw, const short pitch, const short roll, std::vector<byte> *n2kMessage);

	// Encode PGN 127258 NMEA Magnetic Variation
	bool EncodePGN127258(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 127488 NMEA Engine Rapid Update
	bool EncodePGN127488(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 127489 NMEA Engine Static Parameters
	bool EncodePGN127250(const byte engineInstance, const unsigned short oilPressure, const unsigned short engineTemperature, const unsigned short alternatorPotential, std::vector<byte> *n2kMessage);

	// Encode PGN 128259 NMEA Speed & Heading
	bool EncodePGN128259(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 128267 NMEA Depth
	bool EncodePGN128267(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 128275 Distance Log
	bool EncodePGN128275(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129025 NMEA Position Rapid Update
	bool EncodePGN129025(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129026 NMEA COG SOG Rapid Update
	bool EncodePGN129026(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129029 NMEA GNSS Position
	bool EncodePGN129029(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129033 NMEA Date & Time
	bool EncodePGN129033(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129283 NMEA Cross Track Error (XTE)
	bool EncodePGN129283(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129284 Navigation Data
	bool EncodePGN129284(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129285 Navigation Route/WP Information
	bool EncodePGN129285(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129540 GNSS Satellites in View
	bool EncodePGN129540(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 129796 AIS Acknowledge 
	// Encode PGN 129797 AIS Binary Broadcast Message 

	// Encode PGN 129808 DSC Message
	bool EncodePGN129808(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130074 NMEA Route & Waypoint Service - Waypoint List
	bool EncodePGN130074(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130306 NMEA Wind
	bool EncodePGN130306(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130310 NMEA Water & Air Temperature and Pressure
	bool EncodePGN130310(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130311 NMEA Environmental Parameters (supercedes 130310)
	bool EncodePGN130311(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130312 NMEA Temperature
	bool EncodePGN130312(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130316 NMEA Temperature Extended Range
	bool EncodePGN130316(const NMEA0183 *parser, std::vector<byte> *n2kMessage);

	// Encode PGN 130577 NMEA Direction Data
	bool EncodePGN130577(const NMEA0183 *parser, std::vector<byte> *n2kMessage);
		
	private:	
	// NMEA 0183 parser
	NMEA0183 nmeaParser;

	// NMEA 2000 Message Sequence Id
	byte sequenceId;

	// GNSS Satellites in use
	SAT_INFO gpsSatelites[12];

	// AIS Decoding
	TwoCanAis *aisDecoder;
	

};

#endif