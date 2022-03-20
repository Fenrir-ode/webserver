#include "app.h"
#include <wx/panel.h>
#include <wx/stdpaths.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
#include "server-intf.h"
#include "cJSON.h"

#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

static void LoadConfig(AppConfig &appconfig)
{
  // default settings
  appconfig.path = wxString("");
  appconfig.region = wxString("Disabled");

  // load
  FILE *fd = fopen("server.cfg", "rb");
  if (fd)
  {
    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    char *str = (char *)malloc(size);
    fread(str, size, 1, fd);

    cJSON *json = cJSON_Parse(str);
    cJSON *path = cJSON_GetObjectItem(json, "path");
    cJSON *region = cJSON_GetObjectItem(json, "region");

    if (cJSON_IsString(path))
    {
      appconfig.path = wxString::FromAscii(path->valuestring);
    }

    if (cJSON_IsString(region))
    {
      appconfig.region = wxString::FromAscii(region->valuestring);
    }

    cJSON_Delete(json);
    free(str);
    fclose(fd);
  }
}

static void SaveConfig(AppConfig &appconfig)
{
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "path", cJSON_CreateString(appconfig.path));
  cJSON_AddItemToObject(root, "region", cJSON_CreateString(appconfig.region));
  const char *json = cJSON_Print(root);

  FILE *fd = fopen("server.cfg", "wb");
  if (fd)
  {
    fwrite(json, strlen(json), 1, fd);
    fclose(fd);
  }

  free((void *)json);
  cJSON_Delete(root);
}

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

  wxStaticText *selectPathLabel = new wxStaticText(panel, wxID_ANY, "Saturn image path:");
  wxStaticText *regionPatchLabel = new wxStaticText(panel, wxID_ANY, "Console region patch:");

  regionComboBox = new wxComboBox(panel, COMBO_REGION_ID, wxEmptyString, {10, 10});

  wxString region_str[] = {_("Disabled"),
                           _("Japan"),
                           _("Taiwan"),
                           _("USA"),
                           _("Brazil"),
                           _("Korea"),
                           _("Asia Pal"),
                           _("Europe"),
                           _("Latin America")};

  for (int i = 0; i < sizeof(region_str) / sizeof(wxString); i++)
  {
    regionComboBox->Append(region_str[i]);
  }

  regionComboBox->Select(0);

  gameList_ctrl = new wxListView(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
  gameList_ctrl->InsertColumn(0, wxString::Format("Path"), wxLIST_FORMAT_LEFT, 300);

  // Set up the sizer for the panel
  wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
  panelSizer->AddSpacer(15);
  panelSizer->Add(selectPathLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(dirPickerCtrl, 0, wxEXPAND | wxALL, 5);
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

  // Apply config
  LoadConfig(appConfig);
  dirPickerCtrl->SetPath(appConfig.path);
  regionComboBox->SetStringSelection(appConfig.region);

  fenrirServer->SetRegionPatch(appConfig.region);
  fenrirServer->SetIsoDirectory(appConfig.path);
}

void Simple::OnRun(wxCommandEvent &event)
{
  fenrirServer->Run();
}

void Simple::OnComboBox(wxCommandEvent &event)
{

  // wxPostEvent()
  //  int r = event.GetSelection();
  fenrirServer->SetRegionPatch(regionComboBox->GetStringSelection());
  
  appConfig.region = regionComboBox->GetStringSelection();

  SaveConfig(appConfig);
}

void Simple::OnClose(wxCommandEvent &evt)
{
  fenrirServer->StopServer();
  // fenrirServer->Kill();
  Close(true);
}

void Simple::OnPathChanged(wxFileDirPickerEvent &evt)
{
  SetIsoDirectory(evt.GetPath());
  
  appConfig.path = evt.GetPath();
  SaveConfig(appConfig);
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
    wxFileName filename(evt.GetString());
    wxListItem col0;
    col0.SetId(0);
    col0.SetText(filename.GetName());
    gameList_ctrl->InsertItem(col0);
  }
  break;
  default:
    break;
  }
}

BEGIN_EVENT_TABLE(Simple, wxFrame)
EVT_DIRPICKER_CHANGED(DIR_PICKER_ID, Simple::OnPathChanged)
EVT_COMBOBOX(COMBO_REGION_ID, Simple::OnComboBox)
EVT_BUTTON(BUTTON_Close, Simple::OnClose)
EVT_BUTTON(BUTTON_Run, Simple::OnRun)
EVT_COMMAND(wxID_ANY, FENRIR_SERVER_EVENT, Simple::OnServerEvent)
END_EVENT_TABLE()
