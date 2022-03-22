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

static int noop() { return 0; }

static server_events_t _server_events = {
    .run = noop,
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
  printf("  -r --region\t\t"
         " (J, T, U, B, K, A, E, L)\tSet console region.\n");
  printf("  -p --port\t\t"
         " port (80 by default)\n");
  printf("  --verbose\t"
         "Display more information.\n");
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

  // Parse options
  server_config_t server_config = {
      .port = 80};

  static const struct option long_options[] = {
      {"verbose", no_argument, &verbose_flag, 1},
      {"dir", required_argument, NULL, 'd'},
      {"region", required_argument, NULL, 'r'},
      {"port", required_argument, NULL, 'p'},
      {0, 0, 0, 0}};

  while (1)
  {
    c = getopt_long(argc, argv, "r:d:p:", long_options, &option_index);
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
    case 'r':
      // server_config.patch_region = get_image_region(optarg);
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

  server(&server_config);

  if (verbose_flag && flog)
    fclose(flog);

  return 0;
}