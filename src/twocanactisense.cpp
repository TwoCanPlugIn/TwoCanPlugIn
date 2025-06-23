// Copyright(C) 2024 by Steven Adler
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
// Actisense® is a registered Trademark of Active Research Limited

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCanActisense - Logs received NMEA 2000 messages in Actisense EBL format
// Owner: twocanplugin@hotmail.com
// Date: 10/07/2024
// Version History: 
// 1.0 Initial Release of EBL logging

#include "twocanactisense.h"

// Verify the checksum, unused in the plugin. 
// Was used to check my understanding of Actisense Checksum calculations
bool TwoCanActisense::VerifyChecksum(std::vector<byte> data, int verify) {
	if (data.at(1) == data.size() - 3) {
		// the checksum character at the end of the message
		// ensures that the sum of all characters modulo 256 equals 0
		int checksum = 0;
		for (auto it : data) {
			checksum += it;
		}
		if ((checksum % 256) == 0) {
			wxLogDebug("Checksum Modulo OK\n");
		}
		else {
			wxLogDebug("Checksum Modulo not OK\n");
		}

		checksum = 0;
		for (std::vector<unsigned char>::iterator it = data.begin(); it != data.end() - 1; ++it) {
			checksum += *it;
		}
		checksum %= 256;

		if (checksum != 0) {
			checksum = 256 - checksum;
		}

		if (verify == checksum) {
			wxLogDebug("Checksum Matches\n");
			return true;
		}
		else {
			wxLogDebug("Checksum does not match\n");
		}
	}
	else {
		wxLogDebug("Length does not match: %d,  %lu\n", data.at(1), data.size());
	}
	return false;
}

// Writes the Actiense header including EBL version number and time stamp
void TwoCanActisense::WriteHeader(wxFile *logFile) {

	int bytesWritten;

	std::vector<unsigned char> eblHeader;

	// Actisense EBL Header Sections
	eblHeader.push_back(ESC);
	eblHeader.push_back(BEMSTART);
	// UTC Time section header
	eblHeader.push_back(EBL_TIME);
		
#if defined (__WXMSW__)
	SYSTEMTIME st;
	FILETIME ft;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	eblHeader.push_back(ft.dwLowDateTime & 0xFF);
	eblHeader.push_back((ft.dwLowDateTime >> 8) & 0xFF);
	eblHeader.push_back((ft.dwLowDateTime >> 16) & 0xFF);
	eblHeader.push_back((ft.dwLowDateTime >> 24) & 0xFF);
	eblHeader.push_back(ft.dwHighDateTime & 0xFF);
	eblHeader.push_back((ft.dwHighDateTime >> 8) & 0xFF);
	eblHeader.push_back((ft.dwHighDateTime >> 16) & 0xFF);
	eblHeader.push_back((ft.dwHighDateTime >> 24) & 0xFF);
#endif
#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
	timeval currentTime;
	gettimeofday(&currentTime, NULL);   
	eblHeader.push_back(currentTime.tv_sec & 0xFF);
	eblHeader.push_back((currentTime.tv_sec >> 8) & 0xFF);
	eblHeader.push_back((currentTime.tv_sec >> 16) & 0xFF);
	eblHeader.push_back((currentTime.tv_sec >> 24) & 0xFF);
	eblHeader.push_back(currentTime.tv_usec & 0xFF);
	eblHeader.push_back((currentTime.tv_usec >> 8) & 0xFF);
	eblHeader.push_back((currentTime.tv_usec >> 16) & 0xFF);
	eblHeader.push_back((currentTime.tv_usec >> 24) & 0xFF);
#endif
		
	eblHeader.push_back(ESC);
	eblHeader.push_back(BEMEND);
	// Version section Header
	eblHeader.push_back(ESC);
	eblHeader.push_back(BEMSTART);
	eblHeader.push_back(EBL_VERSION);
	// Version 1.001
	// Note there is also version 1.002
	eblHeader.push_back(0xEA);
	eblHeader.push_back(0x03);
	eblHeader.push_back(0x00);
	eblHeader.push_back(0x00);
	eblHeader.push_back(ESC);
	eblHeader.push_back(BEMEND);

	if (logFile->IsOpened()) {
		bytesWritten = logFile->Write(eblHeader.data(), eblHeader.size());
		if (bytesWritten != eblHeader.size()) {
			wxLogError(_T("TwoCan Actisense, Unable to write header %d"), logFile->GetLastError());
			logFile->ClearLastError();
		}
	}
	else {
		wxLogError(_T("TwoCan Actisense, Log File not opened while writing header"));
	}
}

