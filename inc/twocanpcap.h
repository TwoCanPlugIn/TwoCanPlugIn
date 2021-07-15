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

#ifndef TWOCAN_PCAP_H
#define TWOCAN_PCAP_H

#include "twocaninterface.h"


#define PCAP_FILE_HEADER_LENGTH 24
#define PCAP_PACKET_HEADER_LENGTH 16
//Refer to pcap-common.c
#define LINKTYPE_CAN_SOCKETCAN  227

// Can Frame format used in Pcap
typedef struct PcapCanFrame {
unsigned int can_id;  // 32 bit CAN_ID + EFF/RTR/ERR flags 
byte   can_dlc; // data length code: 0 .. 8 
byte reserved[3]; // 3 bytes reserved to pad to 8 byte boundary
byte   data[8];
} PcapCanFrame;


// Implements the generic log file reader on Linux & Mac OSX devices
class TwoCanPcap : public TwoCanInterface {

public:
	// Constructor and destructor
	TwoCanPcap(wxMessageQueue<std::vector<byte>> *messageQueue);
	~TwoCanPcap(void);

	// Raw CAN Frames
	byte canFrame[CONST_FRAME_LENGTH];

	// TwoCan Interface overridden functions
	int Open(const wxString& fileName);
	int Close(void);
	void Read(void);

		
protected:
	// TwoCan Interface overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	// Full path of the pcap log file, fileName appended to the user's documents directory
	wxString logFileName;
	// File stream used to read lines from the log file
	std::ifstream logFileStream;
	
};

#endif
