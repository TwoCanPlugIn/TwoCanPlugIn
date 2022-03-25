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
// Unit: TwoCanAIS - Decodes NMEA 183 AIS VDM sentences and converts to NMEA 2000 messages
// Owner: twocanplugin@hotmail.com
// Date: 04/07/2021
// Version History: 
// 1.0 Initial Release of AIS decoding for Bi-directional Gateway
 

#include "twocanais.h"

TwoCanAis::TwoCanAis(void) {
}

TwoCanAis::~TwoCanAis(void) {
}

// !--VDM,x,x,x,a,s--s,x*hh
//       | | | |   |  Number of fill bits
//       | | | |   Encoded Message
//       | | | AIS Channel
//       | | Sequential Message ID
//       | Sentence Number
//      Total Number of sentences

// AIS VDM bit twiddling routines

// Decode a NMEA 0183 6 bit encoded character to an 8 bit ASCII character
char TwoCanAis::AISDecodeCharacter(char value) {
	char result = value - 48;
	result = result > 40 ? result - 8 : result;
	return result;
}

// Decode the NMEA 0183 ASCII values, derived from 6 bit encoded data to an array of bits
// so that we can gnaw through the bits to retrieve each AIS data field 
std::vector<bool> TwoCanAis::DecodeV1(std::string SixBitData) {
	std::vector<bool> decodedData;
	for (size_t i = 0; i < SixBitData.length(); i++) {
		char testByte = AISDecodeCharacter((char)SixBitData[i]);
		// Perform in reverse order so that we store in LSB order
		for (int j = 5; j >= 0; j--) {
			decodedData.push_back(testByte & (1 << j)); // sets each bit value in the array
		}
	}
	return decodedData;
}

// same as above but c++ implementation
std::vector<bool> TwoCanAis::DecodeV2(std::string SixBitData, unsigned int fillBits) {
	std::vector<bool> decodedData;
	for (auto it = SixBitData.begin(); it != SixBitData.end(); ++it) {
		char testByte = AISDecodeCharacter(*it);
		// Perform in reverse order so that we store in LSB order
		for (int j = 5; j >= 0; j--) {
			decodedData.push_back(testByte & (1 << j)); // sets each bit value in the array
		}
	}
	return decodedData;
}

// Extract an integer from the encoded data
int TwoCanAis::GetIntegerV1(std::vector<bool> binaryData, int start, int length) {
	int result = 0;
	for (int i = 0; i < length; i++) {
		result += binaryData[i + start] << (length - 1 - i);
	}
	return result;
}

int TwoCanAis::GetIntegerV2(std::vector<bool> &binaryData, int start, int length) {
	int result = 0;
	std::vector<bool>::iterator it;
	for (it = binaryData.begin() + start; it != binaryData.begin() + start + length; ++it) {
		result += *it << (length - 1 - std::distance(binaryData.begin() + start, it));
	}
	return result;
}

int TwoCanAis::GetIntegerV3(std::vector<bool> &binaryData, int start, int length) {
	int result = 0;
	for (auto it = binaryData.begin() + start; it != binaryData.begin() + start + length; ++it) {
		result += *it << (length - 1 - std::distance(binaryData.begin() + start, it));
	}
	return result;
}

int TwoCanAis::GetIntegerV4(std::vector<bool> &binaryData, int start, int length) {
	int result = 0;
	int index = 0;
	std::for_each(binaryData.begin() + start, binaryData.begin() + start + length, [&result, &index, &length](int i) { result += i << (length - 1 - index); index++; });
	return result;
}

// Extract a string from the encoded data
std::string TwoCanAis::GetStringV1(std::vector<bool> binaryData, int start, int length) {
	int n = 0;
	int b = 1;
	std::string result;
	for (int i = length - 1; i > 0; i--) {
		n += (binaryData[i] << b);
		b++;
		if ((i % 6) == 0) { // gnaw every 6 bits
			if (n < 32) {
				n += 64;
			}
			result.append(1, n);

			n = 0;
			b = 1;
		} // end gnawing
	}  // end for
	reverse(result.begin(), result.end());
	return result;
}

// As above but a c++ implementation
std::string TwoCanAis::GetStringV2(std::vector<bool> binaryData, int start, int length) {

	// Should check that length is a multiple of 6 (6 bit ASCII encoded characters) and
	// that length is less than binaryData.size() - start.

	// Decode each ASCII character from the 6 bit ASCII according to ITU-R M.1371-4
	// BUG BUG Is this faster or slower than using a lookup table ??
	int value = 0;
	std::string result;
	int i = 0;
	// Perform in reverse because we stored in LSB
	for (auto it = binaryData.begin() + start + length - 1; it != binaryData.begin() + start - 1; --it) {
		value += (*it << i);
		i++;
		if ((i % 6) == 0) {
			if (value < 32) {
				value += 64;
			}
			result.append(1, (int)value);
			i = 0;
			value = 0;
		}

	}
	std::reverse(result.begin(), result.end());
	return result;
}

// Same as V2 but uses a lookup table instead
std::string TwoCanAis::GetStringV3(std::vector<bool> binaryData, int start, int length) {
	int value = 0;
	std::string result;
	int i = 0;
	char lookupTable[] = { '@','A', 'B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'[','\\',']','^','_',' ','!','\'','#','$','%','&','\\','(',')','*','+',',','-','.','/','0','1','2','3','4','5','6','7','8','9',
	':',';','<','=','>','?' };
	for (auto it = binaryData.begin() + start + length - 1; it != binaryData.begin() + start - 1; --it) {
		value += (*it << i);
		i++;
		if ((i % 6) == 0) {
			result.append(1, lookupTable[value]);
			i = 0;
			value = 0;
		}

	}
	std::reverse(result.begin(), result.end());
	return result;

}

