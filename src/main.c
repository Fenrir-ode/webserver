// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved

#include <signal.h>
#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"
#include "httpd.h"

// =============================================================
// Menu
// =============================================================
#define SD_MENU_FILENAME_LENGTH 58
#define SD_DIR_FLAG_DIRECTORY (1 << 31)
#define MAX_ENTITY (3000)
#define ISOS_DIR ("/mnt/e/esp_saturn/fenrir_server/build/isos/")

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

static void printdirentry(const char *name, void *userdata)
{
  uint32_t id = sd_dir_entries_count;
  size_t size = 0;
  time_t mtime = 0;
  char path[MG_PATH_MAX], sz[64], mod[64];

  snprintf(path, sizeof(path), "%s%c%s", ISOS_DIR, '/', name);
  uint32_t flags = mg_fs_posix.stat(path, &size, &mtime);

  strncpy(sd_dir_entries[id].filename, name, SD_MENU_FILENAME_LENGTH);
  sd_dir_entries[id].id = __builtin_bswap16(id);
  sd_dir_entries[id].flag = __builtin_bswap32((flags & MG_FS_DIR) ? SD_DIR_FLAG_DIRECTORY : 0);

  fs_cache[id].name = strdup(name);
  fs_cache[id].path = strdup(path);
  fs_cache[id].id = id;
  fs_cache[id].flag = (flags & MG_FS_DIR) ? SD_DIR_FLAG_DIRECTORY : 0;

  sd_dir_entries_count++;

  log_info("add[%d]: %s %s", id, path, name);
}

static void menu_http_handler_init()
{
  sd_dir_entries_count = 0;
  mg_fs_posix.list("/mnt/e/esp_saturn/fenrir_server/build/isos/", printdirentry, NULL);
}

static uint32_t menu_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  struct mg_str *range = mg_http_get_header(hm, "range");

  if (mg_vcasecmp(&hm->method, "HEAD") == 0)
  {
    mg_printf(c,
              "HTTP/1.0 200 OK\r\n"
              "entry-count: %lu\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Type: application/octet-stream\r\n"
              "Content-Length: %lu\r\n\r\n",
              sd_dir_entries_count,
              (unsigned long)sd_dir_entries_count * sizeof(sd_dir_entry_t));
    c->is_draining = 1;
    return 0;
  }
  else
  {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    if (range && range->ptr)
    {
      if (sscanf((char *)range->ptr, "bytes=%d-%d", &range_start, &range_end) != EOF)
      {
      }
    }
    mg_printf(c,
              "HTTP/1.0 200 OK\r\n"
              "Entry-Count: %lu\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Type: application/octet-stream\r\n"

              "Transfer-Encoding: chunked\r\n\r\n",
              sd_dir_entries_count,
              (unsigned long)sd_dir_entries_count * sizeof(sd_dir_entry_t));

    uint32_t max = sd_dir_entries_count * sizeof(sd_dir_entry_t);
    uint32_t sz = max;
    mg_http_write_chunk(c, (void *)sd_dir_entries, sz);
    return 0;
  }
}

static const httpd_route_t httpd_route_menu = {
    .uri = "/dir",
    .http_handler = menu_http_handler};

// =============================================================
// Toc
// =============================================================
static uint32_t toc_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  char uri[64];
  int id = -1;

  // Check if a game is selected
  memcpy(uri, hm->uri.ptr, hm->uri.len);
  if (sscanf(uri, "/toc_bin/%d", &id) == 1)
  {
    log_debug("Asked to launch:%d", id);
    if (id <= sd_dir_entries_count)
    {
      strcpy(fenrir_user_data->filename, fs_cache[id].path);
      log_debug("Parse to launch:%s", fs_cache[id].name);
    }
  }

  if (cdfmt_parse_toc(fenrir_user_data->filename, fenrir_user_data, fenrir_user_data->toc_dto) == 0)
  {
    log_debug("parse toc: %d tracks found", fenrir_user_data->toc.numtrks);
    size_t sz = sizeof(raw_toc_dto_t) * (3 + fenrir_user_data->toc.numtrks);
    mg_printf(c,
              "HTTP/1.0 200 OK\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Type: application/octet-stream\r\n"
              "Content-Length: %lu\r\n\r\n",
              (unsigned long)sz);

    mg_send(c, fenrir_user_data->toc_dto, sz);
    return 0;
  }
  else
  {
    log_error("parse toc failed");
    mg_http_reply(c, 500, "", "%s", "Error\n");
    return -1;
  }
}

