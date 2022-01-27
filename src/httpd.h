#pragma once

#define MAX_HTTPD_ROUTE       (10)

typedef struct {
    const char * uri;
    uint32_t route_id;
    uint32_t (*http_handler)(struct mg_connection *c, int ev, void *ev_data, void *fn_data);    
    uint32_t (*poll_handler)(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
} httpd_route_t;

uint32_t httpd_init(struct mg_mgr *mgr);
uint32_t httpd_add_route(struct mg_mgr *mgr, const httpd_route_t*route);
void httpd_poll(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
int http_get_range_header(struct mg_http_message *hm, uint32_t *range_start, uint32_t *range_end);

