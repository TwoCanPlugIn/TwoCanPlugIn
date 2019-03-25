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


// Project: TwoCan Plugin
// Description: NMEA 2000 plugin for OpenCPN
// Unit: Preferences Dialog for the Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/8/2018
// Version: 1.0
// 1.3 - Added Support for Linux (socketCAN interfaces)
// Outstanding Features: 
// 1. Prevent selection of driver that is not present
// 2. Prevent user selecting both LogFile reader and Log Raw frames !
// 3. Once NetworkMap and Active Network device complete, re-instantiate the Notebook Pages.
//

#include "twocansettings.h"

// Constructor and destructor implementation
// inherits froms TwoCanSettingsBase which was implemented using wxFormBuilder
TwoCanSettings::TwoCanSettings(wxWindow* parent, wxWindowID id, const wxString& title, \
	const wxPoint& pos, const wxSize& size, long style ) 
	: TwoCanSettingsBase(parent, id, title, pos, size, style) {

	// Set the dialog's 16x16 icon
	wxIcon icon;
	icon.CopyFromBitmap(twocan_16_icon);
	TwoCanSettings::SetIcon(icon);
}

wxString TwoCanSettings::GetCanAdapter(){
	return canAdapterName;
}

void TwoCanSettings::SetCanAdapter(wxString name) {
	canAdapterName = name;
}

int TwoCanSettings::GetParameterGroupNumbers(){
	return parameterGroupNumbers;
}

void TwoCanSettings::SetParameterGroupNumbers(int pgn) {
	parameterGroupNumbers = pgn;
}

int TwoCanSettings::GetRawLogging(){
	return rawLogging;
}

void TwoCanSettings::SetRawLogging(int logging) {
	rawLogging = logging;
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
	settingsDirty = FALSE;
	
	// Populate the pgn listbox and check the respective items from the setting "supportedPGN"
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
	pgn->Add(_T("129038..41") + _("AIS Class A & B messages") + _T(" (VDM)"));

	// Populate the listbox and check/uncheck as appropriate
	for (size_t i = 0; i < pgn->Count(); i++) {
		chkListPGN->Append(pgn->Item(i));
		chkListPGN->Check(i, (parameterGroupNumbers & (int)pow(2, i) ? TRUE : FALSE));
	}

	// Search for the twocan plugin drivers that are present
	EnumerateDrivers();
	
	// Populate the ComboBox and set the default driver selection
	for (adapterIterator = adapters.begin(); adapterIterator != adapters.end(); adapterIterator++){
		// For Windows, the first item of the hashmap is the "friendly name", the second is the full path of the driver
		// For Linux, both the first & second item of the hashmap is either "Log File Reader" or can adapter name, eg. "can0"
		cmbInterfaces->Append(adapterIterator->first);
		// Ensure that the driver being used is selected in the Combobox 
		if (canAdapterName == adapterIterator->second) {
			cmbInterfaces->SetStringSelection(adapterIterator->first);
		}
	}

	// BUG BUG Localization
	checkLogRaw->SetLabel(_("Log raw NMEA 2000 frames"));

	if (rawLogging & FLAGS_LOG_RAW) {
		checkLogRaw->SetValue(TRUE);
	}

	// Bitmap image for the About Page
	bmpAbout->SetBitmap(wxBitmap(twocan_64));

	// BUG BUG Localization
	// BUG BUG Version numbering
	txtAbout->SetLabel(_("TwoCan PlugIn for OpenCPN\nVersion 1.0\nEnables some NMEA2000® data to be directly integrated with OpenCPN.\nSend bug reports to twocanplugin@hotmail.com"));

	// Debug Window
	// BUG BUG Localization
	btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));

	// Network Map
	// not yet working
	//for (int i = 0; i <= CONST_MAX_DEVICES; i++) {
	//	dataGridNetwork->SetCellValue(i, 0, wxString::Format("%d",networkMap[i].networkAddress));
	//	dataGridNetwork->SetCellValue(i, 1, wxString::Format("%d",networkMap[i].uniqueId));
	//	dataGridNetwork->SetCellValue(i, 2, wxString::Format("%d",networkMap[i].manufacturerId));
	//}
	
	// So do not display it
	notebookTabs->DeletePage(1);

	// Similarly do not display NMEA2000 device tab, until it is implemented
	notebookTabs->DeletePage(1);

	// BUG BUG Couldn't work out how to hide the page, rather than delete it.
	
	Fit();
}

