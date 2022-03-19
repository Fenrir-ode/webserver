#include <wx/wx.h>
#include "wx-widgets/app.h"

class MyApp : public wxApp
{
public:
  virtual bool OnInit()
  {
    Simple *simple = new Simple(_("Fenrir Server"));
    simple->Show(true);

    return true;
  }

  
};

IMPLEMENT_APP(MyApp)
