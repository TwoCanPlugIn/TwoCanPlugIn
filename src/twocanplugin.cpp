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
// 1.8 - 10/05/2020 AIS data validation fixes, Mac OSX support
// 1.9 - 20-08-2020 Rusoku adapter support on Mac OSX, OCPN 5.2 Plugin Manager support
// 2.0 - 20-11-2020 Autopilot, Bi-Directional Gateway
// Outstanding Features: 
// 1. Localization ??
//


#include "twocanplugin.h"

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
	return new TwoCan(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
	delete p;
}

// TwoCan plugin constructor. Note it inherits from wxEvtHandler so that we can receive events
// from the NMEA 2000 device when a NMEA 2000 frame is received
TwoCan::TwoCan(void *ppimgr) : opencpn_plugin_116(ppimgr), wxEvtHandler() {
	// Wire up the event handlers
	Connect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnSentenceReceived));

	// BUG BUG Should I have separate event handlers for the NMEA 2000, Autopilot etc. ?
	Connect(wxEVT_AUTOPILOT_COMMAND_EVENT, wxCommandEventHandler(TwoCan::OnAutopilotCommand));

	// Load the plugin bitmaps/icons 
	initialize_images();
}

TwoCan::~TwoCan(void) {
	// Disconnect the event handler
	Disconnect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnSentenceReceived));
	Disconnect(wxEVT_AUTOPILOT_COMMAND_EVENT, wxCommandEventHandler(TwoCan::OnAutopilotCommand));
	// Flush the log. Seems to crash OpenCPN upon exit if we don't
	wxLog::FlushActive();

	// Unload plugin bitmaps/icons
	delete _img_Toucan_16;
	delete _img_Toucan_32;
	delete _img_Toucan_48;
	delete _img_Toucan_64;

}

int TwoCan::Init(void) {
	// Perform any necessary initializations here

	// Maintain a reference to the OpenCPN window
	// BUG BUG Why, it's not used by anything at the moment ??
	parentWindow = GetOCPNCanvasWindow();

	// Maintain a reference to the OpenCPN configuration object 
	configSettings = GetOCPNConfigObject();

	// Save the location for this plugins data folder
	pluginDataFolder = GetPluginDataDir(PLUGIN_PACKAGE_NAME) + wxFileName::GetPathSeparator() + _T("data") + wxFileName::GetPathSeparator();

	// Load the Autopilot toolbar icons
	normalIcon = pluginDataFolder + _T("images") + wxFileName::GetPathSeparator() +_T("autopilot.svg");
	toggledIcon =  pluginDataFolder + _T("images") + wxFileName::GetPathSeparator() + _T("autopilot-bw.svg");
	rolloverIcon = pluginDataFolder + _T("images") + wxFileName::GetPathSeparator() + _T("autopilot-bw-rollover.svg");
	autopilotToolbar = 0;

	// TwoCan preferences dialog
	settingsDialog = NULL;
	
	// Initialize TwoCan Device to a nullptr to prevent crashes when trying to stop an unitialized device
	// which is what happens upon first startup. Bugger, there must be a better way.
	twoCanDevice = nullptr;

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
		// If the autopilotMode is enabled, add the autopilot toolbar and initialize the autopilot class
		if ((deviceMode == TRUE) && (autopilotMode != FLAGS_AUTOPILOT_NONE)) {
			autopilotToolbar = InsertPlugInToolSVG(_T(""), normalIcon, rolloverIcon, toggledIcon, wxITEM_CHECK,_("TwoCan Autopilot"), _T(""), NULL, -1, 0, this);
			twoCanAutopilot = new TwoCanAutopilot(autopilotMode);
		}
		// If the gateway is enabled, the plugin will convert NMEA 183 sentences to NMEA 2000 messages
		if ((deviceMode == TRUE) && (enableGateway == TRUE)) {
			twoCanEncoder = new TwoCanEncoder;
		}

	}
	else {
		wxLogError(_T("TwoCan Plugin, Load Configuration Error. Device not started"));
	}

	// Notify OpenCPN what events we want to receive callbacks for
	return (WANTS_PREFERENCES | WANTS_CONFIG | WANTS_NMEA_SENTENCES | WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL);
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

	// If the autopilot is enabled, cleanup
	if ((deviceMode == TRUE) && (autopilotMode != FLAGS_AUTOPILOT_NONE) && (autopilotToolbar != 0)) {
		RemovePlugInTool(autopilotToolbar);
		delete twoCanAutopilot;
	}

	// If the gateway is enabled , cleanup
	if ((deviceMode == TRUE) && (enableGateway == TRUE)) {
		delete twoCanEncoder;
	}

	// Terminate the TwoCan Device Thread, but only if we have a valid driver selected !
	if (canAdapter.CmpNoCase(_T("None")) != 0) {
		if (twoCanDevice != nullptr) {
			if (twoCanDevice->IsRunning()) {
				StopDevice();
			}
		}
	}
	// Do not need to explicitly call the destructor for detached threads
	return TRUE;
}

