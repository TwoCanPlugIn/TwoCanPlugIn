// Copyright(C) 2018 by Steven Adler
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

#ifndef TWOCANSETTINGS_H
#define TWOCANSETTINGS_H

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

// BUG BUG testing of paths
#include <wx/stdpaths.h>

#ifdef __LINUX__
// For enumerating CAN Adapters
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#endif


// Globally defined settings
// Reference to OpenCPN Settings
//extern wxFileConfig *configSettings;
// Name of currently selected CAN Interface
//extern wxString canAdapter;
// Flag of bit values indicating what PGN's the plug-in converts
//extern int supportedPGN;
// Whether to enable the realtime display of NMEA 2000 frames 
extern bool debugWindowActive;
// Whether the plug-in passively listens to the NMEA 2000 network or is an active device (potential future use)
//extern bool deviceMode;
// Whether to Log raw NMEA 2000 frame, or perhaps any other format in the future
//extern int logLevel;
// List of Network Devices
extern DeviceInformation networkMap[CONST_MAX_DEVICES];

typedef wxChar *(*LPFNDLLDriverName)(void);

class TwoCanSettings : public TwoCanSettingsBase
{
	// Hashmap of CAN adapter names and corresponding "driver" files
	WX_DECLARE_STRING_HASH_MAP(wxString, AvailableAdapters);
	
public:
	TwoCanSettings(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Two Can Preferences"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE);
	~TwoCanSettings() ;
	wxString GetCanAdapter(void);
	void SetCanAdapter(wxString);
	int GetParameterGroupNumbers(void);
	void SetParameterGroupNumbers(int);
	int GetRawLogging(void);
	void SetRawLogging(int);

protected:
	//overridden methods from the base class
	void OnInit(wxInitDialogEvent& event);
	void OnChoice(wxCommandEvent &event);
	void OnCheckPGN(wxCommandEvent &event);
	void OnCheckLog(wxCommandEvent &event);
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
	AvailableAdapters adapters;
	AvailableAdapters::iterator adapterIterator;
	wxString canAdapterName;
	int parameterGroupNumbers;
	int rawLogging;
};

#endif
