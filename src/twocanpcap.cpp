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

// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCanPcap - Reads pcap log files (Eg. Wireshark, log format from Navico chartplotters)
// Owner: twocanplugin@hotmail.com
// Date: 10/7/2021
// Version History: 
// 1.0 Initial Release


#include <twocanpcap.h>

TwoCanPcap::TwoCanPcap(wxMessageQueue<std::vector<byte>> *messageQueue) : TwoCanInterface(messageQueue) {
}

TwoCanPcap::~TwoCanPcap() {
}

int TwoCanPcap::Open(const wxString& fileName) {
    std::vector<byte>  readBuffer(1024, 0);
	std::streamsize bytesRead;
	// Open the log file
	logFileName = wxStandardPaths::Get().GetDocumentsDir() + wxFileName::GetPathSeparator() + fileName;
	
	wxLogMessage(_T("TwoCan Pcap, Opening log file: %s"),logFileName);
	
	logFileStream.open(logFileName, std::ifstream::in | std::ifstream::binary);	

	if (logFileStream.fail()) {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_FILE_NOT_FOUND);
	}
	
	// Test the log file format by reading the pcap file header
    // refer to https://tools.ietf.org/id/draft-gharris-opsawg-pcap-00.html
	logFileStream.read(reinterpret_cast<char *>(readBuffer.data()), PCAP_FILE_HEADER_LENGTH); 

	bytesRead = logFileStream.gcount();

	if (bytesRead != PCAP_FILE_HEADER_LENGTH) {
        wxLogMessage(_T("TwoCan Pcap, Error reading pcap header"));
        return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_INVALID_LOGFILE_FORMAT);
	}

    unsigned int magicNumber = readBuffer[0] | (readBuffer[1] << 8) | (readBuffer[2] << 16) | (readBuffer[3] << 24);
	unsigned int majorVersion = readBuffer[4] | (readBuffer[5] << 8);
	unsigned int minorVersion = readBuffer[6] | (readBuffer[7] << 8);
		
    // Reserved 8,9,10,11,
	// Reserved12, 13, 14,15
		
	unsigned int snapLen = readBuffer[16] | (readBuffer[17] << 8) | (readBuffer[18] << 16) | (readBuffer[19] << 24);
	unsigned int linkType = readBuffer[20] | (readBuffer[21] << 8) | (readBuffer[22] << 16) | ((readBuffer[23] & 0x0F) << 24);
	unsigned char frameCyclicSequence = (readBuffer[23] & 0xF0) >> 4;

	wxLogMessage(_T("TwoCan Pcap, Magic Number: %X"), magicNumber);
	wxLogMessage(_T("TwoCan Pcap, Version: %d.%d"), majorVersion, minorVersion);
	wxLogMessage(_T("TwoCan Pcap, Snap Length: %d"), snapLen);
	wxLogMessage(_T("TwoCan Pcap, Link Type: %d"), linkType);
	wxLogMessage(_T("TwoCan Pcap, Frame Cyclic Sequence: %d"), frameCyclicSequence);

    // Check for a valid magic number, 0xA1B2C3D4 (seconds & microseconds) or 0xA1B23C4D (seconds & nanoseconds)	
    if ((magicNumber != 0xA1B2C3D4) && (magicNumber != 0xA1B23C4D)) { 
		wxLogMessage(_T("TwoCan Pcap, PCAP file invalid magic number: %X"), magicNumber);
        return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_INVALID_LOGFILE_FORMAT);
	}

    // Check that link type is valid, ie. SocketCAN	
	if (linkType != LINKTYPE_CAN_SOCKETCAN) { 
		wxLogMessage(_T("TwoCan Pcap, PCAP file is not Socket CAN: %u"), linkType);
        return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_INVALID_LOGFILE_FORMAT);
	}
	
	wxLogMessage(_T("TwoCan Pcap, File successfully opened"));
	return TWOCAN_RESULT_SUCCESS;
}

int TwoCanPcap::Close(void) {
	if (logFileStream.is_open()) {
		logFileStream.close();
		wxLogMessage(_T("TwoCan Pcap, Log File closed"));
	}
	return TWOCAN_RESULT_SUCCESS;
}


void TwoCanPcap::Read() {
	
	std::vector<byte>  readBuffer(1024, 0);
	std::streamsize bytesRead;
    std::vector<byte> canFrame(CONST_FRAME_LENGTH);

	unsigned int capturePacketLength;

    // Position cursor at begining of packet data
    logFileStream.clear();
	logFileStream.seekg(PCAP_FILE_HEADER_LENGTH, std::ios::beg); 
	

    while (!logFileStream.eof()) {

        if (!TestDestroy()) {
		
            readBuffer.clear();

            // Read packet header
            logFileStream.read(reinterpret_cast<char *>(readBuffer.data()), PCAP_PACKET_HEADER_LENGTH); 

            bytesRead = logFileStream.gcount();

            if (bytesRead == PCAP_PACKET_HEADER_LENGTH) {
                // BUG BUG Note that depending on magic number, microseconds may instead be nanoseconds (also Endianess)
                unsigned int seconds = readBuffer[0] | (readBuffer[1] << 8) | (readBuffer[2] << 16) | (readBuffer[3] << 24);
                unsigned int microSeconds = readBuffer[4] | (readBuffer[5] << 8) | (readBuffer[6] << 16) | (readBuffer[7] << 24);
                capturePacketLength = readBuffer[8] | (readBuffer[9] << 8) | (readBuffer[10] << 16) | (readBuffer[11] << 24);
                unsigned int actualPacketLength = readBuffer[12] | (readBuffer[13] << 8) | (readBuffer[14] << 16) | (readBuffer[15] << 24);

                // Read packet data
                readBuffer.clear();

                logFileStream.read(reinterpret_cast<char *>(readBuffer.data()), actualPacketLength); 

                bytesRead = logFileStream.gcount();

                if (bytesRead == actualPacketLength) {

                    PcapCanFrame *cf = (PcapCanFrame *)readBuffer.data();

                    // BUG BUG Should check that this is an extended frame
                    canFrame[3] = cf->can_id & 0xFF;
                    canFrame[2] = (cf->can_id >> 8) & 0xFF;
                    canFrame[1] = (cf->can_id >> 16) & 0xFF;
                    canFrame[0] = (cf->can_id >> 24) & 0xFF;

                    // BUG BUG Should check that DLC == 8                         
                    canFrame[4] = cf->data[0];
                    canFrame[5] = cf->data[1];
                    canFrame[6] = cf->data[2];
                    canFrame[7] = cf->data[3];
                    canFrame[8] = cf->data[4];
                    canFrame[9] = cf->data[5];
                    canFrame[10] = cf->data[6];
                    canFrame[11] = cf->data[7];
                        
                    // Post frame to TwoCan device
                    deviceQueue->Post(canFrame);
                    // BUG BUG Should really calculate the delay based on the packet time
                    wxThread::Sleep(20);
            
                }
                else {
                    // BUG BUG Error reading data
                }
            }
            else {
                // BUG BUG Error reading packet header   
            }

            // If end of file, rewind to beginning of packet data
            if (logFileStream.eof()) {
                logFileStream.clear();
                logFileStream.seekg(PCAP_FILE_HEADER_LENGTH, std::ios::beg); 
            }
        }
        
        else {
			// Thread Exiting
			break;
		}
        
	} // end not eof
	
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode TwoCanPcap::Entry() {
	// Merely loops continuously waiting for frames to be received by the CAN Adapter
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void TwoCanPcap::OnExit() {
	// Nothing to do ??
}
