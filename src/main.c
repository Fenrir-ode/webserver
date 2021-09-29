// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"

#define DEFAULT_FNAME ("/mnt/g/esp_saturn/fenrir_server/build/isos/Burning Rangers (US)/Burning Rangers (US).cue")
#define SECTOR_SIZE (2352)

enum HTTP_POLL_STATUS
{
  HTTP_POLL_STATUS_UNKN = 0,
  HTTP_POLL_STATUS_DATA
};

static int http_poll_status;

// /toc_bin
static void toc(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;

  if (mame_parse_toc(DEFAULT_FNAME, &fenrir_user_data->toc, fenrir_user_data->toc_dto) == 0)
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
  }
  else
  {
    log_error("parse toc failed");
    mg_http_reply(c, 500, "", "%s", "Error\n");
  }
}

// /data
static void data(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  struct mg_str *range = mg_http_get_header(hm, "range");
  if (fenrir_user_data->toc.numtrks == 0)
  {
    mg_http_reply(c, 404, "", "Toc not valid or no file found");
    log_error("Toc not valid or no file found");
    return;
  }
  if (range)
  {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    if (sscanf((char *)range->ptr, "bytes=%d-%d", &range_start, &range_end) != EOF)
    {
      fenrir_user_data->req_fad = range_start / SECTOR_SIZE;
      fenrir_user_data->req_size = SECTOR_SIZE * 200;

      mg_printf(c, "HTTP/1.1 200 OK\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Content-Type: application/octet-stream\r\n"
                   "Transfer-Encoding: chunked\r\n\r\n");

      http_poll_status = HTTP_POLL_STATUS_DATA;

      log_trace("start chunked response");
      return;
    }
  }
  // fallback
  mg_http_reply(c, 404, "", "Oh no, header is not set...");
  log_error("Oh no, header is not set...");
}

static void poll_data_cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;

  read_data(fenrir_user_data, fenrir_user_data->http_buffer, fenrir_user_data->req_fad, SECTOR_SIZE);
  mg_http_write_chunk(c, fenrir_user_data->http_buffer, SECTOR_SIZE);

  //char *hex = mg_hexdump(fenrir_user_data->http_buffer, 32);
  //log_debug("%s", hex);
  //free(hex);

  fenrir_user_data->req_fad++;
  fenrir_user_data->req_size -= SECTOR_SIZE;

  // End transfert
  if (fenrir_user_data->req_size == 0)
  {
    mg_printf(c, "0\r\n"
                 "\r\n");

    http_poll_status = HTTP_POLL_STATUS_UNKN;
  }
}

static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  if (ev == MG_EV_CLOSE)
  {
    http_poll_status = HTTP_POLL_STATUS_UNKN;
  }
  else if ((ev == MG_EV_POLL || ev == MG_EV_WRITE) && c->is_writable)
  {
    switch (http_poll_status)
    {
    case HTTP_POLL_STATUS_DATA:
      poll_data_cb(c, ev, ev_data, fn_data);
      break;

    default:
      break;
    }
  }
  else if (ev == MG_EV_HTTP_MSG)
  {
    http_poll_status = HTTP_POLL_STATUS_UNKN;
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_http_match_uri(hm, "/toc_bin"))
    {
      toc(c, ev, ev_data, fn_data);
    }
    else if (mg_http_match_uri(hm, "/data"))
    {
      data(c, ev, ev_data, fn_data);
    }
    /*
    else if (mg_http_match_uri(hm, "/api/video1"))
    {
      c->label[0] = 'S'; // Mark that connection as live streamer
      mg_printf(
          c, "%s",
          "HTTP/1.0 200 OK\r\n"
          "Cache-Control: no-cache\r\n"
          "Pragma: no-cache\r\nExpires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
          "Content-Type: multipart/x-mixed-replace; boundary=--foo\r\n\r\n");
    }
    */
    else
    {
      struct mg_http_serve_opts opts = {.root_dir = "isos"};
      mg_http_serve_dir(c, ev_data, &opts);
    }
  }
}

int main(void)
{
  uint8_t *http_buffer = (uint8_t *)malloc(SECTOR_SIZE);
  fenrir_user_data_t fenrir_user_data = {
      .http_buffer = http_buffer};
  struct mg_mgr mgr;
  struct mg_timer t1;

  // chd_get_header(NULL);

  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "http://localhost:8000", cb, (void *)&fenrir_user_data);
  //mg_timer_init(&t1, 1, MG_TIMER_REPEAT, chunk_reponse, &mgr);
  for (;;)
    mg_mgr_poll(&mgr, 1);
  mg_timer_free(&t1);
  mg_mgr_free(&mgr);
  free(http_buffer);
  return 0;
}