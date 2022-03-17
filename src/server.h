#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "cdfmt.h"
#include "fenrir.h"



    typedef struct
    {
        uintptr_t ud;
        int (*run)(uintptr_t ud);
        void (*notify_add_game)(uintptr_t ud, char *fullgame);
    } server_events_t;

    int server(fenrir_user_data_t *fenrir_user_data);
    int get_image_region(char *r);

#ifdef __cplusplus
}
#endif