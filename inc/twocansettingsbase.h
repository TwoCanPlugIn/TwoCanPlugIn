///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Mar 29 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __TWOCANSETTINGSBASE_H__
#define __TWOCANSETTINGSBASE_H__

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/choice.h>
#include <wx/checklst.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/grid.h>
#include <wx/statline.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/statbmp.h>
#include <wx/notebook.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class TwoCanSettingsBase
///////////////////////////////////////////////////////////////////////////////
class TwoCanSettingsBase : public wxDialog 
{
	private:
	
	protected:
		wxNotebook* notebookTabs;
		wxPanel* panelSettings;
		wxStaticText* labelInterface;
		wxStaticText* labelPGN;
		wxCheckBox* chkLogRaw;
		wxPanel* panelNetwork;
		wxStaticText* labelNetwork;
		wxPanel* panelDevice;
		wxCheckBox* chkDeviceMode;
		wxCheckBox* chkEnableHeartbeat;;
		wxStaticLine* m_staticline1;
		wxStaticText* labelNetworkAddress;
		wxStaticText* labelUniqueId;
		wxStaticText* labelManufacturer;
		wxStaticText* labelModelId;
		wxStaticText* labelSoftwareVersion;
		wxStaticText* labelDevice;
		wxStaticText* labelFunction;
		wxPanel* panelDebug;
		wxStaticText* labelDebug;
		wxPanel* panelAbout;
		wxStaticBitmap* bmpAbout;
		wxStaticText* txtAbout;
		wxButton* btnOK;
		wxButton* btnApply;
		wxButton* btnCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnInit( wxInitDialogEvent& event ) { event.Skip(); }
		virtual void OnChoice( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckPGN( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckLog( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckMode( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckHeartbeat(wxCommandEvent& event) { event.Skip(); }
		virtual void OnPause( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCopy( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnApply( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCancel( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		wxChoice* cmbInterfaces;
		wxCheckListBox* chkListPGN;
		wxGrid* dataGridNetwork;
		wxButton* btnPause;
		wxButton* btnCopy;
		wxTextCtrl* txtDebug;
		
		TwoCanSettingsBase( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("TwoCan"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 350,400 ), long style = wxDEFAULT_DIALOG_STYLE ); 
		~TwoCanSettingsBase();
	
};

#endif //__TWOCANSETTINGSBASE_H__