// Indicate what version of the OpenCPN Plugin API we support
int TwoCan::GetAPIVersionMajor() {
	return OCPN_API_VERSION_MAJOR;
}

int TwoCan::GetAPIVersionMinor() {
	return OCPN_API_VERSION_MINOR;
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
	return _T(PLUGIN_COMMON_NAME);
}

wxString TwoCan::GetShortDescription() {
	//Trademark character ® code is \xae
	return _T(PLUGIN_SHORT_DESCRIPTION);
}

wxString TwoCan::GetLongDescription() {
	// Localization ??
    return _T(PLUGIN_LONG_DESCRIPTION);
}

// 32x32 pixel PNG file is converted to XPM containing the variable declaration: char *[] twocan_32 ...
wxBitmap* TwoCan::GetPlugInBitmap() {
	return _img_Toucan_32;
}

// We only install a singe toolbar item
int TwoCan::GetToolbarToolCount(void) {
 return 1;
}

// Return the id for the Autopilot toolbar
int TwoCan::GetToolbarItemId() { 
	return autopilotToolbar; 
}

// Display the Autopilot Dialog
// BUG BUG, Dialog or Window. If Window how do we veto closing of OpenCPN?
void TwoCan::OnToolbarToolCallback(int id) {
	TwoCanAutopilotDialog *autopilotDialog = new TwoCanAutopilotDialog(parentWindow, this);
	autopilotDialog->ShowModal();
	delete autopilotDialog;
	SetToolbarItemState(id, false);
}

void TwoCan::SetPositionFix(PlugIn_Position_Fix &pfix) {
	// BUG BUG REMOVE Prototype to encode PGN 129025 Position Rapid Update
	if ((deviceMode == TRUE) && (enableGateway == TRUE)){

		CanHeader header;
		header.destination = CONST_GLOBAL_ADDRESS;
		header.source = networkAddress;
		header.pgn = 129025;
		header.priority = CONST_PRIORITY_MEDIUM;

		int latitude = pfix.Lat * 1e7;
		int longitude = pfix.Lon * 1e7;

		// BUG BUG Could use std::vector<byte> payload;
		byte payload[8];
		payload[0] = latitude & 0xFF;
		payload[1] = (latitude >> 8) & 0xFF;
		payload[2] = (latitude >> 16) & 0xFF;
		payload[3] = (latitude >> 24) & 0xFF;

		payload[4] = longitude & 0xFF;
		payload[5] = (longitude >> 8) & 0xFF;
		payload[6] = (longitude >> 16) & 0xFF;
		payload[7] = (longitude >> 24) & 0xFF;

		unsigned int id;
		TwoCanUtils::EncodeCanHeader(&id, &header);

		int returnCode = twoCanDevice->TransmitFrame(id, payload);
		if (returnCode != TWOCAN_RESULT_SUCCESS) {
			wxLogMessage(_T("TwoCanPlugin, Error sending position fix: %ul"), returnCode);
		}
	}
}


