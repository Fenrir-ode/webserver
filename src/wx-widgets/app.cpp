#include "app.h"
#include <wx/panel.h>
#include <wx/stdpaths.h>
#include <wx/filepicker.h>
#include "server-intf.h"

Simple::Simple(const wxString &title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(250, 150))
{
  runningStatus = 0;
  fenrirServer = new FenrirServer(this);
  fenrirServer->Init();

  // Create a top-level panel to hold all the contents of the frame
  wxPanel *panel = new wxPanel(this, wxID_ANY);

  // Create a wxFilePickerCtrl control
  wxDirPickerCtrl *dirPickerCtrl = new wxDirPickerCtrl(panel, DIR_PICKER_ID,
                                                       wxEmptyString, wxDirSelectorPromptStr,
                                                       wxDefaultPosition, wxSize(350, wxDefaultCoord));

  wxStaticText *selectPathLabel = new wxStaticText(panel, wxID_ANY, "Selected Path:");
  wxStaticText *regionPatchLabel = new wxStaticText(panel, wxID_ANY, "Region patch:");
  isoDirectoryText_ctrl = new wxTextCtrl(panel, wxID_ANY);

  wxComboBox *regionComboBox = new wxComboBox(panel, COMBO_REGION_ID, wxEmptyString, {10, 10});

  wxString region_str[] = {
      _("Japan"),
      _("Taiwan"),
      _("USA"),
      _("Brazil"),
      _("Korea"),
      _("Asia Pal"),
      _("Europe"),
      _("Latin America")};

  regionComboBox->Append(_("Disabled"));
  for (int i = 0; i < sizeof(region_str) / sizeof(wxString); i++)
  {
    regionComboBox->Append(region_str[i]);
  }

  regionComboBox->Select(0);

  gameList_ctrl = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
  gameList_ctrl->InsertColumn(0, wxString::Format("Path"));

  // Set up the sizer for the panel
  wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
  panelSizer->Add(dirPickerCtrl, 0, wxEXPAND | wxALL, 5);
  panelSizer->AddSpacer(15);
  panelSizer->Add(selectPathLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(isoDirectoryText_ctrl, 0, wxEXPAND | wxALL, 5);
  panelSizer->AddSpacer(15);
  panelSizer->Add(regionPatchLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(regionComboBox, 0, wxEXPAND | wxALL, 5);
  panelSizer->Add(gameList_ctrl, 0, wxEXPAND | wxALL, 5);

  // Set up the sizer for the frame and resize the frame
  // according to its contents

  wxBoxSizer *btnBoxSizer = new wxBoxSizer(wxHORIZONTAL);

  run_btn = new wxButton(panel, BUTTON_Run, _("Run"));
  close_btn = new wxButton(panel, BUTTON_Close, _("Close"));
  btnBoxSizer->Add(run_btn, 0);
  btnBoxSizer->Add(close_btn, 0, wxLEFT | wxBOTTOM, 5);

  panelSizer->AddSpacer(15);
  panelSizer->Add(btnBoxSizer, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

  panel->SetSizer(panelSizer);

  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  topSizer->Add(panel, 1, wxEXPAND);
  SetSizerAndFit(topSizer);
  Centre();
}

void Simple::OnRun(wxCommandEvent &event)
{
  fenrirServer->Run();
}

void Simple::OnComboBox(wxCommandEvent &event)
{
  // wxPostEvent()
}

void Simple::OnClose(wxCommandEvent &evt)
{
  fenrirServer->StopServer();
  // fenrirServer->Kill();
  Close(true);
}

void Simple::OnPathChanged(wxFileDirPickerEvent &evt)
{
  if (isoDirectoryText_ctrl)
  {
    isoDirectoryText_ctrl->SetValue(evt.GetPath());
  }

  SetIsoDirectory(evt.GetPath());
}

void Simple::SetIsoDirectory(wxString dir)
{
  // wxLogMessage("Hello world from wxWidgets!");
  fenrirServer->SetIsoDirectory(dir);
}

void Simple::OnServerEvent(wxCommandEvent &evt)
{
  // wxLogError("OnServerEvent");
  switch (evt.GetInt())
  {
  case FENRIR_SERVER_EVENT_TYPE_PENDING:
    runningStatus = FENRIR_SERVER_EVENT_TYPE_PENDING;
    run_btn->Enable(false);
    // close_btn->Enable(false);
    break;
  case FENRIR_SERVER_EVENT_TYPE_RUN:
    run_btn->Enable(false);
    // close_btn->Enable(false);
    runningStatus = FENRIR_SERVER_EVENT_TYPE_RUN;
    break;
  case FENRIR_SERVER_EVENT_TYPE_STOPPED:
    run_btn->Enable(false);
    // close_btn->Enable(false);
    runningStatus = FENRIR_SERVER_EVENT_TYPE_STOPPED;
    break;
  case FENRIR_SERVER_EVENT_TYPE_NOTIFY_GAME:
  {
    wxListItem col0;
    col0.SetId(0);
    col0.SetText(wxString(evt.GetString()));
    col0.SetWidth(50);
    gameList_ctrl->InsertItem(col0);
  }
  break;
  default:
    break;
  }
}

BEGIN_EVENT_TABLE(Simple, wxFrame)
EVT_DIRPICKER_CHANGED(DIR_PICKER_ID, Simple::OnPathChanged)
EVT_COMBOBOX(DIR_PICKER_ID, Simple::OnComboBox)
EVT_BUTTON(BUTTON_Close, Simple::OnClose)
EVT_BUTTON(BUTTON_Run, Simple::OnRun)
EVT_COMMAND(wxID_ANY, FENRIR_SERVER_EVENT, Simple::OnServerEvent)
END_EVENT_TABLE()