// BUG BUG Should prevent the user from shooting themselves in the foot if they select a driver that is not present
void TwoCanSettings::OnChoice(wxCommandEvent &event) {
	// BUG BUG should only set the dirty flag if we've actually selected a different driver
	settingsDirty = TRUE;
}

// Select NMEA 2000 parameter group numbers to be converted to their respective NMEA 0183 sentences
void TwoCanSettings::OnCheckPGN(wxCommandEvent &event) {
	settingsDirty = TRUE;
} 

// Enable Logging of Raw NMEA 2000 frames
void TwoCanSettings::OnCheckLog(wxCommandEvent &event) {
	settingsDirty = TRUE;
}

void TwoCanSettings::OnPause(wxCommandEvent &event) {
	// Toggle real time display of received NMEA 2000 frames
	debugWindowActive = !debugWindowActive;
	// BUG BUG Localization
	TwoCanSettings::btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));
}

void TwoCanSettings::OnCopy(wxCommandEvent &event) {
	// Copy the text box contents to the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(txtDebug->GetValue()));
		wxTheClipboard->Close();
	}
}

void TwoCanSettings::OnOK(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

	// Save the settings
	if (settingsDirty) {
		SaveSettings();
		settingsDirty = FALSE;
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
	if (settingsDirty) {
		SaveSettings();
		settingsDirty = FALSE;
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

	parameterGroupNumbers = 0;
	// Save the bitflags representing the checked items
	for (wxArrayInt::const_iterator it = checkedItems.begin(); it < checkedItems.end(); it++) {
		parameterGroupNumbers |= 1 << (int)*it;
	}

	rawLogging = 0;
	if (checkLogRaw->IsChecked()) {
		rawLogging= FLAGS_LOG_RAW;
	}

	if (cmbInterfaces->GetSelection() != wxNOT_FOUND) {
		canAdapterName = adapters[cmbInterfaces->GetStringSelection()];
	} 
	else {
		canAdapterName = _T("None");
	}
}

bool TwoCanSettings::EnumerateDrivers(void) {
	
#ifdef  __WXMSW__ 

	// Trying to be a good wxWidgets citizen but how unintuitive !!
	wxFileName adapterDirectoryName(::wxStandardPaths::Get().GetDataDir(),wxEmptyString);

	adapterDirectoryName.AppendDir(_T("plugins"));
	adapterDirectoryName.AppendDir(_T("twocan_pi"));
	adapterDirectoryName.AppendDir(_T("drivers"));
	adapterDirectoryName.Normalize();

	// BUG BUG Should we log this ?
	wxLogMessage(_T("TwoCan Settings, Driver Path: %s"),adapterDirectoryName.GetFullPath());

	wxDir adapterDirectory;

	if (adapterDirectory.Exists(adapterDirectoryName.GetFullPath())) {
		adapterDirectory.Open(adapterDirectoryName.GetFullPath());
	
		wxString fileName;
		wxString fileSpec = wxT("*.dll");
		
		bool cont = adapterDirectory.GetFirst(&fileName, fileSpec, wxDIR_FILES);
		while (cont){

			// Construct full path to selected driver and fetch driver information
			GetDriverInfo(wxString::Format("%s%s", adapterDirectoryName.GetFullPath(), fileName));

			cont = adapterDirectory.GetNext(&fileName);
		} 
	}
	else {
		// BUG BUG Should we log this ??
		wxLogMessage(_T("TwoCan Settings, driver folder not found"));
	}
	
#endif
	
#ifdef __LINUX__
	// Add the built-in Log File Reader to the Adapter hashmap
	// BUG BUG add a #define for this string constant
	adapters["Log File Reader"] = "Log File Reader";
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

	return TRUE;
}

#ifdef  __WXMSW__ 

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


