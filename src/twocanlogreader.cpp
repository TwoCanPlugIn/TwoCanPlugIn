// Copyright(C) 2019 by Steven Adler
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

// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCanLogReader - Reads log files on Linux 
// Owner: twocanplugin@hotmail.com
// Date: 16/3/2019
// Version History: 
// 1.0 Initial Release
//

#include <twocanlogreader.h>

TwoCanLogReader::TwoCanLogReader(wxMessageQueue<std::vector<byte>> *messageQueue) : wxThread(wxTHREAD_JOINABLE) {
	// Save the TwoCAN Device message queue
	// NMEA 2000 messages are 'posted' to the TwoCan device for subsequent parsing
	deviceQueue = messageQueue;
	
	// Initialize the different log file format regular expressions
	//KeesRegex.assign(CONST_KEES_REGEX);
	//TwoCanRegex.assign(CONST_TWOCAN_REGEX);
	//CanDumpRegex.assign(CONST_CANDUMP_REGEX);
	//YachtDevicesRegex.assign(CONST_YACHTDEVICES_REGEX);
}

TwoCanLogReader::~TwoCanLogReader() {
// Nothing to do in the destructor ??
}

int TwoCanLogReader::Open(const wchar_t *fileName) {
	// Open the log file
	logFileName = wxString::Format("%s/%s", wxStandardPaths::Get().GetDocumentsDir(),fileName);
	
	wxLogMessage(_T("TwoCanLogReader::Open, Opening log file: %s"),logFileName);
	
	logFileStream.open(logFileName, std::ifstream::in);	

	if (logFileStream.fail()) {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_FILE_NOT_FOUND);
	}
	
	// Test the log file format
	std::string line;
	getline(logFileStream, line);
	logFileFormat = TestFormat(line);
	if (logFileFormat == Undefined) {
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_INVALID_LOGFILE_FORMAT);
	}
	
	// If a valid format, rewind to the beginning and return SUCCESS
	logFileStream.clear();
	logFileStream.seekg(0, std::ios::beg); 
	
	wxLogMessage(_T("TwoCanLogReader, File opened, Log File Format: %u"),logFileFormat);
	return TWOCAN_RESULT_SUCCESS;
}

int TwoCanLogReader::Close(void) {
	if (logFileStream.is_open()) {
		logFileStream.close();
	}
	return TWOCAN_RESULT_SUCCESS;
}

void TwoCanLogReader::ParseTwoCan(std::string str) {
	if (twoCanRegEx.Matches(str,wxRE_DEFAULT)) {
	// BUG BUG I'm sure strtok is more efficient !!
		canFrame[0] = std::strtoul(twoCanRegEx.GetMatch(str,1), NULL, 16);
		canFrame[1] = std::strtoul(twoCanRegEx.GetMatch(str,2), NULL, 16);
		canFrame[2] = std::strtoul(twoCanRegEx.GetMatch(str,3), NULL, 16);
		canFrame[3] = std::strtoul(twoCanRegEx.GetMatch(str,4), NULL, 16);
		canFrame[4] = std::strtoul(twoCanRegEx.GetMatch(str,5), NULL, 16);
		canFrame[5] = std::strtoul(twoCanRegEx.GetMatch(str,6), NULL, 16);
		canFrame[6] = std::strtoul(twoCanRegEx.GetMatch(str,7), NULL, 16);
		canFrame[7] = std::strtoul(twoCanRegEx.GetMatch(str,8), NULL, 16);
		canFrame[8] = std::strtoul(twoCanRegEx.GetMatch(str,9), NULL, 16);
		canFrame[9] = std::strtoul(twoCanRegEx.GetMatch(str,10), NULL, 16);
		canFrame[10] = std::strtoul(twoCanRegEx.GetMatch(str,11), NULL, 16);
		canFrame[11] = std::strtoul(twoCanRegEx.GetMatch(str,12), NULL, 16);
	}
	return;
}

void TwoCanLogReader::ParseCanDump(std::string str) {
	if (twoCanRegEx.Matches(str,wxRE_DEFAULT)) {
		unsigned long temp = std::strtoul(twoCanRegEx.GetMatch(str,1), NULL, 16);
		memcpy(&canFrame[0],&temp, 4);
		wxString tmpString = twoCanRegEx.GetMatch(str,2).c_str();
		byte tmpChar[tmpString.size()];
        memcpy(&tmpChar[0], tmpString,tmpString.size());
       	TwoCanUtils::ConvertHexStringToByteArray(&tmpChar[0], 8, &canFrame[4]);
	}
	return;	
}