// Log a complete NMEA 2000 message including header, checksum and escaping special characters
bool TwoCanActisense::WriteData(const CanHeader header, const byte *data, const unsigned int dataLen, wxFile *logFile) {

	unsigned int payloadLength = dataLen + 11; // actual data plus additional packet data bytes
	int checksum = 0;

	// An example Actisense data packet
	// command (1):		0x93
	// length (1):      0x13
	// priority (1);    0x02
	// PGN (3):         0x01 0xF8 0x01
	// destination(1):  0xFF
	// source (1):      0x01
	// time (4):        0x76 0xC2 0x52 0x00
	// len (1):         0x08
	// data (len):      0x08 0x70 0xEB 0x14 0xE8 0x8E 0x52 0xD2
	// CRC (1):			0xBB

	int bytesWritten;
	std::vector<byte> eblData;
	
	// Actisense Command Byte (in this case Rx)
	eblData.push_back(N2K_RX_CMD);

	// Total Length, Excludes command, length and checksum bytes
	eblData.push_back(payloadLength);

	// CAN Header
	eblData.push_back(header.priority);
	eblData.push_back(header.pgn & 0xFF);
	eblData.push_back((header.pgn >> 8) & 0xFF);
	eblData.push_back((header.pgn >> 16) & 0xFF);
	eblData.push_back(header.source);
	eblData.push_back(header.destination);

	// BUG BUG, Probably broken. Fix properly by recording the start time and then generate offsets
	// Time (milliseconds)
#if defined (__WXMSW__)
	SYSTEMTIME st;
	GetSystemTime(&st);
	eblData.push_back(st.wMilliseconds & 0xFF);
	eblData.push_back((st.wMilliseconds >> 8) & 0xFF);
	eblData.push_back((st.wMilliseconds >> 16) & 0xFF);
	eblData.push_back((st.wMilliseconds >> 24) & 0xFF);
#endif
#if (defined (__APPLE__) && defined (__MACH__)) || defined (__LINUX__)
	timeval currentTime;
	gettimeofday(&currentTime, NULL);   

	eblData.push_back(currentTime.tv_usec & 0xFF);
	eblData.push_back((currentTime.tv_usec >> 8) & 0xFF);
	eblData.push_back((currentTime.tv_usec >> 16) & 0xFF);
	eblData.push_back((currentTime.tv_usec >> 24) & 0xFF);
#endif

	// NMEA 2000 Data Size
	eblData.push_back(dataLen);

	// Copy payload
	for (size_t i = 0; i < dataLen; i++) {
		eblData.push_back(data[i]);
	}

	// Calculate and append the checksum
	for (auto it : eblData) {
		checksum += it;
	}

	// checksum ensures modulo 256 = 0
	checksum %= 256;

	if (checksum != 0) {
		checksum = 256 - checksum;
	}
	
	eblData.push_back(checksum);

	// Escape DLE & ESC characters
	for (std::vector<byte>::iterator it = eblData.begin(); it != eblData.end(); ++it) {
		if (*it == DLE) { // if the value is the same as a DLE it needs to be escaped
			it = eblData.insert(it , DLE);
			it++;
		}
		if (*it == ESC) { // if the value is the same as an ESC it also needs to be escaped
			it = eblData.insert(it, ESC);
			it++;
		}
	}

	// Insert the Start of Message
	eblData.insert(eblData.begin(), 1, STX);
	eblData.insert(eblData.begin(), 1, DLE);
	
	// Finally, append the End of Message
	eblData.push_back(DLE);
	eblData.push_back(ETX);

	if (logFile->IsOpened()) {
		bytesWritten = logFile->Write(eblData.data(), eblData.size());
		if (bytesWritten == eblData.size()) {
			return TRUE;
		}
		else {
			wxLogError(_T("TwoCan Actisense, Unable to write data %d"), logFile->GetLastError());
			logFile->ClearLastError();
		}
	}
	else {
		wxLogError(_T("TwoCan Actisense, Log File unexpectedly closed"));
	}
	return FALSE;
}