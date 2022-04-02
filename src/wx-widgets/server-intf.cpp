
#include <wx/wx.h>
#include <thread>
#include <mutex>
#include <functional>
#include "server-intf.h"

#include "../server.h"
#include "../log.h"

wxDEFINE_EVENT(FENRIR_SERVER_EVENT, wxCommandEvent);

static std::mutex server_mtx;
static std::thread server_th;

server_config_t server_config;

static int server_stopped = 0;

static void server_thread_func(int id)
{
    if (server(&server_config) == -1) {
        wxMessageBox( wxT("The server can not be launched. Check your port setting."), wxT("Error"), wxICON_ERROR);
        exit(-1);
    }
}

static int _run(uintptr_t ud)
{
    return server_stopped;
}

server_events_t _server_events = {
    .ud = 0,
    .run = _run,
    .notify_add_game = NULL};

server_events_t *server_events = &_server_events;

void FenrirServer::Init()
{
    server_stopped = 0;

    auto notifier = [](uintptr_t ud, const char *gamename)
    {
        FenrirServer *self = (FenrirServer *)(ud);
        server_mtx.lock();
        wxCommandEvent event(FENRIR_SERVER_EVENT);
        event.SetInt(FENRIR_SERVER_EVENT_TYPE_NOTIFY_GAME);
        event.SetString(gamename);
        wxPostEvent(self->parent, event);
        server_mtx.unlock();
    };

    _server_events.ud = (uintptr_t)this;
    _server_events.notify_add_game = notifier;
}

void FenrirServer::Run()
{
    log_set_level(LOG_TRACE);
    PostEvent(FENRIR_SERVER_EVENT_TYPE_PENDING);
    server_th = std::thread(server_thread_func, 0);
    PostEvent(FENRIR_SERVER_EVENT_TYPE_RUN);
}

bool FenrirServer::Joinable()
{
    return server_th.joinable();
}

void FenrirServer::SetPort(int port)
{
    server_config.port = port;
}

void FenrirServer::SetIsoDirectory(wxString directory)
{
    strcpy(server_config.image_path, directory.mbc_str());
}

void FenrirServer::StopServer()
{
    PostEvent(FENRIR_SERVER_EVENT_TYPE_PENDING);

    server_stopped = 1;
    if (server_th.joinable())
        server_th.join();

    PostEvent(FENRIR_SERVER_EVENT_TYPE_STOPPED);
}

void FenrirServer::PostEvent(int eventType)
{
    wxCommandEvent event(FENRIR_SERVER_EVENT);
    event.SetInt(eventType);
    wxPostEvent(parent, event);
}
