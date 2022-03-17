#include "app.h"
#include <wx/panel.h>
#include <wx/stdpaths.h>
#include <wx/filepicker.h>

BEGIN_EVENT_TABLE(Simple, wxFrame)
  EVT_BUTTON(BUTTON_DirDialog, Simple::OnDialog)
  EVT_DIRPICKER_CHANGED(DIR_PICKER_ID, Simple::OnPathChanged)
  EVT_COMBOBOX(DIR_PICKER_ID, Simple::OnComboBox)  
  EVT_BUTTON(BUTTON_Close,  Simple::OnQuit) 
END_EVENT_TABLE()

Simple::Simple(const wxString &title)
    : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(250, 150))
{
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

  // Set up the sizer for the panel
  wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
  panelSizer->Add(dirPickerCtrl, 0, wxEXPAND | wxALL, 5);
  panelSizer->AddSpacer(15);
  panelSizer->Add(selectPathLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(isoDirectoryText_ctrl, 0, wxEXPAND | wxALL, 5);
  panelSizer->AddSpacer(15);
  panelSizer->Add(regionPatchLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(regionComboBox, 0, wxEXPAND | wxALL, 5);


  // Set up the sizer for the frame and resize the frame
  // according to its contents

  wxBoxSizer *btnBoxSizer = new wxBoxSizer(wxHORIZONTAL);
  
  wxButton *btn1 = new wxButton(panel, wxID_ANY, _("Ok"));
  wxButton *btn2 = new wxButton(panel, wxID_ANY, _("Close"));
  btnBoxSizer->Add(btn1, 0);
  btnBoxSizer->Add(btn2, BUTTON_Close, wxLEFT | wxBOTTOM, 5);

  panelSizer->AddSpacer(15);
  panelSizer->Add(btnBoxSizer, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);


  panel->SetSizer(panelSizer);

  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
  topSizer->Add(panel, 1, wxEXPAND);
  SetSizerAndFit(topSizer);
  Centre();
}

void Simple::OnComboBox(wxCommandEvent &event)
{
}

void Simple::OnQuit(wxCommandEvent &evt) {
  
    Close(true);
}

void Simple::OnDialog(wxCommandEvent &event)
{
  wxDirDialog *d = new wxDirDialog(this, _("Choose a directory"),
                                   _("."), 0, wxDefaultPosition);

  if (d->ShowModal() == wxID_OK)
  {
    SetIsoDirectory(d->GetPath());
  }
}

void Simple::OnPathChanged(wxFileDirPickerEvent &evt)
{
  if (isoDirectoryText_ctrl)
  {
    isoDirectoryText_ctrl->SetValue(evt.GetPath());
  }
}

void Simple::SetIsoDirectory(wxString dir)
{
}
