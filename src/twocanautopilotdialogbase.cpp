///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Nov  9 2019)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "twocanautopilotdialogbase.h"

 ///////////////////////////////////////////////////////////////////////////

TwoCanAutopilotDialogBase::TwoCanAutopilotDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* sizerFrame;
	sizerFrame = new wxBoxSizer( wxVERTICAL );

	wxGridSizer* sizerMode;
	sizerMode = new wxGridSizer( 0, 2, 0, 0 );

	wxString radioBoxPowerChoices[] = { wxT("Off"), wxT("On"), wxT("Standby") };
	int radioBoxPowerNChoices = sizeof( radioBoxPowerChoices ) / sizeof( wxString );
	radioBoxPower = new wxRadioBox( this, wxID_ANY, wxT("Power"), wxDefaultPosition, wxDefaultSize, radioBoxPowerNChoices, radioBoxPowerChoices, 1, wxRA_SPECIFY_COLS );
	radioBoxPower->SetSelection( 0 );
	sizerMode->Add( radioBoxPower, 0, wxALL, 5 );

	wxString radioBoxModeChoices[] = { wxT("Heading"), wxT("Wind"), wxT("GPS") };
	int radioBoxModeNChoices = sizeof( radioBoxModeChoices ) / sizeof( wxString );
	radioBoxMode = new wxRadioBox( this, wxID_ANY, wxT("Mode"), wxDefaultPosition, wxDefaultSize, radioBoxModeNChoices, radioBoxModeChoices, 1, wxRA_SPECIFY_COLS );
	radioBoxMode->SetSelection( 0 );
	sizerMode->Add( radioBoxMode, 0, wxALL, 5 );


	sizerFrame->Add( sizerMode, 1, wxEXPAND, 5 );

	wxGridSizer* sizerLeftRight;
	sizerLeftRight = new wxGridSizer( 2, 2, 0, 0 );

	buttonPortTen = new wxButton( this, wxID_ANY, wxT("<< 10"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLeftRight->Add( buttonPortTen, 0, wxALL, 5 );

	buttonStbdTen = new wxButton( this, wxID_ANY, wxT("10 >>"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLeftRight->Add( buttonStbdTen, 0, wxALL, 5 );

	buttonPortOne = new wxButton( this, wxID_ANY, wxT("<< 1"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLeftRight->Add( buttonPortOne, 0, wxALL, 5 );

	buttonStbdOne = new wxButton( this, wxID_ANY, wxT("1 >>"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLeftRight->Add( buttonStbdOne, 0, wxALL, 5 );


	sizerFrame->Add( sizerLeftRight, 1, wxEXPAND, 5 );

	wxBoxSizer* sizerHeading;
	sizerHeading = new wxBoxSizer( wxVERTICAL );

	labelHeading = new wxStaticText( this, wxID_ANY, wxT("Heading"), wxDefaultPosition, wxDefaultSize, 0 );
	labelHeading->Wrap( -1 );
	sizerHeading->Add( labelHeading, 0, wxALL, 5 );


	sizerFrame->Add( sizerHeading, 1, wxEXPAND, 5 );

	sizerButtons = new wxStdDialogButtonSizer();
	sizerButtonsCancel = new wxButton( this, wxID_CANCEL );
	sizerButtons->AddButton( sizerButtonsCancel );
	sizerButtons->Realize();

	sizerFrame->Add( sizerButtons, 1, wxEXPAND, 5 );


	this->SetSizer( sizerFrame );
	this->Layout();
	sizerFrame->Fit( this );

	this->Centre( wxBOTH );

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( TwoCanAutopilotDialogBase::OnClose ) );
	this->Connect( wxEVT_INIT_DIALOG, wxInitDialogEventHandler( TwoCanAutopilotDialogBase::OnInit ) );
	radioBoxPower->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnPowerChanged ), NULL, this );
	radioBoxMode->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnModeChanged ), NULL, this );
	buttonPortTen->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnPortTen ), NULL, this );
	buttonStbdTen->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnStbdTen ), NULL, this );
	buttonPortOne->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnPortOne ), NULL, this );
	buttonStbdOne->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnStbdOne ), NULL, this );
	sizerButtonsCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnCancel ), NULL, this );
}

TwoCanAutopilotDialogBase::~TwoCanAutopilotDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( TwoCanAutopilotDialogBase::OnClose ) );
	this->Disconnect( wxEVT_INIT_DIALOG, wxInitDialogEventHandler( TwoCanAutopilotDialogBase::OnInit ) );
	radioBoxPower->Disconnect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnPowerChanged ), NULL, this );
	radioBoxMode->Disconnect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnModeChanged ), NULL, this );
	buttonPortTen->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnPortTen ), NULL, this );
	buttonStbdTen->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnStbdTen ), NULL, this );
	buttonPortOne->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnPortOne ), NULL, this );
	buttonStbdOne->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnStbdOne ), NULL, this );
	sizerButtonsCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanAutopilotDialogBase::OnCancel ), NULL, this );

}
