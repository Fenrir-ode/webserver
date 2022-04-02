#pragma once

#ifndef MAX_ENTRIES
#define MAX_ENTRIES (3000)
#endif


/**
 * http routes registered:
 *  GET /dir [chunked]
 *  HEAD /dir
 */
void menu_register_routes(struct mg_mgr *mgr);
int menu_get_filename_by_id(uint32_t id, char *filename);
void menu_http_handler_init(server_config_t *server_config);
