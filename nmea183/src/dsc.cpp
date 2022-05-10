/***************************************************************************
 *
 * Project: TwoCan plugin for OpenCPN
 * Purpose: DSC Decoder used by TwoCan plugin
 * Author: Steven Adler
 * Copyright (C) 2021
 *
 ***************************************************************************
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

DSC::DSC()
{
   Mnemonic = _T("DSC");
   Empty();
}

DSC::~DSC()
{
   Mnemonic.Empty();
   Empty();
}

void DSC::Empty( void )
{
//   ASSERT_VALID( this );

	formatSpecifer = 0;
	mmsiNumber = 0;
	category = 0;
	natureOfDistressOrFirstTelecommand = 0;
	subsequentCommunicationsOrSecondTelecommand = 0;
	positionOrFrequency = wxEmptyString;
	timeOrTelephone = wxEmptyString;
	relayMMSI = 0;
	relayNatureOfDistress = 0;
	ack = wxEmptyString;
	dseExpansion = NFalse;
}

bool DSC::Parse( const SENTENCE& sentence )
{
//   ASSERT_VALID( this );

	
   /*
   ** First we check the checksum...
   */

   if ( sentence.IsChecksumBad( 12 ) == NTrue )
   {
      SetErrorMessage( _T("Invalid Checksum") );
      return( FALSE );
   }

   formatSpecifer    = sentence.Integer( 1 );
   mmsiNumber  = sentence.ULongLong( 2 );
   category = sentence.Integer( 3 );
   natureOfDistressOrFirstTelecommand = sentence.Integer(4);
   subsequentCommunicationsOrSecondTelecommand = sentence.Integer(5);
   positionOrFrequency = sentence.Field(6);
   timeOrTelephone = sentence.Field(7);
   relayMMSI = sentence.ULongLong(8);
   relayNatureOfDistress = sentence.Integer(9);
   ack = sentence.Field( 10 );
   dseExpansion = sentence.Field(11) == "E" ? NTrue : NFalse;


   return( TRUE );
}

bool DSC::Write( SENTENCE& sentence )
{
//   ASSERT_VALID( this );

   /*
   ** Let the parent do its thing
   */

   RESPONSE::Write( sentence );

   sentence += formatSpecifer;
   sentence += mmsiNumber;
   sentence += category;
   sentence += natureOfDistressOrFirstTelecommand;
   sentence += subsequentCommunicationsOrSecondTelecommand;
   sentence += timeOrTelephone;
   sentence += positionOrFrequency;
   sentence += relayMMSI;
   sentence += relayNatureOfDistress;
   sentence += ack;
   sentence += dseExpansion;

   sentence.Finish();

   return( TRUE );
}

const DSC& DSC::operator = ( const DSC &source )
{
  if ( this != &source ) {
	  formatSpecifer = source.formatSpecifer;
	  mmsiNumber = source.mmsiNumber;
	  category = source.category;
	  natureOfDistressOrFirstTelecommand = source.natureOfDistressOrFirstTelecommand;
	  subsequentCommunicationsOrSecondTelecommand = source.subsequentCommunicationsOrSecondTelecommand;
	  positionOrFrequency = source.positionOrFrequency;
	  timeOrTelephone = source.timeOrTelephone;
	  relayMMSI = source.relayMMSI;
	  relayNatureOfDistress = source.relayNatureOfDistress;
	  ack = source.ack;
	  dseExpansion = source.dseExpansion;
  }
   return *this;
}
