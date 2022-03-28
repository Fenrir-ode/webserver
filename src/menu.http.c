#include <stdio.h>
#include <string.h>
#include "mongoose.h"
#include "cdfmt.h"
#include "fenrir.h"
#include "server.h"
#include "menu.http.h"
#include "log.h"
#include "httpd.h"
#include "scandir.h"

server_shared_t server_shared;

extern server_events_t *server_events;

#ifdef _MSC_VER
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// =============================================================
// Menu
// =============================================================
static bool ext_is_handled(const char *name)
{
    char *exts[] = {
        ".iso", ".cue", ".chdr", ".chd", ".gdi", ".nrg", NULL};

    char *file_ext = strchr(name, '.');
    if (file_ext)
        for (char **ext = exts; *ext != NULL; ext++)
        {
            if (strcasecmp(*ext, file_ext) == 0)
            {
                return true;
            }
        }

    return false;
}

int scandir_cbk(const char *fullpath, const char *entryname, uintptr_t ud)
{
    server_config_t *server_config = (server_config_t *)ud;
    if (ext_is_handled(fullpath))
    {
        uint32_t id = server_shared.sd_dir_entries_count;
        strncpy(server_shared.sd_dir_entries[id].filename, entryname, SD_MENU_FILENAME_LENGTH - 1);
        server_shared.sd_dir_entries[id].id = __builtin_bswap16(id);
        server_shared.sd_dir_entries[id].flag = 0;

        server_shared.fs_cache[id].name = strdup(entryname);
        server_shared.fs_cache[id].path = strdup(fullpath);
        server_shared.fs_cache[id].id = id;
        server_shared.fs_cache[id].flag = 0;

        server_shared.sd_dir_entries_count++;

        log_info("add[%d]: %s %s", id, fullpath, entryname);

        if (server_events->notify_add_game)
            server_events->notify_add_game(server_events->ud, fullpath);
    }

    return 0;
}

static void tree_walk(server_config_t *server_config)
{
    tree_scandir(server_config->image_path, scandir_cbk, (uintptr_t)server_config);
}

void menu_http_handler_init(server_config_t *server_config)
{
    memset(&server_shared, 0, sizeof(server_shared_t));
    server_shared.sd_dir_entries_count = 0;
    tree_walk(server_config);
}

static uint32_t menu_read_data(fenrir_transfert_t *request)
{

    if (request->entries_offset < (server_shared.sd_dir_entries_count * sizeof(sd_dir_entry_t)))
    {
        uint32_t max = (server_shared.sd_dir_entries_count * sizeof(sd_dir_entry_t)) - request->entries_offset;
        uint32_t sz = MIN(max, SECTOR_SIZE_2048);
        uintptr_t sd_dir_ptr = (uintptr_t)server_shared.sd_dir_entries + request->entries_offset;
        memcpy(request->http_buffer, (void *)sd_dir_ptr, sz);
        request->entries_offset += sz;
        return 0;
    }
    else
    {
        return -1;
    }
}

int menu_get_filename_by_id(uint32_t id, char *filename)
{
    if (id <= server_shared.sd_dir_entries_count)
    {
        strcpy(filename, server_shared.fs_cache[id].path);
        return 0;
    }

    return -1;
}

uint32_t menu_close_handler(struct mg_connection *c, uintptr_t data) {    
    per_request_data_t *per_request_data = (per_request_data_t *)data;
    free(per_request_data->data);
    return 0;
}

uint32_t menu_accept_handler(struct mg_connection *c, uintptr_t data)
{
    per_request_data_t *per_request_data = (per_request_data_t *)data;
    per_request_data->data = (uintptr_t *)calloc(sizeof(fenrir_transfert_t), 1);
    return 0;
}

uint32_t menu_poll_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    fenrir_transfert_t *request = (fenrir_transfert_t *)fn_data;

    if (menu_read_data(request) == 0)
    {
        mg_http_write_chunk(c, request->http_buffer, SECTOR_SIZE_2048);
        return 0;
    }
    else
    {
        log_trace("End transfert...");
        c->is_draining = 1;
        // End transfert
        mg_http_printf_chunk(c, "");
        return 1;
    }
}

uint32_t menu_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    fenrir_transfert_t *request = (fenrir_transfert_t *)fn_data;
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    if (mg_vcasecmp(&hm->method, "HEAD") == 0)
    {
        mg_printf(c,
                  "HTTP/1.0 200 OK\r\n"
                  "entry-count: %lu\r\n"
                  "Cache-Control: no-cache\r\n"
                  "Content-Type: application/octet-stream\r\n"
                  "Content-Length: %lu\r\n"
                  "\r\n",
                  server_shared.sd_dir_entries_count,
                  (unsigned long)server_shared.sd_dir_entries_count * sizeof(sd_dir_entry_t));
        c->is_draining = 1;
        return -1;
    }
    else
    {
        uint32_t range_start = 0;
        uint32_t range_end = 0;
        http_get_range_header(hm, &range_start, &range_end);

        mg_printf(c, "HTTP/1.1 206 Partial Content\r\n"
                     "Accept-Ranges: bytes\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Content-Type: application/octet-stream\r\n"
                     "Transfer-Encoding: chunked\r\n\r\n");

        request->entries_offset = range_start;
        log_trace("Stream dir start at : %d", request->entries_offset);
        return 0;
    }
}

static const httpd_route_t httpd_route_menu = {
    .uri = "/dir",
    .accept_handler = menu_accept_handler,
    .close_handler = menu_close_handler,
    .poll_handler = menu_poll_handler,
    .http_handler = menu_http_handler};

void menu_register_routes(struct mg_mgr *mgr)
{
    httpd_add_route(mgr, &httpd_route_menu);
}
