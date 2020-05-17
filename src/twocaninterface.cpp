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

// Project: TwoCan Plugin
// Description: TwoCan plugin for OpenCPN
// Unit: TwoCanInterface - Abstract class for CAN Adapter interfaces on Linux & Mac OSX
// Owner: twocanplugin@hotmail.com
// Date: 10/5/2020
// Version History: 
// 1.8 Initial Release, Mac OSX support
//

#include <twocaninterface.h>

// Constructor
TwoCanInterface::TwoCanInterface(wxMessageQueue<std::vector<byte>> *messageQueue) : wxThread(wxTHREAD_JOINABLE) {
	// Save the TwoCan Device message queue
	// NMEA 2000 messages are 'posted' to the TwoCan device for subsequent parsing
	deviceQueue = messageQueue;
}

// Destructor
TwoCanInterface::~TwoCanInterface() {
}

// Entry, the method that is invoked upon thread start
wxThread::ExitCode TwoCanInterface::Entry() {
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, the method that is invoked when thread is being destroyed
void TwoCanInterface::OnExit() {
}

// Detect & connect to the adapter interface and "open" the CAN bus
int TwoCanInterface::Open(const wxString& fileName) {
	return TWOCAN_RESULT_SUCCESS;
}

// Close the CAN bus and perform any necessary clean up
int TwoCanInterface::Close(void) {
	return TWOCAN_RESULT_SUCCESS;
}

// Read data from the bus/log file, convert as necessary and pass to the TwoCan device message queue
void TwoCanInterface::Read() {
}

// Convert NMEA 2000 message as required for transmission onto the CAN bus
int TwoCanInterface::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
	return TWOCAN_RESULT_SUCCESS;
}

// Generate a 29bit Unique number, using random numbers and a pairing function
int TwoCanInterface::GetUniqueNumber(unsigned long *uniqueNumber) {
	srand(CONST_PRODUCT_CODE);
	unsigned int pair1, pair2;
	pair1 = rand();
	pair2 = rand();
	*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;
	*uniqueNumber &= 0x1FFFFF;
	return TWOCAN_RESULT_SUCCESS;
}
