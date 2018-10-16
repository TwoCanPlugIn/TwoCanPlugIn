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

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: OpenCPN Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// Outstanding Features: 
// 1. Collect all images into single xpm file
// 2. Localization ??
//


#include "twocanplugin.h"

// Globally accessible varaibles used by both the plugin and the settings dialog.
wxFileConfig *configSettings;
wxString canAdapter;
int supportedPGN;
bool deviceMode;
bool debugWindowActive;
int logLevel;

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
	return new TwoCan(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
    delete p;
}

// TwoCan plugin constructor. Note it inherits from wxEvtHandler so that we can receive events
// from the NMEA 2000 device when a NMEA 2000 frame is received
TwoCan::TwoCan(void *ppimgr) : wxEvtHandler(), opencpn_plugin_18(ppimgr) {
	// Wire up the event handler
	Connect(wxEVT_FRAME_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnFrameReceived));
}

TwoCan::~TwoCan(void) {
	// Disconnect the event handler
	Disconnect(wxEVT_FRAME_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnFrameReceived));
}

int TwoCan::Init(void) {
	// Perform any necessary initializations here

	// Maintain a reference to the OpenCPN window
	// BUG BUG Why, it's not used by anything at the moment ??
	parentWindow = GetOCPNCanvasWindow();

	// Maintain a reference to the OpenCPN configuration object 
	configSettings = GetOCPNConfigObject();

    // TwoCan preferences dialog
	settingsDialog = NULL;

	// Toggles display of captured NMEA 2000 frames in the "debug" tab of the preferences dialog
	debugWindowActive = FALSE;

	// Load the configuration items
	if (LoadConfiguration()) {
		// Initialize and run the TwoCanDevice in it's own thread
		// If the plugin has yet to be configured, the canAdapterName = "None", so do not start
		if (canAdapter.CmpNoCase(_T("None")) != 0)  {
			StartDevice();
		}
		else {
			wxLogError(_T("TwoCan Plugin, No driver selected. Device not started"));
		}
	}
	else {
		wxLogError(_T("TwoCan Plugin, Load Configuration Error. Device not started"));
	}
		
	// Notify OpenCPN what events we want to receive callbacks for
	// WANTS_NMEA_SENTENCES could be used for future versions where we might convert NMEA 0183 sentences to NMEA 2000
	return ( WANTS_PREFERENCES | WANTS_CONFIG);
}

// OpenCPN is either closing down, or we have been disabled from the Preferences Dialog
bool TwoCan::DeInit(void) {
    // Terminate the TwoCan Device Thread, but only if we have a valid driver selected !
	if (canAdapter.CmpNoCase(_T("None")) != 0) {
		if (twoCanDevice->IsRunning()) {
			StopDevice();
		}
	}
	// Do not need to explicitly call the destructor for detached threads
	return TRUE;
}

// Indicate what version of the OpenCPN Plugin API we support
int TwoCan::GetAPIVersionMajor() {
      return OPENCPN_API_VERSION_MAJOR;
}

int TwoCan::GetAPIVersionMinor() {
      return OPENCPN_API_VERSION_MINOR;
}

// The TwoCan plugin version numbers. 
// BUG BUG anyway to automagically generate version numbers like Visual Studio & .NET assemblies ?
int TwoCan::GetPlugInVersionMajor() {
    return PLUGIN_VERSION_MAJOR;
}

int TwoCan::GetPlugInVersionMinor() {
    return PLUGIN_VERSION_MINOR;
}

// Return descriptions for the TwoCan Plugin
wxString TwoCan::GetCommonName() {
	return _T("TwoCan Plugin");
}

wxString TwoCan::GetShortDescription() {
    return _T("TwoCan Plugin integrates OpenCPN with NMEA2000® networks");
}

wxString TwoCan::GetLongDescription() {
	// Localization ??
    return _T("TwoCan PlugIn integrates OpenCPN with NMEA2000® networks\nEnables some NMEA2000® data to be directly integrated with OpenCPN.\n\nThe following NMEA2000® Parameter Group Numbers (with corresponding\nNMEA 0183 sentences indicated in braces) are supported:\n- 128259 Speed (VHW)\n- 128267 Depth (DPT)\n- 129026 COG & SOG (RMC)\n- 129029 Navigation (GLL & ZDA)\n- 130306 Wind (MWV)\n- 130310 Water Temperature (MWT)");
}

// 32x32 pixel PNG file is converted to XPM containing the variable declaration: char *[] twocan_32 ...
wxBitmap* TwoCan::GetPlugInBitmap() {
	pluginBitmap = wxBitmap(twocan_32);
	return &pluginBitmap;
}

// BUG BUG for future versions
void TwoCan::SetNMEASentence(wxString &sentence) {
	// Maintain a local copy of the NMEA Sentence for conversion to a NMEA 2000 PGN
    //wxString nmea183Sentence = sentence;    
}

