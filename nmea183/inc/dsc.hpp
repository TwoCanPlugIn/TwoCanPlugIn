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

#if ! defined( DSC_CLASS_HEADER )
#define DSC_CLASS_HEADER

/*
** Author: Samuel R. Blackburn
** CI$: 76300,326
** Internet: sammy@sed.csc.com
**
** You can use it any way you like.
*/

enum class DSC_FORMAT_SPECIFIER
{
	GEOGRAPHY = 2,
	VTS = 3,
	DISTRESS = 12,
	COMMON = 14,
	ALLSHIPS = 16,
	INDIVIDUAL = 20,
	AUTO = 23
};

enum class DSC_CATEGORY
{
	ROUTINE = 0,
	SAFETY = 8,
	URGENCY = 10,
	DISTRESS = 12
};

enum class DSC_NATURE_OF_DISTRESS
{
	FIRE = 0,
	FLOODING = 1,
	COLLISION = 2,
	GROUNDING = 3,
	CAPSIZE = 4,
	SINKING = 5,
	DISABLED = 6,
	UNDESIGNATED = 7,
	ABANDON = 8,
	PIRATES = 9,
	OVERBOARD = 10,
	EPIRB = 12
};

enum class DSC_FIRST_TELECOMMAND
{
	ALL = 0,
	DUPLEX = 1,
	POLLING = 3,
	UNABLE = 4,
	ENDCALL = 5,
	DATA = 6,
	J3E = 9,
	DISTRESSACK = 10,
	DISTRESSRELAY = 12,
	TTYFEC = 13,
	TTYARQ = 15,
	TEST = 18,
	UPDATE = 21,
	NOINFO = 26
};

enum class DSC_SECOND_TELECOMMAND
{
	NOREASON = 0,
	CONGESTION = 1,
	BUSY = 2,
	QUEUE = 3,
	BARRED = 4,
	NOOPERATOR = 5,
	TEMPOPERATOR = 6,
	DISABLED = 7,
	NOCHANNEL = 8,
	NOMODE = 9,
	RES18 = 10,
	MEDICAL = 11,
	PAYPHONE = 12,
	FAX = 13,
	NOINFO = 26
};


class DSC : public RESPONSE
{
   
   public:

      DSC();
	  ~DSC();

		  /*
		  ** Data
		  */
 
	  int formatSpecifer;
	  unsigned long long mmsiNumber;
	  int category;
	  int natureOfDistressOrFirstTelecommand;
	  int subsequentCommunicationsOrSecondTelecommand;
	  wxString positionOrFrequency;
	  wxString timeOrTelephone;
	  unsigned long long relayMMSI;
	  int relayCategory;
	  wxString ack;
	  NMEA0183_BOOLEAN dseExpansion;

      /*
      ** Methods
      */

      virtual void Empty( void );
      virtual bool Parse( const SENTENCE& sentence );
      virtual bool Write( SENTENCE& sentence );

      /*
      ** Operators
      */

      const DSC& operator = ( const DSC &source );
};

#endif // DSC_CLASS_HEADER
