// Copyright(C) 2018-2020 by Steven Adler
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
// 2.0 - 04/07/2021 Bi-Directional Gateway, Kvaser support on Mac OSX, Fast Message Assembly & SID generation fix, PCAP log file support 
// 2.1 - 20/05/2022 Minor fix to GGA/DBT in Gateway, DSC & MOB Sentences, Waypoint creation, epoch time fixes (time_t)0
// wxWidgets 3.15 support for MacOSX, Fusion Media control, OCPN Messaging for NMEA 2000 Transmit, Extend PGN 130312 for Engine Exhaust
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
	// Wire up the event handler
	Connect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnSentenceReceived));

	// Load the plugin bitmaps/icons 
	initialize_images();
}

TwoCan::~TwoCan(void) {
	// Disconnect the event handlers
	Disconnect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(TwoCan::OnSentenceReceived));
	
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

	// TwoCan preferences dialog
	settingsDialog = nullptr;
	
	// Initialize TwoCan Device, Encoder, MediaPlayer & Autopilot to a nullptr to prevent crashes when trying to stop an unitialized device
	// which is what happens upon first startup. Bugger, there must be a better way.
	twoCanDevice = nullptr;
	twoCanEncoder = nullptr;
	twoCanAutopilot = nullptr;
	twoCanMedia = nullptr;
	
	// Toggles display of captured NMEA 2000 frames in the "debug" tab of the preferences dialog
	debugWindowActive = FALSE;

	// if the rug is pulled from underneath us
	isRunning = TRUE;

	// Load the configuration items
	if (LoadConfiguration()) {
		// Initialize and run the TwoCanDevice in it's own thread
		// If the plugin has yet to be configured, the canAdapter Name == "None", so do not start
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
	return (WANTS_PREFERENCES | WANTS_CONFIG | WANTS_NMEA_SENTENCES | WANTS_PLUGIN_MESSAGING);
}

