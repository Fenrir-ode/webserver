#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#define __MAX_PATH_LEN (4096)

#define SECTOR_SIZE (2352)
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

    typedef struct
    {
        // entry file name
        char *filename;
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
        uint8_t *http_buffer;
    } fenrir_user_data_t;

    uint32_t parse_toc(const char *tocfname, fenrir_user_data_t *, raw_toc_dto_t *fenrir_toc);
    uint32_t read_data(fenrir_user_data_t *fenrir_user_data, uint8_t *data, uint32_t fad, uint32_t size);

    void fenrir_set_track(raw_toc_dto_t *fenrir_toc, uint8_t track, uint8_t ctrladr, uint32_t fad);
    void fenrir_set_leadin_leadout(cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t *fenrir_toc, uint8_t numtrks, uint32_t leadout);

#ifdef __cplusplus
}
#endif