// AIS Message parsing routine
bool TwoCanAis::ParseAisMessage(VDM vdmMessage, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	std::vector<bool> decodedMessage;
	bool result = FALSE;

	// single sentence message
	if (vdmMessage.sentences == 1) {
		if (vdmMessage.sentenceNumber == 1) {
			decodedMessage = DecodeV2(vdmMessage.message.ToAscii().data(), vdmMessage.fillbits);
		}

	}

	// multi sentence message
	else {
		// First sentence in this message
		if (vdmMessage.sentenceNumber == 1) {
			// We use the AIS SequentialID as the index to the array
			AisMessageQueue[vdmMessage.sequentialID].message = wxEmptyString;
			AisMessageQueue[vdmMessage.sequentialID].message.append(vdmMessage.message);
			AisMessageQueue[vdmMessage.sequentialID].totalSentences = vdmMessage.sentences;
			AisMessageQueue[vdmMessage.sequentialID].sentenceNumber = vdmMessage.sentenceNumber;
		}
		// Subsequent messages
		else {
			// check if we have had a previous message
			if (((AisMessageQueue[vdmMessage.sequentialID].sentenceNumber + 1) == vdmMessage.sentenceNumber) &&
				(!AisMessageQueue[vdmMessage.sequentialID].message.IsEmpty())) {

				AisMessageQueue[vdmMessage.sequentialID].message.append(vdmMessage.message);
				AisMessageQueue[vdmMessage.sequentialID].sentenceNumber = vdmMessage.sentenceNumber;

				// if have the last sentence, process it
				if (AisMessageQueue[vdmMessage.sequentialID].totalSentences == AisMessageQueue[vdmMessage.sequentialID].sentenceNumber) {
					decodedMessage = DecodeV2(AisMessageQueue[vdmMessage.sequentialID].message.ToAscii().data(), vdmMessage.fillbits);
				}
			}
			// if not the next message, must have dropped a previous message
			else {
				AisMessageQueue[vdmMessage.sequentialID].message = wxEmptyString;
			}
		}
	}

	// Do we have a complete ais message to process
	if ((decodedMessage.size() > 0) &&
		// This is either a single sentence message
		(((vdmMessage.sentences == 1) && (vdmMessage.sentenceNumber == 1)) ||
		// Or we have all of the sentences from a multi-sentence message
		((AisMessageQueue[vdmMessage.sequentialID].totalSentences == AisMessageQueue[vdmMessage.sequentialID].sentenceNumber)))) {

		// All AIS messages have the same initial bit sequence
		int messageType = GetIntegerV3(decodedMessage, 0, 6);
		int raimFlag = GetIntegerV3(decodedMessage, 6, 2);
		int MMSI = GetIntegerV3(decodedMessage, 8, 30);

		// Populate the NMEA 2000 transceiver value depending on the AIS VHF Channel
		transceiverInformation = vdmMessage.channel == ChannelA ? 0 : 1;

		switch (messageType) {

			case 1: // Position Report
			case 2: // Position Report
			case 3: // Special Position Report
				result = EncodePGN129038(decodedMessage, payload, parameterGroupNumber);
				break;
			case 4: // Base Station Report
				result = EncodePGN129793(decodedMessage, payload, parameterGroupNumber);
				break;
			case 5: // Static and voyage related data
				result = EncodePGN129794(decodedMessage, payload, parameterGroupNumber);
				break;
			case 6: // Binary addressed message
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 7: // Binary acknowledgement
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 8: // Binary broadcast message
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 9: // Standard SAR aircraft position report
				result = EncodePGN129798(decodedMessage, payload, parameterGroupNumber);
				break;
			case 10: // UTC/date inquiry
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 11: // UTC/date response
				result = EncodePGN129793(decodedMessage, payload, parameterGroupNumber);
				break;
			case 12: // Addressed safety related message
				result = EncodePGN129801(decodedMessage, payload, parameterGroupNumber);
				break;
			case 13: // Safety related acknowledgement
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 14: // Safety related broadcast message
				result = EncodePGN129802(decodedMessage, payload, parameterGroupNumber);
				break;
			case 15: // Interrogation
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 16: // Assignment mode command
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 17: // DGNSS broadcast binary message
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 18: // Standard Class B equipment position report
				result = EncodePGN129039(decodedMessage, payload, parameterGroupNumber);
				break;
			case 19: // Extended Class B equipment position report
				result = EncodePGN129040(decodedMessage, payload, parameterGroupNumber);
				break;
			case 20: // Data link management message
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 21: // Aids-to-navigation report
				result = EncodePGN129041(decodedMessage, payload, parameterGroupNumber);
				break;
			case 22: // Channel management
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 23: // Group assignment command
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 24: // Static data report, either Part 1 or Part 2.
				// Check whether it is a Part 1 message.
				result = EncodePGN129809(decodedMessage, payload, parameterGroupNumber);
				if (!result) {
					// If not, then it must be a Part 2 message.
					result = EncodePGN129810(decodedMessage, payload, parameterGroupNumber);
				}
				break;
			case 25: // Single slot binary message
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 26: // Multiple slot binary message with Communications State
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			case 27: // Position report for long range applications
				wxLogDebug("Unsupported AIS Message Type: %d\n", messageType);
				break;
			default:
				wxLogDebug("Unknown AIS message type: %d\n", messageType);
				break;
		}
	} 
	return result;
}

