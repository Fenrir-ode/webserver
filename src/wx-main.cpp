#include <wx/wx.h>
#include "wx-widgets/app.h"

class MyApp : public wxApp
{
  Simple *simple;
public:
  virtual bool OnInit()
  {
    simple = new Simple(_("Fenrir Server"));
    simple->Show(true);

    return true;
  }

  
};

IMPLEMENT_APP(MyApp)
