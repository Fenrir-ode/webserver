#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "pack.h"

#define __MAX_PATH_LEN (4096)

#define SECTOR_SIZE (2352)
#define SECTOR_SIZE_2048 (2048)

#define CD_MAX_TRACKS (99) /* AFAIK the theoretical limit */

    enum IMAGE_TYPE
    {
        IMAGE_TYPE_MAME_LDR,
        IMAGE_TYPE_CHD
    };

    typedef struct _chd_file chd_file;
    typedef struct _chd_header chd_header;

    typedef struct
    {
        /* fields used by CHDMAN and in MAME */
        uint32_t trktype;     /* track type */
        uint32_t subtype;     /* subcode data type */
        uint32_t datasize;    /* size of data in each sector of this track */
        uint32_t subsize;     /* size of subchannel data in each sector of this track */
        uint32_t frames;      /* number of frames in this track */
        uint32_t extraframes; /* number of "spillage" frames in this track */
        uint32_t pregap;      /* number of pregap frames */
        uint32_t postgap;     /* number of postgap frames */
        uint32_t pgtype;      /* type of sectors in pregap */
        uint32_t pgsub;       /* type of subchannel data in pregap */
        uint32_t pgdatasize;  /* size of data in each sector of the pregap */
        uint32_t pgsubsize;   /* size of subchannel data in each sector of the pregap */

        /* fields used in CHDMAN only */
        uint32_t padframes;   /* number of frames of padding to add to the end of the track; needed for GDI */
        uint32_t splitframes; /* number of frames to read from the next file; needed for Redump split-bin GDI */

        /* fields used in MAME/MESS only */
        uint32_t logframeofs;  /* logical frame of actual track data - offset by pregap size if pregap not physically present */
        uint32_t physframeofs; /* physical frame of actual track data in CHD data */
        uint32_t chdframeofs;  /* frame number this track starts at on the CHD */
        uint32_t logframes;    /* number of frames from logframeofs until end of track data */

        /* fields used in multi-cue GDI */
        uint32_t multicuearea;

        /* fields used for fenrir webserver */
        bool swap;       // data needs to be byte swapped
        uint32_t offset; // offset in the data file for each track
        uint32_t idx0offs;
        uint32_t idx1offs;
        char filename[__MAX_PATH_LEN]; // filename for each track
        FILE *fp;

    } cdrom_track_info_t;

    typedef struct
    {
        uint32_t numtrks; /* number of tracks */
        uint32_t flags;   /* see FLAG_ above */
        cdrom_track_info_t tracks[CD_MAX_TRACKS];
    } cdrom_toc_t;

#ifdef __cplusplus
}
#endif