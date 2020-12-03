// Copyright(C) 2018-2020 by Steven Adler
//
// This file is part of Actisense plugin for OpenCPN.
//


// Project: TwoCan Plugin
// Description: TwoCan plugin for OpenCPN
// Unit: Autopilot Dialog
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//


#include "twocanautopilotdialog.h"

// Constructor and destructor implementation
// inherits froms twocanautopilotsettingsbase which was implemented using wxFormBuilder
TwoCanAutopilotDialog::TwoCanAutopilotDialog( wxWindow* parent, wxEvtHandler *handler) 
	: TwoCanAutopilotDialogBase(parent, -1, _T("TwoCan Autopilot"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE) {
		
	// Save the parent event handler address
	eventHandlerAddress = handler;
	
	Fit();
	
	autopilotStatus = 0;
	autopilotMode = 0;
	desiredHeading = 0;
	radioBoxPower->SetSelection(autopilotStatus);
	radioBoxMode->SetSelection(autopilotMode);
	labelHeading->SetLabel(wxString::Format("%d",desiredHeading));
	
}


TwoCanAutopilotDialog::~TwoCanAutopilotDialog() {
// Nothing to do
}

void TwoCanAutopilotDialog::OnInit(wxActivateEvent& event) {
	event.Skip();
}

void TwoCanAutopilotDialog::OnWindowDestroy(wxWindowDestroyEvent& event) {
	if (autopilotStatus > 0) {
		wxMessageBox("Please disengage autopilot before exiting",_T("Destroy"), wxICON_WARNING);
		//event.Skip();
	}
}


void TwoCanAutopilotDialog::OnClose(wxCloseEvent& event) {
	if (autopilotStatus > 0) {
		wxMessageBox("Please disengage autopilot before exiting",_T("Close"), wxICON_WARNING);
		event.Veto(false);
	}
	else {
		event.Skip();
		// or Destroy();
	}
}

void TwoCanAutopilotDialog::RaiseEvent(int commandId, int command) {
	wxCommandEvent *event = new wxCommandEvent(wxEVT_AUTOPILOT_COMMAND_EVENT, commandId);
	event->SetString(std::to_string(command));
	wxQueueEvent(eventHandlerAddress, event);
}


void TwoCanAutopilotDialog::OnPowerChanged(wxCommandEvent &event) {
	autopilotStatus = radioBoxPower->GetSelection();
	RaiseEvent(AUTOPILOT_POWER_EVENT, autopilotStatus);
}


void TwoCanAutopilotDialog::OnModeChanged(wxCommandEvent &event) {
	autopilotMode = radioBoxMode->GetSelection();
	RaiseEvent(AUTOPILOT_MODE_EVENT, autopilotMode);
}


void TwoCanAutopilotDialog::OnPortTen(wxCommandEvent &event) {
	desiredHeading -= 10;
	labelHeading->SetLabel(wxString::Format("%d",desiredHeading));
	RaiseEvent(AUTOPILOT_HEADING_EVENT, -10);
}


void TwoCanAutopilotDialog::OnStbdTen(wxCommandEvent &event) {
	desiredHeading += 10;
	labelHeading->SetLabel(wxString::Format("%d",desiredHeading));
	RaiseEvent(AUTOPILOT_HEADING_EVENT, 10);
}


void TwoCanAutopilotDialog::OnPortOne(wxCommandEvent &event) {
	desiredHeading -= 1;
	labelHeading->SetLabel(wxString::Format("%d",desiredHeading));
	RaiseEvent(AUTOPILOT_HEADING_EVENT, -1);
}


void TwoCanAutopilotDialog::OnStbdOne(wxCommandEvent &event) {
	desiredHeading += 1;
	labelHeading->SetLabel(wxString::Format("%d",desiredHeading));
	RaiseEvent(AUTOPILOT_HEADING_EVENT, 1);
}


void TwoCanAutopilotDialog::OnCancel(wxCommandEvent &event) {
// Only close if the autopilot is not powered on
	if (autopilotStatus > 0) {
		wxMessageBox("Please disengage autopilot before exiting",_T("OnCancel"), wxICON_WARNING);
		event.Skip(false);
	}
	else {
		//Close();
		EndModal(wxID_OK);
	}
}

