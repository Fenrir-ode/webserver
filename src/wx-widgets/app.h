#pragma once
#include <wx/wx.h>

#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/filepicker.h>
#include <wx/listctrl.h>
#include "server-intf.h"

class Simple : public wxFrame
{
    wxTextCtrl *isoDirectoryText_ctrl;
    wxListCtrl *gameList_ctrl;

    wxButton *run_btn;
    wxButton *close_btn;
public:
    Simple(const wxString &title);

    void OnPathChanged(wxFileDirPickerEvent &evt);
    void OnComboBox(wxCommandEvent &evt);
    void OnClose(wxCommandEvent &evt);
    void OnRun(wxCommandEvent &evt);
    void OnServerEvent(wxCommandEvent &evt);

    DECLARE_EVENT_TABLE()

private:
    FenrirServer *fenrirServer;
    int runningStatus;
    void SetIsoDirectory(wxString dir);
    void SetRegion(int r);
};

enum
{
    BUTTON_DirDialog = wxID_HIGHEST + 1,
    DIR_PICKER_ID,
    COMBO_REGION_ID,
    BUTTON_Close,
    BUTTON_Run,

};
