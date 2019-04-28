// Copyright(C) 2018-2019 by Steven Adler
//
// This file is part of TwoCan, a plugin for OpenCPN.
//
// TwoCan is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TwoCan is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with TwoCan. If not, see <https://www.gnu.org/licenses/>.
//
// NMEA2000® is a registered Trademark of the National Marine Electronics Association

#ifndef TWOCAN_SETTINGS_H
#define TWOCAN_SETTINGS_H

// The settings dialog base class from which we are derived
// Note wxFormBuilder used to generate UI
#include "twocansettingsbase.h"

#include "twocanutils.h"
#ifdef __LINUX__
#include "twocansocket.h"
#endif

#include "../img/twocan_64.xpm"
#include "../img/twocan_16_icon.xpm"

// wxWidgets includes 
// For copy-n-paste from debug text control
#include <wx/clipbrd.h>
// For wxMessageBox
#include <wx/msgdlg.h>
// For logging
#include <wx/log.h>
// For file & folder functions
#include <wx/filefn.h>
#include <wx/dir.h>
// For config settings
#include <wx/fileconf.h>
// For math.pow function (bit twiddling)
#include <math.h>
// For common folders
#include <wx/stdpaths.h>

// STL
// For a dictionary of registered NMEA 2000 manufacturers
#include <unordered_map>

// List of Manufacturer Id's
static std::unordered_map<int, std::string> deviceManufacturers = {
	{ 78, "FW Murphy" },
	{ 80, "Twin Disc" },
	{ 85, "Kohler Power Systems" },
	{ 88, "Hemisphere GPS" },
	{ 135, "Airmar" },
	{ 137, "Maretron" },
	{ 140, "Lowrance Electronics" },
	{ 144, "Mercury Marine" },
	{ 147, "Nautibus Electronic Gmbh" },
	{ 148, "Blue Water Data" },
	{ 154, "Westerbeke Corp." },
	{ 161, "Offshore Systems UK" },
	{ 163, "Evinrude" },
	{ 165, "CPAC Systems AB" },
	{ 168, "Xantrex Technology" },
	{ 172, "Yanmar" },
	{ 174, "Volvo Penta" },
	{ 176, "Carling Technologies" },
	{ 185, "Beede Electrical" },
	{ 192, "Floscan Instrument Co., Inc." },
	{ 193, "Nobeltec" },
	{ 198, "Mystic Valley Communications" },
	{ 199, "Actia Corporation" },
	{ 201, "Disenos Y Technologia" },
	{ 211, "Digital Switching Systems" },
	{ 215, "Aetna Engineering" },
	{ 224, "Emmi Network" },
	{ 228, "ZF Marine Electronics" },
	{ 229, "Garmin" },
	{ 233, "Yacht Monitoring Solutions" },
	{ 235, "Sailormade Marine Telemetry" },
	{ 243, "Eride" },
	{ 257, "Honda Motor" },
	{ 272, "Groco" },
	{ 273, "Actisense" },
	{ 274, "Amphenol LTW Technology" },
	{ 275, "Navico" },
	{ 283, "Hamilton Jet" },
	{ 285, "Sea Recovery" },
	{ 286, "Coelmo Srl Italy" },
	{ 295, "BEP Marine" },
	{ 304, "Empir Bus" },
	{ 305, "Novatel" },
	{ 306, "Sleipner Motor As" },
	{ 307, "MBW Technologies" },
	{ 315, "Icom" },
	{ 328, "Qwerty" },
	{ 329, "Dief" },
	{ 345, "Korea Maritime University" },
	{ 351, "Thrane And Thrane" },
	{ 355, "Mastervolt" },
	{ 356, "Fischer Panda" },
	{ 358, "Victron" },
	{ 370, "Rolls Royce Marine" },
	{ 373, "Electronic Design" },
	{ 374, "Northern Lights" },
	{ 378, "Glendinning" },
	{ 381, "B&G" },
	{ 384, "Rose Point" },
	{ 385, "Geonav" },
	{ 394, "Capi 2" },
	{ 396, "Beyond Measure" },
	{ 400, "Livorsi Marine" },
	{ 404, "Com Nav" },
	{ 419, "Fusion Electronics" },
	{ 421, "Vertex Standard Co Ltd" },
	{ 422, "True Heading" },
	{ 426, "Egersund Marine Electronics AS" },
	{ 427, "Em-Trak Marine Electronics Ltd" },
	{ 431, "Tohatsu Co Jp" },
	{ 437, "Digital Yacht" },
	{ 440, "Cummins" },
	{ 443, "VDO" },
	{ 451, "Parker Hannifin" },
	{ 459, "Alltek Marine Electronics Corp" },
	{ 460, "San Giorgio S.E.I.N. Srl" },
	{ 466, "Veethree" },
	{ 467, "Hummingbird Marine Electronics" },
	{ 470, "Sitex" },
	{ 471, "Sea Cross Marine Ab" },
	{ 475, "Standard Communications Pty Ltd" },
	{ 481, "Chetco Digital Instruments" },
	{ 478, "Ocean Sat BV" },
	{ 493, "Watcheye" },
	{ 499, "LCJ Capteurs" },
	{ 502, "Attwood Marine" },
	{ 503, "Naviop" },
	{ 504, "Vesper Marine" },
	{ 510, "Marinesoft" },
	{ 517, "NoLand Engineering" },
	{ 529, "National Instruments Korea" },
	{ 573, "McMurdo" },
	{ 579, "KVH" },
	{ 580, "San Jose Technology" },
	{ 585, "Suzuki" },
	{ 612,"Samwon IT" },
	{ 644, "WEMA" },
	{ 1850, "Teleflex" },
	{ 1851, "Raymarine, Inc." },
	{ 1852, "Navionics" },
	{ 1853, "Japan Radio Co" },
	{ 1854, "Northstar Technologies" },
	{ 1855, "Furuno" },
	{ 1856, "Trimble" },
	{ 1857, "Simrad" },
	{ 1858, "Litton" },
	{ 1859, "Kvasar Ab 	" },
	{ 1860, "MMP" },
	{ 1861, "Vector Cantech" },
	{ 1862, "Yamaha Marine" },
	{ 1863, "Faria Instruments" },
	{ 2019, "TwoCan" }
};

