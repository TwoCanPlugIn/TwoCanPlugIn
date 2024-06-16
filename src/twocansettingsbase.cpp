///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Oct 26 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "twocansettingsbase.h"

///////////////////////////////////////////////////////////////////////////

TwoCanSettingsBase::TwoCanSettingsBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxSize( -1,-1 ) );

	wxBoxSizer* sizerDialog;
	sizerDialog = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* sizerTabs;
	sizerTabs = new wxBoxSizer( wxVERTICAL );

	notebookTabs = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	panelSettings = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelSettings;
	sizerPanelSettings = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sizerInterfaces;
	sizerInterfaces = new wxStaticBoxSizer( new wxStaticBox( panelSettings, wxID_ANY, wxT("NMEA 2000 Interfaces") ), wxHORIZONTAL );

	wxArrayString cmbInterfacesChoices;
	cmbInterfaces = new wxChoice( sizerInterfaces->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, cmbInterfacesChoices, 0 );
	cmbInterfaces->SetSelection( 0 );
	sizerInterfaces->Add( cmbInterfaces, 0, wxALL, 5 );


	sizerPanelSettings->Add( sizerInterfaces, 0, wxEXPAND, 5 );

	wxStaticBoxSizer* sizerPGN;
	sizerPGN = new wxStaticBoxSizer( new wxStaticBox( panelSettings, wxID_ANY, wxT("Parameter Group Numbers") ), wxHORIZONTAL );

	wxArrayString chkListPGNChoices;
	chkListPGN = new wxCheckListBox( sizerPGN->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, chkListPGNChoices, 0 );
	sizerPGN->Add( chkListPGN, 1, wxALL|wxEXPAND, 5 );


	sizerPanelSettings->Add( sizerPGN, 1, wxEXPAND, 5 );


	panelSettings->SetSizer( sizerPanelSettings );
	panelSettings->Layout();
	sizerPanelSettings->Fit( panelSettings );
	notebookTabs->AddPage( panelSettings, wxT("Settings"), true );
	panelNetwork = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelNetwork;
	sizerPanelNetwork = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* sizerLabelDevices;
	sizerLabelDevices = new wxBoxSizer( wxHORIZONTAL );

	lblNetwork = new wxStaticText( panelNetwork, wxID_ANY, wxT("NMEA 2000 Devices"), wxDefaultPosition, wxDefaultSize, 0 );
	lblNetwork->Wrap( -1 );
	sizerLabelDevices->Add( lblNetwork, 0, wxALL, 5 );


	sizerPanelNetwork->Add( sizerLabelDevices, 0, 0, 5 );

	wxBoxSizer* sizerGridDevices;
	sizerGridDevices = new wxBoxSizer( wxHORIZONTAL );

	dataGridNetwork = new wxGrid( panelNetwork, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

	// Grid
	dataGridNetwork->CreateGrid( 253, 3 );
	dataGridNetwork->EnableEditing( false );
	dataGridNetwork->EnableGridLines( true );
	dataGridNetwork->EnableDragGridSize( false );
	dataGridNetwork->SetMargins( 0, 0 );

	// Columns
	dataGridNetwork->SetColSize( 0, 85 );
	dataGridNetwork->SetColSize( 1, 120 );
	dataGridNetwork->SetColSize( 2, 120 );
	dataGridNetwork->EnableDragColMove( false );
	dataGridNetwork->EnableDragColSize( true );
	dataGridNetwork->SetColLabelSize( 30 );
	dataGridNetwork->SetColLabelValue( 0, wxT("Unique Id") );
	dataGridNetwork->SetColLabelValue( 1, wxT("Manufacturer") );
	dataGridNetwork->SetColLabelValue( 2, wxT("Model Id") );
	dataGridNetwork->SetColLabelAlignment( wxALIGN_LEFT, wxALIGN_CENTER );

	// Rows
	dataGridNetwork->EnableDragRowSize( true );
	dataGridNetwork->SetRowLabelSize( 80 );
	dataGridNetwork->SetRowLabelAlignment( wxALIGN_LEFT, wxALIGN_CENTER );

	// Label Appearance

	// Cell Defaults
	dataGridNetwork->SetDefaultCellAlignment( wxALIGN_LEFT, wxALIGN_TOP );
	sizerGridDevices->Add( dataGridNetwork, 0, wxALL, 5 );


	sizerPanelNetwork->Add( sizerGridDevices, 1, 0, 5 );


	panelNetwork->SetSizer( sizerPanelNetwork );
	panelNetwork->Layout();
	sizerPanelNetwork->Fit( panelNetwork );
	notebookTabs->AddPage( panelNetwork, wxT("Network"), false );
	panelDevice = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelDevice;
	sizerPanelDevice = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sizerDevice;
	sizerDevice = new wxStaticBoxSizer( new wxStaticBox( panelDevice, wxID_ANY, wxT("NMEA 2000 Device Mode") ), wxVERTICAL );

	chkDeviceMode = new wxCheckBox( sizerDevice->GetStaticBox(), wxID_ANY, wxT("Enable Active Mode"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerDevice->Add( chkDeviceMode, 0, wxALL, 5 );

	chkHeartbeat = new wxCheckBox( sizerDevice->GetStaticBox(), wxID_ANY, wxT("Send Heartbeats"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerDevice->Add( chkHeartbeat, 0, wxALL, 5 );

	chkGateway = new wxCheckBox( sizerDevice->GetStaticBox(), wxID_ANY, wxT("Bi-Directional Gateway"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerDevice->Add( chkGateway, 0, wxALL, 5 );

	chkWaypoint = new wxCheckBox( sizerDevice->GetStaticBox(), wxID_ANY, wxT("Receive Waypoints"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerDevice->Add( chkWaypoint, 0, wxALL, 5 );

	chkMedia = new wxCheckBox( sizerDevice->GetStaticBox(), wxID_ANY, wxT("Fusion Media Player Integration"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerDevice->Add( chkMedia, 0, wxALL, 5 );


	sizerPanelDevice->Add( sizerDevice, 0, wxEXPAND, 5 );

	wxStaticBoxSizer* sizerDetails;
	sizerDetails = new wxStaticBoxSizer( new wxStaticBox( panelDevice, wxID_ANY, wxT("NMEA 2000 Device Details") ), wxVERTICAL );

	labelNetworkAddress = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Network Address"), wxDefaultPosition, wxDefaultSize, 0 );
	labelNetworkAddress->Wrap( -1 );
	sizerDetails->Add( labelNetworkAddress, 0, wxALL, 5 );

	labelUniqueId = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Unique Id"), wxDefaultPosition, wxDefaultSize, 0 );
	labelUniqueId->Wrap( -1 );
	sizerDetails->Add( labelUniqueId, 0, wxALL, 5 );

	labelManufacturer = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Manufacturer"), wxDefaultPosition, wxDefaultSize, 0 );
	labelManufacturer->Wrap( -1 );
	sizerDetails->Add( labelManufacturer, 0, wxALL, 5 );

	labelModelId = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Model Id"), wxDefaultPosition, wxDefaultSize, 0 );
	labelModelId->Wrap( -1 );
	sizerDetails->Add( labelModelId, 0, wxALL, 5 );

	labelSoftwareVersion = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Software Version"), wxDefaultPosition, wxDefaultSize, 0 );
	labelSoftwareVersion->Wrap( -1 );
	sizerDetails->Add( labelSoftwareVersion, 0, wxALL, 5 );

	labelDevice = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Device Class"), wxDefaultPosition, wxDefaultSize, 0 );
	labelDevice->Wrap( -1 );
	sizerDetails->Add( labelDevice, 0, wxALL, 5 );

	labelFunction = new wxStaticText( sizerDetails->GetStaticBox(), wxID_ANY, wxT("Device Function"), wxDefaultPosition, wxDefaultSize, 0 );
	labelFunction->Wrap( -1 );
	sizerDetails->Add( labelFunction, 0, wxALL, 5 );


	sizerPanelDevice->Add( sizerDetails, 0, wxEXPAND, 5 );

	wxStaticBoxSizer* sizerRawLogging;
	sizerRawLogging = new wxStaticBoxSizer( new wxStaticBox( panelDevice, wxID_ANY, wxT("Raw Logging") ), wxHORIZONTAL );

	wxArrayString cmbLoggingChoices;
	cmbLogging = new wxChoice( sizerRawLogging->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, cmbLoggingChoices, 0 );
	cmbLogging->SetSelection( 0 );
	sizerRawLogging->Add( cmbLogging, 0, wxALL, 5 );


	sizerPanelDevice->Add( sizerRawLogging, 0, wxEXPAND, 5 );


	panelDevice->SetSizer( sizerPanelDevice );
	panelDevice->Layout();
	sizerPanelDevice->Fit( panelDevice );
	notebookTabs->AddPage( panelDevice, wxT("Device"), false );
	panelAutopilot = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxStaticBoxSizer* sizerAutopilot;
	sizerAutopilot = new wxStaticBoxSizer( new wxStaticBox( panelAutopilot, wxID_ANY, wxT("NMEA 2000 Autopilot Settings") ), wxVERTICAL );

	wxString rdoBoxAutopilotChoices[] = { wxT("None"), wxT("Garmin"), wxT("Raymarine"), wxT("Navico (B&&G, Simrad)") };
	int rdoBoxAutopilotNChoices = sizeof( rdoBoxAutopilotChoices ) / sizeof( wxString );
	rdoBoxAutopilot = new wxRadioBox( sizerAutopilot->GetStaticBox(), wxID_ANY, wxT("Autopilot Model"), wxDefaultPosition, wxDefaultSize, rdoBoxAutopilotNChoices, rdoBoxAutopilotChoices, 1, wxRA_SPECIFY_COLS );
	rdoBoxAutopilot->SetSelection( 1 );
	sizerAutopilot->Add( rdoBoxAutopilot, 0, wxALL, 5 );


	panelAutopilot->SetSizer( sizerAutopilot );
	panelAutopilot->Layout();
	sizerAutopilot->Fit( panelAutopilot );
	notebookTabs->AddPage( panelAutopilot, wxT("Autopilot"), false );
	panelDebug = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerPanelDebug;
	sizerPanelDebug = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* sizerLabelDebug;
	sizerLabelDebug = new wxBoxSizer( wxHORIZONTAL );

	labelDebug = new wxStaticText( panelDebug, wxID_ANY, wxT("Received Frames"), wxDefaultPosition, wxDefaultSize, 0 );
	labelDebug->Wrap( -1 );
	sizerLabelDebug->Add( labelDebug, 0, wxALL, 5 );


	sizerLabelDebug->Add( 0, 0, 1, wxEXPAND, 5 );

	btnPause = new wxButton( panelDebug, wxID_ANY, wxT("Start"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLabelDebug->Add( btnPause, 0, wxALL, 5 );

	btnCopy = new wxButton( panelDebug, wxID_ANY, wxT("Copy"), wxDefaultPosition, wxDefaultSize, 0 );
	sizerLabelDebug->Add( btnCopy, 0, wxALL, 5 );


	sizerPanelDebug->Add( sizerLabelDebug, 0, wxEXPAND, 5 );

	wxBoxSizer* sizerTextDebug;
	sizerTextDebug = new wxBoxSizer( wxHORIZONTAL );

	txtDebug = new wxTextCtrl( panelDebug, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY );
	sizerTextDebug->Add( txtDebug, 1, wxALL|wxEXPAND, 5 );


	sizerPanelDebug->Add( sizerTextDebug, 1, wxEXPAND, 5 );


	panelDebug->SetSizer( sizerPanelDebug );
	panelDebug->Layout();
	sizerPanelDebug->Fit( panelDebug );
	notebookTabs->AddPage( panelDebug, wxT("Debug"), false );
	panelAbout = new wxPanel( notebookTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* sizerAbout;
	sizerAbout = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* sizerIcon;
	sizerIcon = new wxBoxSizer( wxHORIZONTAL );

	bmpAbout = new wxStaticBitmap( panelAbout, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0 );
	sizerIcon->Add( bmpAbout, 0, wxALL, 5 );


	sizerAbout->Add( sizerIcon, 0, wxEXPAND, 5 );

	wxBoxSizer* sizerTextAbout;
	sizerTextAbout = new wxBoxSizer( wxHORIZONTAL );

	txtAbout = new wxStaticText( panelAbout, wxID_ANY, wxT("About BlahBlah"), wxDefaultPosition, wxDefaultSize, 0 );
	txtAbout->Wrap( 512 );
	sizerTextAbout->Add( txtAbout, 0, wxALL, 5 );


	sizerAbout->Add( sizerTextAbout, 0, wxFIXED_MINSIZE, 5 );


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
	cmbInterfaces->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnChoiceInterfaces ), NULL, this );
	chkListPGN->Connect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckPGN ), NULL, this );
	chkListPGN->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( TwoCanSettingsBase::OnRightClick ), NULL, this );
	chkDeviceMode->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckMode ), NULL, this );
	chkHeartbeat->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckHeartbeat ), NULL, this );
	chkGateway->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckGateway ), NULL, this );
	chkWaypoint->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckWaypoint ), NULL, this );
	chkMedia->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckMedia ), NULL, this );
	cmbLogging->Connect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnChoiceLogging ), NULL, this );
	rdoBoxAutopilot->Connect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnAutopilotModelChanged ), NULL, this );
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
	cmbInterfaces->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnChoiceInterfaces ), NULL, this );
	chkListPGN->Disconnect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckPGN ), NULL, this );
	chkListPGN->Disconnect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( TwoCanSettingsBase::OnRightClick ), NULL, this );
	chkDeviceMode->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckMode ), NULL, this );
	chkHeartbeat->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckHeartbeat ), NULL, this );
	chkGateway->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckGateway ), NULL, this );
	chkWaypoint->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckWaypoint ), NULL, this );
	chkMedia->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCheckMedia ), NULL, this );
	cmbLogging->Disconnect( wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnChoiceLogging ), NULL, this );
	rdoBoxAutopilot->Disconnect( wxEVT_COMMAND_RADIOBOX_SELECTED, wxCommandEventHandler( TwoCanSettingsBase::OnAutopilotModelChanged ), NULL, this );
	btnPause->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnPause ), NULL, this );
	btnCopy->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCopy ), NULL, this );
	btnOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnOK ), NULL, this );
	btnApply->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnApply ), NULL, this );
	btnCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( TwoCanSettingsBase::OnCancel ), NULL, this );

}
