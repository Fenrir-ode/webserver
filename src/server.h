#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __MAX_PATH_LEN
#define __MAX_PATH_LEN (4096)
#endif

#include <stdint.h>

    typedef struct
    {
        uintptr_t ud;
        int (*started)(uintptr_t ud);
        int (*run)(uintptr_t ud);
        void (*notify_add_game)(uintptr_t ud, const char *fullgame);
    } server_events_t;

    typedef struct
    {
        int port;
        char image_path[__MAX_PATH_LEN];
    } server_config_t;

    extern server_events_t *server_events;

    //int server(fenrir_user_data_t *fenrir_user_data);
    int server(server_config_t *server_config);

    /**
     * send a mdns query then a http request to setup fenrir and server 
     **/
    int mdns_setup_fenrir(server_config_t *);

#ifdef __cplusplus
}
#endif