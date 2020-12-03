#include "nmea0183.h"

/*
** Author: Samuel R. Blackburn
** Internet: sam_blackburn@pobox.com
**
** You can use it any way you like as long as you don't try to sell it.
**
** Copyright, 1996, Samuel R. Blackburn
**
** $Workfile: rot.cpp $
** $Revision: 4 $
** $Modtime: 10/10/98 2:56p $
*/

#ifdef _MSC_VER
#   pragma warning(disable:4996)
#endif // _MSC_VER

ROT::ROT()
{
   Mnemonic = "ROT";
   Empty();
}

ROT::~ROT()
{
   //Mnemonic.Empty();
   Empty();
}

void ROT::Empty( void )
{
   RateOfTurn  = 0.0;
   IsDataValid = Unknown0183;
}

bool ROT::Parse( const SENTENCE& sentence )
{
   /*
   ** ROT - Rate Of Turn
   **
   **        1   2 3
   **        |   | |
   ** $--ROT,x.x,A*hh<CR><LF>
   **
   ** Field Number: 
   **  1) Rate Of Turn, degrees per minute, "-" means bow turns to port
   **  2) Status, A means data is valid
   **  3) Checksum
   */

   /*
   ** First we check the checksum...
   */

   if ( sentence.IsChecksumBad( 3 ) == true )
   {
      SetErrorMessage( "Invalid Checksum" );
      return( FALSE );
   } 

   RateOfTurn  = sentence.Double( 1 );
   IsDataValid = sentence.Boolean( 2 );

   return( TRUE );
}


bool ROT::Write( SENTENCE& sentence )
{
   /*
   ** Let the parent do its thing
   */
   
   RESPONSE::Write( sentence );

   sentence += RateOfTurn;
   sentence += IsDataValid;
   
   sentence.Finish();

   return( TRUE );
}

const ROT& ROT::operator = ( const ROT& source )
{
   RateOfTurn  = source.RateOfTurn;
   IsDataValid = source.IsDataValid;

   return( *this );
}
