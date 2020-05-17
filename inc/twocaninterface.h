// Copyright(C) 2018-2020 by Steven Adler
//
// This file is part of TwoCan plugin for OpenCPN.
//
// TwoCan plugin for OpenCPN is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TwoCan plugin for OpenCPN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the TwoCan plugin for OpenCPN. If not, see <https://www.gnu.org/licenses/>.
//
// NMEA2000® is a registered trademark of the National Marine Electronics Association

#ifndef TWOCAN_INTERFACE_H
#define TWOCAN_INTERFACE_H

#include "twocanerror.h"
#include "twocanutils.h"

// wxWidgets
// BUG BUG work out which ones we really need
#include <wx/defs.h>

// String format, string comparisons etc.
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

// For path separator
#include <wx/filename.h>

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
#include <vector>


// abstract class for CAN Adapter interfaces (Mac OSX & Linux Log File Readers, Linux SocketCAN, Mac OSX Serial USB devices such as Canable Cantact)
class TwoCanInterface : public wxThread {

public:
	// Constructor and destructor
	TwoCanInterface(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanInterface(void);

	// Reference to TwoCan Device message queue to which we post NMEA 2000 messages
	wxMessageQueue<std::vector<byte>> *deviceQueue;
	
	// Functions to be overridden in derived classes
	virtual int Open(const wxString& fileName);
	virtual int Close(void);
	virtual void Read();
	virtual int Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload);
	virtual int GetUniqueNumber(unsigned long *uniqueNumber);
	
	
protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();
	
};

#endif
