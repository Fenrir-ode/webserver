
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

static fenrir_user_data_t *fenrir_user_data = NULL;

static int server_stopped = 0;

static void server_thread_func(int id)
{
    server(fenrir_user_data);
}

static int _run(uintptr_t ud)
{
    return server_stopped;
}

extern "C" server_events_t _server_events = {
    .ud = 0,
    .run = _run,
    .notify_add_game = NULL};

extern "C" server_events_t *server_events = &_server_events;

void FenrirServer::Init()
{
    fenrir_user_data = (fenrir_user_data_t *)malloc(sizeof(fenrir_user_data_t));
    if (fenrir_user_data == NULL)
    {
        wxLogError(_T("Failled to allocate fernrir user buffer"));
        exit(-1);
    }

    fenrir_user_data->http_buffer = (uint8_t *)malloc(4 * 2048);
    fenrir_user_data->patch_region = -1;

    server_stopped = 0;

    auto notifier = [](uintptr_t ud, char *gamename)
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

void FenrirServer::SetRegionPatch(wxString r)
{
    wxCharBuffer region=r.ToUTF8();
    fenrir_user_data->patch_region = get_image_region(region.data());
}

void FenrirServer::SetIsoDirectory(wxString directory)
{
    strcpy(fenrir_user_data->image_path, directory.mbc_str());
}

void FenrirServer::StopServer()
{
    PostEvent(FENRIR_SERVER_EVENT_TYPE_PENDING);

    server_stopped = 1;
    server_th.join();

    free(fenrir_user_data->http_buffer);
    free(fenrir_user_data);

    PostEvent(FENRIR_SERVER_EVENT_TYPE_STOPPED);
}

void FenrirServer::PostEvent(int eventType)
{
    wxCommandEvent event(FENRIR_SERVER_EVENT);
    event.SetInt(eventType);
    wxPostEvent(parent, event);
}
