///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Mar 29 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "twocansettingsbase.h"

///////////////////////////////////////////////////////////////////////////

TwoCanSettingsBase::TwoCanSettingsBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	wxBoxSizer* sizerDialog;
	sizerDialog = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* sizerTabs;
	sizerTabs = new wxBoxSizer( wxVERTICAL );
	
	notebookTabs = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	panelSettings = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelSettings;
	sizerPanelSettings = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* sizerLabelInterface;
	sizerLabelInterface = new wxBoxSizer( wxHORIZONTAL );
	
	labelInterface = new wxStaticText( panelSettings, wxID_ANY, wxT("NMEA 2000 Interfaces"), wxDefaultPosition, wxDefaultSize, 0 );
	labelInterface->Wrap( -1 );
	sizerLabelInterface->Add( labelInterface, 0, wxALL, 5 );
	
	
	sizerPanelSettings->Add( sizerLabelInterface, 0, wxEXPAND, 5 );
	
	wxBoxSizer* sizerCmbInterraces;
	sizerCmbInterraces = new wxBoxSizer( wxHORIZONTAL );
	
	wxArrayString cmbInterfacesChoices;
	cmbInterfaces = new wxChoice( panelSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, cmbInterfacesChoices, 0 );
	cmbInterfaces->SetSelection( 0 );
	sizerCmbInterraces->Add( cmbInterfaces, 1, wxALL, 5 );
	
	
	sizerPanelSettings->Add( sizerCmbInterraces, 0, wxEXPAND, 5 );
	
	wxBoxSizer* sizerLabelPGN;
	sizerLabelPGN = new wxBoxSizer( wxHORIZONTAL );
	
	labelPGN = new wxStaticText( panelSettings, wxID_ANY, wxT("Parameter Group Numbers"), wxDefaultPosition, wxDefaultSize, 0 );
	labelPGN->Wrap( -1 );
	sizerLabelPGN->Add( labelPGN, 1, wxALL, 5 );
	
	
	sizerPanelSettings->Add( sizerLabelPGN, 0, wxEXPAND, 5 );
	
	wxBoxSizer* sizerchkListPGN;
	sizerchkListPGN = new wxBoxSizer( wxVERTICAL );
	
	wxArrayString chkListPGNChoices;
	chkListPGN = new wxCheckListBox( panelSettings, wxID_ANY, wxDefaultPosition, wxDefaultSize, chkListPGNChoices, 0 );
	sizerchkListPGN->Add( chkListPGN, 1, wxALL|wxEXPAND, 5 );
	
	chkLogRaw = new wxCheckBox( panelSettings, wxID_ANY, wxT("Log Raw NMEA 2000 frames"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerchkListPGN->Add( chkLogRaw, 0, wxALL, 5 );
	
	
	sizerPanelSettings->Add( sizerchkListPGN, 1, wxEXPAND, 5 );
	
	
	panelSettings->SetSizer( sizerPanelSettings );
	panelSettings->Layout();
	sizerPanelSettings->Fit( panelSettings );
	notebookTabs->AddPage( panelSettings, wxT("Settings"), true );
	panelNetwork = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelNetwork;
	sizerPanelNetwork = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* sizerLabelNetwork;
	sizerLabelNetwork = new wxBoxSizer( wxHORIZONTAL );
	
	labelNetwork = new wxStaticText( panelNetwork, wxID_ANY, wxT("NMEA 2000 Devices"), wxDefaultPosition, wxDefaultSize, 0 );
	labelNetwork->Wrap( -1 );
	sizerLabelNetwork->Add( labelNetwork, 1, wxALL, 5 );
	
	
	sizerPanelNetwork->Add( sizerLabelNetwork, 0, wxEXPAND, 5 );
	
	wxBoxSizer* sizerGridViewNetwork;
	sizerGridViewNetwork = new wxBoxSizer( wxHORIZONTAL );
	
	dataGridNetwork = new wxGrid(panelNetwork, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
	
	// Grid
	dataGridNetwork->CreateGrid( 253, 3 );
	dataGridNetwork->EnableEditing( false );
	dataGridNetwork->EnableGridLines( true );
	dataGridNetwork->EnableDragGridSize( false );
	dataGridNetwork->SetMargins( 0, 0 );
	
	// Columns
	dataGridNetwork->SetColSize( 0, 85 );
	dataGridNetwork->SetColSize( 1, 112 );
	dataGridNetwork->SetColSize( 2, 112 );
	dataGridNetwork->EnableDragColMove( false );
	dataGridNetwork->EnableDragColSize( true );
	dataGridNetwork->SetColLabelSize( 32 );
	dataGridNetwork->SetColLabelValue( 0, wxT("Unique Id") );
	dataGridNetwork->SetColLabelValue( 1, wxT("Manufacturer") );
	dataGridNetwork->SetColLabelValue( 2, wxT("Model Id") );
	dataGridNetwork->SetColLabelAlignment( wxALIGN_LEFT, wxALIGN_CENTRE );
	
	// Rows
	dataGridNetwork->EnableDragRowSize( true );
	dataGridNetwork->SetRowLabelSize( 50 );
	dataGridNetwork->SetRowLabelAlignment( wxALIGN_LEFT, wxALIGN_CENTRE );

	// Label Appearance
	
	// Cell Defaults
	dataGridNetwork->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
	sizerGridViewNetwork->Add(dataGridNetwork, 0, wxALL, 5);
	
	
	sizerPanelNetwork->Add( sizerGridViewNetwork, 0, wxEXPAND, 5 );
	
	
	panelNetwork->SetSizer( sizerPanelNetwork );
	panelNetwork->Layout();
	sizerPanelNetwork->Fit( panelNetwork );
	notebookTabs->AddPage( panelNetwork, wxT("Network"), false );
	panelDevice = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerpanelDevice;
	sizerpanelDevice = new wxBoxSizer( wxVERTICAL );
	
	chkDeviceMode = new wxCheckBox( panelDevice, wxID_ANY, wxT("Enable Active Mode"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerpanelDevice->Add( chkDeviceMode, 0, wxALL, 5 );

	chkEnableHeartbeat = new wxCheckBox(panelDevice, wxID_ANY, wxT("Send heartbeats"), wxDefaultPosition, wxDefaultSize, 0);
	sizerpanelDevice->Add(chkEnableHeartbeat, 0, wxALL, 5);
	
	m_staticline1 = new wxStaticLine( panelDevice, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	sizerpanelDevice->Add( m_staticline1, 0, wxEXPAND | wxALL, 5 );
	
	labelNetworkAddress = new wxStaticText(panelDevice, wxID_ANY, wxT("Network Address:"), wxDefaultPosition, wxDefaultSize, 0);
	labelNetworkAddress->Wrap(-1);
	sizerpanelDevice->Add(labelNetworkAddress, 0, wxALL, 5);


	labelUniqueId = new wxStaticText( panelDevice, wxID_ANY, wxT("Unique ID:"), wxDefaultPosition, wxDefaultSize, 0 );
	labelUniqueId->Wrap( -1 );
	sizerpanelDevice->Add( labelUniqueId, 0, wxALL, 5 );

	labelManufacturer = new wxStaticText(panelDevice, wxID_ANY, wxT("Manufacturer:"), wxDefaultPosition, wxDefaultSize, 0);
	labelManufacturer->Wrap(-1);
	sizerpanelDevice->Add(labelManufacturer, 0, wxALL, 5);
	
	labelModelId = new wxStaticText(panelDevice, wxID_ANY, wxT("Model Id:"), wxDefaultPosition, wxDefaultSize, 0);
	labelModelId->Wrap(-1);
	sizerpanelDevice->Add(labelModelId, 0, wxALL, 5);

	labelSoftwareVersion = new wxStaticText(panelDevice, wxID_ANY, wxT("Software Version:"), wxDefaultPosition, wxDefaultSize, 0);
	labelSoftwareVersion->Wrap(-1);
	sizerpanelDevice->Add(labelSoftwareVersion, 0, wxALL, 5);

	labelDevice = new wxStaticText( panelDevice, wxID_ANY, wxT("Device Class: Inter/Intranetwork Device (25)"), wxDefaultPosition, wxDefaultSize, 0 );
	labelDevice->Wrap( -1 );
	sizerpanelDevice->Add( labelDevice, 0, wxALL, 5 );
	
	labelFunction = new wxStaticText( panelDevice, wxID_ANY, wxT("Device Function: PG Gateway (130)"), wxDefaultPosition, wxDefaultSize, 0 );
	labelFunction->Wrap( -1 );
	sizerpanelDevice->Add( labelFunction, 0, wxALL, 5 );
	
	panelDevice->SetSizer( sizerpanelDevice );
	panelDevice->Layout();
	sizerpanelDevice->Fit( panelDevice );
	notebookTabs->AddPage( panelDevice, wxT("Device"), false );
	panelDebug = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelDebug;
	sizerPanelDebug = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* sizerLabelDebug;
	sizerLabelDebug = new wxBoxSizer( wxHORIZONTAL );
	
	labelDebug = new wxStaticText( panelDebug, wxID_ANY, wxT("Received Frames"), wxDefaultPosition, wxDefaultSize, 0 );
	labelDebug->Wrap( -1 );
	sizerLabelDebug->Add( labelDebug, 0, wxALL, 5 );
	
	
	sizerLabelDebug->Add( 0, 0, 1, wxEXPAND, 5 );
	
	btnPause = new wxButton( panelDebug, wxID_ANY, wxT("Pause"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLabelDebug->Add( btnPause, 0, wxALL, 5 );
	
	btnCopy = new wxButton( panelDebug, wxID_ANY, wxT("Copy"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLabelDebug->Add( btnCopy, 0, wxALL, 5 );
	
	
	sizerPanelDebug->Add( sizerLabelDebug, 0, wxEXPAND, 5 );
	
	wxBoxSizer* sizerTxtDebug;
	sizerTxtDebug = new wxBoxSizer( wxVERTICAL );
	
	txtDebug = new wxTextCtrl( panelDebug, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE );
	sizerTxtDebug->Add( txtDebug, 1, wxALL|wxEXPAND, 5 );
	
	
	sizerPanelDebug->Add( sizerTxtDebug, 1, wxEXPAND, 5 );
	
	
	panelDebug->SetSizer( sizerPanelDebug );
	panelDebug->Layout();
	sizerPanelDebug->Fit( panelDebug );
	notebookTabs->AddPage( panelDebug, wxT("Debug"), false );
	panelAbout = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerAbout;
	sizerAbout = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* sizerIcon;
	sizerIcon = new wxBoxSizer( wxVERTICAL );
	
	bmpAbout = new wxStaticBitmap( panelAbout, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	sizerIcon->Add( bmpAbout, 1, wxALL, 5 );
	
	
	sizerIcon->Add( 0, 0, 1, wxEXPAND, 5 );
	
	
	sizerAbout->Add( sizerIcon, 1, wxEXPAND, 5 );
	
	wxBoxSizer* sizerAboutLabel;
	sizerAboutLabel = new wxBoxSizer( wxVERTICAL );
	
	txtAbout = new wxStaticText( panelAbout, wxID_ANY, wxT("About blah blah blah \nblah blah"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT );
	txtAbout->Wrap( -1 );
	sizerAboutLabel->Add( txtAbout, 1, wxALL|wxEXPAND, 5 );
	
	
	sizerAbout->Add( sizerAboutLabel, 1, wxEXPAND, 5 );
	
	
	panelAbout->SetSizer( sizerAbout );
	panelAbout->Layout();
	sizerAbout->Fit( panelAbout );
	notebookTabs->AddPage( panelAbout, wxT("About"), false );
	
	sizerTabs->Add( notebookTabs, 1, wxEXPAND | wxALL, 5 );
	
	
	sizerDialog->Add( sizerTabs, 1, wxEXPAND, 5 );
	
	wxBoxSizer* sizerButtons;
	sizerButtons = new wxBoxSizer( wxHORIZONTAL );
	
	
	sizerButtons->Add( 0, 0, 1, wxEXPAND, 5 );
	
	btnOK = new wxButton( this, wxID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerButtons->Add( btnOK, 0, wxALL, 5 );
	
	btnApply = new wxButton( this, wxID_APPLY, wxT("Apply"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerButtons->Add( btnApply, 0, wxALL, 5 );
	
	btnCancel = new wxButton( this, wxID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerButtons->Add( btnCancel, 0, wxALL, 5 );
	
	
	sizerDialog->Add( sizerButtons, 0, wxEXPAND, 5 );
	
	
	this->SetSizer( sizerDialog );
	this->Layout();
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_INIT_DIALOG, wxInitDialogEventHandler( TwoCanSettingsBase::OnInit ) );
	cmbInterfaces->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnChoice ), NULL, this );
	chkListPGN->Connect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckPGN ), NULL, this );
	chkLogRaw->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckLog ), NULL, this );
	chkEnableHeartbeat->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TwoCanSettingsBase::OnCheckHeartbeat), NULL, this);
	chkDeviceMode->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckMode ), NULL, this );
	btnPause->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnPause ), NULL, this );
	btnCopy->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCopy ), NULL, this );
	btnOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnOK ), NULL, this );
	btnApply->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnApply ), NULL, this );
	btnCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCancel ), NULL, this );
}

TwoCanSettingsBase::~TwoCanSettingsBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_INIT_DIALOG, wxInitDialogEventHandler( TwoCanSettingsBase::OnInit ) );
	cmbInterfaces->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnChoice ), NULL, this );
	chkListPGN->Disconnect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckPGN ), NULL, this );
	chkLogRaw->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckLog ), NULL, this );
	chkDeviceMode->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckMode ), NULL, this );
	chkEnableHeartbeat->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TwoCanSettingsBase::OnCheckHeartbeat), NULL, this);
	btnPause->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnPause ), NULL, this );
	btnCopy->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCopy ), NULL, this );
	btnOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnOK ), NULL, this );
	btnApply->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnApply ), NULL, this );
	btnCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCancel ), NULL, this );
	
}
