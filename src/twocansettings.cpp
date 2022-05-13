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


// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: Preferences Dialog for the Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// 1.3 - Added Support for Linux (socketCAN interfaces)
// 1.4 - Implemented support for active mode, participate on NMEA 2000 network
// 1.5 - 10/7/2019. Flags for XTE, Attitude, Additional log formats
// 1.6 - 10/10/2019 Flags for Rudder, Engine and Fluid levels
// 1.7 - 10/12/2019 Flags for Battery
// 1.8 - 10/05/2020 AIS data validation fixes, Mac OSX support
// 1.9 - 20/08/2020 Rusoku adapter support on Mac OSX, OCPN 5.2 Plugin Manager support
// 2.0 - 04/07/2021 Bi-directional gateway, PCAP log files
// 2.1 - 04/04/2022 Add configuration items for Media Player, Waypoint Creation and Autopilot (not yet implemented)
// Outstanding Features: 
// 1. Prevent selection of driver that is not physically present
// 2. Prevent user selecting both LogFile reader and Log Raw frames !
//

#include "twocansettings.h"

// Constructor and destructor implementation
// inherits froms TwoCanSettingsBase which was implemented using wxFormBuilder
TwoCanSettings::TwoCanSettings(wxWindow* parent, wxWindowID id, const wxString& title, \
	const wxPoint& pos, const wxSize& size, long style )
	: TwoCanSettingsBase(parent, id, title, pos, size, style) {

	parentWindow = parent;

	// Set the dialog's 16x16 icon
	wxIcon icon;
	icon.CopyFromBitmap(*_img_Toucan_16);
	TwoCanSettings::SetIcon(icon);
	togglePGN = FALSE;
}

TwoCanSettings::~TwoCanSettings() {
// Just in case we are closed from the dialog's close button 
	
	// We are closing...
	debugWindowActive = FALSE;

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}
}