static const httpd_route_t httpd_route_toc = {
    .uri = "/toc_bin",
    .http_handler = toc_http_handler};

static const httpd_route_t httpd_route_toc_g = {
    .uri = "/toc_bin/*",
    .http_handler = toc_http_handler};

// =============================================================
// Data
// =============================================================
static uint32_t data_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  struct mg_str *range = mg_http_get_header(hm, "range");
  if (fenrir_user_data->toc.numtrks == 0)
  {
    mg_http_reply(c, 404, "", "Toc not valid or no file found");
    log_error("Toc not valid or no file found");
    return -1;
  }
  if (range)
  {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    if (sscanf((char *)range->ptr, "bytes=%d-%d", &range_start, &range_end) != EOF)
    {
      fenrir_user_data->req_fad = range_start / SECTOR_SIZE;
      // fenrir_user_data->req_size = SECTOR_SIZE * 200;

      mg_printf(c, "HTTP/1.1 206 OK\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Content-Type: application/octet-stream\r\n"
                   "Transfer-Encoding: chunked\r\n\r\n");

      log_trace("start chunked response");
      return 0;
    }
  }
  // fallback
  mg_http_reply(c, 404, "", "Oh no, header is not set...");
  log_error("Oh no, header is not set...");
  return -1;
}

static uint32_t data_poll_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;

  uint32_t err = cdfmt_read_data(fenrir_user_data, fenrir_user_data->http_buffer, fenrir_user_data->req_fad, SECTOR_SIZE);

  if (err == 0)
  {
    mg_http_write_chunk(c, fenrir_user_data->http_buffer, SECTOR_SIZE);
#if 0 //POSTMAN_DBG
    mg_http_printf_chunk(c, ""); // postman dbg
     c->is_draining = 1;
    return 1;
#endif

    fenrir_user_data->req_fad++;
    // fenrir_user_data->req_size -= SECTOR_SIZE;
    return 0;
  }
  else
  {
    log_error("End transfert...");
    c->is_draining = 1;
    // End transfert
    mg_http_printf_chunk(c, "");
    return 1;
  }
}

static const httpd_route_t httpd_route_data = {
    .uri = "/data",
    .http_handler = data_http_handler,
    .poll_handler = data_poll_handler};

static const httpd_route_t httpd_route_data_g = {
    .uri = "/data/*",
    .http_handler = data_http_handler,
    .poll_handler = data_poll_handler};
// =============================================================
// main
// =============================================================
static volatile sig_atomic_t sig_end = 1;

static void sig_handler(int unused)
{
  (void)unused;
  sig_end = 0;
}

int main(int argc, char *argv[])
{
  uint8_t *http_buffer = (uint8_t *)malloc(4 * 2048);
  fenrir_user_data_t fenrir_user_data = {
      .http_buffer = http_buffer};
  strcpy(fenrir_user_data.filename, argv[1]);
  struct mg_mgr mgr;
  struct mg_timer t1;

  if (argc != 2)
  {
    log_error("No file specified");
    return -1;
  }

  log_info("loading: %s", argv[1]);

  // prevent sigpipe signal - (mg_sock_send on disconnected sockets)
  // signal(SIGPIPE, SIG_IGN);

  // chd_get_header(NULL);
  //mg_log_set("4");
  menu_http_handler_init();
  mg_mgr_init(&mgr);
  httpd_init(&mgr);

  httpd_add_route(&mgr, &httpd_route_menu);
  httpd_add_route(&mgr, &httpd_route_toc);
  httpd_add_route(&mgr, &httpd_route_toc_g);
  httpd_add_route(&mgr, &httpd_route_data);
  httpd_add_route(&mgr, &httpd_route_data_g);

  mg_http_listen(&mgr, "http://0.0.0.0:3000", httpd_poll, (void *)&fenrir_user_data);

  while (sig_end)
    mg_mgr_poll(&mgr, 50);
  mg_timer_free(&t1);
  mg_mgr_free(&mgr);
  free(http_buffer);
  return 0;
}