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

WCV::WCV()
{
	Mnemonic = _T("WCV");
	Empty();
}

WCV::~WCV()
{
	Mnemonic.Empty();
	Empty();
}



void WCV::Empty( void )
{
   Velocity = 0.0;
   To.clear();
}

bool WCV::Parse( SENTENCE const& sentence ) 
{
   /*
   ** WCV - Waypoint Closure Velocity
   **
   **        1   2 3    45
   **        |   | |    ||
   ** $--WCV,x.x,N,c--c,A*hh<CR><LF>
   **
   ** Field Number: 
   **  1) Velocity
   **  2) N = knots
   **  3) Waypoint ID
   **  4) FAA Mode
   **  5) Checksum
   */

   /*
   ** First we check the checksum...
   */

	NMEA0183_BOOLEAN check;
	check = sentence.IsChecksumBad( 5 );
	if ( check == NTrue )
   {
       SetErrorMessage(_T("Invalid Checksum"));
       return( false );
   } 

   Velocity = sentence.Double( 1 );
   To       = sentence.Field( 3 );

   return( true );
}

bool WCV::Write( SENTENCE& sentence )
{
   /*
   ** Let the parent do its thing
   */
   
   RESPONSE::Write( sentence );

   sentence += Velocity;
   sentence += "N";
   sentence += To;

   sentence.Finish();
   return( true );
}

WCV const& WCV::operator = ( WCV const& source )
{
   Velocity = source.Velocity;
   To       = source.To;

   return( *this );
}
