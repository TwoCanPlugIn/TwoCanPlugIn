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

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCan utility functions
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// Outstanding Features: 
// 1. Any additional functions ??
//

#include "twocanutils.h"

int TwoCanUtils::ConvertByteArrayToInteger(const byte *buf, unsigned int *value) {
	if ((buf != NULL) && (value != NULL)) {
		*value = buf[3] | buf[2] << 8 | buf[1] << 16 | buf[0] << 24;
		return true;
	}
	else {
		return false;
	}
}

int TwoCanUtils::ConvertIntegerToByteArray(const int value, byte *buf) {
	if (buf != NULL) {
		for (size_t i = 0; i < sizeof(value); i++) {
			buf[i] = ((value >> (8 * i)) & 0XFF);
		}
		return true;
	}
	else {
		return false;
	}
}

int TwoCanUtils::ConvertHexStringToByteArray(const byte *hexstr, const unsigned int len, byte *buf) {
	// Must be a even number of hexadecimal characters
	// BUG BUG Another check is to perform an isxdigit on each character
	if ((len % 2 == 0) && (hexstr != NULL) && (buf != NULL)) {
		byte val;
		byte pair[2];
		for (unsigned int i = 0; i < len; i++) {
			pair[0] = hexstr[i * 2];
			pair[1] = hexstr[(i * 2) + 1];
			val = (byte)strtoul((char *)pair, NULL, 16);
			buf[i] = val;
		}
		return true;
	}
	else	{
		return false;
	}
}


// Decodes a 29 bit CAN header
int TwoCanUtils::DecodeCanHeader(const byte *buf, CanHeader *header) {
	if ((buf != NULL) && (header != NULL)) {
		// source address in b(0)
		header->source = buf[0];

		// I don't think any of NMEA 2000 messages set the Extended Data Page (EDP) bit
		// If (b(3) And &H2) = &H2 Then
    	// Do something with it
		// End If

		if (buf[3] & 0x01) {
			// If Data Page (DP) bit is set, destination is assumed to be Global Address 255
			header->destination = CONST_GLOBAL_ADDRESS;
			// PGN is encoded over bit 1 of b(3) (aka Data Page), b(2) and if b(2) > 239  also b(1)
			header->pgn = (buf[3] & 0x01) << 16 | buf[2] << 8 | (buf[2] > 239 ? buf[1] : 0);
		}
		else {
			// if b(2) > 239 then destination is global and pgn derived from b(2)  and b(1)
			// if b(2) < 230 then destination is in b(1) (aka PDU-S) 
			header->destination = buf[2] > 239 ? CONST_GLOBAL_ADDRESS : buf[1];
			// PGN is in b(2) (aka PDU-F) and if b(2) > 239  also b(1)
			header->pgn = (buf[2] << 8) | (buf[2] > 239 ? buf[1] : 0);
		}
		// Priority is bits 2,3,4 of b(3)
		header->priority = (buf[3] & 0x1c) >> 2;

		return true;
	}
	else {
		return false;
	}
}

// Generates the ID for Fast Messages. 3 high bits are ID, lower 5 bits are the sequence number
byte TwoCanUtils::GenerateID(byte previousSID) {
    byte tmp = (previousSID >> 5) + 1;
    return tmp == 8 ? 0: tmp << 5; 
}

