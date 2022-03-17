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
