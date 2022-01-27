#include "mongoose.h"
#include "log.h"
#include "httpd.h"

static httpd_route_t *httpd_route[MAX_HTTPD_ROUTE];
static uint32_t (*poll_handler)(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

uint32_t httpd_init(struct mg_mgr *mgr)
{
    for (int i = 0; i < MAX_HTTPD_ROUTE; i++)
    {
        httpd_route[i] = NULL;
    }
    poll_handler = NULL;
}

uint32_t httpd_add_route(struct mg_mgr *mgr, const httpd_route_t *route)
{
    for (int i = 0; i < MAX_HTTPD_ROUTE; i++)
    {
        if (httpd_route[i] == NULL)
        {
            httpd_route[i] = (httpd_route_t *)malloc(sizeof(httpd_route_t));
            httpd_route[i]->http_handler = route->http_handler;
            httpd_route[i]->poll_handler = route->poll_handler;
            httpd_route[i]->route_id = route->route_id;
            httpd_route[i]->uri = strdup(route->uri);
            return 0;
        }
    }

    // no more route available
    return -1;
}

void httpd_poll(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    if (ev == MG_EV_CLOSE)
    {
        poll_handler = NULL;
    }
    else if ((ev == MG_EV_POLL || ev == MG_EV_WRITE) && c->is_writable)
    {
        if (poll_handler != NULL)
        {
            uint32_t err = poll_handler(c, ev, ev_data, fn_data);
            // Error or complete
            if (err != 0)
            {
                poll_handler = NULL;
            }
        }
    }
    else if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        for (int i = 0; i < MAX_HTTPD_ROUTE; i++)
        {
            if (!httpd_route[i])
            {
                break;
            }
            if (mg_http_match_uri(hm, httpd_route[i]->uri))
            {
                log_trace(hm->message.ptr);
                // log_info("%s", hm->uri.ptr);
                uint32_t err = httpd_route[i]->http_handler(c, ev, ev_data, fn_data);
                if (err == 0)
                {
                    // no error, use poll handler
                    poll_handler = httpd_route[i]->poll_handler;
                }
                else
                {
                    // error or complete, don't use poll handler
                    poll_handler = NULL;
                }
                return;
            }
        }
        log_debug("fallback to static: %s", hm->uri);
        // fallback
        struct mg_http_serve_opts opts = {.root_dir = "isos"};
        mg_http_serve_dir(c, ev_data, &opts);
        poll_handler = NULL;
    }
}


int http_get_range_header(struct mg_http_message *hm, uint32_t *range_start, uint32_t *range_end)
{
  struct mg_str *range = mg_http_get_header(hm, "range");
  if (range && range->ptr)
  {
    if (sscanf((char *)range->ptr, "bytes=%d-%d", range_start, range_end) != EOF)
    {
      return 0;
    }
  }

  return -1;
}