// Encode PGN 129038 NMEA AIS Class A Position Report
// AIS Message Types 1,2 or 3
bool TwoCanAis::EncodePGN129038(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129038;

	if (binaryData.size() == 168) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte navigationalStatus = GetIntegerV1(binaryData, 38, 4);
		short rateOfTurn = GetIntegerV1(binaryData, 42, 8);
		unsigned short speedOverGround = GetIntegerV1(binaryData, 50, 10);
		byte positionAccuracy = GetIntegerV1(binaryData, 60, 1);
		int longitude = GetIntegerV1(binaryData, 61, 28);
		int latitude = GetIntegerV1(binaryData, 89, 27);
		unsigned short courseOverGround = GetIntegerV1(binaryData, 116, 12);
		unsigned short trueHeading = GetIntegerV1(binaryData, 128, 9);
		byte timeStamp = GetIntegerV1(binaryData, 137, 6);
		byte manoeuverIndicator = GetIntegerV1(binaryData, 143, 2);
		byte spare = GetIntegerV1(binaryData, 145, 3);
		byte raimFlag = GetIntegerV1(binaryData, 148, 1);
		unsigned int communicationState = GetIntegerV1(binaryData, 149, 19);

		// BUG BUG Unknown
		byte reservedForRegionalApplications = 0;

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		// check if lat & long are -ve (ie, South or West)
		if (longitude & 0x8000000) {
			longitude |= 0xF0000000;
		}

		if (latitude & 0x04000000) {
			latitude |= 0xF8000000;
		}

		int longitudeDegrees = static_cast<int>((longitude / 600000.0f) * 1e7);
		int latitudeDegrees = static_cast<int>((latitude / 600000.0f) * 1e7);

		payload->push_back(longitudeDegrees & 0xFF);
		payload->push_back((longitudeDegrees >> 8) & 0xFF);
		payload->push_back((longitudeDegrees >> 16) & 0xFF);
		payload->push_back((longitudeDegrees >> 24) & 0xFF);

		payload->push_back(latitudeDegrees & 0xFF);
		payload->push_back((latitudeDegrees >> 8) & 0xFF);
		payload->push_back((latitudeDegrees >> 16) & 0xFF);
		payload->push_back((latitudeDegrees >> 24) & 0xFF);

		payload->push_back((positionAccuracy & 0x01) | ((raimFlag << 1) & 0x02) | ((timeStamp << 2) & 0xFC));

		courseOverGround = courseOverGround == AIS_INVALID_COG ? USHRT_MAX : DEGREES_TO_RADIANS(courseOverGround) * 1000;
		payload->push_back(courseOverGround & 0xFF);
		payload->push_back((courseOverGround >> 8) & 0xFF);

		speedOverGround = speedOverGround == AIS_INVALID_SOG ? USHRT_MAX : (speedOverGround * 10) / CONVERT_MS_KNOTS;
		payload->push_back(speedOverGround & 0xFF);
		payload->push_back((speedOverGround >> 8) & 0xFF);

		payload->push_back(communicationState & 0xFF);
		payload->push_back((communicationState >> 8) & 0xFF);
		payload->push_back(((communicationState >> 16) & 0x07) | ((transceiverInformation << 3) & 0xF8));

		trueHeading = trueHeading == AIS_INVALID_HEADING ? USHRT_MAX : DEGREES_TO_RADIANS(trueHeading * 10000);
		payload->push_back(trueHeading & 0xFF);
		payload->push_back((trueHeading >> 8) & 0xFF);

		short n2krot;
		short rotDirection = 1; // Need to keep the sign of the rate of turn as it would be lost when squaring the value

		if (rateOfTurn == 128) { // Data not available should have be coded as -128
			rateOfTurn = -128;
		}
		else if ((rateOfTurn & 0x80) == 0x80) { // Two's complement for negative values
			rateOfTurn = rateOfTurn - 256;
			rotDirection = -1;
		}

		// Now the actual Rate Of Turn conversions
		if (rateOfTurn == -128) {
			n2krot = SHRT_MAX;
		}
		else if (rateOfTurn == 127) { // Maximum rate is 709 degrees per minute
			n2krot = DEGREES_TO_RADIANS(pow((709 / 4.733), 2) / 60) / 3.125e-05;
		}
		else if (rateOfTurn == -127) { // Minimum rate is -709 degrees per minute
			n2krot = -1 * DEGREES_TO_RADIANS(pow((-709 / 4.733), 2) / 60) / 3.125e-05;
		}
		else {
			double degreesPerMinute = rotDirection * (pow((rateOfTurn / 4.733), 2));
			double radiansPerMinute = DEGREES_TO_RADIANS(degreesPerMinute);
			double radiansPerSecond = radiansPerMinute / 60.0f;
			n2krot = radiansPerSecond / 3.125e-05;
		}

		payload->push_back(n2krot & 0xFF);
		payload->push_back((n2krot >> 8) & 0xFF);

		payload->push_back((navigationalStatus & 0x0F) | ((manoeuverIndicator << 4) & 0x30) | 0xC0); //0xCO top two bits are reserved

		payload->push_back((spare & 0x07) | ((reservedForRegionalApplications << 3) & 0xF8));
		payload->push_back(aisSequenceID);

		aisSequenceID++;
		if (!TwoCanUtils::IsDataValid(aisSequenceID)) {
			aisSequenceID = 0;
		}

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Encode payload for PGN 129039 NMEA AIS Class B Position Report
// AIS Message Type 18
bool TwoCanAis::EncodePGN129039(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129039;

	if (binaryData.size() == 168) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte spare = GetIntegerV1(binaryData, 38, 8);
		unsigned short speedOverGround = GetIntegerV1(binaryData, 46, 10);
		byte positionAccuracy = GetIntegerV1(binaryData, 56, 1);
		int longitude = GetIntegerV1(binaryData, 57, 28);
		int latitude = GetIntegerV1(binaryData, 85, 27);
		unsigned short courseOverGround = GetIntegerV1(binaryData, 112, 12);
		unsigned short trueHeading = GetIntegerV1(binaryData, 124, 9);
		byte timeStamp = GetIntegerV1(binaryData, 133, 6);
		byte regionalReserved = GetIntegerV1(binaryData, 139, 2);
		byte unitFlag = GetIntegerV1(binaryData, 141, 1);
		byte displayFlag = GetIntegerV1(binaryData, 142, 1);
		byte dscFlag = GetIntegerV1(binaryData, 143, 1);
		byte bandFlag = GetIntegerV1(binaryData, 144, 1);
		byte msg22Flag = GetIntegerV1(binaryData, 145, 1);
		byte assignedModeFlag = GetIntegerV1(binaryData, 146, 1);
		byte raimFlag = GetIntegerV1(binaryData, 147, 1);
		byte sotdmaFlag = GetIntegerV1(binaryData, 148, 1);
		unsigned int communicationState = GetIntegerV1(binaryData, 149, 19);

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		// check if lat & long are -ve (ie, South or West)
		if (longitude & 0x8000000) {
			longitude |= 0xF0000000;
		}

		if (latitude & 0x04000000) {
			latitude |= 0xF8000000;
		}

		int longitudeDegrees;
		if (longitude == 0x6791AC0) {
			longitudeDegrees = INT_MAX;
		}
		else {
			longitudeDegrees = static_cast<int>((longitude / 600000.0f) * 1e7);
		}

		int latitudeDegrees;
		if (latitude == 0x3412140) {
			latitudeDegrees = INT_MAX;
		}
		else {
			latitudeDegrees = static_cast<int>((latitude / 600000.0f) * 1e7);
		}

		payload->push_back(longitudeDegrees & 0xFF);
		payload->push_back((longitudeDegrees >> 8) & 0xFF);
		payload->push_back((longitudeDegrees >> 16) & 0xFF);
		payload->push_back((longitudeDegrees >> 24) & 0xFF);

		payload->push_back(latitudeDegrees & 0xFF);
		payload->push_back((latitudeDegrees >> 8) & 0xFF);
		payload->push_back((latitudeDegrees >> 16) & 0xFF);
		payload->push_back((latitudeDegrees >> 24) & 0xFF);

		payload->push_back((positionAccuracy & 0x01) | ((raimFlag << 1) & 0x02) | ((timeStamp << 2) & 0xFC));

		courseOverGround = courseOverGround == AIS_INVALID_COG ? USHRT_MAX : DEGREES_TO_RADIANS(courseOverGround) * 1000;
		payload->push_back(courseOverGround & 0xFF);;
		payload->push_back((courseOverGround >> 8) & 0xFF);

		speedOverGround = speedOverGround == AIS_INVALID_SOG ? USHRT_MAX : (speedOverGround * 10) / CONVERT_MS_KNOTS;
		payload->push_back(speedOverGround & 0xFF);
		payload->push_back((speedOverGround >> 8) & 0xFF);

		payload->push_back(communicationState & 0xFF);
		payload->push_back((communicationState >> 8) & 0xFF);
		payload->push_back(((communicationState >> 16) & 0x07) | ((transceiverInformation << 3) & 0xF8));

		trueHeading = trueHeading == AIS_INVALID_HEADING ? USHRT_MAX : DEGREES_TO_RADIANS(trueHeading * 10000);
		payload->push_back(trueHeading & 0xFF);
		payload->push_back((trueHeading >> 8) & 0xFF);

		payload->push_back(regionalReserved & 0xFF);

		byte regionalReservedB = 0x03;
		payload->push_back(regionalReserved | ((unitFlag << 2) & 0x04) | ((displayFlag << 3) & 0x8) | ((dscFlag << 4) & 0x10) | ((bandFlag << 5) & 0x20) | ((msg22Flag << 6) & 0x40) | ((assignedModeFlag << 7) & 0x80));

		payload->push_back(sotdmaFlag & 0x01);

		return TRUE;
	}
	return FALSE;
}