// Convert NMEA 183 sentences to NMEA 2000 messages
void TwoCan::SetNMEASentence(wxString &sentence) {
	if ((twoCanDevice != nullptr) && (deviceMode == TRUE) && (enableGateway == TRUE)) {
		std::vector<CanMessage> nmeaMessages;

		if (twoCanEncoder->EncodeMessage(sentence, &nmeaMessages) == TRUE) {
			unsigned int id;
			// Some NMEA 183 sentences are converted to multipe NMEA 2000 PGN's
			// Also if the PGN is a fast message, there will be multiple messages
			for (auto i : nmeaMessages) {
				TwoCanUtils::EncodeCanHeader(&id,&i.header);
				int returnCode;
				returnCode = twoCanDevice->TransmitFrame(id, i.payload.data());
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("Successfully Tx message"));
				}
				else {
					wxLogMessage(_T("Error Tx message %ul"), returnCode);
				}
				
			}
		}
		
	} 
}

// Receive OCPN Plugin Messages
// To control the autopilot, need to be alerted when a route is activated/deactivated
void TwoCan::SetPluginMessage(wxString& message_id, wxString& message_body) {
	if ((deviceMode == TRUE) && (autopilotMode != FLAGS_AUTOPILOT_NONE)) {
		if (message_id == _T("OCPN_RTE_ACTIVATED")) {
			wxMessageOutputDebug().Printf("Route Activated");
			// BUG BUG should query and determine the route name
			twoCanAutopilot->ActivateRoute(_T("Route Name"));
		}
		if (message_id == _T("OCPN_RTE_DEACTIVATED")) {
			wxMessageOutputDebug().Printf("Route Deactivated");
			twoCanAutopilot->DeactivateRoute();
		}
   
	}
}

// Event Handlers
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
			break;
	}
}

