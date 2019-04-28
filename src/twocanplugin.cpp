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

//
// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: OpenCPN Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version History: 
// 1.0 Initial Release
// 1.4 - 25/4/2019. Active Mode implemented. 
// Outstanding Features: 
// 1. Collect all images into single xpm file
// 2. Localization ??
//


#include "twocanplugin.h"

// Globally accessible variables used by the plugin, device and the settings dialog.
wxFileConfig *configSettings;
wxString canAdapter;
int supportedPGN;
bool deviceMode;
bool debugWindowActive;
bool enableHeartbeat;
int logLevel;
unsigned long uniqueId;
int networkAddress;
NetworkInformation networkMap[CONST_MAX_DEVICES];

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
	return new TwoCan(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
	delete p;
}

// TwoCan plugin constructor. Note it inherits from wxEvtHandler so that we can receive events
// from the NMEA 2000 device when a NMEA 2000 frame is received
TwoCan::TwoCan(void *ppimgr) : opencpn_plugin_18(ppimgr), wxEvtHandler() {
	// Wire up the event handler
	Connect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnSentenceReceived));
}

TwoCan::~TwoCan(void) {
	// Disconnect the event handler
	Disconnect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnSentenceReceived));
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
		// If the plugin has yet to be configured, the canAdapterName == "None", so do not start
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
	return (WANTS_PREFERENCES | WANTS_CONFIG);
}

// OpenCPN is either closing down, or we have been disabled from the Preferences Dialog
bool TwoCan::DeInit(void) {
	// Persist our network address to prevent address claim conflicts next time we start
	if (deviceMode == TRUE) {
		if (configSettings) {
			configSettings->SetPath(_T("/PlugIns/TwoCan"));
			configSettings->Write(_T("Address"), networkAddress);
		}
	}
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
	//Trademark character ® code is \xae
	return _T("TwoCan Plugin integrates OpenCPN with NMEA2000\xae networks");
}

wxString TwoCan::GetLongDescription() {
	// Localization ??
    return _T("TwoCan PlugIn integrates OpenCPN with NMEA2000\xae networks\nEnables some NMEA2000\xae data to be directly integrated with OpenCPN.\n\nThe following NMEA2000\xae Parameter Group Numbers (with corresponding\nNMEA 0183 sentences indicated in braces) are supported:\n- 128259 Speed (VHW)\n- 128267 Depth (DPT)\n- 129026 COG & SOG (RMC)\n- 129029 Navigation (GLL & ZDA)\n- 130306 Wind (MWV)\n- 130310 Water Temperature (MWT)");
}

// 32x32 pixel PNG file is converted to XPM containing the variable declaration: char *[] twocan_32 ...
wxBitmap* TwoCan::GetPlugInBitmap() {
	pluginBitmap = wxBitmap(twocan_32);
	return &pluginBitmap;
}

// BUG BUG for future versions
void TwoCan::SetNMEASentence(wxString &sentence) {
	// Maintain a local copy of the NMEA Sentence for conversion to a NMEA 2000 PGN
	wxString nmea183Sentence = sentence;    
	// BUG BUG To Do
	// if (deviceMode==TRUE)
	// if (biDirectionalGateway)
	// Decode NMEA 183 sentence
	// encoode NMEA 2000 frame
	// transmit
}

// Frame received event handler. Events queued from TwoCanDevice.
// NMEA 0183 sentences are passed via the SetString()/GetString() functions
void TwoCan::OnSentenceReceived(wxCommandEvent &event) {
	switch (event.GetId()) {
	case SENTENCE_RECEIVED_EVENT:
		PushNMEABuffer(event.GetString());
		// If the preference dialog is open and the debug tab is toggled, display the NMEA 183 sentences
		// Superfluous as they can be seen in the Connections tab.
		if ((debugWindowActive) && (settingsDialog != NULL)) {
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

	if (settingsDialog->ShowModal() == wxID_OK) {

		// Save the settings
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
			StopDevice();
		}
		
		// Wait a little for the threads to complete and in the case of Windows. the driver dll to be unloaded
		wxThread::Sleep(CONST_ONE_SECOND);

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
		configSettings->Read(_T("Address"), &networkAddress, 0);
		configSettings->Read(_T("Heartbeat"), &enableHeartbeat, FALSE);
		return TRUE;
	}
	else {
		// Default Settings
		supportedPGN = 0;
		deviceMode = FALSE;
		logLevel = 0;
		networkAddress = 0;
		enableHeartbeat = FALSE;
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
		configSettings->Write(_T("Mode"), deviceMode);
		configSettings->Write(_T("Address"), networkAddress);
		configSettings->Write(_T("Heartbeat"), enableHeartbeat);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void TwoCan::StopDevice(void) {
	wxThread::ExitCode threadExitCode;
	wxThreadError threadError;
	if (twoCanDevice->IsRunning()) {
		threadError = twoCanDevice->Delete(&threadExitCode, wxTHREAD_WAIT_DEFAULT);
		if (threadError == wxTHREAD_NO_ERROR) {
			wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Delete Result: %d"), threadExitCode);
		}
		else {
			wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Delete Error: %d"), threadError);
		}
	}
}

void TwoCan::StartDevice(void) {
	twoCanDevice = new TwoCanDevice(this);
	int returnCode = twoCanDevice->Init(canAdapter);
	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("TwoCan Plugin,  TwoCan Device Initialize Error: %lu"), returnCode);
	}
	else {
		wxLogMessage(_T("TwoCan Plugin, TwoCan Device Initialized"));
		int threadResult = twoCanDevice->Run();
		if (threadResult == wxTHREAD_NO_ERROR)    {
			wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Created"));
		}
		else {
			wxLogError(_T("TwoCan Plugin, TwoCan Device Thread Creation Error: %d"), threadResult);
		}
	}
}
