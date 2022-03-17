#pragma once
#include <wx/wx.h>

#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/filepicker.h>

class Simple : public wxFrame
{
public:
    Simple(const wxString &title);
    wxButton *btnDirDialog;
    wxComboBox *comboBox1;
    wxTextCtrl *isoDirectoryText_ctrl;
    DECLARE_EVENT_TABLE()

    void OnDialog(wxCommandEvent &event);

    void OnPathChanged(wxFileDirPickerEvent &evt);
    void OnComboBox(wxCommandEvent &evt);

    void OnQuit(wxCommandEvent &evt);

private:
    void SetIsoDirectory(wxString dir);
    void SetRegion(int r);
};

enum
{
    BUTTON_DirDialog = wxID_HIGHEST + 1,
    DIR_PICKER_ID,
    COMBO_REGION_ID,
    BUTTON_Close,

};