#ifdef __LINUX__
// For enumerating CAN Adapters
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#endif

// Globally defined settings which the settings dialog reads and writes to directly
// Alternative would be to add getter/setter functions to this class, but no real benefit

// Name of currently selected CAN Interface
extern wxString canAdapter;

// Flag of bit values indicating what PGN's the plug-in converts
extern int supportedPGN;

// Whether to enable the realtime display of NMEA 2000 frames 
extern bool debugWindowActive;

// Whether the plug-in passively listens to the NMEA 2000 network or is an active device
extern bool deviceMode;

// If we are in active mode, whether to periodically send PGN 126993 heartbeats
extern bool enableHeartbeat;

// Whether to Log raw NMEA 2000 messages
extern int logLevel;

// List of devices dicovered on the NMEA 2000 network
extern NetworkInformation networkMap[CONST_MAX_DEVICES];

// The uniqueID of this device (also used as the serial number)
extern unsigned long uniqueId;

// The current NMEA 2000 network address of this device
extern int networkAddress;

class TwoCanSettings : public TwoCanSettingsBase
{
	
public:
	TwoCanSettings(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Two Can Preferences"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE);
	~TwoCanSettings() ;
	
protected:
	//overridden methods from the base class
	void OnInit(wxInitDialogEvent& event);
	void OnChoice(wxCommandEvent &event);
	void OnCheckPGN(wxCommandEvent &event);
	void OnCheckLog(wxCommandEvent &event);
	void OnCheckMode(wxCommandEvent &event);
	void OnCheckHeartbeat(wxCommandEvent &event);
	void OnPause(wxCommandEvent &event);
	void OnCopy(wxCommandEvent &event);
	void OnOK(wxCommandEvent &event);
	void OnApply(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);

private:
	void SaveSettings(void);
	bool settingsDirty;
	void GetDriverInfo(wxString fileName);
	bool EnumerateDrivers(void);
	
	// To obtain the "friendly" name of Windows CAN driver
	typedef wxChar *(*LPFNDLLDriverName)(void);

	// Hashmap of CAN adapter names and corresponding "driver" files
	WX_DECLARE_STRING_HASH_MAP(wxString, AvailableAdapters);
	AvailableAdapters adapters;

};

#endif
