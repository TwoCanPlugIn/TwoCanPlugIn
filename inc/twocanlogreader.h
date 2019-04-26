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

#include "twocanerror.h"
#include "twocanutils.h"

// wxWidgets
// BUG BUG work out which ones we really need
#include <wx/defs.h>
// String Format, Comparisons etc.
#include <wx/string.h>
// For converting NMEA 2000 date & time data
#include <wx/datetime.h>
// Raise events to the plugin
#include <wx/event.h>
// Perform read operations in their own thread
#include <wx/thread.h>
// Logging (Info & Errors)
#include <wx/log.h>
// Logging (raw NMEA 2000 frames)
#include <wx/file.h>
// User's paths/documents folder
#include <wx/stdpaths.h>
// Message Queue
#include <wx/msgqueue.h>
// Regular Expressions
#include <wx/regex.h>


// STL
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <initializer_list>
#include <iostream>

#define CONST_TWOCAN_REGEX "^0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2}),0x([0-9A-Fa-f]{2})"
#define CONST_CANDUMP_REGEX "^\\([0-9]+.[0-9]+\\)\\scan[0-9]\\s([0-9A-F]{8})#([0-9A-F]{16})"
#define CONST_KEES_REGEX "^[0-9]{4}-[0-9]{2}-[0-9]{2}[TZ][0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3},([0-9]),([0-9]{5,6}),([0-9]+),([0-9]+),([0-9]),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2}),([0-9A-Fa-f]{2})"
#define CONST_YACHTDEVICES_REGEX "^[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\\sR\\s([0-9A-F]{8})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})"
enum LogFileFormat { Undefined, TwoCanRaw, CanDump, Kees, YachtDevices};

// Implements the log file reader on Linux devices
class TwoCanLogReader : public wxThread {

public:
	// Constructor and destructor
	TwoCanLogReader(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanLogReader(void);

	// Reference to TwoCan Device CAN Frame message queue
	wxMessageQueue<std::vector<byte>> *deviceQueue;
	
	// Raw CAN Frames
	byte canFrame[CONST_FRAME_LENGTH];

	// Open and Close the log file
	// As we don't throw errors in the constructor, invoke functions that may fail from these
	int Open(const wchar_t *fileName);
	int Close(void);
	// Read from the file in a detached thread
	void Read();
	// Detect which log file format is used
	int TestFormat(std::string line);
	// Parse each of the different log file formats
	void ParseTwoCan(std::string str);
	void ParseCanDump(std::string str);
	void ParseKees(std::string str);
	void ParseYachtDevices(std::string str);
	
protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	int logFileFormat;
	wxString logFileName;
	std::ifstream logFileStream;
	wxRegEx twoCanRegEx;
};

#endif
