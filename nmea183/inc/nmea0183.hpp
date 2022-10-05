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


// One location for the source https://devel.hds.utc.fr/software/pacpus/browser/pacpussensors/trunk/NMEA0183?rev=72&order=name

#if ! defined( NMEA_0183_CLASS_HEADER )
#define NMEA_0183_CLASS_HEADER

/*
** Author: Samuel R. Blackburn
** CI$: 76300,326
** Internet: sammy@sed.csc.com
**
** You can use it any way you like.
*/

/*
** General Purpose Classes
*/

#include "sentence.hpp"
#include "response.hpp"
#include "latlong.hpp"
//#include "LoranTD.hpp"
//#include "Manufact.hpp"
//#include "MList.hpp"
//#include "OmegaPar.hpp"
//#include "DeccaLOP.hpp"
//#include "RatioPls.hpp"
//#include "RadarDat.hpp"
//#include "FreqMode.hpp"

/*
** Response Classes
*/

// Comment/uncomment as required
//#include "AAM.hpp"
//#include "ALM.hpp"
#include "apb.hpp"
//#include "ASD.hpp"
//#include "BEC.hpp"
#include "bod.hpp"
//#include "BWC.hpp"
#include "bwr.hpp"
//#include "BWW.hpp"
#include "dbt.hpp"
#include "dsc.hpp"
#include "dse.hpp"
//#include "DCN.hpp"
#include "dpt.hpp"
//#include "FSI.hpp"
#include "gga.hpp"
//#include "GLC.hpp"
#include "gll.hpp"
//#include "GXA.hpp"
//#include "HSC.hpp"
///#include "LCD.hpp"
#include "mob.hpp"
#include "mtw.hpp"
#include "mwv.hpp"
//#include "OLN.hpp"
//#include "OSD.hpp"
//#include "P.hpp"
//#include "RMA.hpp"
#include "hdm.hpp"
#include "hdg.hpp"
#include "hdt.hpp"
#include "rmb.hpp"
#include "rmc.hpp"
#include "wpl.hpp"
#include "rte.hpp"
#include "vtg.hpp"
#include "gsv.hpp"
#include "gga.hpp"
#include "gpwpl.hpp"
#include "rot.hpp"
#include "rpm.hpp"
#include "rsa.hpp"
//#include "RSD.hpp"
//#include "SFI.hpp"
//#include "STN.hpp"
//#include "TRF.hpp"
//#include "TTM.hpp"
//#include "VBW.hpp"
#include "vdr.hpp"
#include "vdm.hpp"
#include "vhw.hpp"
#include "vlw.hpp"
#include "vwr.hpp"
//#include "VPW.hpp"
//#include "VTG.hpp"
#include "wcv.hpp"
//#include "WNC.hpp"
#include "xdr.hpp"
#include "xte.hpp"
//#include "XTR.hpp"
#include "zda.hpp"
//#include "ZFO.hpp"
//#include "ZTG.hpp"


WX_DECLARE_LIST(RESPONSE, MRL);

class NMEA0183
{

   private:

      SENTENCE sentence;

      void initialize( void );

   protected:

      MRL response_table;

      void set_container_pointers( void );
      void sort_response_table( void );

   public:

      NMEA0183();
      virtual ~NMEA0183();

      wxArrayString GetRecognizedArray(void);
      
      //
      // NMEA 0183 Sentences we understand
      //

// Comment/Uncomment as qequired
   //AAM Aam;
   //ALM Alm;
   APB Apb;
   //ASD Asd;
   //BEC Bec;
   BOD Bod;
   //BWC Bwc;
   BWR Bwr;
   //BWW Bww;
   DBT Dbt;
   //DCN Dcn;
   DPT Dpt;
   DSC Dsc;
   DSE Dse;
   //FSI Fsi;
   GGA Gga;
   //GLC Glc;
   GLL Gll;
   GSV Gsv;
   //GXA Gxa;
   HDG Hdg;
   HDM Hdm;
   HDT Hdt;
   //HSC Hsc;
   //LCD Lcd;
   MOB Mob;
   MTW Mtw;
   MWV Mwv;
   //OLN Oln;
   //OSD Osd;
   //P   Proprietary;
   //RMA Rma;
   RMB Rmb;
   RMC Rmc;
   ROT Rot;
   RPM Rpm;
   RSA Rsa;
   //RSD Rsd;
   RTE Rte;
   //SFI Sfi;
   //STN Stn;
   //TRF Trf;
   //TTM Ttm;
   //VBW Vbw;
   VDM Vdm;
   VDR Vdr;
   VHW Vhw;
   VLW Vlw;
   VWR Vwr;
   //VPW Vpw;
   VTG Vtg;
   WCV Wcv;
   //WNC Wnc;
   WPL Wpl;
   GPWPL GPwpl;	  
	XDR Xdr;
   XTE Xte;
   //XTR Xtr;
   ZDA Zda;
   //ZFO Zfo;
   //ZTG Ztg;
	              
      wxString ErrorMessage; // Filled when Parse returns FALSE
      wxString LastSentenceIDParsed; // ID of the last sentence successfully parsed
      wxString LastSentenceIDReceived; // ID of the last sentence received, may not have parsed successfully

      wxString TalkerID;
      wxString ExpandedTalkerID;

//      MANUFACTURER_LIST Manufacturers;

      virtual bool IsGood( void ) const;
      virtual bool Parse( void );
      virtual bool PreParse( void );

      NMEA0183& operator << ( wxString& source );
      NMEA0183& operator >> ( wxString& destination );
};

#endif // NMEA_0183_CLASS_HEADER
