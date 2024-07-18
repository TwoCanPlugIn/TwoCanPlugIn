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

#ifndef TWOCAN_LOGREADER_H
#define TWOCAN_LOGREADER_H

#include "twocaninterface.h"


#define CONST_TWOCAN_REGEX "^0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2})"
#define CONST_CANDUMP_REGEX "^\\([0-9]+.[0-9]+\\)\\s(slcan|vcan|can)[0-9]\\s([0-9A-F]{8})#([0-9A-F]{16})"
#define CONST_KEES_REGEX "^[0-9]{4}-[0-9]{2}-[0-9]{2}[TZ][0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3},([0-9]),([0-9]{5,6}),([0-9]+),([0-9]+),([0-9]),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2})"
#define CONST_YACHTDEVICES_REGEX "^[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\\sR\\s([0-9A-F]{8})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})"
#define CONST_SIGNALK_REGEX "^[0-9]{13};A;[0-9]{4}-[0-9]{2}-[0-9]{2}[TZ][0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}Z,([0-9]),([0-9]{5,6}),([0-9]+),([0-9]+),([0-9]),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2})"
#define CONST_RAYMARINE_REGEX "^(Tx|Rx)\\s[0-9]{8}\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\s([0-9A-Fa-f]{2})\\sFrom:[0-9A-Fa-f]{2}.*"


typedef enum _LogFileFormat {
	LOGFORMAT_UNDEFINED,
	LOGFORMAT_TWOCAN,
	LOGFORMAT_CANDUMP,
	LOGFORMAT_KEES,
	LOGFORMAT_YACHTDEVICES,
	LOGFORMAT_RAYMARINE
} LOG_FILE_FORMAT;


// Implements the generic log file reader on Linux & Mac OSX devices
class TwoCanLogReader : public TwoCanInterface {

public:
	// Constructor and destructor
	TwoCanLogReader(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanLogReader(void);

	// Raw CAN Frames
	byte canFrame[CONST_FRAME_LENGTH];

	// TwoCan Interface overridden functions
	int Open(const wxString& fileName);
	int Close(void);
	void Read();

	// Detect which log file format is used
	int TestFormat(std::string line);
	// Parse each of the different log file formats
	void ParseTwoCan(std::string str);
	void ParseCanDump(std::string str);
	void ParseKees(std::string str);
	void ParseYachtDevices(std::string str);
	void ParseRaymarine(std::string str);

protected:
	// TwoCan Interface overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	// Enum indicating whether the log format is twocan, canboatm candum, yachtdevices
	int logFileFormat;
	// Full path of the log file, fileName appended to the user's documents directory
	wxString logFileName;
	// File stream used to read lines from the log file
	std::ifstream logFileStream;
	// Regular expression to parse the log file format
	wxRegEx twoCanRegEx;
};

#endif
