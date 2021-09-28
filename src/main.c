// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"

typedef struct
{
  // game toc
  cdrom_toc_t toc;
  // ?
  raw_toc_dto_t toc_dto[CD_MAX_TRACKS+3]; // 99 + 3 Metadata track
} fenrir_user_data_t;

#define DEFAULT_FNAME ("/mnt/g/esp_saturn/fenrir_server/build/isos/Burning Rangers (US)/Burning Rangers (US).cue")

// /toc
static void toc(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  if (mame_parse_toc(DEFAULT_FNAME, &fenrir_user_data->toc, fenrir_user_data->toc_dto) == 0)
  {
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
    mg_http_reply(c, 500, "", "%s", "Error\n");
  }
}

// /data
static void data(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  struct mg_http_message *hm = (struct mg_http_message *)ev_data;
  struct mg_str *range = mg_http_get_header(hm, "range");

  mg_http_reply(c, 206, "", "Oh no, header is not set...");
  // mg_http_write_chunk(c, buf, len);
}

// HTTP request handler function. It implements the following endpoints:
//   /api/video1 - hangs forever, returns MJPEG video stream
//   all other URI - serves web_root/ directory
static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  if (ev == MG_EV_HTTP_MSG)
  {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_http_match_uri(hm, "/toc"))
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

/*
// The image stream is simulated by sending MJPEG frames specified by the
// "files" array of file names.
static void broadcast_mjpeg_frame(struct mg_mgr *mgr)
{
  const char *files[] = {"images/1.jpg", "images/2.jpg", "images/3.jpg",
                         "images/4.jpg", "images/5.jpg", "images/6.jpg"};
  size_t nfiles = sizeof(files) / sizeof(files[0]);
  static size_t i;
  const char *path = files[i++ % nfiles];
  size_t size = 0;
  char *data = mg_file_read(path, &size); // Read next file
  struct mg_connection *c;
  for (c = mgr->conns; c != NULL; c = c->next)
  {
    if (c->label[0] != 'S')
      continue; // Skip non-stream connections
    if (data == NULL || size == 0)
      continue; // Skip on file read error
    mg_printf(c,
              "--foo\r\nContent-Type: image/jpeg\r\n"
              "Content-Length: %lu\r\n\r\n",
              (unsigned long)size);
    mg_send(c, data, size);
    mg_send(c, "\r\n", 2);
  }
  free(data);
}

static void timer_callback(void *arg)
{
  broadcast_mjpeg_frame(arg);
}
*/

int main(void)
{
  fenrir_user_data_t fenrir_user_data = {

  };
  struct mg_mgr mgr;
  struct mg_timer t1;

  // chd_get_header(NULL);

  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "http://localhost:8000", cb, (void *)&fenrir_user_data);
  // mg_timer_init(&t1, 500, MG_TIMER_REPEAT, timer_callback, &mgr);
  for (;;)
    mg_mgr_poll(&mgr, 50);
  mg_timer_free(&t1);
  mg_mgr_free(&mgr);

  return 0;
}