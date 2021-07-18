// Copyright(C) 2018-2021 by Steven Adler
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

#ifndef TWOCAN_AIS_H
#define TWOCAN_AIS_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

// wxWidgets
// String Format, Comparisons etc.
#include <wx/string.h>
// For converting NMEA 2000 date & time data
#include <wx/datetime.h>
// Logging (Info & Errors)
#include <wx/log.h>

// for std::reverse etc.
#include <algorithm>

// Error constants and macros
#include "twocanerror.h"

// Constants, typedefs and utility functions for bit twiddling and array manipulation for NMEA 2000 messages
#include "twocanutils.h"

// NMEA 183 Parsing (for gateway & autopilot functions)
#include "nmea0183.h"

// AIS Sequence Id, used to re-assemble multi-sentence AIS VDM messages
#define AIS_MAXIMUM_MESSAGE_ID 10

// Specific NMEA 0183 AIS invalid data constants
// SOG is in 1/10 knot
#define AIS_INVALID_SOG 1023
// COG is in 1/10 degree
#define AIS_INVALID_COG 3600
#define AIS_INVALID_ALTITUDE 4095
//60 if time stamp is not available(default)
//61 if positioning system is in manual input mode
//62 if Electronic Position Fixing System operates in estimated(dead reckoning) mode
//63 if the positioning system is inoperative.
#define AIS_INVALID_TIMESTAMP 60
// Heading is in degrees
#define AIS_INVALID_HEADING 511
#define AIS_INVALID_HOUR 24
#define AIS_INVALID_MINUTE 60
#define AIS_INVALID_SECOND 60
// Lat & long are in 1/10000 minutes, eg. divide by 600000, 181 & 91 degrees indicate  invalid longitide & latitude
#define AIS_INVALID_LONGITUDE 0x6791AC0
#define AIS_INVALID_LATITUDE 0x3412140

// Used to re-assemble AIS multi sentence messages
struct AisSentenceStruct {
	int sentenceNumber = 0;
	int totalSentences = 0;
	wxString message = wxEmptyString;
};

class TwoCanAis {

public:
	TwoCanAis();
	~TwoCanAis(void);
	bool ParseAisMessage(VDM aisMessage, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	
private:
	// AIS VDM 6bit twiddling routines
	char AISDecodeCharacter(char value);
	std::vector<bool> DecodeV1(std::string SixBitData);
	std::vector<bool> DecodeV2(std::string SixBitData, unsigned int fillBits);
	int GetIntegerV1(std::vector<bool> binaryData, int start, int length);
	int GetIntegerV2(std::vector<bool> &binaryData, int start, int length);
	int GetIntegerV3(std::vector<bool> &binaryData, int start, int length);
	int GetIntegerV4(std::vector<bool> &binaryData, int start, int length);
	std::string GetStringV1(std::vector<bool> binaryData, int start, int length);
	std::string GetStringV2(std::vector<bool> binaryData, int start, int length);
	std::string GetStringV3(std::vector<bool> binaryData, int start, int length);
	
	// AIS Decoding/Encoding routines
	bool EncodePGN129038(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129039(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129040(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129041(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129793(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129794(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129798(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129801(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129802(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129809(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);
	bool EncodePGN129810(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber);

	// Array used to re-assemble multi sentence AIS messages
	// The AIS Sequence Id is the index into the array
	AisSentenceStruct AisMessageQueue[AIS_MAXIMUM_MESSAGE_ID];

	// Derived from AIS VDM Sentence, AIS Channel A = 0, AIS Channel B = 1
	byte transceiverInformation;

	// PGN 129038 has its own Sequence Id, however this is not related to
	// the AIS VDM Sequence Id, as AIS Message Types 1,2,3 are only single sentence messages
	byte aisSequenceID;

};

#endif