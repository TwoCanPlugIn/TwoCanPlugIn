// Copyright(C) 2018-2019 by Steven Adler
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
// Unit: TwoCanError - Get/Format error codes and Debug Print functions
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0

#include "twocanerror.h"

#ifdef __LINUX__
#include <cerrno>
#endif

char *GetErrorMessage(int errorCode) {
	
#ifdef  __WXMSW__ 

	// Retrieve the Windows system error message for the last-error code

	LPVOID lpMsgBuf;
	
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);
	return (char *)lpMsgBuf;
	
#endif

#ifdef __LINUX__

	// Retrieve the Linux system error message for the last-error code
	
	return (char *)strerror(errorCode);
	
#endif

}

