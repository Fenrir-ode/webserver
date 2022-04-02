#include "app.h"
#include <wx/panel.h>
#include <wx/stdpaths.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
#include <wx/textctrl.h>
#include "server-intf.h"
#include "cJSON.h"

#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

static void LoadConfig(AppConfig &appconfig)
{
  // default settings
  appconfig.path = wxString("");
  appconfig.port = 3000;

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
    cJSON *port = cJSON_GetObjectItem(json, "port");

    if (cJSON_IsString(path))
    {
      appconfig.path = wxString::FromAscii(path->valuestring);
    }

    if (cJSON_IsNumber(port))
    {
      appconfig.port = port->valueint;
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
  cJSON_AddItemToObject(root, "port", cJSON_CreateNumber(appconfig.port));
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
  dirPickerCtrl = new wxDirPickerCtrl(panel, DIR_PICKER_ID,
                                      wxEmptyString, wxDirSelectorPromptStr,
                                      wxDefaultPosition, wxSize(350, wxDefaultCoord));

  wxStaticText *selectPathLabel = new wxStaticText(panel, wxID_ANY, "Saturn image path:");
  wxStaticText *portLabel = new wxStaticText(panel, wxID_ANY, "Server port:");
  wxString p;
  portCtrl = new wxTextCtrl(this, PORT_ID, "", wxDefaultPosition, wxDefaultSize, wxTE_NO_VSCROLL, wxTextValidator(wxFILTER_DIGITS, &p), wxTextCtrlNameStr);

  gameList_ctrl = new wxListView(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_AUTOARRANGE | wxLC_NO_HEADER | wxLC_LIST | wxLC_REPORT | wxLC_SINGLE_SEL);
  gameList_ctrl->InsertColumn(0, wxString::Format("Path"), wxLIST_FORMAT_LEFT, 350);

  // Set up the sizer for the panel
  wxBoxSizer *panelSizer = new wxBoxSizer(wxVERTICAL);
  panelSizer->AddSpacer(15);
  panelSizer->Add(selectPathLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(dirPickerCtrl, 0, wxEXPAND | wxALL, 5);
  panelSizer->AddSpacer(15);
  panelSizer->Add(portLabel, 0, wxEXPAND | wxLEFT, 5);
  panelSizer->Add(portCtrl, 0, wxEXPAND | wxALL, 5);
  panelSizer->Add(gameList_ctrl, 1, wxEXPAND | wxALL, 5);

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
  portCtrl->SetLabel("label");
  portCtrl->SetValue(wxString::FromDouble(appConfig.port));

  fenrirServer->SetPort(appConfig.port);
  fenrirServer->SetIsoDirectory(appConfig.path);
}

void Simple::OnRun(wxCommandEvent &event)
{
  portCtrl->GetValue().ToLong(&appConfig.port, 10);
  SaveConfig(appConfig);

  fenrirServer->SetIsoDirectory(appConfig.path);
  fenrirServer->SetPort(appConfig.port);
  fenrirServer->Run();
}

void Simple::OnClose(wxCommandEvent &evt)
{
  Close(true);
}

void Simple::OnClose(wxCloseEvent &evt)
{
  Exit();
  Destroy();
}

void Simple::Exit()
{
  fenrirServer->StopServer();
  // fenrirServer->Kill();
}

void Simple::OnPathChanged(wxFileDirPickerEvent &evt)
{
  appConfig.path = evt.GetPath();
}

void Simple::SetIsoDirectory(wxString dir)
{
  // wxLogMessage("Hello world from wxWidgets!");
}

void Simple::OnServerEvent(wxCommandEvent &evt)
{
  // wxLogError("OnServerEvent");
  switch (evt.GetInt())
  {
  case FENRIR_SERVER_EVENT_TYPE_PENDING:
    runningStatus = FENRIR_SERVER_EVENT_TYPE_PENDING;
    run_btn->Enable(false);
    portCtrl->Enable(false);
    dirPickerCtrl->Enable(false);
    // close_btn->Enable(false);
    break;
  case FENRIR_SERVER_EVENT_TYPE_RUN:
    run_btn->Enable(false);
    portCtrl->Enable(false);
    dirPickerCtrl->Enable(false);
    // close_btn->Enable(false);
    runningStatus = FENRIR_SERVER_EVENT_TYPE_RUN;
    break;
  case FENRIR_SERVER_EVENT_TYPE_STOPPED:
    run_btn->Enable(false);
    portCtrl->Enable(false);
    dirPickerCtrl->Enable(false);
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
EVT_BUTTON(BUTTON_Close, Simple::OnClose)
EVT_BUTTON(BUTTON_Run, Simple::OnRun)
EVT_COMMAND(wxID_ANY, FENRIR_SERVER_EVENT, Simple::OnServerEvent)
// EVT_TEXT_ENTER(PORT_ID, Simple::OnPortChanged)
EVT_CLOSE(Simple::OnClose)
END_EVENT_TABLE()
