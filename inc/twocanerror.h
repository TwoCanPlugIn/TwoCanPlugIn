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

#ifndef TWOCAN_ERROR_H
#define TWOCAN_ERROR_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#ifdef  __WXMSW__ 
	#define WINDOWS_LEAN_AND_MEAN
	#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

// Formatted debug output
void DebugPrintf(wchar_t *fmt, ...);

#ifdef  __WXMSW__ 
	// Routine to retrieve human readable win32 error message
	char *GetErrorMessage(int win32ErrorCode);
#endif
#ifdef __LINUX__
	// Routine to retrieve human readable Linux error message
	char *GetErrorMessage(int linuxErrorCode);
#endif

// Macros to set and print error codes

#define SET_ERROR(level,source,code) (int) (level | source | (code << 16))

#define PRINT_ERROR(error) wprintf(L"Level 0x%X\nSource 0x%X\nCode %d\n", error & 0x60000000, error & 0x1C000000, (error & 0xFF0000) >> 16);


// Error Codes and Sources
// 0       1       2       3
// 01234567012345670123456701234567
// Byte 0, Bit 0 - unused
// Byte 0, Bits 1,2,3 Error Level
// Byte 0, Bits 4,5,6,7 Error Source
// Byte 1, Error Code
// Byte 2,3  Win32 Error Codes (from GetLastError)

// Error levels
#define TWOCAN_RESULT_SUCCESS 0
#define TWOCAN_RESULT_FATAL 0x60000000 
#define TWOCAN_RESULT_ERROR 0x40000000 
#define TWOCAN_RESULT_WARNING 0x20000000 

// Error sources
#define TWOCAN_SOURCE_PLUGIN 0x1C000000
#define TWOCAN_SOURCE_DEVICE 0xC000000
#define TWOCAN_SOURCE_SETTINGS 0x8000000
#define TWOCAN_SOURCE_DRIVER 0x4000000

// Error codes
#define TWOCAN_ERROR_CREATE_FRAME_RECEIVED_EVENT 1
#define TWOCAN_ERROR_CREATE_FRAME_RECEIVED_MUTEX 2
#define TWOCAN_ERROR_CREATE_THREAD_COMPLETE_EVENT 3
#define TWOCAN_ERROR_CONFIGURE_ADAPTER 4
#define TWOCAN_ERROR_CONFIGURE_PORT 5
#define TWOCAN_ERROR_DELETE_FRAME_RECEIVED_EVENT 6
#define TWOCAN_ERROR_DELETE_FRAME_RECEIVED_MUTEX 7
#define TWOCAN_ERROR_DELETE_THREAD_COMPLETE_EVENT 8
#define TWOCAN_ERROR_DELETE_THREAD_HANDLE 9
#define TWOCAN_ERROR_CREATE_THREAD_HANDLE 10
#define TWOCAN_ERROR_CREATE_SERIALPORT 11
#define TWOCAN_ERROR_DELETE_SERIALPORT 12
#define TWOCAN_ERROR_SET_FRAME_RECEIVED_MUTEX 13
#define TWOCAN_ERROR_SET_FRAME_RECEIVED_EVENT 14
#define TWOCAN_ERROR_SET_BUS_ON 15
#define TWOCAN_ERROR_SET_BUS_SPEED 16
#define TWOCAN_ERROR_SET_BUS_OFF 17
#define TWOCAN_ERROR_GET_SETTINGS 18
#define TWOCAN_ERROR_SET_SETTINGS 19
#define TWOCAN_ERROR_LOAD_LIBRARY 20
#define TWOCAN_ERROR_UNLOAD_LIBRARY 21
#define TWOCAN_ERROR_OPEN_LOGFILE 22
#define TWOCAN_ERROR_CLOSE_LOGFILE 23
#define TWOCAN_ERROR_INVALID_OPEN_FUNCTION 24
#define TWOCAN_ERROR_INVALID_READ_FUNCTION 25
#define TWOCAN_ERROR_INVALID_CLOSE_FUNCTION 26
#define TWOCAN_ERROR_FAST_MESSAGE_BUFFER_FULL 27
#define TWOCAN_ERROR_DRIVER_NOT_FOUND 28
#define TWOCAN_ERROR_OPEN_DATA_RECEIVED_EVENT 29
#define TWOCAN_ERROR_ADDRESS_CLAIM_FAILURE 30
#define TWOCAN_ERROR_DUPLICATE_ADDRESS 31
#define TWOCAN_ERROR_COMANDED_ADDRESS 32
#define TWOCAN_ERROR_PRODUCT_INFO_FAILURE 33
#define TWOCAN_ERROR_TRANSMIT_FAILURE 34
#define TWOCAN_ERROR_RECEIVE_FAILURE 35
#define TWOCAN_ERROR_PATH_NOT_FOUND 36
#define TWOCAN_ERROR_FILE_NOT_FOUND 37
#define TWOCAN_ERROR_ADAPTER_NOT_FOUND 38
#define TWOCAN_ERROR_INVALID_LOGFILE_FORMAT 39
#define TWOCAN_ERROR_SOCKET_CREATE 40
#define TWOCAN_ERROR_SOCKET_IOCTL 41
#define TWOCAN_ERROR_SOCKET_BIND 42
#define TWOCAN_ERROR_SOCKET_FLAGS 43
#define TWOCAN_ERROR_SOCKET_READ 44
#define TWOCAN_ERROR_SOCKET_DOWN 45
#define TWOCAN_ERROR_SOCKET_WRITE 46
#define TWOCAN_ERROR_INVALID_WRITE_FUNCTION 47
#endif