// Frame received event handler. Events queued from TwoCanDevice.
// NMEA 0183 sentences are passed via the SetString()/GetString() properties
void TwoCan::OnFrameReceived(wxCommandEvent &event) {
	switch (event.GetId()) {
		case FRAME_RECEIVED_EVENT:
			PushNMEABuffer(event.GetString());
			// If the preference dialog is open and the debug tab is toggled, display the NMEA 183 sentences
			// Superfluous as they can be seen in the Connections tab.
			if (debugWindowActive) {
				settingsDialog->txtDebug->AppendText(event.GetString());
			}
		     break;
		default:
			event.Skip();
	}
}

// Display TwoCan preferences dialog
void TwoCan::ShowPreferencesDialog(wxWindow* parent) {
	settingsDialog = new TwoCanSettings(parent);

	// initialize the dialog settings
	settingsDialog->SetCanAdapter(canAdapter);
	settingsDialog->SetParameterGroupNumbers(supportedPGN);
	settingsDialog->SetRawLogging(logLevel);

	if (settingsDialog->ShowModal() == wxID_OK) {

			// Save the settings
			canAdapter = settingsDialog->GetCanAdapter();
			supportedPGN = settingsDialog->GetParameterGroupNumbers();
			logLevel = settingsDialog->GetRawLogging();

			if (SaveConfiguration()) {
				wxLogMessage(_T("TwoCan Plugin, Settings Saved"));
			}
			else {
				wxLogMessage(_T("TwoCan Plugin, Error Saving Settings"));
			}

			// BUG BUG Refactor duplicated code
			// Assume settings have been changed so reload them
			// But protect ourselves in case user still has not selected a driver !
			if (canAdapter.CmpNoCase(_T("None")) != 0) {
				// Terminate the TwoCan Device Thread if it is already running
				if (twoCanDevice->IsRunning()) {
					StopDevice();
				}
			}
			
			if (LoadConfiguration()) {
				// Start the TwoCan Device in it's own thread
				if (canAdapter.CmpNoCase(_T("None")) != 0)  {
					StartDevice();
				}
				else {
					wxLogError(_T("TwoCan Plugin, No driver selected. Device not started"));
				}
			}
			else {
				wxLogError(_T("TwoCan Plugin, Load Configuration Error. Device not started"));
			}
	}

	delete settingsDialog;
	settingsDialog = NULL;
}

// Loads a previously saved configuration
bool TwoCan::LoadConfiguration(void) {
	if (configSettings) {
		configSettings->SetPath(_T("/PlugIns/TwoCan"));
		configSettings->Read(_T("Adapter"), &canAdapter, _T("None"));
		configSettings->Read(_T("PGN"), &supportedPGN, 0);
		configSettings->Read(_T("Mode"), &deviceMode, FALSE);
		configSettings->Read(_T("Log"), &logLevel, 0);
        return TRUE;
	}
	else {
		// Default Settings
		supportedPGN = 0;
		deviceMode = FALSE;
		logLevel = 0;
		// BUG BUG Automagically find an installed adapter
		canAdapter = _T("None");
		return TRUE;
	}
}

bool TwoCan::SaveConfiguration(void) {
	if (configSettings) {
		configSettings->SetPath(_T("/PlugIns/TwoCan"));
		configSettings->Write(_T("Adapter"), canAdapter);
		configSettings->Write(_T("PGN"), supportedPGN);
		configSettings->Write(_T("Log"), logLevel);
		// BUG BUG Mode not yet implemented
		configSettings->Write(_T("Mode"), 0);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void TwoCan::StopDevice(void) {
	wxThread::ExitCode threadExitCode;
	wxThreadError threadError = twoCanDevice->Delete(&threadExitCode, wxTHREAD_WAIT_DEFAULT);
	wxLogMessage(_T("TwoCan Plugin, Device Thread Error Code: %d Exit Code: %d"), threadError, threadExitCode);
}

void TwoCan::StartDevice(void) {
	twoCanDevice = new TwoCanDevice(this);
	int returnCode = twoCanDevice->Init(canAdapter);
	if (returnCode == TWOCAN_RESULT_SUCCESS)    {
		wxLogMessage(_T("TwoCan Plugin, TwoCan Device Initialized"));
		int threadResult = twoCanDevice->Run();
		if (threadResult == TWOCAN_RESULT_SUCCESS)    {
			wxLogMessage(_T("TwoCan Plugin, Create Device Thread Result: %d"), threadResult);
		}
		else {
			wxLogError(_T("TwoCan Plugin, Create Device Thread Error: %d"), threadResult);
		}
	}
	else {
		wxLogError(_T("TwoCan Plugin,  TwoCanDevice Initialize Error: %lu"), returnCode);
	}	 
}