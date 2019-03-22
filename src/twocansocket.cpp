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
// Unit: TwoCanSocket - Implements SocketCAN interface for Linux devices
// Owner: twocanplugin@hotmail.com
// Date: 16/3/2019
// Version History: 
// 1.0 Initial Release
//

#include <twocansocket.h>

TwoCanSocket::TwoCanSocket(wxMessageQueue<std::vector<byte>> *messageQueue) : wxThread(wxTHREAD_JOINABLE) {
	// Save the TwoCAN Device message queue
	// NMEA 2000 messages are 'posted' to the TwoCan device for subsequent parsing
	deviceQueue = messageQueue;
}


TwoCanSocket::~TwoCanSocket() {
}

// List the available CAN interfaces, used by the Preferences Dialog to select a CAN hardware interface
std::vector<wxString> TwoCanSocket::ListCanInterfaces()  {
std::vector<wxString> canInterfaces;
struct if_nameindex *interfaceNameIndexes, *nameIndex;
	// Enumerate all network interfaces
    interfaceNameIndexes = if_nameindex();
    if (interfaceNameIndexes != NULL )  {
        for (nameIndex = interfaceNameIndexes; nameIndex->if_index != 0 || nameIndex->if_name != NULL; nameIndex++) {
			// Check if it is a CAN interface
			if (strstr(nameIndex->if_name, "can") != NULL) {
				canInterfaces.push_back(nameIndex->if_name);
				// Could also check if it is a CAN adapter if it has the correct (albeit default) MTU size
				//interfaceRequest.ifr_ifindex = nameIndex->if_index;
				//if(ioctl(socketDescriptor, SIOCGIFMTU, &interfaceRequest) >= 0) {
				//if (CAN_MTU == interfaceRequest.ifr_metric) {
				// Means it could be a CAN adapter, assuming no one has set the MTU for other interfaces to be the same
				//}
			}
        }
        // don't forget to free the memory!
		if_freenameindex(interfaceNameIndexes);
    }
    return canInterfaces;
}

// Open a socket descriptor
int TwoCanSocket::Open(const char *port) {
	
	canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (canSocket < 0)
	{
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_SOCKET_CREATE);
	}

	strcpy(canRequest.ifr_name, port);
	
	// Get the index of the interface
	if (ioctl(canSocket, SIOCGIFINDEX, &canRequest) < 0)
	{
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_SOCKET_IOCTL);
	}

	canAddress.can_family = AF_CAN;
	canAddress.can_ifindex = canRequest.ifr_ifindex;
	
	// Check if the interface is UP
	if (ioctl(canSocket, SIOCGIFFLAGS, &canRequest) < 0) 
    {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_SOCKET_IOCTL);
	}
     
    if ((canRequest.ifr_flags & IFF_UP)) { 
		wxLogMessage(_T("TwoCan Socket, %s interface is UP"), port); 
	}
	else {
		wxLogMessage(_T("TwoCan Socket, %s interface is DOWN"), port); 
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_SOCKET_DOWN);
	}

	// Set to non blocking
	// fcntl(canSocket, F_SETFL, O_NONBLOCK);

	// and if so, then bind
	if (bind(canSocket, (struct sockaddr *)&canAddress, sizeof(canAddress)) < 0)
	{
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_SOCKET_BIND);
	}

	return TWOCAN_RESULT_SUCCESS;
}

int TwoCanSocket::Close(void) {
	close(canSocket);
	return TWOCAN_RESULT_SUCCESS;
}

void TwoCanSocket::Read() {
	struct can_frame canSocketFrame;
	std::vector<byte> postedFrame(CONST_FRAME_LENGTH);
	int recvbytes = 0;
	
	canThreadIsAlive = 1;
	while (canThreadIsAlive)
	{
		struct timeval socketTimeout = { 1, 0 };
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(canSocket, &readSet);

		if (select((canSocket + 1), &readSet, NULL, NULL, &socketTimeout) >= 0)
		{
			if (TestDestroy())
			{
				canThreadIsAlive = 0;
				break;
			}
			if (FD_ISSET(canSocket, &readSet))
			{
				recvbytes = read(canSocket, &canSocketFrame, sizeof(struct can_frame));
				if (recvbytes)
				{
					// Copy the CAN Header										
					TwoCanUtils::ConvertIntegerToByteArray(canSocketFrame.can_id,&postedFrame[0]);
					
					// And the CAN Data
					for (int i = 0; i < canSocketFrame.can_dlc; i++) {
						postedFrame[4+i] = canSocketFrame.data[i];
					}
					
					// Post frame to the TwoCanDevice
					//memcpy(&postedFrame[0],&canFrame[0],CONST_FRAME_LENGTH);
					
					CanHeader header;
					TwoCanUtils::DecodeCanHeader(&postedFrame[0],&header);
					wxLogMessage(_T("Header: %u"),canSocketFrame.can_id);
					wxLogMessage(_T("PGN: %u"),header.pgn);
					wxLogMessage(_T("Source: %u"),header.source);
					wxLogMessage(_T("Destination: %u"),header.destination);
					wxLogMessage(_T("Priority: %u"),header.priority);
					wxLogMessage(_T("Length: %u"),canSocketFrame.can_dlc);
					
					deviceQueue->Post(postedFrame);
				}
			}
		}

	}

}
// Entry, the method that is executed upon thread start
wxThread::ExitCode TwoCanSocket::Entry() {
	// Merely loops continuously waiting for frames to be received by the CAN Adapter
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void TwoCanSocket::OnExit() {
	// Nothing to do ??
}

															
