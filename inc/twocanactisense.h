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
// Actisense® is a registered Trademark of Active Research Limited

#ifndef TWOCAN_EBL_H
#define TWOCAN_EBL_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
      #include <wx/wx.h>
#endif

#include "wx/file.h"
#include "wx/log.h"

#include "twocanutils.h"

// ASCII Control Chracters
const byte DLE = 0x10;
const byte STX = 0x02;
const byte ETX = 0x03;
const byte ESC = 0x1B;
const byte BEMSTART = 0x01;
const byte BEMEND = 0x0A;
const byte EBL_VERSION = 0x01;
const byte EBL_DESCRIPTION = 0x02;
const byte EBL_TIME = 0x03;
const byte EBL_ELEMENT = 0x04;

// Actisense NGT-1 commands
const byte N2K_TX_CMD = 0x92;
const byte N2K_RX_CMD = 0x93;
const byte NGT_TX_CMD = 0xA1;
const byte NGT_RX_CMD = 0xA3;

class TwoCanActisense {
	
public:
      
static bool VerifyChecksum(std::vector<byte> data, int verify);
static void WriteHeader(wxFile *logFile);
static bool WriteData(const CanHeader header, const byte *data, const unsigned int dataLen, wxFile *logFile);

};

#endif