void TwoCanLogReader::ParseKees(std::string str) {
	if (twoCanRegEx.Matches(str,wxRE_DEFAULT)) {
		CanHeader header;
		
		header.source = atoi(twoCanRegEx.GetMatch(str,3));
		header.destination = atoi(twoCanRegEx.GetMatch(str,4));
		header.pgn = atoi(twoCanRegEx.GetMatch(str,2));
		header.priority = atoi(twoCanRegEx.GetMatch(str,1));
		
		// BUG BUG Should create a function EncodeCanHeader
		// Encodes a 29 bit CAN header
		canFrame[3] = ((header.pgn >> 16) & 0x01) | (header.priority << 2);
		canFrame[2] = ((header.pgn & 0xFF00) >> 8);
		canFrame[1] = (canFrame[2] > 239) ? (header.pgn & 0xFF) : header.destination;
		canFrame[0] = header.source;
		
		canFrame[4] = std::strtoul(twoCanRegEx.GetMatch(str,6), NULL, 16);
		canFrame[5] = std::strtoul(twoCanRegEx.GetMatch(str,7), NULL, 16);
		canFrame[6] = std::strtoul(twoCanRegEx.GetMatch(str,8), NULL, 16);
		canFrame[7] = std::strtoul(twoCanRegEx.GetMatch(str,9), NULL, 16);
		canFrame[8] = std::strtoul(twoCanRegEx.GetMatch(str,10), NULL, 16);
		canFrame[9] = std::strtoul(twoCanRegEx.GetMatch(str,11), NULL, 16);
		canFrame[10] = std::strtoul(twoCanRegEx.GetMatch(str,12), NULL, 16);
		canFrame[11] = std::strtoul(twoCanRegEx.GetMatch(str,13), NULL, 16);
	}
	return;
}

void TwoCanLogReader::ParseYachtDevices(std::string str) {
	if (twoCanRegEx.Matches(str,wxRE_DEFAULT)) {
		unsigned long temp = std::strtoul(twoCanRegEx.GetMatch(str,1), NULL, 16);
		memcpy(&canFrame[0], &temp, 4);
		canFrame[4] = std::strtoul(twoCanRegEx.GetMatch(str,2), NULL, 16);
		canFrame[5] = std::strtoul(twoCanRegEx.GetMatch(str,3), NULL, 16);
		canFrame[6] = std::strtoul(twoCanRegEx.GetMatch(str,4), NULL, 16); 
		canFrame[7] = std::strtoul(twoCanRegEx.GetMatch(str,5), NULL, 16);
		canFrame[8] = std::strtoul(twoCanRegEx.GetMatch(str,6), NULL, 16);
		canFrame[9] = std::strtoul(twoCanRegEx.GetMatch(str,7), NULL, 16);
		canFrame[10] = std::strtoul(twoCanRegEx.GetMatch(str,8), NULL, 16);
		canFrame[11] = std::strtoul(twoCanRegEx.GetMatch(str,9), NULL, 16);
	}
	return;
}

int TwoCanLogReader::TestFormat(std::string line) {
	// BUG BUG Should check that the Regular Expression is valid
	// eg. twoCanRegEx.IsValid()
	twoCanRegEx.Compile(CONST_TWOCAN_REGEX, wxRE_ADVANCED |  wxRE_NEWLINE);
	if (twoCanRegEx.Matches(line,wxRE_DEFAULT)) {
		return TwoCanRaw;
	}
	twoCanRegEx.Compile(CONST_CANDUMP_REGEX, wxRE_ADVANCED | wxRE_NEWLINE);
	if (twoCanRegEx.Matches(line,wxRE_DEFAULT)) {
		return CanDump;
	}
	twoCanRegEx.Compile(CONST_KEES_REGEX, wxRE_ADVANCED | wxRE_NEWLINE);
	if (twoCanRegEx.Matches(line,wxRE_DEFAULT))  {
		return Kees;
	}
	twoCanRegEx.Compile(CONST_YACHTDEVICES_REGEX, wxRE_ADVANCED |  wxRE_NEWLINE);
	if (twoCanRegEx.Matches(line,wxRE_DEFAULT)) {
		return YachtDevices;
	}
	return Undefined;
}

void TwoCanLogReader::Read() {
	std::string inputLine;
	std::vector<byte> foobar(CONST_FRAME_LENGTH);
	while (!logFileStream.eof()) {
		getline(logFileStream, inputLine);
		if (!TestDestroy()) {
		// process the line
			switch(logFileFormat) {
				case TwoCanRaw:
					ParseTwoCan(inputLine);
					break;
				case CanDump:
					ParseCanDump(inputLine);
					break;
				case Kees:
					ParseKees(inputLine);
					break;
				case YachtDevices:
					ParseYachtDevices(inputLine);
					break;
				case Undefined:
					// should log invalid log file format message here.
					break;
			}
			
			// Post frame to TwoCan device
			// BUG BUG Do we need to protect access to the canFrame using a mutex
			memcpy(&foobar[0],&canFrame[0],CONST_FRAME_LENGTH);
			deviceQueue->Post(foobar);
			wxThread::Sleep(20);
		} 
		else {
			// Thread Exiting
			break;
		}
		
		// If end of file, rewind to beginning
		if (logFileStream.eof()) {
			logFileStream.clear();
			logFileStream.seekg(0, std::ios::beg); 
		}
	} // end while not eof
	
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode TwoCanLogReader::Entry() {
	// Merely loops continuously waiting for frames to be received by the CAN Adapter
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void TwoCanLogReader::OnExit() {
	// Nothing to do ??
}