// Encode payload for PGN 129040 AIS Class B Extended Position Report
// AIS Message Type 19
bool TwoCanAis::EncodePGN129040(std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {
	*parameterGroupNumber = 129040;

	if (binaryData.size() == 312) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte regionalReservedA = GetIntegerV1(binaryData, 38, 8);
		unsigned short speedOverGround = GetIntegerV1(binaryData, 46, 10);
		byte positionAccuracy = GetIntegerV1(binaryData, 56, 1);
		int longitude = GetIntegerV1(binaryData, 57, 28);
		int latitude = GetIntegerV1(binaryData, 85, 27);
		unsigned short courseOverGround = GetIntegerV1(binaryData, 112, 12);
		unsigned short trueHeading = GetIntegerV1(binaryData, 124, 9);
		byte timeStamp = GetIntegerV1(binaryData, 133, 6);
		byte regionalReservedB = GetIntegerV1(binaryData, 139, 4);
		std::string shipName = GetStringV2(binaryData, 143, 120);
		byte shipType = GetIntegerV1(binaryData, 263, 8);
		unsigned short refBow = GetIntegerV1(binaryData, 271, 9);
		unsigned short shipLength = GetIntegerV1(binaryData, 280, 9);
		unsigned short refStarboard = GetIntegerV1(binaryData, 289, 6);
		unsigned short shipBeam = GetIntegerV1(binaryData, 295, 6);
		byte gnssType = GetIntegerV1(binaryData, 301, 4);
		byte raimFlag = GetIntegerV1(binaryData, 305, 1);
		byte dteFlag = GetIntegerV1(binaryData, 306, 1);
		byte assignedModeFlag = GetIntegerV1(binaryData, 307, 1);
		byte spare = GetIntegerV1(binaryData, 308, 4);

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		// check if lat & long are -ve (ie, South or West)
		if (longitude & 0x8000000) {
			longitude |= 0xF0000000;
		}

		if (latitude & 0x04000000) {
			latitude |= 0xF8000000;
		}

		int longitudeDegrees;
		if (longitude == 0x6791AC0) {
			longitudeDegrees = INT_MAX;
		}
		else {
			longitudeDegrees = static_cast<int>((longitude / 600000.0f) * 1e7);
		}

		int latitudeDegrees;
		if (latitude == 0x3412140) {
			latitudeDegrees = INT_MAX;
		}
		else {
			latitudeDegrees = static_cast<int>((latitude / 600000.0f) * 1e7);
		}

		payload->push_back(longitudeDegrees & 0xFF);
		payload->push_back((longitudeDegrees >> 8) & 0xFF);
		payload->push_back((longitudeDegrees >> 16) & 0xFF);
		payload->push_back((longitudeDegrees >> 24) & 0xFF);

		payload->push_back(latitudeDegrees & 0xFF);
		payload->push_back((latitudeDegrees >> 8) & 0xFF);
		payload->push_back((latitudeDegrees >> 16) & 0xFF);
		payload->push_back((latitudeDegrees >> 24) & 0xFF);

		payload->push_back((positionAccuracy & 0x01) | ((raimFlag << 1) & 0x02) | ((timeStamp << 2) & 0xFC));

		courseOverGround = courseOverGround == AIS_INVALID_COG ? USHRT_MAX : DEGREES_TO_RADIANS(courseOverGround) * 1000;
		payload->push_back(courseOverGround & 0xFF);;
		payload->push_back((courseOverGround >> 8) & 0xFF);

		speedOverGround = speedOverGround == AIS_INVALID_SOG ? USHRT_MAX : (speedOverGround * 10) / CONVERT_MS_KNOTS;
		payload->push_back(speedOverGround & 0xFF);
		payload->push_back((speedOverGround >> 8) & 0xFF);

		payload->push_back(regionalReservedA);

		payload->push_back((regionalReservedB & 0x0F) | (0xF0));

		payload->push_back(shipType);

		trueHeading = trueHeading == AIS_INVALID_HEADING ? USHRT_MAX : DEGREES_TO_RADIANS(trueHeading * 10000);
		payload->push_back(trueHeading & 0xFF);
		payload->push_back((trueHeading >> 8) & 0xFF);

		payload->push_back(((gnssType << 4) & 0xF0) | (0x0F));

		payload->push_back(((shipLength + refBow) * 10) & 0xFF);
		payload->push_back((((shipLength + refBow) * 10) >> 8) & 0xFF);

		payload->push_back(((shipBeam  + refStarboard) * 10) & 0xFF);
		payload->push_back((((shipBeam + refStarboard) * 10) >> 8) & 0xFF);

		payload->push_back((refStarboard * 10) & 0xFF);
		payload->push_back(((refStarboard * 10) >> 8) & 0xFF);

		payload->push_back((refBow * 10) & 0xFF);
		payload->push_back(((refBow * 10) >> 8) & 0xFF);

		// BUG BUG need to pad with '@' and must be 20 characters in length
		for (auto it = shipName.begin(); it != shipName.end(); ++it) {
			payload->push_back(*it);
		}

		payload->push_back((dteFlag & 0x01) | ((assignedModeFlag << 1) & 0x02) | (0x3C) | ((transceiverInformation & 0x03) << 6));

		payload->push_back((transceiverInformation & 0x1C) >> 2);

		return TRUE;
	}
	return FALSE;
}


