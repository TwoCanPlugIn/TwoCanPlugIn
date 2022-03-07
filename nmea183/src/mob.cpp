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
** Author: Samuel R. Blackburn
** CI$: 76300,326
** Internet: sammy@sed.csc.com
**
** You can use it any way you like.
*/

// $--MOB,hhhhh,a,hhmmss.ss,x,xxxxxx,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x.x,x.x,xxxxxxxxx,x*hh

MOB::MOB()
{
    Mnemonic = _T("MOB");
   Empty();
}

MOB::~MOB()
{
   Mnemonic.Empty();
   Empty();
}

void MOB::Empty( void )
{
	EmitterID.Empty();
	MobStatus = MOB_STATUS::Error;
	ActivationTime.Empty();
	PositionReference = 1;
	Date.Empty();
	UTCTime.Empty();
	Position.Empty();
	SpeedOverGround = 0.0;
	CourseOverGround = 0.0;
	mmsiNumber.Empty();
	BatteryStatus = 0;

} 

bool MOB::Parse( const SENTENCE& sentence )
{
//   ASSERT_VALID( this );

  
   /*
   ** First we check the checksum...
   */
	if (sentence.IsChecksumBad(15) == NTrue)
	{
		SetErrorMessage(_T("Invalid Checksum"));
		return(FALSE);
	}

  EmitterID = sentence.Field( 1 );
  MobStatus = (MOB_STATUS)std::atoi(sentence.Field(2));
  ActivationTime = sentence.Field(3);
  PositionReference = std::atoi(sentence.Field(4));
  Date = sentence.Field( 5 );
  UTCTime = sentence.Field(6);
  Position.Parse( 7, 8, 9, 10, sentence );
  SpeedOverGround = sentence.Double( 11 );
  CourseOverGround  = sentence.Double( 12 );
  mmsiNumber = sentence.Field( 13 );
  BatteryStatus = std::atoi(sentence.Field(14));
   
   return( TRUE );
}

bool MOB::Write( SENTENCE& sentence )
{
//   ASSERT_VALID( this );

   /*
   ** Let the parent do its thing
   */

   RESPONSE::Write( sentence );

   sentence += EmitterID;
   sentence += MobStatus;
   sentence += ActivationTime;
   sentence += PositionReference;
   sentence += Date;
   sentence += UTCTime;
   sentence += Position;
   sentence += SpeedOverGround;
   sentence += CourseOverGround;
   sentence += mmsiNumber;
   sentence += BatteryStatus;

   sentence.Finish();

   return( TRUE );
}

const MOB& MOB::operator = ( const MOB& source )
{
//   ASSERT_VALID( this );

	EmitterID = source.EmitterID;
	MobStatus = source.MobStatus;
	ActivationTime = source.ActivationTime;
	PositionReference = source.PositionReference;
	Date = source.Date;
	UTCTime = source.UTCTime;
	Position = source.Position;
	SpeedOverGround = source.SpeedOverGround;
	CourseOverGround = source.CourseOverGround;
	mmsiNumber = source.mmsiNumber;
	BatteryStatus = source.BatteryStatus;
	
	return( *this );
}
