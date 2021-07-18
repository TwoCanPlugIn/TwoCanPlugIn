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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *                                                                         *
 *   S Blackburn's original source license:                                *
 *         "You can use it any way you like."                              *
 *   More recent (2010) license statement:                                 *
 *         "It is BSD license, do with it what you will"                   *
 */

#include "nmea0183.h"

BWR::BWR()
{
	Mnemonic = _T("BWR");
	Empty();
}

BWR::~BWR()
{
	Mnemonic.Empty();
	Empty();
}

void BWR::Empty( void ) 
{
   UTCTime.clear();
   BearingTrue     = 0.0;
   BearingMagnetic = 0.0;
   NauticalMiles   = 0.0;
   To.clear();
   Position.Empty();
}

bool BWR::Parse( SENTENCE const& sentence )
{
   /*
   ** BWR - Bearing and Distance to Waypoint - Rhumb Line
   ** Latitude, N/S, Longitude, E/W, UTC, Status
   **                                                       11
   **        1         2       3 4        5 6   7 8   9 10  | 12   13
   **        |         |       | |        | |   | |   | |   | |    |
   ** $--BWR,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x.x,T,x.x,M,x.x,N,c--c*hh<CR><LF>
   **
   **  1) UTCTime
   **  2) Waypoint Latitude
   **  3) N = North, S = South
   **  4) Waypoint Longitude
   **  5) E = East, W = West
   **  6) Bearing, True
   **  7) T = True
   **  8) Bearing, Magnetic
   **  9) M = Magnetic
   ** 10) Nautical Miles
   ** 11) N = Nautical Miles
   ** 12) Waypoint ID
   ** 13) FAA Mode
   ** 14) Checksum
   */

   /*
   ** First we check the checksum...
   */

	NMEA0183_BOOLEAN check = sentence.IsChecksumBad( 14 );
	if ( check == NTrue )
   {
       SetErrorMessage(_T("Invalid Checksum"));
       return( false );
   } 

   UTCTime         = sentence.Field( 1 );
   Position.Parse( 2, 3, 4, 5, sentence );
   BearingTrue     = sentence.Double( 6 );
   BearingMagnetic = sentence.Double( 8 );
   NauticalMiles   = sentence.Double( 10 );
   To              = sentence.Field( 12 );

   return( true );
}

bool BWR::Write( SENTENCE& sentence ) 
{
   /*
   ** Let the parent do its thing
   */
   
   RESPONSE::Write( sentence );

   sentence += UTCTime;
   sentence += Position;
   sentence += BearingTrue;
   sentence += "T";
   sentence += BearingMagnetic;
   sentence += "M";
   sentence += NauticalMiles;
   sentence += "N";
   sentence += To;

   sentence.Finish();

   return( TRUE );
}

const BWR& BWR::operator = ( const BWR& source )
{
   UTCTime         = source.UTCTime;
   Position        = source.Position;
   BearingTrue     = source.BearingTrue;
   BearingMagnetic = source.BearingMagnetic;
   NauticalMiles   = source.NauticalMiles;
   To              = source.To;

   return( *this );
}