// Encode payload for PGN 129041 AIS Aids To Navigation (AToN) Report
// AIS Message Type 21
bool TwoCanAis::EncodePGN129041(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129041;
	// Variable size message
	if ((binaryData.size() >= 272) && (binaryData.size() <= 360)) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte AToNType = GetIntegerV1(binaryData, 38, 5);
		std::string AToNName = GetStringV2(binaryData, 43, 120);
		byte positionAccuracy = GetIntegerV1(binaryData, 163, 1);
		int longitude = GetIntegerV1(binaryData, 164, 28);
		int latitude = GetIntegerV1(binaryData, 192, 27);
		unsigned short refBow = GetIntegerV1(binaryData, 219, 9);
		unsigned short shipLength = GetIntegerV1(binaryData, 228, 9);
		unsigned short refStarboard = GetIntegerV1(binaryData, 237, 6);
		unsigned short shipBeam = GetIntegerV1(binaryData, 243, 6);
		byte gnssType = GetIntegerV1(binaryData, 249, 4);
		byte timeStamp = GetIntegerV1(binaryData, 253, 6);
		byte offPositionFlag = GetIntegerV1(binaryData, 259, 1);
		byte AToNStatus = GetIntegerV1(binaryData, 260, 8);
		byte raimFlag = GetIntegerV1(binaryData, 268, 1);
		byte virtualAToN = GetIntegerV1(binaryData, 269, 1);
		byte assignedModeFlag = GetIntegerV1(binaryData, 270, 1);
		byte spare = GetIntegerV1(binaryData, 271, 1);
		// The Aid To Navigation Name is padded to be on an 8 bit boundary
		AToNName.append(GetStringV2(binaryData, 272, binaryData.size() - 272 - ((binaryData.size() - 272) % 8)));

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		// check if lat & long are -ve (ie, South or West)
		if (longitude & 0x8000000) {
			longitude |= 0xF0000000;
		}

		if (latitude & 0x04000000) {
			latitude |= 0xF8000000;
		}

		int longitudeDegrees;
		if (longitude == 0x6791AC0) {
			longitudeDegrees = INT_MAX;
		}
		else {
			longitudeDegrees = static_cast<int>((longitude / 600000.0f) * 1e7);
		}

		int latitudeDegrees;
		if (latitude == 0x3412140) {
			latitudeDegrees = INT_MAX;
		}
		else {
			latitudeDegrees = static_cast<int>((latitude / 600000.0f) * 1e7);
		}

		payload->push_back(longitudeDegrees & 0xFF);
		payload->push_back((longitudeDegrees >> 8) & 0xFF);
		payload->push_back((longitudeDegrees >> 16) & 0xFF);
		payload->push_back((longitudeDegrees >> 24) & 0xFF);

		payload->push_back(latitudeDegrees & 0xFF);
		payload->push_back((latitudeDegrees >> 8) & 0xFF);
		payload->push_back((latitudeDegrees >> 16) & 0xFF);
		payload->push_back((latitudeDegrees >> 24) & 0xFF);

		payload->push_back((positionAccuracy & 0x01) | ((raimFlag << 1) & 0x02) | ((timeStamp << 2) & 0xFC));

		payload->push_back(((shipLength + refBow) * 10) & 0xFF);
		payload->push_back((((shipLength + refBow) * 10) >> 8) & 0xFF);

		payload->push_back(((shipBeam + refStarboard) * 10) & 0xFF);
		payload->push_back((((shipBeam + refStarboard) * 10) >> 8) & 0xFF);

		payload->push_back((refStarboard * 10) & 0xFF);
		payload->push_back(((refStarboard * 10) >> 8) & 0xFF);

		payload->push_back((refBow * 10) & 0xFF);
		payload->push_back(((refBow * 10) >> 8) & 0xFF);

		payload->push_back((AToNType & 0x1F) | ((offPositionFlag << 5) & 0x20) | ((virtualAToN << 6) & 0x40) | ((assignedModeFlag << 7) & 0x80));

		payload->push_back(0xE1 | ((gnssType << 1) & 0x1E)); // Note spare & reserved bits

		payload->push_back(AToNStatus);

		payload->push_back((transceiverInformation  & 0x1F) | 0xE0); // Note reserved bits

		payload->push_back(AToNName.length() + 2);

		payload->push_back(0x01); // 1 indicates that the string is in ASCII, 0 indicates Unicode

		for (auto it = AToNName.begin(); it != AToNName.end(); ++it) {
			payload->push_back(*it);
		}

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 129793 AIS Date and Time report
// AIS Message Type 4 and if date is present also Message Type 11
bool TwoCanAis::EncodePGN129793(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129793;

	if (binaryData.size() == 168) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		unsigned int year = GetIntegerV1(binaryData, 38, 14);
		byte month = GetIntegerV1(binaryData, 52, 4);
		unsigned short day = GetIntegerV1(binaryData, 56, 5);
		byte hour = GetIntegerV1(binaryData, 61, 5);
		byte minute = GetIntegerV1(binaryData, 66, 6);
		byte second = GetIntegerV1(binaryData, 72, 6);
		byte positionAccuracy = GetIntegerV1(binaryData, 78, 1);
		int longitude = GetIntegerV1(binaryData, 79, 28);
		int latitude = GetIntegerV1(binaryData, 107, 27);
		byte gnssType = GetIntegerV1(binaryData, 134, 4);
		byte longRangeFlag = GetIntegerV1(binaryData, 138, 1); 
		unsigned short spare = GetIntegerV1(binaryData, 139, 9);
		byte raimFlag = GetIntegerV1(binaryData, 148, 1);
		unsigned int communicationState = GetIntegerV1(binaryData, 149, 19);

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		// check if lat & long are -ve (ie, South or West)
		if (longitude & 0x8000000) {
			longitude |= 0xF0000000;
		}

		if (latitude & 0x04000000) {
			latitude |= 0xF8000000;
		}

		int longitudeDegrees;
		if (longitude == 0x6791AC0) {
			longitudeDegrees = INT_MAX;
		}
		else {
			longitudeDegrees = static_cast<int>((longitude / 600000.0f) * 1e7);
		}

		int latitudeDegrees;
		if (latitude == 0x3412140) {
			latitudeDegrees = INT_MAX;
		}
		else {
			latitudeDegrees = static_cast<int>((latitude / 600000.0f) * 1e7);
		}

		payload->push_back(longitudeDegrees & 0xFF);
		payload->push_back((longitudeDegrees >> 8) & 0xFF);
		payload->push_back((longitudeDegrees >> 16) & 0xFF);
		payload->push_back((longitudeDegrees >> 24) & 0xFF);

		payload->push_back(latitudeDegrees & 0xFF);
		payload->push_back((latitudeDegrees >> 8) & 0xFF);
		payload->push_back((latitudeDegrees >> 16) & 0xFF);
		payload->push_back((latitudeDegrees >> 24) & 0xFF);

		payload->push_back((positionAccuracy & 0x01) | ((raimFlag << 1) & 0x02) | 0xFC); // Note reserved value

		wxDateTime now;
		// Note that wxDateTime month is zero indexed
		now.Set(day, static_cast<wxDateTime::Month>(month - 1), year, hour < AIS_INVALID_HOUR ? hour : 0,
			minute < AIS_INVALID_MINUTE ? minute : 0,
			second < AIS_INVALID_SECOND ? second : 0, 0);
		wxDateTime epoch((time_t)0);
		wxTimeSpan diff = now - epoch;

		unsigned short daysSinceEpoch = diff.GetDays();
		unsigned int secondsSinceMidnight = (unsigned int)(diff.GetSeconds().ToLong() - (diff.GetDays() * 24 * 60 * 60)) * 10000;

		payload->push_back(secondsSinceMidnight & 0xFF);
		payload->push_back((secondsSinceMidnight >> 8) & 0xFF);
		payload->push_back((secondsSinceMidnight >> 16) & 0xFF);
		payload->push_back((secondsSinceMidnight >> 24) & 0xFF);

		payload->push_back(communicationState & 0xFF);
		payload->push_back((communicationState >> 8) & 0xFF);
		payload->push_back(((communicationState >> 16) & 0x07) | ((transceiverInformation << 3) & 0xF8));

		payload->push_back(daysSinceEpoch & 0xFF);
		payload->push_back((daysSinceEpoch >> 8) & 0xFF);

		payload->push_back(((gnssType << 4) & 0xF0) | 0x0F);

		payload->push_back(spare & 0xFF);

		return TRUE;

	}
	return FALSE;
}

