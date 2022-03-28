#include "mongoose.h"
#include "log.h"
#include "httpd.h"

static httpd_route_t *httpd_route[MAX_HTTPD_ROUTE];

uint32_t httpd_init(struct mg_mgr *mgr)
{
    for (int i = 0; i < MAX_HTTPD_ROUTE; i++)
    {
        httpd_route[i] = NULL;
    }
}

void httpd_free()
{
    for (int i = 0; i < MAX_FUD; i++)
    {
    }
}

static void http_route_clone(httpd_route_t *dst, const httpd_route_t *src)
{
    dst->http_handler = src->http_handler;
    dst->poll_handler = src->poll_handler;
    dst->close_handler = src->close_handler;
    dst->accept_handler = src->accept_handler;
    dst->route_id = src->route_id;
    if (src->uri)
        dst->uri = strdup(src->uri);
    else
        dst->uri = NULL;
}

uint32_t httpd_add_route(struct mg_mgr *mgr, const httpd_route_t *route)
{
    for (int i = 0; i < MAX_HTTPD_ROUTE; i++)
    {
        if (httpd_route[i] == NULL)
        {
            httpd_route[i] = (httpd_route_t *)calloc(sizeof(httpd_route_t), 1);
            http_route_clone(httpd_route[i], route);
            return 0;
        }
    }

    // no more route available
    return -1;
}

void httpd_poll(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    per_request_data_t *per_request_data = c->fn_data ? (per_request_data_t *)c->fn_data : NULL;

    if (ev == MG_EV_ACCEPT)
    {
        c->fn_data = (per_request_data_t *)calloc(sizeof(per_request_data_t), 1);
    }
    else if (ev == MG_EV_CLOSE)
    {
        if (per_request_data && per_request_data->route.close_handler)
        {
            per_request_data->route.close_handler(c, per_request_data);
        }
        if (c->fn_data)
            free(c->fn_data);
    }
    else if ((ev == MG_EV_POLL || ev == MG_EV_WRITE) && c->is_writable)
    {

        if (per_request_data && per_request_data->route.poll_handler)
        {
            uint32_t err = per_request_data->route.poll_handler(c, ev, ev_data, per_request_data->data);
            // Error or complete
            if (err != 0)
            {
                per_request_data->route.poll_handler = NULL;
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
                uint32_t err = -1;
                http_route_clone(&per_request_data->route, httpd_route[i]);
                log_trace(hm->message.ptr);
                // log_info("%s", hm->uri.ptr);
                if (per_request_data->route.accept_handler)
                {
                    err = per_request_data->route.accept_handler(c, per_request_data);
                    if (err != 0)
                    {
                        log_debug("accept_handler failed");
                        return;
                    }
                }
                err = per_request_data->route.http_handler(c, ev, ev_data, per_request_data->data);
                if (err != 0)
                {
                    // error or complete, don't use poll handler
                    per_request_data->route.poll_handler = NULL;
                }
                return;
            }
        }
        char uri[256];
        memcpy(uri, hm->uri.ptr, hm->uri.len);
        uri[hm->uri.len] = 0;
        log_debug("unhandled route: %s", uri);
        mg_http_reply(c, 404, "", "Nothing here");
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

int http_is_request_behind_proxy(struct mg_http_message *hm)
{
    // check if X-Upstream header is present
    struct mg_str *range = mg_http_get_header(hm, "X-Real-IP");
    return range && range->ptr;
}

int http_get_route_id(struct mg_http_message *hm)
{
    char uri[64];
    int id = -1;
    memcpy(uri, hm->uri.ptr, hm->uri.len);
    uri[hm->uri.len] = 0;
    if ((sscanf(uri, "/toc_bin/%d", &id) == 1) || (sscanf(uri, "/data/%d", &id) == 1))
    {
        return id;
    }
    log_debug("Id for route %s not found", uri);
}
