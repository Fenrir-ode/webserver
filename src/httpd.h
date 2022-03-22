#pragma once

#define MAX_FUD               (10)
#define MAX_HTTPD_ROUTE       (10)

typedef struct {
    const char * uri;
    uint32_t route_id;
    uint32_t (*http_handler)(struct mg_connection *c, int ev, void *ev_data, void *fn_data);    
    uint32_t (*poll_handler)(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
} httpd_route_t;

uint32_t httpd_init(struct mg_mgr *mgr, size_t per_connect_user_size, uint32_t (*close_handler)(struct mg_connection *c, int ev, void *ev_data, void *fn_data));
void httpd_free();
uint32_t httpd_add_route(struct mg_mgr *mgr, const httpd_route_t*route);

void httpd_poll(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
int http_get_range_header(struct mg_http_message *hm, uint32_t *range_start, uint32_t *range_end);
int http_is_request_behind_proxy(struct mg_http_message *hm);

