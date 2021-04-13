// Copyright(C) 2018-2020 by Steven Adler
//
// This file is part of Actisense plugin for OpenCPN.
//


#ifndef TWOCAN_AUTOPILOT_DIALOG_H
#define TWOCAN_AUTOPILOT_DIALOG_H

// The settings dialog base class from which we are derived
// Note wxFormBuilder used to generate UI
#include "twocanautopilotdialogbase.h"

#include "twocanutils.h"

// For logging
#include <wx/log.h>


// Events passed up to the plugin
const int AUTOPILOT_POWER_EVENT = wxID_HIGHEST + 2;
const int AUTOPILOT_MODE_EVENT = wxID_HIGHEST + 3;
const int AUTOPILOT_HEADING_EVENT = wxID_HIGHEST + 4;

extern const wxEventType wxEVT_AUTOPILOT_COMMAND_EVENT;

class TwoCanAutopilotDialog : public TwoCanAutopilotDialogBase {
	
public:
	TwoCanAutopilotDialog(wxWindow *parent, wxEvtHandler *handler);
	~TwoCanAutopilotDialog();

	// Reference to event handler address, ie. the TwoCan PlugIn
	wxEvtHandler *eventHandlerAddress;

	// Event raised when an autopilot command is issued
	void RaiseEvent(int commandId, int command);
	
protected:
	//overridden methods from the base class
	void OnInit(wxActivateEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnWindowDestroy(wxWindowDestroyEvent& event);
	void OnModeChanged(wxCommandEvent &event);
	void OnPowerChanged(wxCommandEvent &event);
	void OnPortTen(wxCommandEvent &event);
	void OnStbdTen(wxCommandEvent &event);
	void OnPortOne(wxCommandEvent &event);
	void OnStbdOne(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);
	
private:
	// BUG BUG perhaps use an enum
	int autopilotMode;
	int autopilotStatus;
	int desiredHeading;
		
};

#endif
