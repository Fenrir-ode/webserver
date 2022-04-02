#pragma once
#include <wx/wx.h>

#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/filepicker.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include "server-intf.h"

typedef struct
{
    long port;
    wxString path;
} AppConfig;

class Simple : public wxFrame
{
    AppConfig appConfig;
    wxListView *gameList_ctrl;
    wxDirPickerCtrl *dirPickerCtrl;
    wxTextCtrl *portCtrl;

    wxButton *run_btn;
    wxButton *close_btn;

public:
    Simple(const wxString &title);

    void OnPathChanged(wxFileDirPickerEvent &evt);
    void OnPortChanged(wxCloseEvent &evt);
    void OnClose(wxCommandEvent &evt);
    void OnClose(wxCloseEvent &evt);
    void OnRun(wxCommandEvent &evt);
    void OnServerEvent(wxCommandEvent &evt);
    void Exit();

    DECLARE_EVENT_TABLE()

private:
    FenrirServer *fenrirServer;
    int runningStatus;
    void SetIsoDirectory(wxString dir);
    void SetPort(wxString port);
};

enum
{
    BUTTON_DirDialog = wxID_HIGHEST + 1,
    DIR_PICKER_ID,
    COMBO_REGION_ID,
    PORT_ID,
    BUTTON_Close,
    BUTTON_Run,

};
