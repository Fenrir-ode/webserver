#include <getopt.h>
#include <signal.h>
#include "mongoose.h"
#include "log.h"
#include "cdfmt.h"
#include "fenrir.h"
#include "server.h"
#include "httpd.h"
#include "menu.http.h"
#include "data.http.h"
#include "patch.h"

extern server_events_t *server_events;

// =============================================================
// main
// =============================================================
static volatile sig_atomic_t sig_end = 1;

static void sig_handler(int unused)
{
  (void)unused;
  sig_end = 0;
}

#if 0
void __() {
    // setup buffer
  http_buffer = (uint8_t *)malloc(4 * 2048);
  if (http_buffer == NULL)
  {
    log_error("Failled to allocate http buffer");
    return -1;
  }
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)calloc(sizeof(fenrir_user_data_t), 1);
  if (fenrir_user_data == NULL)
  {
    log_error("Failled to allocate fernrir user buffer");
    return -1;
  }
  fenrir_user_data->http_buffer = http_buffer;
  fenrir_user_data->patch_region = -1;

  
  free(http_buffer);
  free(fenrir_user_data);
}
#endif

// Pipe event handler
static void pcb(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  if (ev == MG_EV_READ)
  {
    struct mg_connection *t;
    for (t = c->mgr->conns; t != NULL; t = t->next)
    {
      if (t->label[0] != 'W')
        continue; // Ignore un-marked connections
      mg_http_reply(t, 200, "Host: foo.com\r\n", "%.*s\n", c->recv.len,
                    c->recv.buf); // Respond!
      t->label[0] = 0;            // Clear mark
    }
  }
}

int server(server_config_t *server_config)
{
  struct mg_connection *pipe;
  struct mg_mgr mgr;
  struct mg_timer t1;

  // cache all files
  log_info("Cache cd image folder");
  menu_http_handler_init(server_config);

  // start http server
  log_info("start http server");
  mg_mgr_init(&mgr);
  httpd_init(&mgr, sizeof(fenrir_user_data_t));

  menu_register_routes(&mgr);
  data_register_routes(&mgr);

 // pipe = mg_mkpipe(&mgr, httpd_poll, server_config);
  mg_http_listen(&mgr, "http://0.0.0.0:3000", httpd_poll, NULL);

  while (sig_end)
  {
    mg_mgr_poll(&mgr, 50);
    if (server_events->run)
    {
      if (server_events->run(server_events->ud) != 0)
      {
        break;
      }
    }
  }

  // exit
  mg_timer_free(&t1);
  mg_mgr_free(&mgr);
  return 0;
}