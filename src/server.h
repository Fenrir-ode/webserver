#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MAX_ENTRIES
#define MAX_ENTRIES (3000)
#endif
#ifndef __MAX_PATH_LEN
#define __MAX_PATH_LEN (4096)
#endif

#include <stdint.h>

    typedef struct
    {
        uintptr_t ud;
        int (*run)(uintptr_t ud);
        void (*notify_add_game)(uintptr_t ud, const char *fullgame);
    } server_events_t;

    typedef struct
    {
        int port;
        int nb_threads;
        char image_path[__MAX_PATH_LEN];
    } server_config_t;

    typedef struct
    {
        uint16_t id;
        uint32_t flag;
        char *path;
        char *name;
    } fs_cache_t;

    typedef struct {
        fs_cache_t fs_cache[MAX_ENTRIES];
        sd_dir_entry_t sd_dir_entries[MAX_ENTRIES];        
        uint32_t sd_dir_entries_count;
    } server_shared_t;

    extern server_events_t *server_events;
    extern server_shared_t server_shared;

    //int server(fenrir_user_data_t *fenrir_user_data);
    int server(server_config_t *server_config);

#ifdef __cplusplus
}
#endif