void TwoCanSettings::OnInit(wxInitDialogEvent& event) {
	this->settingsDirty = FALSE;
		
	// Settings Tab
	wxArrayString *pgn = new wxArrayString();

	// BUG BUG Localization
	// Note to self, order must match FLAGS
	pgn->Add(_T("127250 ") + _("Heading") + _T(" (HDG)"));
	pgn->Add(_T("128259 ") + _("Speed") + _T(" (VHW)"));
	pgn->Add(_T("128267 ") + _("Depth") + _T(" (DPT)"));
	pgn->Add(_T("129025 ") + _("Position") + _T(" (GLL)"));
	pgn->Add(_T("129026 ") + _("Course and Speed over Ground") + _(" (VTG)"));
	pgn->Add(_T("129029 ") + _("GNSS") + _T(" (GGA)"));
	pgn->Add(_T("129033 ") + _("Time") + _T(" (ZDA)"));
	pgn->Add(_T("130306 ") + _("Wind") + _T(" (MWV)"));
	pgn->Add(_T("130310 ") + _("Water Temperature") + _(" (MWT)"));
	pgn->Add(_T("129808 ") + _("Digital Selective Calling") + _T(" (DSC)"));
	pgn->Add(_T("129038..41 ") + _("AIS Class A & B messages") + _T(" (VDM)"));
	pgn->Add(_T("129285 ") + _("Route/Waypoint") + _T(" (BWR/BOD/WPL/RTE)"));
	pgn->Add(_T("127251 ") + _("Rate of Turn") + _T(" (ROT)"));
	pgn->Add(_T("129283 ") + _("Cross Track Error") + _T(" (XTE)"));
	pgn->Add(_T("127257 ") + _("Attitude") + _T(" (XDR)"));
	pgn->Add(_T("127488..49 ") + _("Engine Parameters") + _T(" (XDR)"));
	pgn->Add(_T("127505 ") + _("Fluid Levels") + _T(" (XDR) "));
	pgn->Add(_T("127245 ") + _("Rudder Angle") + _T(" (RSA)"));
	pgn->Add(_T("127508 ") + _("Battery Status") + _T(" (XDR)"));
	pgn->Add(_T("129284 ") + _("Navigation Data") + _T(" (BWC/BWR/BOD/WCV)"));
	pgn->Add(_T("128275 ") + _("Vessel Trip Details") +_T(" (VLW)"));
	pgn->Add(_T("130323 ") + _("Meteorological Details") +_T(" (MDA)"));
	pgn->Add(_T("127233 ") + _("Man Overboard") + _T(" (MOB)"));
	
	// Populate the listbox and check/uncheck as appropriate
	for (size_t i = 0; i < pgn->Count(); i++) {
		chkListPGN->Append(pgn->Item(i));
		chkListPGN->Check(i, (supportedPGN & (int)pow(2, i) ? TRUE : FALSE));
	}

	// Search for the twocan plugin drivers that are present
	EnumerateDrivers();
	
	// Populate the ComboBox and set the default driver selection
	for (AvailableAdapters::iterator it = this->adapters.begin(); it != this->adapters.end(); it++){
		// For Windows, the first item of the hashmap is the "friendly name", the second is the full path of the driver
		// For Linux, both the first & second item of the hashmap is either "Log File Reader" or can adapter name, eg. "can0"
		cmbInterfaces->Append(it->first);
		// Ensure that the driver being used is selected in the Combobox 
		if (canAdapter == it->second) {
			cmbInterfaces->SetStringSelection(it->first);
		}
	}
	
	// About Tab
	bmpAbout->SetBitmap(wxBitmap(*_img_Toucan_64));
  
	// BUG BUG Localization & version numbers
	txtAbout->SetLabel(_T("TwoCan PlugIn for OpenCPN\nEnables some NMEA2000\xae data to be directly integrated with OpenCPN.\nSend bug reports to twocanplugin@hotmail.com"));
	txtAbout->Wrap(512);

	// Debug Tab
	// BUG BUG Localization
	btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));

	// Network Tab
	
	for (int i = 0; i < CONST_MAX_DEVICES; i++) {
		// Renumber row labels to match network address 0 - 253
		dataGridNetwork->SetRowLabelValue(i, std::to_string(i));
		// No need to iterate over non-existent entries
		if ((networkMap[i].uniqueId > 0) || (strlen(networkMap[i].productInformation.modelId) > 0) ) {
			dataGridNetwork->SetCellValue(i, 0, wxString::Format("%lu", networkMap[i].uniqueId));
			// Look up the manufacturer name
			std::unordered_map<int, std::string>::iterator it = deviceManufacturers.find(networkMap[i].manufacturerId);
			if (it != deviceManufacturers.end()) {
				dataGridNetwork->SetCellValue(i, 1, it->second);
			}
			else {
				dataGridNetwork->SetCellValue(i, 1, wxString::Format("%d", networkMap[i].manufacturerId));
			}
			dataGridNetwork->SetCellValue(i, 2, wxString::Format("%s", networkMap[i].productInformation.modelId));
			// We don't receive our own heartbeats so ignore our time stamp value
			if (networkMap[i].uniqueId != uniqueId) {
				wxGridCellAttr *attr;
				attr = new wxGridCellAttr;
				// Differentiate dead/alive devices 
				attr->SetTextColour((wxDateTime::Now() > (networkMap[i].timestamp + wxTimeSpan::Seconds(60))) ? *wxRED : *wxGREEN);
				dataGridNetwork->SetAttr(i, 0, attr);
			}
		}
	}
	
	// Device tab
	chkDeviceMode->SetValue(deviceMode);
	chkHeartbeat->Enable(chkDeviceMode->IsChecked());
	chkGateway->Enable(chkDeviceMode->IsChecked());
	chkAutopilot->Enable(chkDeviceMode->IsChecked());
	chkMedia->Enable(chkDeviceMode->IsChecked());
	chkWaypoint->Enable(chkDeviceMode->IsChecked());
	if (deviceMode == TRUE) {
		chkHeartbeat->SetValue(enableHeartbeat);
		chkGateway->SetValue(enableGateway);
		chkAutopilot->SetValue(enableAutopilot);
		chkMedia->SetValue(enableMusic);
		chkWaypoint->SetValue(enableWaypoint);
	}
	else {
		chkHeartbeat->SetValue(FALSE);
		chkGateway->SetValue(FALSE);
	}

	labelNetworkAddress->SetLabel(wxString::Format("Network Address: %u", networkAddress));
	labelUniqueId->SetLabel(wxString::Format("Unique ID: %lu", uniqueId));
	labelModelId->SetLabel(wxString::Format("Model ID: %s", PLUGIN_COMMON_NAME));
	labelManufacturer->SetLabel("Manufacturer: TwoCan");
	labelSoftwareVersion->SetLabel(wxString::Format("Software Version: %d.%d.%d", PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH));
	labelDevice->SetLabel(wxString::Format("Device Class: %d", CONST_DEVICE_CLASS));
	labelFunction->SetLabel(wxString::Format("Device Function: %d", CONST_DEVICE_FUNCTION));

	// Logging Options
	// Add Logging Options to the hashmap
	logging["None"] = FLAGS_LOG_NONE;
	logging["TwoCan"] = FLAGS_LOG_RAW;
	logging["Canboat"] = FLAGS_LOG_CANBOAT;
	logging["Candump"] = FLAGS_LOG_CANDUMP;
	logging["YachtDevices"] = FLAGS_LOG_YACHTDEVICES;
	logging["CSV"] = FLAGS_LOG_CSV;

	for (LoggingOptions::iterator it = this->logging.begin(); it != this->logging.end(); it++){
		cmbLogging->Append(it->first);
		if (logLevel == it->second) {
			cmbLogging->SetStringSelection(it->first);
		}
	}

	// BUG BUG I really don't understand wxWidgets sizers, but this seems to do what I want
	wxSize newSize = this->GetSize();
	dataGridNetwork->SetMinSize(wxSize(512, 20 * dataGridNetwork->GetDefaultRowSize()));
	dataGridNetwork->SetMaxSize(wxSize(-1, 20 * dataGridNetwork->GetDefaultRowSize()));
		
	Fit();

	// After we've fitted in everything adjust the dataGrid column widths
	int colWidth = (int)((dataGridNetwork->GetSize().GetWidth() - dataGridNetwork->GetRowLabelSize() - wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, NULL)) / 3);
	dataGridNetwork->SetColSize(0, colWidth);
	dataGridNetwork->SetColSize(1, colWidth);
	dataGridNetwork->SetColSize(2, colWidth);
	
}