// Encode payload for PGN 129794 NMEA AIS Class A Static and Voyage Related Data
// AIS Message Type 5
bool TwoCanAis::EncodePGN129794(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129794;

	if (binaryData.size() == 426) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte aisVersion = GetIntegerV1(binaryData, 38, 2);
		unsigned int imoNumber = GetIntegerV1(binaryData, 40, 30);
		std::string callSign = GetStringV2(binaryData, 70, 42);
		std::string shipName = GetStringV2(binaryData, 112, 120);
		byte shipType = GetIntegerV1(binaryData, 232, 8);
		unsigned short refBow = GetIntegerV1(binaryData, 240, 9);
		unsigned short shipLength = GetIntegerV1(binaryData, 249, 9);
		unsigned short shipBeam = GetIntegerV1(binaryData, 258, 6);
		unsigned short refStarboard = GetIntegerV1(binaryData, 264, 6);
		byte gnssType = GetIntegerV1(binaryData, 270, 4);
		byte month = GetIntegerV1(binaryData, 274, 4);
		byte day = GetIntegerV1(binaryData, 278, 5);
		byte hour = GetIntegerV1(binaryData, 283, 5);
		byte minute = GetIntegerV1(binaryData, 288, 6);
		unsigned short draft = GetIntegerV1(binaryData, 294, 8);
		std::string destination = GetStringV2(binaryData, 302, 120);
		byte dteFlag = GetIntegerV1(binaryData, 422, 1);
		byte spare = GetIntegerV1(binaryData, 423, 1);

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		payload->push_back(imoNumber & 0xFF);
		payload->push_back((imoNumber >> 8) & 0xFF);
		payload->push_back((imoNumber >> 16) & 0xFF);
		payload->push_back((imoNumber >> 24) & 0xFF);

		for (int i = 0; i < 7; i++) {
			payload->push_back(callSign[i]);
		}

		for (int i = 0; i < 20; i++) {
			payload->push_back(shipName[i]);
		}

		payload->push_back(shipType);

		payload->push_back(((shipLength + refBow) * 10) & 0xFF);
		payload->push_back((((shipLength + refBow) * 10) >> 8) & 0xFF);

		payload->push_back(((shipBeam + refStarboard) * 10) & 0xFF);
		payload->push_back((((shipBeam + refStarboard) * 10) >> 8) & 0xFF);

		payload->push_back((refStarboard * 10) & 0xFF);
		payload->push_back(((refStarboard * 10) >> 8) & 0xFF);

		payload->push_back((refBow * 10) & 0xFF);
		payload->push_back(((refBow * 10) >> 8) & 0xFF);


		unsigned short daysSinceEpoch;
		unsigned int secondsSinceMidnight;

		wxDateTime now;
		now = wxDateTime::Now();
		
		// If no ETA date & time 
		// If current month (eg. August) is greater than eta month (eg. January), then the ETA is January next year.
		// Note month is zero based, wxTime hour nust be < 24, minutes < 60.

		if ((month == 0) && (day == 0) && (((hour == 0) && (minute == 0)) || ((hour == 24) && (minute == 60)))) {
			daysSinceEpoch = USHRT_MAX;;
			secondsSinceMidnight = UINT_MAX;
		}
		// If current month (eg. December) is greater than eta month (eg. January), then the ETA is January next year.
		// Note month is zero based, wxTime day > 0, hour must be < 24, minutes < 60.
		else {
			wxDateTime eta;
			eta.Set(day, static_cast<wxDateTime::Month>(month - 1), now.GetMonth() + 1 > month ? now.GetYear() + 1 : now.GetYear(), 
				hour < AIS_INVALID_HOUR ? hour : 0, minute < AIS_INVALID_MINUTE ? minute : 0, 0);

			wxDateTime epoch((time_t)0);

			wxTimeSpan diff = eta - epoch;

			daysSinceEpoch = diff.GetDays();
			secondsSinceMidnight = (diff.GetSeconds().GetValue() - (diff.GetDays() * 24 * 60 * 60)) * 1e4;

		}

		payload->push_back(daysSinceEpoch & 0xFF);
		payload->push_back((daysSinceEpoch >> 8) & 0xFF);

		payload->push_back(secondsSinceMidnight & 0xFF);
		payload->push_back((secondsSinceMidnight >> 8) & 0xFF);
		payload->push_back((secondsSinceMidnight >> 16) & 0xFF);
		payload->push_back((secondsSinceMidnight >> 24) & 0xFF);

		draft = draft * 10;
		payload->push_back(draft & 0xFF);
		payload->push_back((draft >> 8) & 0xFF);

		for (int i = 0; i < 20; i++) {
			payload->push_back(destination[i]);
		}

		payload->push_back((aisVersion & 0x03) | ((gnssType << 2) & 0x3C) | ((dteFlag << 6) & 0x40));

		payload->push_back(transceiverInformation & 0x1F);

		return TRUE;

	}
	return FALSE;
}

