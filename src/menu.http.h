#pragma once

/**
 * http routes registered:
 *  GET /dir [chunked]
 *  HEAD /dir
 */ 
void menu_register_routes(struct mg_mgr *mgr);
int menu_get_filename_by_id(fenrir_user_data_t *fenrir_user_data, uint32_t id, char * filename);
void menu_http_handler_init(server_config_t *server_config);