// BUG BUG Should prevent the user from shooting themselves in the foot if they select a driver that is not present
void TwoCanSettings::OnChoiceInterfaces(wxCommandEvent &event) {
	// BUG BUG should only set the dirty flag if we've actually selected a different driver
	this->settingsDirty = TRUE;
}

// Select NMEA 2000 parameter group numbers to be converted to their respective NMEA 0183 sentences
void TwoCanSettings::OnCheckPGN(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
} 

// Enable Logging of Raw NMEA 2000 frames
void TwoCanSettings::OnChoiceLogging(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Toggle real time display of received NMEA 2000 frames
void TwoCanSettings::OnPause(wxCommandEvent &event) {
	debugWindowActive = !debugWindowActive;
	// BUG BUG Localization
	TwoCanSettings::btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));
}

// Copy the text box contents to the clipboard
void TwoCanSettings::OnCopy(wxCommandEvent &event) {
	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(txtDebug->GetValue()));
		wxTheClipboard->Close();
	}
}

void TwoCanSettings::OnExportWaypoint(wxCommandEvent &event) {
	wxMessageBox("Export Waypoint Settings Dialog");
}

// Set whether the device is an actve or passive node on the NMEA 2000 network
void TwoCanSettings::OnCheckMode(wxCommandEvent &event) {
	chkHeartbeat->Enable(chkDeviceMode->IsChecked());
	chkHeartbeat->SetValue(enableHeartbeat);
	chkGateway->Enable(chkDeviceMode->IsChecked());
	chkGateway->SetValue(enableGateway);
	chkMedia->Enable(chkDeviceMode->IsChecked());
	chkMedia->SetValue(enableMusic);
	chkWaypoint->Enable(chkDeviceMode->IsChecked());
	chkWaypoint->SetValue(enableWaypoint);
	chkAutopilot->Enable(chkDeviceMode->IsChecked());
	chkAutopilot->SetValue(enableAutopilot);
	this->settingsDirty = TRUE;
}

