#include <stdio.h>
#include <string.h>
#include <ftw.h>
#include <libgen.h>
#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"
#include "httpd.h"
#include "scandir.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// =============================================================
// Menu
// =============================================================
#define SD_MENU_FILENAME_LENGTH 58
#define SD_DIR_FLAG_DIRECTORY (1 << 31)
#define MAX_ENTITY (3000)

// 64bytes - fenrir fmt
typedef struct
{
    uint16_t id;
    uint32_t flag;
    char filename[SD_MENU_FILENAME_LENGTH];
} __attribute__((packed)) sd_dir_entry_t;

typedef struct
{
    uint16_t id;
    uint32_t flag;
    char *path;
    char *name;
} __attribute__((packed)) fs_cache_t;

static sd_dir_entry_t sd_dir_entries[MAX_ENTITY];
static fs_cache_t fs_cache[MAX_ENTITY];

static uint32_t sd_dir_entries_count = 0;

static void _printdirentry(const char *name, fenrir_user_data_t *fud)
{
    uint32_t id = sd_dir_entries_count;
    size_t size = 0;
    time_t mtime = 0;
    uint32_t flags = 0;
    char path[MG_PATH_MAX];

    snprintf(path, sizeof(path), "%s%c%s", fud->image_path, '/', name);
    flags = mg_fs_posix.stat(path, &size, &mtime);

    strncpy(sd_dir_entries[id].filename, name, SD_MENU_FILENAME_LENGTH - 1);
    sd_dir_entries[id].id = __builtin_bswap16(id);
    sd_dir_entries[id].flag = __builtin_bswap32((flags & MG_FS_DIR) ? SD_DIR_FLAG_DIRECTORY : 0);

    fs_cache[id].name = strdup(name);
    fs_cache[id].path = strdup(path);
    fs_cache[id].id = id;
    fs_cache[id].flag = (flags & MG_FS_DIR) ? SD_DIR_FLAG_DIRECTORY : 0;

    sd_dir_entries_count++;

    log_info("add[%d]: %s %s", id, path, name);
}

static bool ext_is_handled(const char *name)
{
    char *exts[] = {
        ".iso", ".cue", ".chdr", ".gdi", ".nrg", NULL};

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

static int ftw_process(const char *name, const struct stat *sb, int flag)
{
    // Accept files only
    if (flag == 0)
    {

        if (ext_is_handled(name))
        {
            const char *entryname = basename(name);

            uint32_t id = sd_dir_entries_count;
            strncpy(sd_dir_entries[id].filename, entryname, SD_MENU_FILENAME_LENGTH - 1);
            sd_dir_entries[id].id = __builtin_bswap16(id);
            sd_dir_entries[id].flag = 0;

            fs_cache[id].name = strdup(entryname);
            fs_cache[id].path = strdup(name);
            fs_cache[id].id = id;
            fs_cache[id].flag = 0;

            sd_dir_entries_count++;

            log_info("add[%d]: %s %s", id, name, entryname);
        }
    }

    return 0;
}

int scandir_cbk(const char * fullpath, const char * name, int attr) {

            log_info("%s", fullpath);
            printf("%s\n", name);
}

static void tree_walk(fenrir_user_data_t *fenrir_user_data)
{
    //char path[MG_PATH_MAX];
    //snprintf(path, sizeof(path), "%s%c%s", fenrir_user_data->image_path, '/', name);
    ftw(fenrir_user_data->image_path, ftw_process, 2);

   //fscandir(fenrir_user_data->image_path, scandir_cbk);
}

void menu_http_handler_init(fenrir_user_data_t *fud)
{
    sd_dir_entries_count = 0;
    // mg_fs_posix.list(fud->image_path, printdirentry, fud);

    tree_walk(fud);
}

static uint32_t menu_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
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
                  sd_dir_entries_count,
                  (unsigned long)sd_dir_entries_count * sizeof(sd_dir_entry_t));
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

static uint32_t menu_read_data(fenrir_user_data_t *fenrir_user_data)
{

    if (fenrir_user_data->sd_dir_entries_offset < (sd_dir_entries_count * sizeof(sd_dir_entry_t)))
    {
        uint32_t max = (sd_dir_entries_count * sizeof(sd_dir_entry_t)) - fenrir_user_data->sd_dir_entries_offset;
        uint32_t sz = MIN(max, SECTOR_SIZE_2048);
        void *sd_dir_ptr = (void *)sd_dir_entries + fenrir_user_data->sd_dir_entries_offset;
        memcpy(fenrir_user_data->http_buffer, sd_dir_ptr, sz);

        //log_debug("sd_dir_ptr : %p, inc: %d", sd_dir_ptr, sz);
        //char *hex = mg_hexdump(sd_dir_ptr, sz);
        //log_debug("sync_header %s", hex);
        //free(hex);

        fenrir_user_data->sd_dir_entries_offset += sz;
        return 0;
    }
    else
    {
        return -1;
    }
}

static uint32_t menu_poll_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
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

int menu_get_filename_by_id(int id, char *filename)
{
    if (id <= sd_dir_entries_count)
    {
        strcpy(filename, fs_cache[id].path);
        return 0;
    }

    return -1;
}

static const httpd_route_t httpd_route_menu = {
    .uri = "/dir",
    .poll_handler = menu_poll_handler,
    .http_handler = menu_http_handler};

void menu_register_routes(struct mg_mgr *mgr)
{
    httpd_add_route(mgr, &httpd_route_menu);
}
