#pragma once

/**
 * http routes registered:
 *  GET /dir [chunked]
 *  HEAD /dir
 */ 
void menu_register_routes(struct mg_mgr *mgr);

int menu_get_filename_by_id(int id, char * filename);


void menu_http_handler_init(fenrir_user_data_t *fenrir_user_data);