// OpenCPN is either closing down, or we have been disabled from the Preferences Dialog
bool TwoCan::DeInit(void) {

	// Notify other threads to end their work cleanly
	isRunning = FALSE;

	// Persist our network address to prevent address claim conflicts next time we start
	if (deviceMode == TRUE) {
		if (configSettings) {
			configSettings->SetPath(_T("/PlugIns/TwoCan"));
			configSettings->Write(_T("Address"), networkAddress);
		}
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

// Convert NMEA 183 sentences to NMEA 2000 messages
void TwoCan::SetNMEASentence(wxString &sentence) {
	if ((isRunning) && (twoCanEncoder != nullptr)  && (twoCanDevice != nullptr) && (deviceMode == TRUE) && (enableGateway == TRUE)) {
		std::vector<CanMessage> nmeaMessages;

		if (twoCanEncoder->EncodeMessage(sentence, &nmeaMessages) == TRUE) {
			unsigned int id;
			int returnCode;
			// Some NMEA 183 sentence conversions generate multipe NMEA 2000 PGN's
			// Also if the PGN is a fast message, there will be multiple frames
			// So we need a vector of frames to be sent
			for (auto it : nmeaMessages) {
				TwoCanUtils::EncodeCanHeader(&id, &it.header);
				returnCode = twoCanDevice->TransmitFrame(id, it.payload.data());
				if (returnCode != TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("TwoCan Plugin, Error sending converted NMEA 183 sentence: %d"), returnCode);
				}
				wxThread::Sleep(CONST_TEN_MILLIS);
				
			}
		}
	} 
}

// Process various OCPN Plugin Messages
void TwoCan::SetPluginMessage(wxString& message_id, wxString& message_body) {
	// Receive MOB events from OpenCPN and generate corresponding NMEA 2000 message, PGN 127233
	if (message_id == _T("OCPN_MAN_OVERBOARD")) {
		if ((deviceMode == TRUE) && (twoCanDevice != nullptr) && (twoCanEncoder != nullptr)) {
			wxJSONValue root;
			wxJSONReader reader;
			if (reader.Parse(message_body, &root) > 0) {
				wxLogMessage("TwoCan plugin, JSON Error in following text:");
				wxLogMessage("%s", message_body);
				wxArrayString jsonErrors = reader.GetErrors();
				for (auto it : jsonErrors) {
					wxLogMessage(it);
				}
				return;
			}
			else {
				wxString guid = root[_T("GUID")].AsString();
				// Retrieve the route by the guid
				std::unique_ptr<PlugIn_Route> mobRoute;
				mobRoute = GetRoute_Plugin(guid);
				// Now iterate the waypoints in the route to find the actual mob position
				// What the fuck were the OpenCPN developers thinking, this is so fucking stupid
				Plugin_WaypointList *pWaypointList = mobRoute->pWaypointList;
				for (auto it : *pWaypointList) {

					// What a stupid way to identify the MOB, by using the name of a MOB icon.
					if (it->m_IconName.Lower() == _T("mob")) {

						// Encode it as NMEA 0183 and then parse it to the twocanencoder method
						// to generate the the NMEA 2000 message. A bit inefficient, but eliminates code duplication
						NMEA0183 nmea0183;
						SENTENCE sentence;
						nmea0183.TalkerID = "EC";
						nmea0183.Mob.BatteryStatus = 0;
						nmea0183.Mob.Position.Latitude.Latitude = it->m_lat;
						nmea0183.Mob.Position.Latitude.Northing = it->m_lat >= 0 ? NORTHSOUTH::North : NORTHSOUTH::South;
						nmea0183.Mob.Position.Longitude.Longitude = it->m_lon;
						nmea0183.Mob.Position.Longitude.Easting = it->m_lon >= 0 ? EASTWEST::East : EASTWEST::West;
						nmea0183.Mob.Write(sentence);

						std::vector<CanMessage> messages;

						if (twoCanEncoder->EncodeMessage(sentence, &messages) == TRUE) {
							unsigned int id;
							int returnCode;
							for (auto it : messages) {
								TwoCanUtils::EncodeCanHeader(&id, &it.header);
								returnCode = twoCanDevice->TransmitFrame(id, it.payload.data());
								if (returnCode != TWOCAN_RESULT_SUCCESS) {
									wxLogMessage(_T("TwoCan Plugin, Error sending MOB message: %d"), returnCode);
								}
								wxThread::Sleep(CONST_TEN_MILLIS);
							}
						}

						break;
					}
				}
			}
		}
	}

	// Control Fusion Media player, media player commands are generated by TwoCan Media plugin
	else if (message_id == _T("TWOCAN_MEDIA_COMMAND")) {
		if ((deviceMode == TRUE) && (enableMusic == TRUE) && (twoCanDevice != nullptr) && (twoCanMedia != nullptr)) {
			std::vector<CanMessage> messages;
			unsigned int id;
			int returnCode;
			if (twoCanMedia->EncodeMediaCommand(message_body, &messages)) {
				for (auto it : messages) {
					TwoCanUtils::EncodeCanHeader(&id, &it.header);
					returnCode = twoCanDevice->TransmitFrame(id, it.payload.data());
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage(_T("TwoCan Plugin, Error sending Media Player command: %d"), returnCode);
					}
					wxThread::Sleep(CONST_TEN_MILLIS);
				}
			}
		}
	}

	// Handle request to export waypoints via NMEA 2000 - initiated by Two Tools plugin
	// Not used as the Toys plugin uses the TWOCAN_TRAMSIT_MESSAGE mechanism
	else if (message_id == _T("TWOCAN_EXPORT_WAYPOINTS")) {
		if ((deviceMode == TRUE) && (enableWaypoint == TRUE) && (twoCanDevice != nullptr) && (twoCanEncoder != nullptr)) {
			wxJSONValue root;
			wxJSONReader reader;
			NMEA0183 nmea0183;
			SENTENCE sentence;

			if (reader.Parse(message_body, &root) > 0) {
				wxLogMessage("TwoCan plugin, JSON Error in following text:");
				wxLogMessage("%s", message_body);
				wxArrayString jsonErrors = reader.GetErrors();
				for (auto it : jsonErrors) {
					wxLogMessage(it);
				}
				return;
			}
			else {
				if (root["navico"]["exportwaypoint"].IsBool()) {
					if (root["navico"]["exportwaypoint"].AsBool() == true) {

						// No counterpart for PGN 130074 description to store the description value
						//root["navico"]["exportwaypoint"]["description"]

						// Use the existing NMEA 183 WPL encoder to encode the NMEA 2000 message. 
						// As described above, a little inefficient
						nmea0183.TalkerID = "EC";
						nmea0183.Wpl.To = root["navico"]["exportwaypoint"]["name"].AsString();
						nmea0183.Wpl.Position.Latitude.Latitude = root["navico"]["exportwaypoint"]["latitude"].AsDouble();
						nmea0183.Wpl.Position.Latitude.Northing = nmea0183.Wpl.Position.Latitude.Latitude >= 0 ? NORTHSOUTH::North : NORTHSOUTH::South;
						nmea0183.Wpl.Position.Longitude.Longitude = root["navico"]["exportwaypoint"]["longitude"].AsDouble(); // GetJsonDouble(value);
						nmea0183.Wpl.Position.Longitude.Easting = nmea0183.Wpl.Position.Longitude.Longitude >= 0 ? EASTWEST::East : EASTWEST::West;
						nmea0183.Wpl.Write(sentence);

						std::vector<CanMessage> messages;

						if (twoCanEncoder->EncodeMessage(sentence, &messages) == TRUE) {
							unsigned int id;
							int returnCode;
							for (auto it : messages) {
								TwoCanUtils::EncodeCanHeader(&id, &it.header);
								returnCode = twoCanDevice->TransmitFrame(id, it.payload.data());
								if (returnCode != TWOCAN_RESULT_SUCCESS) {
									wxLogMessage(_T("TwoCan Plugin, Error sending Waypoint export message: %d"), returnCode);
								}
								wxThread::Sleep(CONST_TEN_MILLIS);
							}
						}
					}
				}
			}
		}
	}

	// Allow a plugin to send NMEA 2000 frames onto the network
	else if (message_id == _T("TWOCAN_TRANSMIT_FRAME")) {
		if ((deviceMode == TRUE) && (twoCanDevice != nullptr)) {
			wxJSONValue root;
			wxJSONReader reader;
			CanHeader header;
			std::vector<byte> payload;

			if (reader.Parse(message_body, &root) > 0) {
				// Save the erroneous json text for debugging
				wxLogMessage("TwoCan plugin, JSON Error in following text:");
				wxLogMessage("%s", message_body);
				wxArrayString jsonErrors = reader.GetErrors();
				for (auto it : jsonErrors) {
					wxLogMessage(it);
				}
				return;
			}
			else {
				header.pgn = root["nmea2000"]["pgn"].AsInt();
				header.priority = root["nmea2000"]["priority"].AsInt();
				header.destination = root["nmea2000"]["destination"].AsInt();
				header.source = root["source"].AsInt();

				if (root["nmea2000"]["data"].IsArray()) {
					wxJSONValue data = root["nmea2000"]["data"];

					for (int i = 0; i < data.Size(); i++) {
						payload.push_back(data[i].AsInt());
					}
				}

				if (payload.size() > 0) {
					int returnCode = twoCanDevice->FragmentFastMessage(&header, payload.size(), payload.data());
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage("TwoCan Plugin, Error sending raw message: %d", returnCode);
					}
					else {
						wxLogMessage("TwoCan Plugin, Sent raw message %s", message_body);
					}
				}
			}
		}
	}

	// Handle Autopilot Plugin dialog commands
	// Not yet implemented
	else if (message_id == _T("TWOCAN_AUTOPILOT_COMMAND")) {
		if ((deviceMode == TRUE) && (enableAutopilot == TRUE) && (twoCanDevice != nullptr) && (twoCanAutopilot != nullptr)) {
			std::vector<CanMessage> messages;
			unsigned int id;
			int returnCode;
			if (twoCanAutopilot->EncodeAutopilotCommand(message_body, &messages)) {
				for (auto it : messages) {
					TwoCanUtils::EncodeCanHeader(&id, &it.header);
					returnCode = twoCanDevice->TransmitFrame(id, it.payload.data());
					if (returnCode != TWOCAN_RESULT_SUCCESS) {
						wxLogMessage(_T("TwoCan Plugin, Error sending Autopilot command: %d"), returnCode);
					}
					wxThread::Sleep(CONST_TEN_MILLIS);
				}
			}
		}
	}
}