//	Encode payload for PGN 129798 AIS SAR Aircraft Position Report
// AIS Message Type 9
bool TwoCanAis::EncodePGN129798(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129798;

	if (binaryData.size() == 168) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		long long altitude = GetIntegerV1(binaryData, 38, 12);
		unsigned short speedOverGround = GetIntegerV1(binaryData, 50, 10);
		byte positionAccuracy = GetIntegerV1(binaryData, 60, 1);
		int longitude = GetIntegerV1(binaryData, 61, 28);
		int latitude = GetIntegerV1(binaryData, 89, 27);
		unsigned short courseOverGround = GetIntegerV1(binaryData, 116, 12);
		byte timeStamp = GetIntegerV1(binaryData, 128, 6);
		byte altitudeSensor = GetIntegerV1(binaryData, 134, 1);
		byte reservedForRegionalApplications = GetIntegerV1(binaryData, 135, 7); 
		byte dteFlag = GetIntegerV1(binaryData, 142, 1);
		byte spare = GetIntegerV1(binaryData, 143, 3);
		byte assignedModeFlag = GetIntegerV1(binaryData, 146, 1);
		byte raimFlag = GetIntegerV1(binaryData, 147, 1);
		byte sotdmaFlag = GetIntegerV1(binaryData, 148, 1);
		unsigned int communicationState = GetIntegerV1(binaryData, 149, 19);

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(userID & 0xFF);
		payload->push_back((userID >> 8) & 0xFF);
		payload->push_back((userID >> 16) & 0xFF);
		payload->push_back((userID >> 24) & 0xFF);

		// check if lat & long are -ve (ie, South or West)
		if (longitude & 0x8000000) {
			longitude |= 0xF0000000;
		}

		if (latitude & 0x04000000) {
			latitude |= 0xF8000000;
		}

		int longitudeDegrees;
		if (longitude == 0x6791AC0) {
			longitudeDegrees = INT_MAX;
		}
		else {
			longitudeDegrees = static_cast<int>((longitude / 600000.0f) * 1e7);
		}

		int latitudeDegrees;
		if (latitude == 0x3412140) {
			latitudeDegrees = INT_MAX;
		}
		else {
			latitudeDegrees = static_cast<int>((latitude / 600000.0f) * 1e7);
		}

		payload->push_back(longitudeDegrees & 0xFF);
		payload->push_back((longitudeDegrees >> 8) & 0xFF);
		payload->push_back((longitudeDegrees >> 16) & 0xFF);
		payload->push_back((longitudeDegrees >> 24) & 0xFF);

		payload->push_back(latitudeDegrees & 0xFF);
		payload->push_back((latitudeDegrees >> 8) & 0xFF);
		payload->push_back((latitudeDegrees >> 16) & 0xFF);
		payload->push_back((latitudeDegrees >> 24) & 0xFF);

		payload->push_back((positionAccuracy & 0x01) | ((raimFlag << 1) & 0x02) | ((timeStamp << 2) & 0xFC));

		courseOverGround = courseOverGround == AIS_INVALID_COG ? USHRT_MAX : DEGREES_TO_RADIANS(courseOverGround) * 1000;
		payload->push_back(courseOverGround & 0xFF);
		payload->push_back((courseOverGround >> 8) & 0xFF);

		speedOverGround = speedOverGround == AIS_INVALID_SOG ? USHRT_MAX : (speedOverGround * 10) / CONVERT_MS_KNOTS;
		payload->push_back(speedOverGround & 0xFF);
		payload->push_back((speedOverGround >> 8) & 0xFF);

		payload->push_back(communicationState & 0xFF);
		payload->push_back((communicationState >> 8) & 0xFF);
		payload->push_back(((communicationState >> 16) & 0x7F) | ((transceiverInformation << 3) & 0xF8));

		altitude = altitude == AIS_INVALID_ALTITUDE ? LLONG_MAX : altitude * 1e6;
		payload->push_back(altitude & 0xFF);
		payload->push_back((altitude >> 8) & 0xFF);
		payload->push_back((altitude >> 16) & 0xFF);
		payload->push_back((altitude >> 24) & 0xFF);
		payload->push_back((altitude >> 32) & 0xFF);
		payload->push_back((altitude >> 40) & 0xFF);
		payload->push_back((altitude >> 48) & 0xFF);
		payload->push_back((altitude >> 56) & 0xFF);

		payload->push_back(reservedForRegionalApplications & 0xFF);

		payload->push_back((dteFlag & 0x01) | ((assignedModeFlag << 1) & 0x02) | ((sotdmaFlag << 2) & 0x04) | ((altitudeSensor << 3) & 0x08) | 0xF0);

		payload->push_back(0xFF); // reserved

		return TRUE;

	}
	return FALSE;
}
//	Encode payload for PGN 129801 AIS Addressed Safety Related Message
// AIS Message Type 12
bool TwoCanAis::EncodePGN129801(const std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129801;

	if (binaryData.size() >= 72) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int sourceID = GetIntegerV1(binaryData, 8, 30);
		byte sequenceNumber = GetIntegerV1(binaryData, 38, 2);
		unsigned int destinationID = GetIntegerV1(binaryData, 40, 30);
		byte retransmitFlag = GetIntegerV1(binaryData, 70, 1);
		byte spare = GetIntegerV1(binaryData, 71, 1);
		// Could use the fillBits to determine the padding, however modulo 6 achieves the same
		std::string safetyMessage = GetStringV2(binaryData, 72, binaryData.size() - 72 - ((binaryData.size() - 72) % 6));

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(sourceID & 0xFF);
		payload->push_back((sourceID >> 8) & 0xFF);
		payload->push_back((sourceID >> 16) & 0xFF);
		payload->push_back((sourceID >> 24) & 0xFF);

		payload->push_back(0x01 | ((transceiverInformation << 1) & 0x3E) | ((sequenceNumber << 6) & 0xC0));

		payload->push_back(destinationID & 0xFF);
		payload->push_back((destinationID >> 8) & 0xFF);
		payload->push_back((destinationID >> 16) & 0xFF);
		payload->push_back((destinationID >> 24) & 0xFF);

		payload->push_back(0x3F | ((retransmitFlag << 6) & 0x40) | 0x80);

		// Length also includes length & control field
		payload->push_back(safetyMessage.length() + 2);
		payload->push_back(0x01); // First byte of safety message indicates encoding; 0 for Unicode, 1 for ASCII

		for (auto it = safetyMessage.begin(); it != safetyMessage.end(); ++it) {
			payload->push_back(*it);
		}

		return TRUE;

	}
	return FALSE;
}

