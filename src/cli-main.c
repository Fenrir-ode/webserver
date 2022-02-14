#include <getopt.h>
#include <signal.h>
#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"
#include "httpd.h"
#include "menu.http.h"
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
  printf("  --verbose\t"
         "Display more information.\n");
}

int server(fenrir_user_data_t *fenrir_user_data, int (*cbk)(void *), void *ud);

static void noop() {}
int main(int argc, char *argv[])
{
  struct mg_mgr mgr;
  struct mg_timer t1;
  uint8_t *http_buffer = NULL;
  int c;
  int option_index = 0;
  int option_valid = 0;

  // setup buffer
  http_buffer = (uint8_t *)malloc(4 * 2048);
  fenrir_user_data_t fenrir_user_data = {
      .http_buffer = http_buffer};

  // Parse options
  static int verbose_flag;

  static const struct option long_options[] = {
      {"verbose", no_argument, &verbose_flag, 1},
      {"dir", required_argument, NULL, 'd'},
      {0, 0, 0, 0}};

  while (1)
  {
    c = getopt_long(argc, argv, "d:", long_options, &option_index);
    if (c == -1)
      break;

    switch (c)
    {
    case 0:
      break;
    case 'd':
      option_valid++;
      strcpy(fenrir_user_data.image_path, optarg);
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

  if (verbose_flag)
  {
    log_set_level(LOG_TRACE);
  }
  else
  {
    log_set_level(LOG_ERROR);
  }

  server(&fenrir_user_data, noop, NULL);

  free(http_buffer);
  return 0;
}