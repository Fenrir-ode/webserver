#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#define __MAX_PATH_LEN (4096)

#define CD_MAX_TRACKS (99) /* AFAIK the theoretical limit */

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

    } cdrom_track_info_t;

    typedef struct
    {
        uint32_t numtrks; /* number of tracks */
        uint32_t flags;   /* see FLAG_ above */
        cdrom_track_info_t tracks[CD_MAX_TRACKS];
    } cdrom_toc_t;

    typedef struct
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
    } raw_toc_dto_t;

    uint32_t mame_parse_toc(const char *tocfname, cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t * fenrir_toc);

#ifdef __cplusplus
}
#endif