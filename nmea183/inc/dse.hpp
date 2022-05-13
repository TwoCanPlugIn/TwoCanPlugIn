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
 *
 *   S Blackburn's original source license:                                *
 *         "You can use it any way you like."                              *
 *   More recent (2010) license statement:                                 *
 *         "It is BSD license, do with it what you will"                   *
 */

#if ! defined( DSE_CLASS_HEADER )
#define DSE_CLASS_HEADER

/*
** Author: Samuel R. Blackburn
** CI$: 76300,326
** Internet: sammy@sed.csc.com
**
** You can use it any way you like.
*/

enum class DSE_DATA_SPECIFIER
{
	POSITION = 0,
	DATUM = 1,
	SPEED = 2,
	COURSE = 3,
	INFORMATION = 4,
	GEOGRAPHY = 5,
	PERSONS = 6
};

enum class DSE_QUERY_FLAG
{
	QUERY = 0,
	REPLY = 1,
	AUTOMATIC = 2
};


class DSE : public RESPONSE
{
   
   public:

      DSE();
	  ~DSE();

		  /*
		  ** Data
		  */

	  int totalSentences;
	  int sentenceNumber;
	  int queryFlag;
	  unsigned long long mmsiNumber;
	  std::vector<int> codeFields;
	  std::vector<wxString> dataFields;

      /*
      ** Methods
      */

      virtual void Empty( void );
      virtual bool Parse( const SENTENCE& sentence );
      virtual bool Write( SENTENCE& sentence );

      /*
      ** Operators
      */

      const DSE& operator = ( const DSE &source );
};

#endif // DSC_CLASS_HEADER
