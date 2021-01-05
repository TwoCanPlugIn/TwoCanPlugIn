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

VDM::VDM()
{
   Mnemonic = _T("VDM");
   Empty();
}

VDM::~VDM()
{
   Mnemonic.Empty();
   Empty();
}

void VDM::Empty( void )
{
//   ASSERT_VALID( this );

	sentences = 0;
	sentenceNumber = 0;
	sequentialID = 0;
	channel = 'A';
	message = wxEmptyString;
}

bool VDM::Parse( const SENTENCE& sentence )
{
//   ASSERT_VALID( this );

	// !--VDM,x,x,x,a,s--s,x*hh
	//        | | | |   |  Number of fill bits
	//        | | | |   Encoded Message
	//        | | | AIS Channel
	//        | | Sequential Message ID
	//        | Sentence Number
	//      Total Number of sentences

   /*
   ** First we check the checksum...
   */

   if ( sentence.IsChecksumBad( 7 ) == NTrue )
   {
      SetErrorMessage( _T("Invalid Checksum") );
      return( FALSE );
   }

   sentences    = sentence.Integer( 1 );
   sentenceNumber  = sentence.Integer( 2 );
   sequentialID = sentence.Integer( 3 );
   channel = sentence.Integer( 4 );
   message = sentence.Field( 5 );
   fillbits = sentence.Integer( 6 );


   return( TRUE );
}

bool VDM::Write( SENTENCE& sentence )
{
//   ASSERT_VALID( this );

   /*
   ** Let the parent do its thing
   */

   RESPONSE::Write( sentence );

   sentence += sentences;
   sentence += sentenceNumber;
   sentence += sequentialID;
   sentence += channel;
   sentence += message;
   sentence += fillbits;

   sentence.Finish();

   return( TRUE );
}

const VDM& VDM::operator = ( const VDM &source )
{
  if ( this != &source ) {
    sentences = source.sentences;
    sentenceNumber  = source.sentenceNumber;
    sequentialID = source.sequentialID;
    message = source.message;
    fillbits = source.fillbits;
  }
   return *this;
}
