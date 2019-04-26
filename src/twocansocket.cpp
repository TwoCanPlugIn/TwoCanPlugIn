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

// Generate a unique number derived from the MAC Address to be used as a NMEA 2000 device's unique address
int TwoCanSocket::GetUniqueNumber(unsigned long *uniqueNumber) {
	*uniqueNumber = 0;
    struct if_nameindex *interfaceNameIndexes, *nameIndex;
    // Enumerate all network interfaces
    interfaceNameIndexes = if_nameindex();
    if (interfaceNameIndexes != NULL )  {
        for (nameIndex = interfaceNameIndexes; nameIndex->if_index != 0 || nameIndex->if_name != NULL; nameIndex++) {
			// Check if it is an ethernet or wireless interface, each of which posses a unique MAC address
			// We'll just use the first interface from which to derive a unique number
			if ((strstr(nameIndex->if_name, "eth") != NULL) || (strstr(nameIndex->if_name, "wlan") != NULL )){
				int fileDescriptor;
				fileDescriptor= socket(AF_INET, SOCK_DGRAM, 0);
				if (fileDescriptor >= 0) {
					struct ifreq interfaceRequest;
					interfaceRequest.ifr_addr.sa_family = AF_INET;
					strncpy((char *)interfaceRequest.ifr_name , nameIndex->if_name , IFNAMSIZ-1);
					if (ioctl(fileDescriptor, SIOCGIFHWADDR, &interfaceRequest) >=0 ) {
						// close the socket, we're not going to use it any further
						close(fileDescriptor);
						// use two halves of the mac address in a pairing function to derive a shorter integer value for the unique number
						// as I'm not sure what is the maximum length for a NMEA 2000 unique address
						char temp[9];
						unsigned int pair1, pair2;
						sprintf(temp,"%d%d%d",interfaceRequest.ifr_hwaddr.sa_data[0], interfaceRequest.ifr_hwaddr.sa_data[1], interfaceRequest.ifr_hwaddr.sa_data[2]);
						pair1 = atoi(temp); 
						sprintf(temp,"%d%d%d",interfaceRequest.ifr_hwaddr.sa_data[3], interfaceRequest.ifr_hwaddr.sa_data[4], interfaceRequest.ifr_hwaddr.sa_data[5]);
						pair2 = atoi(temp); 
						*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;;
						break;
					}
				}
			}
        }
        // don't forget to free the memory!
		if_freenameindex(interfaceNameIndexes);
    }
	
	if (*uniqueNumber == 0) {
		srand(CONST_PRODUCT_CODE);
		unsigned int pair1, pair2;
		pair1 = rand();
		pair2 = rand();
		*uniqueNumber = (((pair1 + pair2) * (pair1 + pair2 + 1)) / 2) + pair2;;
	}
	// Unique Number is a maximum of 21 bits in length;
	*uniqueNumber &= 0x1FFFFF;
	return TWOCAN_RESULT_SUCCESS;
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
						postedFrame[CONST_HEADER_LENGTH + i] = canSocketFrame.data[i];
					}
					
					// Post frame to the TwoCanDevice
					deviceQueue->Post(postedFrame);
				}
			}
		}

	}

}
// Write, Transmit a CAN frame onto the NMEA 2000 network
int TwoCanSocket::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
	struct can_frame canSocketFrame;
	// Set the 29 bit CAN Id, note no RTR/ERR flags but must set EFF flag as we're using a 29 bit ID
	// BUG BUG #define EFF 0x80000000
	canSocketFrame.can_id =  0x80000000 | canId;
	// Set the data length and the data
	canSocketFrame.can_dlc = payloadLength;
	memcpy(canSocketFrame.data,payload,payloadLength);
	// Now send it
	int returnCode;
	returnCode = write(canSocket, &canSocketFrame, sizeof(struct can_frame));
	if (returnCode != sizeof(struct can_frame)) {
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_SOCKET_WRITE);
	}
	else {
		return (TWOCAN_RESULT_SUCCESS);
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

															
