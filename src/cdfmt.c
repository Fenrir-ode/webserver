#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "cdfmt.h"
#include "libchdr/chd.h"
#include "libchdr/cdrom.h"

/* mame stuff */
extern void cdrom_get_info_from_type_string(const char *typestring, uint32_t *trktype, uint32_t *datasize);
extern uint32_t mame_parse_toc(const char *tocfname, cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t *fenrir_toc);

/* ECC utilities */
extern int ecc_verify(const uint8_t *sector);
extern void ecc_generate(uint8_t *sector);
extern void ecc_clear(uint8_t *sector);

static uint32_t cdrom_get_track_start(cdrom_toc_t *cdtoc, uint32_t track)
{

    /* handle lead-out specially */
    if (track == 0xaa)
        track = cdtoc->numtrks - 1;

    return cdtoc->tracks[track].logframeofs;
}

static void fad_to_msf(uint32_t val, uint8_t *m, uint8_t *s, uint8_t *f)
{
    uint32_t temp;
    m[0] = val / 4500;
    temp = val % 4500;
    s[0] = temp / 75;
    f[0] = temp % 75;
}

static uint32_t chd_parse_toc(const char *tocfname, fenrir_user_data_t *fenrir_ud, raw_toc_dto_t *fenrir_toc)
{
    cdrom_toc_t *toc = &fenrir_ud->toc;
    chd_error err;
    if ((err = chd_open(tocfname, CHD_OPEN_READ, NULL, &fenrir_ud->chd_file)) == CHDERR_NONE)

    {
        char tmp[512];
        int track = 0, frames = 0, pregap = 0, postgap = 0;
        char type[64], subtype[32], pgtype[32], pgsub[32];
        int numtrks = 0;
        uint32_t fad = 0;

        uint32_t physofs = 0;
        uint32_t logofs = 0;
        uint32_t chdofs = 0;

        const chd_header *chd_header = chd_get_header(fenrir_ud->chd_file);
        fenrir_ud->chd_header = chd_header;
        fenrir_ud->sectors_per_hunk = chd_header->hunkbytes / chd_header->unitbytes;

        fenrir_ud->chd_hunk_buffer = (uint8_t *)malloc(chd_header->hunkbytes);
        fenrir_ud->cur_hunk = -1;

        for (int i = 0; i < CD_MAX_TRACKS; i++)
        {
            /* parse chd metadata */
            if (chd_get_metadata(fenrir_ud->chd_file, CDROM_TRACK_METADATA2_TAG, i, tmp, sizeof(tmp), NULL, NULL, NULL) == CHDERR_NONE)
            {
                if (sscanf(tmp, CDROM_TRACK_METADATA2_FORMAT, &track, type, subtype, &frames, &pregap, pgtype, pgsub, &postgap) != 8)
                {
                    log_error("Invalid chd metadata");
                    return -1;
                }
            }
            else if (chd_get_metadata(fenrir_ud->chd_file, CDROM_TRACK_METADATA_TAG, i, tmp, sizeof(tmp), NULL, NULL, NULL) == CHDERR_NONE)
            {
                if (sscanf(tmp, CDROM_TRACK_METADATA_FORMAT, &track, type, subtype, &frames) != 4)
                {
                    log_error("Invalid chd metadata");
                    return -1;
                }
            }
            else
            {
                break;
            }

            log_info("track: %d / frame: %d / type:%s", track, frames, type);

            int padded = (frames + CD_TRACK_PADDING - 1) / CD_TRACK_PADDING;
            int extraframes = padded * CD_TRACK_PADDING - frames;

            uint32_t _pgtype, _pgdatasize = 0;

            /* handle pregap */
            if (pregap > 0)
            {
                if (pgtype[0] == 'V')
                {
                    cdrom_get_info_from_type_string(&pgtype[1], &_pgtype, &_pgdatasize);
                }
            }

            /* build toc */
            if (_pgdatasize == 0)
            {
                logofs += pregap;
            }
            else
            {
                toc->tracks[track - 1].logframeofs = pregap;
            }

            toc->tracks[track - 1].physframeofs = physofs;
            toc->tracks[track - 1].logframeofs += logofs;
            toc->tracks[track - 1].chdframeofs = chdofs;

            chdofs += frames;
            chdofs += extraframes;
            logofs += frames;
            physofs += frames;

            // Fenrir toc
            fenrir_toc[2 + track].ctrladr = (strcmp(type, "AUDIO") == 0) ? 0x01 : 0x41;
            fenrir_toc[2 + track].frame = 0;
            fenrir_toc[2 + track].min = 0;
            fenrir_toc[2 + track].sec = 2;
            fenrir_toc[2 + track].pframe = 0;
            fenrir_toc[2 + track].pmin = 0;
            fenrir_toc[2 + track].psec = 0;
            fenrir_toc[2 + track].point = track;
            fenrir_toc[2 + track].tno = 0;
            fenrir_toc[2 + track].zero = 0;

            fad_to_msf(toc->tracks[track - 1].logframeofs + 150, &fenrir_toc[2 + track].pmin, &fenrir_toc[2 + track].psec, &fenrir_toc[2 + track].pframe);
            fad += frames;

            numtrks++;
        }

        // leadin/leadout
        {
            fenrir_toc[0].ctrladr = 0x41;
            fenrir_toc[0].point = 0xa0;

            fenrir_toc[0].frame = 0;
            fenrir_toc[0].min = 0;
            fenrir_toc[0].sec = 2;

            fenrir_toc[0].pframe = 0;
            fenrir_toc[0].pmin = 1;
            fenrir_toc[0].psec = 0;

            fenrir_toc[0].tno = 0;
            fenrir_toc[0].zero = 0;
        }
        {
            fenrir_toc[1].ctrladr = 0x01;
            fenrir_toc[1].point = 0xa1;

            fenrir_toc[1].frame = 0;
            fenrir_toc[1].min = 0;
            fenrir_toc[1].sec = 2;

            fenrir_toc[1].pframe = 0;
            fenrir_toc[1].pmin = numtrks;
            fenrir_toc[1].psec = 0;

            fenrir_toc[1].tno = 0;
            fenrir_toc[1].zero = 0;
        }
        {
            fenrir_toc[2].ctrladr = 0x01;
            fenrir_toc[2].point = 0xa2;

            fenrir_toc[2].frame = 0;
            fenrir_toc[2].min = 0;
            fenrir_toc[2].sec = 2;

            fenrir_toc[2].pframe = 0;
            fenrir_toc[2].pmin = 0;
            fenrir_toc[2].psec = 0;

            fenrir_toc[2].tno = 0;
            fenrir_toc[2].zero = 0;

            fad_to_msf(toc->tracks[numtrks - 1].logframeofs + 150, &fenrir_toc[2].pmin, &fenrir_toc[2].psec, &fenrir_toc[2].pframe);
        }

        fenrir_ud->toc.numtrks = numtrks;

        return 0;
    }
    return 1;
}

