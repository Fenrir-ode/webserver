#pragma once

#include <wx/wx.h>
#include <wx/thread.h>

enum FENRIR_SERVER_EVENT_TYPE {
  FENRIR_SERVER_EVENT_TYPE_PENDING,
  FENRIR_SERVER_EVENT_TYPE_RUN,
  FENRIR_SERVER_EVENT_TYPE_STOPPED,
  FENRIR_SERVER_EVENT_TYPE_NOTIFY_GAME,
};

wxDECLARE_EVENT(FENRIR_SERVER_EVENT, wxCommandEvent);

class FenrirServer
{
private:
  wxFrame *parent;
  void PostEvent(int eventType);
public:
  FenrirServer(wxFrame *parent)
  {
    this->parent = parent;
  }

  bool Joinable();

  void Init();
  void Run();

  void SetRegionPatch(wxString region);
  void SetIsoDirectory(wxString directory);

  void StopServer();
};