// Set whether the device sends heartbeats onto the network
void TwoCanSettings::OnCheckHeartbeat(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device acts as a bi-directional gateway, NMEA 183 -> NMEA 2000
void TwoCanSettings::OnCheckGateway(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device integrates with NMEA 2000 autopilots
void TwoCanSettings::OnCheckAutopilot(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device integrates with Fusion Media players
void TwoCanSettings::OnCheckMedia(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device will create an OpenCPN waypoint on reception of PGN 130074
void TwoCanSettings::OnCheckWaypoint(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Right mouse click to check/uncheck all parameter group numbers
void TwoCanSettings::OnRightClick(wxMouseEvent& event) {
	togglePGN = !togglePGN;
	for (unsigned int i = 0; i < chkListPGN->GetCount(); i++) {
		chkListPGN->Check(i, togglePGN);
	}
	this->settingsDirty = TRUE;
}


void TwoCanSettings::OnOK(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

	// Save the settings
	if (this->settingsDirty) {
		SaveSettings();
		this->settingsDirty = FALSE;
	}

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}
	
	// Return OK
	EndModal(wxID_OK);
}

void TwoCanSettings::OnApply(wxCommandEvent &event) {
	// Save the settings
	if (this->settingsDirty) {
		SaveSettings();
		this->settingsDirty = FALSE;
	}
}

void TwoCanSettings::OnCancel(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}

	// Ignore any changed settings and return CANCEL
	EndModal(wxID_CANCEL);
}

void TwoCanSettings::SaveSettings(void) {
	// Enumerate the check list box to determine checked items
	wxArrayInt checkedItems;
	chkListPGN->GetCheckedItems(checkedItems);

	supportedPGN = 0;
	// Save the bitflags representing the checked items
	for (wxArrayInt::const_iterator it = checkedItems.begin(); it < checkedItems.end(); it++) {
		supportedPGN |= 1 << (int)*it;
	}

	enableHeartbeat = FALSE;
	if (chkHeartbeat->IsChecked()) {
		enableHeartbeat = TRUE;
	}

	enableGateway = FALSE;
	if (chkGateway->IsChecked()) {
		enableGateway = TRUE;
	}
		
	deviceMode = FALSE;
	if (chkDeviceMode->IsChecked()) {
		deviceMode = TRUE;
	}

	enableAutopilot = FALSE;
	if (chkAutopilot->IsChecked()) {
		enableAutopilot = TRUE;
	}

	enableMusic = FALSE;
	if (chkMedia->IsChecked()) {
		enableMusic = TRUE;
	}

	enableWaypoint = FALSE;
	if (chkWaypoint->IsChecked()) {
		enableWaypoint = TRUE;
	}

	if (cmbInterfaces->GetSelection() != wxNOT_FOUND) {
		canAdapter = adapters[cmbInterfaces->GetStringSelection()];
	} 
	else {
		canAdapter = _T("None");
	}

	logLevel = FLAGS_LOG_NONE;
	if (cmbLogging->GetSelection() != wxNOT_FOUND) {
		logLevel = logging[cmbLogging->GetStringSelection()];
	}
	else {
		logLevel = FLAGS_LOG_NONE;
	}
}

// Lists the available CAN bus or Logfile interfaces 
bool TwoCanSettings::EnumerateDrivers(void) {
	
#if defined (__WXMSW__)
wxString driversFolder = pluginDataFolder +  _T("drivers") + wxFileName::GetPathSeparator();

	// BUG BUG Should we log this ?
	wxLogMessage(_T("TwoCan Settings, Driver Path: %s"), driversFolder);

	wxDir adapterDirectory;

	if (adapterDirectory.Exists(driversFolder)) {
		adapterDirectory.Open(driversFolder);
	
		wxString fileName;
		wxString fileSpec = wxT("*.dll");
		
		bool foundFile = adapterDirectory.GetFirst(&fileName, fileSpec, wxDIR_FILES);
		while (foundFile){

			// Construct full path to selected driver and fetch driver information
			GetDriverInfo(wxString::Format("%s%s", driversFolder, fileName));

			foundFile = adapterDirectory.GetNext(&fileName);
		} 
	}
	else {
		// BUG BUG Should we log this ??
		wxLogMessage(_T("TwoCan Settings, driver folder not found"));
	}
	
#endif
	
#if defined (__LINUX__)
	// Add the built-in Log File Reader to the Adapter hashmap
	// BUG BUG Should add a #define for this string constant
	adapters["Log File Reader"] = "Log File Reader";
	adapters["Pcap File Reader"] = "Pcap File Reader";
	// Add any physical CAN Adapters
	std::vector<wxString> canAdapters;
	// Enumerate installed CAN adapters
	canAdapters = TwoCanSocket::ListCanInterfaces();
	if (canAdapters.size() == 0) {
		// Log that no CAN interfaces exist
		wxLogMessage(_T("TwoCan Settings, No CAN interface present"));
	}
	else {
		//wxLogMessage(_T("TwoCan CAN Interface Socket: %i"),socketDescriptor);
		// Retrieve the name of the interface, by using the index.
		// BUG BUG Why doesn't the socketDescriptor remain constant
		// interfaceRequest.ifr_ifindex = 3; //socketDescriptor;
		// returnCode = ioctl(socketDescriptor, SIOCGIFNAME, &interfaceRequest);
		// wxLogMessage(_T("TwoCan, Get CAN Interface result: %d"),returnCode);
		// if (returnCode != -1)  {
		//	wxLogMessage(_T("TwoCan, CAN Adapter Name: %s"),interfaceRequest.ifr_name);
		//	adapters[interfaceRequest.ifr_name] = interfaceRequest.ifr_name;
		for (auto it = canAdapters.begin(); it != canAdapters.end(); ++it) {
			wxLogMessage(_T("TwoCan Settings, Found CAN adapter: %s"),*it);
			adapters[*it] = *it;
		}
		
	}
	
#endif

#if defined (__APPLE__) && defined (__MACH__)
	// Add the built-in Log File Reader, Pcap file reader, Cantact, Kvaser and Rusoku interfaces to the Adapter hashmap
	adapters["Log File Reader"] = "Log File Reader";
	adapters["Pcap File Reader"] = "Pcap File Reader";
	adapters["Cantact"] = "Cantact";
	adapters["Kvaser"] = "Kvaser";
	adapters["Rusoku"] = "Rusoku";
#endif


	return TRUE;
}

#if defined (__WXMSW__) 

// Retrieve the human friendly name for the different Windows drivers to populate the combo box
void TwoCanSettings::GetDriverInfo(wxString fileName) {
	HMODULE dllHandle;
	LPFNDLLDriverName driverName = NULL;
	
	// BUG BUG Log this
	 wxLogMessage(_T("TwoCan Settings, Attempting to load driver: %s"), fileName);

	// Get a handle to the DLL module.
	dllHandle = LoadLibrary(fileName);
	
	// If the handle is valid, try to get the function addresses. 
	if (dllHandle != NULL)	{

		// Get pointer to driverName function
		driverName = (LPFNDLLDriverName)GetProcAddress(dllHandle, "DriverName");

		// If the function address is valid, call the function. 
		if (driverName != NULL)	{
			
			// Add entry to the Adapter hashmap
			adapters[driverName()] = fileName;
		}
		else {

			// BUG BUG Log an error indicating a DLL in the driver directory does not support the expected methods
			wxLogError(_T("TwoCan Settings, Invalid driverName function %d for %s"), GetLastError(), fileName);
		}

		// Free the DLL module.
		BOOL freeResult = FreeLibrary(dllHandle);
	}
	else {

		// BUG BUG Should log this
		wxLogError(_T("TwoCan Settings, Invalid DLL Handle Error: %d for %s"), GetLastError(), fileName);
	}
}

#endif


