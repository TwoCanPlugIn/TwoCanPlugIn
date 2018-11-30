// Copyright(C) 2018 by Steven Adler
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

#ifndef TWOCAN_UTILS_H
#define TWOCAN_UTILS_H

// For specific Windows functions & typedefs such as LPDWORD etc.
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

// For wxWidgets Pi
#include <wx/math.h>

// NMEA 2000 Device Information, transmitted in PGN 60928 ISO Address Claim
typedef struct DeviceInformation {
	// Network Address is not part of the address claim, but it is the source address of the frame. 
	// Maintain a reference to it to use as an index into entries for a network map
	byte networkAddress;
	unsigned int uniqueId;
	unsigned int deviceClass;
	unsigned int deviceFunction;
	byte deviceInstance;
	byte industryGroup;
	unsigned int manufacturerId;
} DeviceInformation;

// Some NMEA 2000 constants
#define CONST_GLOBAL_ADDRESS 255
#define CONST_HEADER_LENGTH 4
#define CONST_PAYLOAD_LENGTH 8
#define CONST_FRAME_LENGTH (CONST_HEADER_LENGTH + CONST_PAYLOAD_LENGTH)
// Note to self, I think this is correct, address 255 is the global address, 254 may be used for something else ??
#define CONST_MAX_DEVICES 253 
// Maximum payload for NMEA multi-frame Fast Message
#define CONST_MAX_FAST_PACKET 223_LENGTH
// Maximum payload for ISO 11783 Multi Packet
#define CONST_MAX_ISO_MULTI_PACKET_LENGTH 1785 

// Maximum number of multi-frame Fast Messages we can support, just an arbitary number
#define CONST_MAX_MESSAGES 100
// Stale Fast Message expiration  (I think Fast Messages must be sent within 250 msec)
#define CONST_TIME_EXCEEDED 250
// Whether an existing Fast Message entry exists, in order to append a frame
#define NOT_FOUND -1


// Some useful conversion functions
// Radians to degrees and vice versa
#define RADIANS_TO_DEGREES(x) (x * 180 / M_PI)
#define DEGREES_TO_RADIANS(x) (x * M_PI / 180)

// Metres per second
#define CONVERT_MS_KNOTS 1.94384
#define CONVERT_MS_KMH 3.6
#define CONVERT_MS_MPH 2.23694

// Metres to feet, fathoms
#define CONVERT_FATHOMS_FEET 6
#define CONVERT_METRES_FEET 3.28084
#define CONVERT_METRES_FATHOMS (CONVERT_METRES_FEET / CONVERT_FATHOMS_FEET)
#define CONVERT_METRES_NATICAL_MILES 0.000539957

// Kelvin to celsius
#define CONST_KELVIN -273.15
#define CONVERT_KELVIN(x) (x + CONST_KELVIN )

// NMEA 183 GPS Fix Modes
#define GPS_MODE_AUTONOMOUS 'A' 
#define GPS_MODE_DIFFERENTIAL 'D' 
#define GPS_MODE_ESTIMATED 'E' 
#define GPS_MODE_MANUAL  'M'
#define GPS_MODE_SIMULATED 'S'
#define GPS_MODE_INVALID 'N' 

// Some defintions used in NMEA 2000 PGN's

#define HEADING_TRUE 0
#define HEADING_MAGNETIC 1

#define	GNSS_FIX_NONE 0
#define	GNSS_FIX_GNSS 1
#define	GNSS_FIX_DGNSS 2
#define	GNSS_FIX_PRECISE_GNSS 3
#define	GNSS_FIX_REAL_TIME_KINEMATIC_INT 4
#define	GNSS_FIX_REAL_TIME_KINEMATIC_FLOAT 5
#define	GNSS_FIX_ESTIMATED 6
#define	GNSS_FIX_MANUAL 7
#define	GNSS_FIX_SIMULATED 8

#define	GNSS_INTEGRITY_NONE 0
#define	GNSS_INTEGRITY_SAFE 1
#define	GNSS_INTEGRITY_CAUTION 2

#define	WIND_REFERENCE_TRUE 0
#define	WIND_REFERENCE_MAGNETIC 1
#define	WIND_REFERENCE_APPARENT 2
#define	WIND_REFERENCE_BOAT_TRUE 3
#define	WIND_REFERENCE_BOAT_MAGNETIC 4

#define	TEMPERATURE_SEA 0
#define	TEMPERATURE_EXTERNAL 1
#define	TEMPERATURE_INTERNAL 2
#define	TEMPERATURE_ENGINEROOM 3
#define	TEMPERATURE_MAINCABIN 4
#define	TEMPERATURE_LIVEWELL 5
#define	TEMPERATURE_BAITWELL 6
#define	TEMPERATURE_REFRIGERATOR 7
#define	TEMPERATURE_HEATING 8
#define	TEMPERATURE_DEWPOINT 9
#define	TEMPERATURE_APPARENTWINDCHILL 10
#define	TEMPERATURE_THEORETICALWINDCHILL 11
#define	TEMPERATURE_HEATINDEX 12
#define	TEMPERATURE_FREEZER 13
#define	TEMPERATURE_EXHAUST 14


// Bit values to determine what NMEA 2000 PGN's are converted to their NMEA 0183 equivalent
// Warning must match order of items in Preferences Dialog !!
#define FLAGS_HDG 1
#define FLAGS_VHW 2 
#define FLAGS_DPT 4
#define FLAGS_GLL 8
#define FLAGS_VTG 16
#define FLAGS_GGA 32
#define FLAGS_ZDA 64
#define FLAGS_MWV 128
#define FLAGS_MWT 256
#define FLAGS_DSC 512
#define FLAGS_AIS 1024


// Bit values to determine what Logging to support
// RAW is the only mode currently implemented
#define FLAGS_LOG_RAW 1 // My format 12 pairs of hex digits, 4 the CAN 2.0 Id, 8 the payload 
#define FLAGS_LOG_KEES 2 // I recall seeing this format used in Open Skipper
#define FLAGS_LOG_ACTISENSE 4 // And this one as well.
#define FLAGS_LOG_YACHTDEVICES 8 
#define FLAGS_LOG_CANDUMP 16


// All the NMEA 2000 data is transmitted as an unsigned char which for convenience sake, I call a byte
typedef unsigned char byte;

// CAN v2.0 29 bit header as used by NMEA 2000
typedef struct CanHeader {
	byte priority;
	byte source;
	byte destination;
	unsigned int pgn;
} CanHeader;

// Utility functions used by both the TwoCanDevice and the CAN adapters

class TwoCanUtils {
	
public:
	
	// Convert four bytes to an integer so that some NMEA 2000 values can be 
	// derived from odd length, non byte aliged bitmasks
	// Can't use memcpy or casting due to Endianess
	static int ConvertByteArrayToInteger(const byte *buf, unsigned int *value);

	// BUG BUG Any other conversion functions required ??
	
};

#endif