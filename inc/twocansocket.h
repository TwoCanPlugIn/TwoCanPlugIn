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

#ifndef TWOCAN_SOCKET_H
#define TWOCAN_SOCKET_H

#include "twocanerror.h"
#include "twocanutils.h"

// SocketCAN
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include <vector>

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

// Implements the SocketCAN interface on Linux devices
class TwoCanSocket : public wxThread {

public:
	// Constructor and destructor
	TwoCanSocket(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanSocket(void);

	// Reference to TwoCan Device CAN Frame message queue
	wxMessageQueue<std::vector<byte>> *deviceQueue;

	// Open and Close the CAN interface
	// As we don't throw errors in the ctor, invoke functions that may fail from these
	int Open(const char *port);
	int Close(void);
	int Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload);
	void Read();
	static std::vector<wxString> ListCanInterfaces();
	static int GetUniqueNumber(unsigned long *uniqueNumber);


protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	// CAN connection variables
	struct sockaddr_can canAddress;
	// Interface Request
	struct ifreq canRequest;
	// Socket Descriptor
	int canSocket;
	int flags;
	// Detect when thread is killed
	int canThreadIsAlive;

};

#endif
