#include "mongoose.h"
#include "client.h"

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    client_data_t *client = (client_data_t *)fn_data;
    if (ev == MG_EV_CONNECT)
    {
        struct mg_str host = mg_url_host(client->url);

        switch (client->method)
        {
        case CLIENT_METHOD_GET:
            mg_printf(c,
                      "GET %s HTTP/1.0\r\n"
                      "Host: %.*s\r\n"
                      "\r\n",
                      mg_url_uri(client->url), (int)host.len, host.ptr);

            break;
        case CLIENT_METHOD_POST:
            mg_printf(c,
                      "POST %s HTTP/1.0\r\n"
                      "Host: %.*s\r\n",
                      "Content-Length: %d\r\n",
                      "\r\n",
                      mg_url_uri(client->url), (int)host.len, host.ptr, client->data_len);
            mg_send(c, client->post, client->data_len);

            break;

        default:
            break;
        }
    }
    else if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        c->is_closing = 1;
        client->finished = true;
    }
    else if (ev == MG_EV_ERROR)
    {
        client->finished = true;
    }
}

int http_client(client_data_t *client_data)
{
    struct mg_mgr mgr;
    client_data->finished = false;

    mg_log_set("3");
    mg_mgr_init(&mgr);
    mg_http_connect(&mgr, client_data->url, fn, client_data);
    while (!client_data->finished)
        mg_mgr_poll(&mgr, 1000);
    mg_mgr_free(&mgr);
    return 0;
}