// Event Handlers
// Frame received event handler. Events queued from TwoCanDevice.
// NMEA 0183 sentences are passed via the SetString()/GetString() functions
void TwoCan::OnSentenceReceived(wxCommandEvent &event) {
	switch (event.GetId()) {
		case SENTENCE_RECEIVED_EVENT:
			if (isRunning) {
				PushNMEABuffer(event.GetString());
				// If the preference dialog is open and the debug tab is toggled, display the NMEA 183 sentences
				// Superfluous as they can be seen in the Connections tab.
				if ((debugWindowActive) && (settingsDialog != nullptr)) {
					settingsDialog->txtDebug->AppendText(event.GetString());
				}
			}
			break;

		case DSE_EXPIRED_EVENT: 
			// A received DSC Sentence has timed out waiting for a DSE sentence, so we send PGN 129808 without the DSE data
			if (isRunning) {
				CanHeader header;
				header.source = networkAddress;
				header.destination = CONST_GLOBAL_ADDRESS;
				header.priority = CONST_PRIORITY_MEDIUM;
				header.pgn = std::atoi(event.GetString());

				std::vector<byte> *payload = (std::vector<byte> *) event.GetClientData();
				int returnCode = twoCanDevice->FragmentFastMessage(&header, payload->size(), payload->data());
				if (returnCode != TWOCAN_RESULT_SUCCESS) {
					wxLogMessage("TwoCan Plugin, Error sending expired DSC message: %d", returnCode);
				}
			}
			break;

		default:
			event.Skip();
			break;
	}
}

