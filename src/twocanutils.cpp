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

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: TwoCan utility functions
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// Outstanding Features: 
// 1. Move DecodeCanHeader etc. from TwoCanDevice to here.
// 3. Any additional functions ??
//

#include "twocanutils.h"

int TwoCanUtils::ConvertByteArrayToInteger(const byte *buf, unsigned int *value) {
	if ((buf != NULL) && (value != NULL)) {
		*value = buf[3] | buf[2] << 8 | buf[1] << 16 | buf[0] << 24;
		return TRUE;
	}
	else {
		return FALSE;
	}
}