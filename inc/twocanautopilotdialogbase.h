///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Nov  9 2019)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/radiobox.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/stattext.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class TwoCanAutopilotDialog
///////////////////////////////////////////////////////////////////////////////
class TwoCanAutopilotDialogBase : public wxDialog
{
	private:

	protected:
		wxRadioBox* radioBoxPower;
		wxRadioBox* radioBoxMode;
		wxButton* buttonPortTen;
		wxButton* buttonStbdTen;
		wxButton* buttonPortOne;
		wxButton* buttonStbdOne;
		wxStaticText* labelHeading;
		wxStdDialogButtonSizer* sizerButtons;
		wxButton* sizerButtonsCancel;

		// Virtual event handlers, overide them in your derived class
		virtual void OnClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void OnInit( wxInitDialogEvent& event ) { event.Skip(); }
		virtual void OnPowerChanged( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnModeChanged( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnPortTen( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnStbdTen( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnPortOne( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnStbdOne( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCancel( wxCommandEvent& event ) { event.Skip(); }


	public:

		TwoCanAutopilotDialogBase( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Autopilot"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE );
		~TwoCanAutopilotDialogBase();

};

