#include <getopt.h>
#include <signal.h>
#include "mongoose.h"
#include "log.h"
// #include "libchdr/chd.h"
#include "cdfmt.h"
#include "fenrir.h"
#include "httpd.h"
#include "menu.http.h"
#include "server.h"

#include <wx/wx.h>
#include "wx-widgets/app.h"

class MyApp : public wxApp
{
public:
  virtual bool OnInit()
  {
    Simple *simple = new Simple(wxT("Fenrir Server"));
    simple->Show(true);

    return true;
  }

  
};

IMPLEMENT_APP(MyApp)

int __main(int argc, char *argv[])
{
  struct mg_mgr mgr;
  struct mg_timer t1;
  uint8_t *http_buffer = NULL;
  int c;
  int option_index = 0;
  int option_valid = 0;
  static int verbose_flag = 0;

  // setup buffer
  http_buffer = (uint8_t *)malloc(4 * 2048);
  if (http_buffer == NULL)
  {
    log_error("Failled to allocate http buffer");
    return -1;
  }
  fenrir_user_data_t *fenrir_user_data = (fenrir_user_data_t *)malloc(sizeof(fenrir_user_data_t));
  if (fenrir_user_data == NULL)
  {
    log_error("Failled to allocate fernrir user buffer");
    return -1;
  }
  fenrir_user_data->http_buffer = http_buffer;
  fenrir_user_data->patch_region = -1;

  return 0;
}