//
// Author: Steven Adler
// 
// Modified the existing dashboard plugin to create an "Engine Dashboard"
// Parses NMEA 0183 RSA, RPM & XDR sentences and displays Engine RPM, Oil Pressure, Water Temperature, 
// Alternator Voltage, Engine Hours andFluid Levels in a dashboard
//
// Version 1.0
// 10-10-2019
// 
// Please send bug reports to twocanplugin@hotmail.com or to the opencpn forum
//
/***************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  NMEA0183 Support Classes
 * Author:   Samuel R. Blackburn, David S. Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by Samuel R. Blackburn, David S Register           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 *   S Blackburn's original source license:                                *
 *         "You can use it any way you like."                              *
 *   More recent (2010) license statement:                                 *
 *         "It is BSD license, do with it what you will"                   *
 */


#include "nmea0183.h"

 /*
 $--RPM, a, x, x.x, x.x, A*hh<CR><LF>
		 |  |   |    |   |
		 |  |   |    |   Status: A = Data valid, V = Data invalid
		 |  |   |    Propeller pitch, % of max., "-" = astern
		 |  |   |Speed, rev / min, "-" = counter - clockwise
		 |  |Engine or shaft number, numbered from centerline 0 = single or on centerline
		Source, shaft / engine S / E odd = starboard,	even = port

 */


 //IMPLEMENT_DYNAMIC( RSA, RESPONSE)

RPM::RPM()
{
	Mnemonic = _T("RPM");
	Empty();
}

RPM::~RPM()
{
	Mnemonic.Empty();
	Empty();
}

void RPM::Empty(void)
{
	//   ASSERT_VALID( this);

	RevolutionsPerMinute = 0.0;
	EngineNumber = Unknown0183;
	PropellerPitch = 0.0;
	IsDataValid = Unknown0183;
	Source = wxEmptyString;
}

bool RPM::Parse(const SENTENCE& sentence)
{
	//   ASSERT_VALID( this);

	if (sentence.IsChecksumBad(6) == TRUE)
	{
		SetErrorMessage(_T("Invalid Checksum"));
		return(FALSE);
	}

	Source = sentence.Field(1).Mid(0, 1); 
	EngineNumber = sentence.Integer(2);
	RevolutionsPerMinute = sentence.Double(3);
	PropellerPitch = sentence.Double(4);
	IsDataValid = sentence.Boolean(5);

	return(TRUE);
}

bool RPM::Write(SENTENCE& sentence)
{
	//   ASSERT_VALID( this);

	   /*
	   ** Let the parent do its thing
	   */

	RESPONSE::Write(sentence);

	sentence += Source;
	sentence += EngineNumber;
	sentence += RevolutionsPerMinute;
	sentence += PropellerPitch;
	sentence += IsDataValid;

	sentence.Finish();

	return(TRUE);
}

const RPM& RPM::operator = (const RPM& source)
{
	//   ASSERT_VALID( this);

	Source = source.Source;
	EngineNumber = source.EngineNumber;
	RevolutionsPerMinute = source.RevolutionsPerMinute;
	PropellerPitch = source.PropellerPitch;
	IsDataValid = source.IsDataValid;

	return(*this);
}