// Autopilot event handler
// Autopilot commands are issued from the Autopilot dialog
// BUG BUG To Do Route & Waypoint import/export, commands are issued from the preferences dialog
void TwoCan::OnAutopilotCommand(wxCommandEvent &event) {
	// Used for transmitting autopilot commands onto the network
	std::vector<CanMessage> nmeaMessages;
	unsigned int id;
	// Process commands (events) raised from the autopilot dialog
	unsigned int command = std::atoi(event.GetString());

	if (twoCanAutopilot->ParseCommand(event.GetId(), command, &nmeaMessages)) {
	// Send the autopilot command/s onto the network
		for (auto i : nmeaMessages) {
			TwoCanUtils::EncodeCanHeader(&id,&i.header);
			int returnCode = TRUE;
			//returnCode = twoCanDevice->TransmitFrame(id, i.payload.data());
			if (returnCode == TWOCAN_RESULT_SUCCESS) {
				wxLogMessage(_T("Successfully Tx Auopilot message"));
			}
			else {
				wxLogMessage(_T("Error Tx Autopilot message %ul"), returnCode);
			}
				
		}

	}

	switch (event.GetId()) {
		// BUG BUG TODO For autopilots we need to retrieve the pgn from twocanautopilot 
		// and then transmit via the twoCanDevice 
		case AUTOPILOT_POWER_EVENT:
			wxLogMessage("####AUTOPILOT POWER: %d", command);
			break;

		case AUTOPILOT_MODE_EVENT:
			wxLogMessage("####AUTOPILOT MODE: %d", command);
			break;

		case AUTOPILOT_HEADING_EVENT:
			wxLogMessage("####AUTOPILOT HEADING: %d", command);
			break;

	// For routes & waypoints, the dialog will request the relevant PGN's
	// and OpenCP API calls. 
	/*
	case EXPORT_WAYPOINTS_EVENT:
	// Following code retrieves all waypoints
		PlugIn_Waypoint waypoint;
		wxArrayString waypointGUIDS = GetWaypointGUIDArray();
		for (auto waypointGUID : waypointGUIDS) {
			GetSingleWaypoint(waypointGUID, &waypoint);
			//wxMessageBox(wxString::Format("Waypoint Name: %s\nWaypoint GUID: %s\nLat: %f\nLong: %f", waypoint.m_MarkName, waypoint.m_GUID, waypoint.m_lat, waypoint.m_lon));
		}
		break:


	// To retrieve routes need to send OpenCPN a message.
	 OCPN_ROUTELIST_REQUEST
	 {"mode": "Not track"}
	 // Prototyped in raceplgin
	*/
	default:
		event.Skip();
		break;
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
			
			// Only add the toolbar if it has not been added previously
			if ((deviceMode == TRUE) && (autopilotMode != FLAGS_AUTOPILOT_NONE) && (autopilotToolbar == 0)) {
				autopilotToolbar = InsertPlugInToolSVG(_T(""), normalIcon, rolloverIcon, toggledIcon, wxITEM_CHECK,_("TwoCan Autopilot"), _T(""), NULL, -1, 0, this);
				twoCanAutopilot = new TwoCanAutopilot(autopilotMode);
			}
			// or if it was added previously and no we are no longer an autopilot
			else if (((deviceMode == FALSE) || (autopilotMode == FLAGS_AUTOPILOT_NONE)) && (autopilotToolbar != 0)) {
				RemovePlugInTool(autopilotToolbar);
				autopilotToolbar = 0;
				delete twoCanAutopilot;
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
		configSettings->Read(_T("Log"), &logLevel, FLAGS_LOG_NONE);
		configSettings->Read(_T("Address"), &networkAddress, 0);
		configSettings->Read(_T("Heartbeat"), &enableHeartbeat, FALSE);
		configSettings->Read(_T("Gateway"), &enableGateway, FALSE);
		//configSettings->Read(_T("Waypoint"), &enableWaypoint, FALSE);
		//configSettings->Read(_T("Music"), &enableMusic, FALSE);
		//configSettings->Read(_T("SignalK"), &enableSignalK, FALSE);
		configSettings->Read(_T("Autopilot"), &autopilotMode, FLAGS_AUTOPILOT_NONE);
		return TRUE;
	}
	else {
		// Default Settings
		supportedPGN = 0;
		deviceMode = FALSE;
		logLevel = FLAGS_LOG_NONE;
		networkAddress = 0;
		enableHeartbeat = FALSE;
		enableGateway = FALSE;
		enableWaypoint = FALSE;
		enableMusic = FALSE;
		enableSignalK = FALSE;
		autopilotMode = FLAGS_AUTOPILOT_NONE;

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
		configSettings->Write(_T("Gateway"), enableGateway);
		configSettings->Write(_T("Autopilot"), autopilotMode);
		//configSettings->Write(_T("Waypoint"), enableWaypoint);
		//configSettings->Write(_T("Music"), enableMusic);
		//configSettings->Write(_T("SignalK"), enableSignalK);
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void TwoCan::StopDevice(void) {
	wxThread::ExitCode threadExitCode;
	wxThreadError threadError;
	if (twoCanDevice != nullptr) {
		if (twoCanDevice->IsRunning()) {
			wxLogMessage(_T("TwoCan Plugin, Terminating device thread id (0x%x)\n"), twoCanDevice->GetId());
			threadError = twoCanDevice->Delete(&threadExitCode, wxTHREAD_WAIT_BLOCK);
			if (threadError == wxTHREAD_NO_ERROR) {
				wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Delete Result: %lu"), threadExitCode);
				// BUG BUG Following is to prevent wxLog message "Error: Can not wait for thread termination (error 6: the handle is invalid.)"
				// when runing on Windows, due to some bug in wxWidgets
				if (twoCanDevice->IsRunning()) {
					twoCanDevice->Wait(wxTHREAD_WAIT_BLOCK);
				}
				delete twoCanDevice;
			}
			else {
				wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Delete Error: %d"), threadError);
			}
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