// Encodes a 29 bit CAN header
int TwoCanUtils::EncodeCanHeader(unsigned int *id, const CanHeader *header) {
	if ((id != NULL) && (header != NULL)) {
		byte buf[4];
		buf[3] = ((header->pgn >> 16) & 0x01) | (header->priority << 2);
		buf[2] = ((header->pgn & 0xFF00) >> 8);
		buf[1] = (buf[2] > 239) ? (header->pgn & 0xFF) : header->destination;
		buf[0] = header->source;
		memcpy(id,&buf[0],4);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

#if defined (__WXMSW__)

// Derive a unique number from a network interface MAC address
// Windows only, corresponding Linux function call is defined in twocansocket

int TwoCanUtils::GetUniqueNumber(unsigned long *uniqueNumber) {
	IP_ADAPTER_INFO *adapterInformation;
	ULONG bufferLength;
	
	*uniqueNumber = 0;

	// This first call is used to size the buffer. 
	if (GetAdaptersInfo(NULL, &bufferLength) == ERROR_BUFFER_OVERFLOW) {
		adapterInformation = (IP_ADAPTER_INFO *)malloc(bufferLength);
		// BUG BUG Should guard against not enough memory for the malloc
		// BUG BUG No idea what the return code is if there are no network adapters. Can you buy a PC taday without one ?

		// Retrieve all network adapter entries	
		if (GetAdaptersInfo(adapterInformation, &bufferLength) == ERROR_SUCCESS) {
			// Just use the first adapter rather than iterate through all
			// Derive the unique number from two parts of the mac address using a pairing function
			char temp[9];
			unsigned int pair1, pair2;
			_snprintf(temp, sizeof(temp), "%d%d%d", adapterInformation->Address[0], adapterInformation->Address[1], adapterInformation->Address[2]);
			pair1 = atoi(temp); 
			_snprintf(temp, sizeof(temp), "%d%d%d", adapterInformation->Address[3], adapterInformation->Address[4], adapterInformation->Address[5]);
			pair2 = atoi(temp); 
			*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;;
	
			free(adapterInformation);
		}
	}
	
	// No unique number from a network MAC address, so generate a random number
	if (*uniqueNumber == 0) {
		srand(CONST_PRODUCT_CODE);
		unsigned int pair1, pair2;
		pair1 = rand();
		pair2 = rand();
		*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;;
	}
	// Unique Number is a maximum of 21 bits in length;
	*uniqueNumber &= 0x1FFFFF;
	return TWOCAN_RESULT_SUCCESS;
}

#endif

unsigned long long TwoCanUtils::GetTimeInMicroseconds() {
	#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);   
	return (currentTime.tv_sec * 1e6 ) + currentTime.tv_usec;
#endif
#if defined (__WXMSW__) 
	FILETIME currentTime;
	GetSystemTimeAsFileTime(&currentTime);
	unsigned long long totalTime = currentTime.dwHighDateTime;
	totalTime <<= 32;
	totalTime |= currentTime.dwLowDateTime;
	totalTime /= 10; // Windows file time is expressed in tenths of microseconds (or 100 nanoseconds)
	totalTime -= 11644473600000000ULL; // convert from Windows epoch (1/1/1601) to Posix Epoch 1/1/1970	
	return totalTime;
#endif
}

// Return a Date/Time variable initialized to the Posix & NMEA 2000 epoch, 1/1/1970
wxDateTime TwoCanUtils::GetEpochTime(void) {
	return wxDateTime::wxDateTime((time_t)0);
}

// Calculate the Date/Time value based on the NMEA 2000 days since epoch and seconds since midnight values
wxDateTime TwoCanUtils::CalculateTime(unsigned short days, unsigned int seconds) {
	wxDateTime tm = GetEpochTime();
	if ((IsDataValid(days)) && (IsDataValid(seconds))) {
		tm += wxDateSpan::Days(days);
		tm += wxTimeSpan::Seconds((wxLongLong)seconds / 10000);
	}
	return tm;
}


// Encode the manufacturer proprietary pgn to set the day/night mode for Navico (B&G, Simrad, Lowrance) displays.
bool TwoCanUtils::EncodeNavicoNightMode(int networkAddress, int networkGroup, bool nightMode, std::vector<CanMessage> *canMessages) {
	CanMessage message;
	CanHeader header;

	// Encode PGN 130845, Manufacturer Proprietary Fast Messge
	message.header.pgn = 130845;
	message.header.source = networkAddress;
	message.header.priority = 7;
	message.header.destination = CONST_GLOBAL_ADDRESS;

	// We'll hard code the multiple frames of a fast message
	message.payload.push_back(0xA0); // Arbitary SID
	message.payload.push_back(0x0E); //14 data bytes
	message.payload.push_back(0x41); //0x41 0x9F encodes as manufacturer code Navico, Industry Code 4 (marine)
	message.payload.push_back(0x9F);
	message.payload.push_back(0xFF);
	message.payload.push_back(0xFF);
	message.payload.push_back(networkGroup + 1);
	message.payload.push_back(0xFF);

	canMessages->push_back(message);

	message.payload.clear();

	message.payload.push_back(0xA1); // Next SID
	message.payload.push_back(0xFF);
	message.payload.push_back(0x26); // Guessing that this is a command
	message.payload.push_back(0x00);
	message.payload.push_back(0x01); // Perhaps gueesing some other command
	if (nightMode) {
		message.payload.push_back(0x04); // Enable night Mode
	}
	else {
		message.payload.push_back(0x02); // Enable day mode
	}
	message.payload.push_back(0x00);
	message.payload.push_back(0x00);

	canMessages->push_back(message);

	message.payload.clear();

	message.payload.push_back(0xA2); // Next SID
	message.payload.push_back(0x00);
	message.payload.push_back(0xFF);
	message.payload.push_back(0xFF);
	message.payload.push_back(0xFF);
	message.payload.push_back(0xFF);
	message.payload.push_back(0xFF);
	message.payload.push_back(0xFF);

	canMessages->push_back(message);

	return TRUE;
}