#include <getopt.h>
#include <signal.h>
#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"
#include "httpd.h"
#include "menu.http.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

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
    if (menu_get_filename_by_id(id, fenrir_user_data->filename) == -1)
    {
      log_error("Nothing found for %d", id);
    }
    else
    {
      log_trace("Found: %s", fenrir_user_data->filename);
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
    .uri = "/toc_bin/*",
    .http_handler = toc_http_handler};

// =============================================================
// Data
// =============================================================
static uint32_t data_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  uint32_t range_start = 0;
  uint32_t range_end = 0;
  if (fenrir_user_data->toc.numtrks == 0)
  {
    mg_http_reply(c, 404, "", "Toc not valid or no file found");
    log_error("Toc not valid or no file found");
    return -1;
  }

  if (http_get_range_header(hm, &range_start, &range_end) == 0)
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
#if 0 // POSTMAN_DBG
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
    log_trace("End transfert...");
    c->is_draining = 1;
    // End transfert
    mg_http_printf_chunk(c, "");
    return 1;
  }
}

static const httpd_route_t httpd_route_data = {
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

int server(fenrir_user_data_t *fenrir_user_data, int (*cbk)(void *), void *ud)
{
  struct mg_mgr mgr;
  struct mg_timer t1;

  // cache all files
  log_info("Cache cd image folder");
  menu_http_handler_init(fenrir_user_data);

  // start http server
  log_info("start http server");
  mg_mgr_init(&mgr);
  httpd_init(&mgr);

  httpd_add_route(&mgr, &httpd_route_toc);
  httpd_add_route(&mgr, &httpd_route_data);
  menu_register_routes(&mgr);

  mg_http_listen(&mgr, "http://0.0.0.0:3000", httpd_poll, (void *)fenrir_user_data);

  while (sig_end)
  {
    mg_mgr_poll(&mgr, 50);
    if (cbk)
      cbk(ud);
  }

  // exit
  mg_timer_free(&t1);
  mg_mgr_free(&mgr);
  return 0;
}