// Encode payload for PGN 129802 AIS Safety Related Broadcast Message 
// AIS Message Type 14
bool TwoCanAis::EncodePGN129802(std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129802;

	if (binaryData.size() >= 40) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int sourceID = GetIntegerV1(binaryData, 8, 30);
		byte spare = GetIntegerV1(binaryData, 38, 2);
		std::string safetyMessage = GetStringV2(binaryData, 40, binaryData.size() - 40 - ((binaryData.size() - 40) % 6));

		payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

		payload->push_back(sourceID & 0xFF);
		payload->push_back((sourceID >> 8) & 0xFF);
		payload->push_back((sourceID >> 16) & 0xFF);
		payload->push_back((sourceID >> 24) & 0xFF);

		payload->push_back((transceiverInformation & 0x1F) | 0xE0);

		payload->push_back(safetyMessage.length() + 2);
		payload->push_back(0x01); // first byte of safety message indicates encoding; 0 for Unicode, 1 for ASCII

		for (auto it = safetyMessage.begin(); it != safetyMessage.end(); ++it) {
			payload->push_back(*it);
		}

		return TRUE;
	}
	return FALSE;
}

// Encode payload for PGN 129809 AIS Class B Static Data Report, Part A 
// AIS Message Type 24, Part A
bool TwoCanAis::EncodePGN129809(std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129809;

	// Should be padded to 168, but hey ho....
	if (binaryData.size() >= 162) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte messageType = GetIntegerV1(binaryData, 38, 2); // Part A = 0

		if (messageType == 0) {

			std::string shipName = GetStringV2(binaryData, 40, 120);

			payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

			payload->push_back(userID & 0xFF);
			payload->push_back((userID >> 8) & 0xFF);
			payload->push_back((userID >> 16) & 0xFF);
			payload->push_back((userID >> 24) & 0xFF);

			for (auto it = shipName.begin(); it != shipName.end(); ++it) {
				payload->push_back(*it);
			}

			payload->push_back((transceiverInformation & 0x1F) | 0xE0);

			return TRUE;
		}
	}
	return FALSE;
}

// Encode payload for PGN 129810 AIS Class B Static Data Report, Part B 
// AIS Message Type 24, Part B
bool TwoCanAis::EncodePGN129810(std::vector<bool> binaryData, std::vector<byte> *payload, unsigned int *parameterGroupNumber) {

	*parameterGroupNumber = 129810;

	if (binaryData.size() == 168) {

		byte messageID = GetIntegerV1(binaryData, 0, 6);
		byte repeatIndicator = GetIntegerV1(binaryData, 6, 2);
		unsigned int userID = GetIntegerV1(binaryData, 8, 30);
		byte messageType = GetIntegerV1(binaryData, 38, 2); // Part B = 1

		if (messageType == 1) {

			byte shipType = GetIntegerV1(binaryData, 40, 8);
			std::string vendorID = GetStringV2(binaryData, 48, 42);
			std::string callSign = GetStringV2(binaryData, 90, 42);
			unsigned short refBow = GetIntegerV1(binaryData, 132, 9);
			unsigned short shipLength = GetIntegerV1(binaryData, 141, 9);
			unsigned short shipBeam = GetIntegerV1(binaryData, 150, 6);
			unsigned short refStarboard = GetIntegerV1(binaryData, 156, 6);
			byte spare = GetIntegerV1(binaryData, 162, 6);

			payload->push_back((messageID & 0x3F) | ((repeatIndicator << 6) & 0xC0));

			payload->push_back(userID & 0xFF);
			payload->push_back((userID >> 8) & 0xFF);
			payload->push_back((userID >> 16) & 0xFF);
			payload->push_back((userID >> 24) & 0xFF);

			payload->push_back(shipType);

			for (auto it = vendorID.begin(); it != vendorID.end(); ++it) {
				payload->push_back(*it);
			}

			for (auto it = callSign.begin(); it != callSign.end(); ++it) {
				payload->push_back(*it);
			}

			// BUB BUG if dimensions are zero, set to USHRT_MAX
			payload->push_back(((shipLength + refBow) * 10) & 0xFF);
			payload->push_back((((shipLength + refBow) * 10) >> 8) & 0xFF);

			payload->push_back(((shipBeam + refStarboard) * 10) & 0xFF);
			payload->push_back((((shipBeam + refStarboard) * 10) >> 8) & 0xFF);

			payload->push_back((refStarboard * 10) & 0xFF);
			payload->push_back(((refStarboard * 10) >> 8) & 0xFF);

			payload->push_back((refBow * 10) & 0xFF);
			payload->push_back(((refBow * 10) >> 8) & 0xFF);

			// motherShip MMSI number
			payload->push_back(0xFF);
			payload->push_back(0xFF);
			payload->push_back(0xFF);
			payload->push_back(0xFF);

			payload->push_back(0xFF); // reserved = 0x03, spare = 0xFC

			payload->push_back(transceiverInformation & 0x1F);

			return TRUE;
		}

	}
	return FALSE;
}