// Display TwoCan preferences dialog
void TwoCan::ShowPreferencesDialog(wxWindow* parent) {
	
	settingsDialog = new TwoCanSettings(parent);

	if (settingsDialog->ShowModal() == wxID_OK) {

		// BUG BUG Refactor duplicated code
		// Assume settings have been changed so reload them
		// But protect ourselves in case user still has not selected a driver !
		if (canAdapter.CmpNoCase(_T("None")) != 0) {
			StopDevice();
		}

		// Wait a little for the threads to complete and in the case of Windows. the driver dll to be unloaded
		wxThread::Sleep(CONST_ONE_SECOND);


		// Save the settings
		if (SaveConfiguration()) {
			wxLogMessage(_T("TwoCan Plugin, Settings Saved"));
		}
		else {
			wxLogMessage(_T("TwoCan Plugin, Error Saving Settings"));
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
	settingsDialog = nullptr;
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
		configSettings->Read(_T("Waypoint"), &enableWaypoint, FALSE);
		configSettings->Read(_T("Music"), &enableMusic, FALSE);
		configSettings->Read(_T("Autopilot"), &enableAutopilot, FALSE);
		autopilotModel = (AUTOPILOT_MODEL)configSettings->ReadLong(_T("AutopilotModel"), AUTOPILOT_MODEL::NONE);
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
		enableAutopilot = FALSE;
		autopilotModel = AUTOPILOT_MODEL::NONE;

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
		configSettings->Write(_T("Waypoint"), enableWaypoint);
		configSettings->Write(_T("Music"), enableMusic);
		configSettings->Write(_T("Autopilot"), enableAutopilot);
		configSettings->Write(_T("AutopilotModel"), (int)autopilotModel);
		
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
			wxLogMessage(_T("TwoCan Plugin, Terminating device thread id (0x%lx)\n"), twoCanDevice->GetId());
			threadError = twoCanDevice->Delete(&threadExitCode, wxTHREAD_WAIT_BLOCK);
			if (threadError == wxTHREAD_NO_ERROR) {
				wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Delete Result: %p"), threadExitCode);
				// BUG BUG Following is to prevent wxLog message "Error: Can not wait for thread termination (error 6: the handle is invalid.)"
				// when runing on Windows, refer to https://forums.wxwidgets.org/viewtopic.php?t=13726
				#if defined (__WXMSW__) 
					if (twoCanDevice->IsRunning()) {
						twoCanDevice->Wait(wxTHREAD_WAIT_BLOCK);
					}
				#endif
				delete twoCanDevice;
				twoCanDevice = nullptr;

				// If the gateway is enabled, cleanup
				if ((deviceMode == TRUE) && (enableGateway == TRUE)) {
					if (twoCanEncoder != nullptr) {
						delete twoCanEncoder;
						twoCanEncoder = nullptr;
					}
				}

				// If the media player interface is enabled, cleanup
				if ((deviceMode == TRUE) && (enableMusic == TRUE)) {
					if (twoCanMedia != nullptr) {
						delete twoCanMedia;
						twoCanMedia = nullptr;
					}
				}

				// If the autopilot interface is enabled, cleanup
				if ((deviceMode == TRUE) && (enableAutopilot == TRUE)) {
					if (twoCanAutopilot != nullptr) {
						delete twoCanAutopilot;
						twoCanAutopilot = nullptr;
					}
				}
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
	if ((returnCode == TWOCAN_RESULT_SUCCESS) || (((returnCode & 0xFF0000) >> 16) == TWOCAN_ERROR_INVALID_WRITE_FUNCTION)) {
		wxLogMessage(_T("TwoCan Plugin, TwoCan Device Initialized"));
		int threadResult = twoCanDevice->Run();
		if (threadResult != wxTHREAD_NO_ERROR)    {
			wxLogError(_T("TwoCan Plugin, TwoCan Device Thread Creation Error: %d"), threadResult);
		}
		else {
			wxLogMessage(_T("TwoCan Plugin, TwoCan Device Thread Created"));	
			
			// If the gateway is enabled, the plugin will convert NMEA 183 sentences to NMEA 2000 messages
			if ((deviceMode == TRUE) && (enableGateway == TRUE)) {
				twoCanEncoder = new TwoCanEncoder(this);
				wxLogMessage(_T("TwoCan Plugin, Created Bi-Directional Gateway"));
			}

			// Fusion Media Player Integration
			if ((deviceMode == TRUE) && (enableMusic == TRUE)) {
				twoCanMedia = new TwoCanMedia();
				wxLogMessage(_T("TwoCan Plugin, Created Fusion Media Player interface"));
			}

			// Autopilot Integration
			if ((deviceMode == TRUE) && (enableAutopilot == TRUE)) {
				twoCanAutopilot = new TwoCanAutopilot(autopilotModel);
				wxLogMessage(_T("TwoCan Plugin, Created TwoCan Autopilot interface"));
			}

		}
	}
	else {
		wxLogError(_T("TwoCan Plugin,  TwoCan Device Initialize Error: %d"), returnCode);
	}
}
