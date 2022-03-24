
#include <getopt.h>
#include <signal.h>
#include "mongoose.h"
#include "log.h"
#include "cdfmt.h"
#include "fenrir.h"
#include "httpd.h"
#include "server.h"
#include "data.http.h"
#include "menu.http.h"
#include "patch.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define FENRIR_USER_CACHE (10)

static fenrir_user_data_t fenrir_user_data_cache[FENRIR_USER_CACHE];

fenrir_user_data_t *find_toc_in_cache(int id)
{
  for (int i = 0; i < FENRIR_USER_CACHE; i++)
  {
    if (fenrir_user_data_cache[i].id == id)
    {
      return &fenrir_user_data_cache[i];
    }
  }
  return NULL;
}

fenrir_user_data_t *find_empty_slot()
{
  for (int i = 0; i < FENRIR_USER_CACHE; i++)
  {
    if (fenrir_user_data_cache[i].id == -1)
    {
      return &fenrir_user_data_cache[i];
    }
  }
  // todo add ts
  // no slot found... use the first...
  return &fenrir_user_data_cache[0];
}

static fenrir_user_data_t *find_toc(int id)
{
  // check in cache
  fenrir_user_data_t *ud = NULL;
  ud = find_toc_in_cache(id);
  if (ud)
  {
    return ud;
  }
  // toc not found in cache, open a new one

  // find an empty slot
  ud = find_empty_slot();

  if (menu_get_filename_by_id(id, ud->filename) == -1)
  {
    log_error("Nothing found for %d", id);
    return NULL;
  }

  // parse toc...
  if (cdfmt_parse_toc(ud->filename, ud, ud->toc_dto) == 0)
  {
    log_debug("parse toc: %d tracks found", ud->toc.numtrks);
    size_t sz = sizeof(raw_toc_dto_t) * (3 + ud->toc.numtrks);
    ud->id = id;
    return ud;
  }
  else
  {
    log_error("parse toc failed");
    return NULL;
  }
}

// =============================================================
// Toc
// =============================================================
static uint32_t toc_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  // fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;

  fenrir_transfert_t *request = (fenrir_transfert_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  char uri[64];

  int id = http_get_route_id(hm);
  fenrir_user_data_t *fenrir_user_data = find_toc(id);

  if (fenrir_user_data != NULL)
  {
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

static uint32_t data_close_handler(struct mg_connection *c, uintptr_t *data)
{
  per_request_data_t *per_request_data = (per_request_data_t *)data;
  free(per_request_data->data);
  return 0;
}

static uint32_t data_accept_handler(struct mg_connection *c, uintptr_t *data)
{
  per_request_data_t *per_request_data = (per_request_data_t *)data;
  per_request_data->data = (uintptr_t *)calloc(sizeof(fenrir_transfert_t), 1);
  return 0;
}

static const httpd_route_t httpd_route_toc = {
    .uri = "/toc_bin/*",
    .close_handler = data_close_handler,
    .accept_handler = data_accept_handler,
    .http_handler = toc_http_handler};

// =============================================================
// Data
// =============================================================
static uint32_t data_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_transfert_t *request = (fenrir_transfert_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  uint32_t range_start = 0;
  uint32_t range_end = 0;
  int id = http_get_route_id(hm);
  fenrir_user_data_t *fenrir_user_data = find_toc(id);

  if (fenrir_user_data == NULL)
  {
    mg_http_reply(c, 404, "", "Toc not valid or no file found");
    log_error("Toc not valid or no file found");
    return -1;
  }

  // if running with a proxy, and current file is "proxifiable"
  if (http_is_request_behind_proxy(hm) && fenrir_user_data->type == IMAGE_TYPE_MAME_LDR)
  {
    char httpRedirectLocation[512];
    snprintf(httpRedirectLocation, 512, "Location: http://localhost:80%s\r\n", fenrir_user_data->toc.tracks[0].filename);
    mg_http_reply(c, 301, httpRedirectLocation, "");
    log_trace("redirect...");
    return -1;
  }

  if (http_get_range_header(hm, &range_start, &range_end) == 0)
  {

    request->req_fad = range_start / SECTOR_SIZE;
    request->id = id;
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
  fenrir_transfert_t *request = (fenrir_transfert_t *)fn_data;
  fenrir_user_data_t *fenrir_user_data = find_toc(request->id);

  uint32_t err = cdfmt_read_data(fenrir_user_data, request->http_buffer, request->req_fad, SECTOR_SIZE);

  if (err == 0)
  {
    mg_http_write_chunk(c, request->http_buffer, SECTOR_SIZE);
    request->req_fad++;
    return 0;
  }
  else
  {
    // End transfert
    log_trace("End transfert...");
    c->is_draining = 1;
    mg_http_printf_chunk(c, "");
    return 1;
  }
}

static const httpd_route_t httpd_route_data = {
    .uri = "/data/*",
    .close_handler = data_close_handler,
    .accept_handler = data_accept_handler,
    .http_handler = data_http_handler,
    .poll_handler = data_poll_handler};

void data_register_routes(struct mg_mgr *mgr)
{
  httpd_add_route(mgr, &httpd_route_toc);
  httpd_add_route(mgr, &httpd_route_data);

  for (int i = 0; i < FENRIR_USER_CACHE; i++)
  {
    memset(&fenrir_user_data_cache[i], 0, sizeof(fenrir_user_data_t));
    fenrir_user_data_cache[i].id = -1;
  }
}
