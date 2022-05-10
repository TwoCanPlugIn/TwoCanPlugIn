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

DSE::DSE()
{
   Mnemonic = _T("DSE");
   Empty();
}

DSE::~DSE()
{
   Mnemonic.Empty();
   Empty();
}

void DSE::Empty( void )
{
//   ASSERT_VALID( this );

	totalSentences = 0;
	sentenceNumber = 0;
	queryFlag = static_cast<int>(DSE_QUERY_FLAG::REPLY);
	mmsiNumber = 0;
	codeFields.clear();
	dataFields.clear();
}

bool DSE::Parse( const SENTENCE& sentence )
{
//   ASSERT_VALID( this );

	
   /*
   ** First we check the checksum...
   */

   if ( sentence.IsChecksumBad(sentence.GetNumberOfDataFields() + 1) == NTrue )
   {
      SetErrorMessage( _T("Invalid Checksum") );
      return( FALSE );
   }

   totalSentences = sentence.Integer(1);
   sentenceNumber = sentence.Integer(2);
   queryFlag = sentence.Integer(3);
   mmsiNumber = sentence.ULongLong(4);
   codeFields.clear();
   dataFields.clear();

   for (int i = 0; i < (sentence.GetNumberOfDataFields() - 4) / 2; i++) {
	   codeFields.push_back(sentence.Integer(5 + (2 * i)));
	   dataFields.push_back(sentence.Field(6 + (2 * i)));
   }
   return( TRUE );
}

bool DSE::Write( SENTENCE& sentence )
{
//   ASSERT_VALID( this );

   /*
   ** Let the parent do its thing
   */

   RESPONSE::Write( sentence );

   sentence += totalSentences;
   sentence += sentenceNumber;
   sentence += queryFlag;
   sentence += mmsiNumber;
   for (size_t i = 0; i < codeFields.size(); i++) {
	   sentence += codeFields.at(i);
	   sentence += dataFields.at(i);
   }
   sentence.Finish();

   return( TRUE );
}

const DSE& DSE::operator = ( const DSE &source )
{
  if ( this != &source ) {

	  totalSentences = source.totalSentences;
	  sentenceNumber = source.sentenceNumber;
	  queryFlag = source.queryFlag;
	  mmsiNumber = source.mmsiNumber;
	  codeFields = source.codeFields;
	  dataFields = source.dataFields;
	  
  }
   return *this;
}
