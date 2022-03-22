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

static uint32_t user_data_close_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)fn_data;
  cdfmt_close(fenrir_user_data);
  memset(fenrir_user_data, 0, sizeof(fenrir_user_data_t));
  return 0;
}

int server(server_config_t *server_config)
{
  struct mg_mgr mgr;
  struct mg_timer t1;

  // cache all files
  log_info("Cache cd image folder");
  menu_http_handler_init(server_config);

  // start http server
  log_info("start http server");
  mg_mgr_init(&mgr);
  httpd_init(&mgr, sizeof(fenrir_user_data_t), user_data_close_handler);

  menu_register_routes(&mgr);
  data_register_routes(&mgr);

  char url[512];
  snprintf(url, sizeof(url), "http://0.0.0.0:%d", server_config->port);

  mg_http_listen(&mgr, url, httpd_poll, NULL);

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