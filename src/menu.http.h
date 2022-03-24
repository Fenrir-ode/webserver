#pragma once

#ifndef MAX_ENTRIES
#define MAX_ENTRIES (3000)
#endif

typedef struct
{
    fs_cache_t fs_cache[MAX_ENTRIES];
    sd_dir_entry_t sd_dir_entries[MAX_ENTRIES];
    uint32_t sd_dir_entries_count;
} server_shared_t;

extern server_shared_t server_shared;

/**
 * http routes registered:
 *  GET /dir [chunked]
 *  HEAD /dir
 */
void menu_register_routes(struct mg_mgr *mgr);
int menu_get_filename_by_id(uint32_t id, char *filename);
void menu_http_handler_init(server_config_t *server_config);
