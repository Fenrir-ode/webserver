#include <stdio.h>
#include <string.h>
#include "mongoose.h"
#include "server.h"
#include "cdfmt.h"
#include "fenrir.h"
//#include <ftw.h>
//#include <libgen.h>
#include "log.h"
// #include "libchdr/chd.h"
#include "httpd.h"
#include "scandir.h"

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
        ".iso", ".cue", ".chdr",".chd", ".gdi", ".nrg", NULL};

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
    fenrir_user_data_t *fud = (fenrir_user_data_t *)ud;
    if (ext_is_handled(fullpath))
    {
        uint32_t id = fud->sd_dir_entries_count;
        strncpy(fud->sd_dir_entries[id].filename, entryname, SD_MENU_FILENAME_LENGTH - 1);
        fud->sd_dir_entries[id].id = __builtin_bswap16(id);
        fud->sd_dir_entries[id].flag = 0;

        fud->fs_cache[id].name = strdup(entryname);
        fud->fs_cache[id].path = strdup(fullpath);
        fud->fs_cache[id].id = id;
        fud->fs_cache[id].flag = 0;

        fud->sd_dir_entries_count++;

        log_info("add[%d]: %s %s", id, fullpath, entryname);

        if (server_events->notify_add_game)
            server_events->notify_add_game(server_events->ud, fullpath);
    }

    return 0;
}

static void tree_walk(fenrir_user_data_t *fenrir_user_data)
{
    tree_scandir(fenrir_user_data->image_path, scandir_cbk, (uintptr_t)fenrir_user_data);
}

void menu_http_handler_init(fenrir_user_data_t *fud)
{
    fud->sd_dir_entries_count = 0;
    fud->sd_dir_entries_offset = 0;
    tree_walk(fud);
}

static uint32_t menu_read_data(fenrir_user_data_t *fenrir_user_data)
{

    if (fenrir_user_data->sd_dir_entries_offset < (fenrir_user_data->sd_dir_entries_count * sizeof(sd_dir_entry_t)))
    {
        uint32_t max = (fenrir_user_data->sd_dir_entries_count * sizeof(sd_dir_entry_t)) - fenrir_user_data->sd_dir_entries_offset;
        uint32_t sz = MIN(max, SECTOR_SIZE_2048);
        uintptr_t sd_dir_ptr = (uintptr_t)fenrir_user_data->sd_dir_entries + fenrir_user_data->sd_dir_entries_offset;
        memcpy(fenrir_user_data->http_buffer, (void *)sd_dir_ptr, sz);

        fenrir_user_data->sd_dir_entries_offset += sz;
        return 0;
    }
    else
    {
        return -1;
    }
}

int menu_get_filename_by_id(fenrir_user_data_t *fenrir_user_data, uint32_t id, char *filename)
{
    if (id <= fenrir_user_data->sd_dir_entries_count)
    {
        strcpy(filename, fenrir_user_data->fs_cache[id].path);
        return 0;
    }

    return -1;
}

uint32_t menu_poll_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;

    if (menu_read_data(fenrir_user_data) == 0)
    {
        mg_http_write_chunk(c, fenrir_user_data->http_buffer, SECTOR_SIZE_2048);
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
    fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
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
                  fenrir_user_data->sd_dir_entries_count,
                  (unsigned long)fenrir_user_data->sd_dir_entries_count * sizeof(sd_dir_entry_t));
        c->is_draining = 1;
        return -1;
    }
    else
    {
        uint32_t range_start = 0;
        uint32_t range_end = 0;
        http_get_range_header(hm, &range_start, &range_end);

        mg_printf(c, "HTTP/1.1 206 OK\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Content-Type: application/octet-stream\r\n"
                     "Transfer-Encoding: chunked\r\n\r\n");

        fenrir_user_data->sd_dir_entries_offset = range_start;
        log_trace("Stream dir start at : %d", fenrir_user_data->sd_dir_entries_offset);
        return 0;
    }
}

static const httpd_route_t httpd_route_menu = {
    .uri = "/dir",
    .poll_handler = menu_poll_handler,
    .http_handler = menu_http_handler};

void menu_register_routes(struct mg_mgr *mgr)
{
    httpd_add_route(mgr, &httpd_route_menu);
}
