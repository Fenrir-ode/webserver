#include <getopt.h>
#include <signal.h>
#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"
#include "fenrir.h"
#include "server.h"
#include "httpd.h"
#include "menu.http.h"

static int noop(void *arg) { return 0; }
static int started(void *);

static server_events_t _server_events = {
    .run = noop,
    .started = noop,
    .notify_add_game = NULL,
    .ud = 0};

server_events_t *server_events = &_server_events;

// =============================================================
// main
// =============================================================
static volatile sig_atomic_t sig_end = 1;

static void sig_handler(int unused)
{
  (void)unused;
  sig_end = 0;
}

void usage(char *progname)
{
  const char *basename = strrchr(progname, '/');
  basename = basename ? basename + 1 : progname;

  printf("usage: %s [OPTION]\n", basename);
  printf("  -d, --dir\t\t"
         "CD Image directory to serve.\n");
  printf("  -p --port\t\t"
         " port (80 by default)\n");
  printf("  --verbose\t"
         "Display more information.\n");
  printf("  --link\t"
         "Detect and setup local Fenrir to access this server.\n");
}

int started(void *arg)
{
    mdns_setup_fenrir();
}

int main(int argc, char *argv[])
{
  struct mg_mgr mgr;
  struct mg_timer t1;
  uint8_t *http_buffer = NULL;
  int c;
  int option_index = 0;
  int option_valid = 0;
  static int verbose_flag = 0;
  static int proxy_flag = 0;
  static int autolink = 0;

  // Parse options
  server_config_t server_config = {
      .port = 80};

  static const struct option long_options[] = {
      {"verbose", no_argument, &verbose_flag, 1},
      {"dir", required_argument, NULL, 'd'},
      {"port", required_argument, NULL, 'p'},
      {"link", no_argument, &autolink, 1},
      {0, 0, 0, 0}};

  while (1)
  {
    c = getopt_long(argc, argv, "d:p:", long_options, &option_index);
    if (c == -1)
      break;

    switch (c)
    {
    case 0:
      break;
    case 'p':
      server_config.port = atoi(optarg);
      break;
    case 'd':
      option_valid++;
      strcpy(server_config.image_path, optarg);
      break;
    default:
      log_error("");
      abort();
    }
  }

  if (option_valid == 0)
  {
    usage(argv[0]);
    exit(-1);
  }

  FILE *flog = NULL;

  if (verbose_flag)
  {
    flog = fopen("fenrir.log", "wb");
    log_add_fp(flog, LOG_DEBUG);
    log_set_level(LOG_TRACE);
  }
  else
  {
    log_set_level(LOG_ERROR);
  }

  if (autolink)
  {
    server_events->started = started;
  }

  server(&server_config);

  if (verbose_flag && flog)
    fclose(flog);

  return 0;
}