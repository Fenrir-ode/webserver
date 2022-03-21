#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "pack.h"
#define MAX_ENTRIES (3000)

#ifndef __MAX_PATH_LEN
#define __MAX_PATH_LEN (4096)
#endif
#ifndef SECTOR_SIZE
#define SECTOR_SIZE (2352)
#endif
#define SECTOR_SIZE_2048 (2048)

#ifndef CD_MAX_TRACKS
#define CD_MAX_TRACKS (99) /* AFAIK the theoretical limit */
#endif

#define DUMB_STATIC_ASSERT(test) typedef char assertion_on_mystruct[(!!(test)) * 2 - 1]

#define SD_MENU_FILENAME_LENGTH 58
#define SD_DIR_FLAG_DIRECTORY (1 << 31)

    typedef enum regions
    {
        region_japan,
        region_taiwan,
        region_usa,
        region_brazil,
        region_korea,
        region_asiapal,
        region_europe,
        region_latin_america
    } regions_t;

    // 64bytes - fenrir fmt
    typedef PACKED(
        struct
        {
            uint16_t id;
            uint32_t flag;
            char filename[SD_MENU_FILENAME_LENGTH];
        }) sd_dir_entry_t;

    typedef PACKED(
        struct
        {
            uint8_t ctrladr;
            uint8_t tno;
            uint8_t point;
            uint8_t min;
            uint8_t sec;
            uint8_t frame;
            uint8_t zero;
            uint8_t pmin;
            uint8_t psec;
            uint8_t pframe;
        }) raw_toc_dto_t;

    typedef struct
    {
        // entry file name
        char filename[__MAX_PATH_LEN];
        char image_path[__MAX_PATH_LEN];

        // chd or other image
        uint8_t type;
        // mame toc
        cdrom_toc_t toc;
        // chd file
        chd_file *chd_file;
        const chd_header *chd_header;
        uint8_t *chd_hunk_buffer;
        uint32_t sectors_per_hunk;
        uint32_t cur_hunk;
        // fenrir toc
        raw_toc_dto_t toc_dto[CD_MAX_TRACKS + 3]; // 99 + 3 Metadata track
        // streams
        uint32_t req_fad;
        uint32_t req_size;
        uint8_t http_buffer[4 * 2048];
        // dir
        uint32_t entries_offset;
        int patch_region;

    } fenrir_user_data_t;

    uint32_t cdfmt_parse_toc(const char *tocfname, fenrir_user_data_t *, raw_toc_dto_t *fenrir_toc);
    uint32_t cdfmt_close(fenrir_user_data_t *fenrir_user_data);
    uint32_t cdfmt_read_data(fenrir_user_data_t *fenrir_user_data, uint8_t *data, uint32_t fad, uint32_t size);

    void fenrir_set_track(raw_toc_dto_t *fenrir_toc, uint8_t track, uint8_t ctrladr, uint32_t fad);
    void fenrir_set_leadin_leadout(cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t *fenrir_toc, uint8_t numtrks, uint32_t leadout);

    // Compile time assert
    DUMB_STATIC_ASSERT(sizeof(sd_dir_entry_t) == 64);
    DUMB_STATIC_ASSERT(sizeof(raw_toc_dto_t) == 10);
#ifdef __cplusplus
}
#endif