uint32_t parse_toc(const char *tocfname, fenrir_user_data_t *fenrir_ud, raw_toc_dto_t *fenrir_toc)
{
    if (strstr(tocfname, ".chd") != NULL)
    {
        fenrir_ud->type = IMAGE_TYPE_CHD;
        return chd_parse_toc(tocfname, fenrir_ud, fenrir_toc);
    }
    else
    {
        fenrir_ud->type = IMAGE_TYPE_MAME_LDR;
        return mame_parse_toc(tocfname, &fenrir_ud->toc, fenrir_toc);
    }
}

static uint8_t get_track_number(cdrom_toc_t *toc, uint32_t frame)
{
    uint8_t track = 0;

    for (track = 0; track < toc->numtrks; track++)
    {
        if (frame < toc->tracks[track + 1].logframeofs)
        {
            return track;
        }
    }
    return track;
}

uint32_t read_data(fenrir_user_data_t *fenrir_user_data, uint8_t *data, uint32_t fad, uint32_t size)
{
    memset(data, 0, size);
    if (fenrir_user_data == NULL)
    {
        return 0;
    }

    if (fenrir_user_data->type == IMAGE_TYPE_MAME_LDR)
    {
        uint8_t track = get_track_number(&fenrir_user_data->toc, fad);
        cdrom_track_info_t *track_info = &fenrir_user_data->toc.tracks[track];

        uint64_t offset = track_info->offset + (fad - track_info->logframeofs) * track_info->datasize;
        log_debug("read at: %08x", offset);

        if (fseek(track_info->fp, offset, SEEK_SET) == 0)
        {
            if (fread(data, 1, track_info->datasize, track_info->fp) == track_info->datasize)
            {
                // convert to 2352
                if (track_info->datasize == 2048)
                {
                    uint8_t msf = lba_to_msf(fad);
                    memmove(data + 16, data, track_info->datasize);

                    uint8_t sync_header[] = {
                        0x00, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0x00,
                        (msf >> 16) & 0xFF, (msf >> 8) & 0xFF, (msf >> 0) & 0xFF, 0x01};

                    memcpy(data, sync_header, 16);

                    ecc_generate(data);
                }
                return 0;
            }
            else
            {
                log_error("Failed to read at %d - %d", offset, track_info->datasize);
                return 1;
            }
        }
        else
        {
            log_error("Failed to seek to %d", offset);
            return 1;
        }
    }
    else if (fenrir_user_data->type == IMAGE_TYPE_CHD)
    {
        static int i = 0;
        uint32_t hunknumber = (fad) / fenrir_user_data->sectors_per_hunk;
        uint32_t hunkoffset = (fad) % fenrir_user_data->sectors_per_hunk;

        log_debug("chdread at: %08x/%08x (%d)", hunknumber, hunkoffset, fad);
        if (fenrir_user_data->cur_hunk != hunknumber)
        {
            chd_error err = chd_read(fenrir_user_data->chd_file, hunknumber, fenrir_user_data->chd_hunk_buffer);
            if (err != CHDERR_NONE)
            {
                log_error("chd_read  :%d", err);
                return 1;
            }
        }
        memcpy(data, fenrir_user_data->chd_hunk_buffer + hunkoffset * (CD_FRAME_SIZE), CD_MAX_SECTOR_DATA);
        fenrir_user_data->cur_hunk = hunknumber;
        return 0;
    }

    return 1;
}