#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "cdfmt.h"
#include "libchdr/chd.h"
#include "libchdr/cdrom.h"

extern uint32_t mame_parse_toc(const char *tocfname, cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t *fenrir_toc);

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
    chd_error err;
    if ((err = chd_open(tocfname, CHD_OPEN_READ, NULL, &fenrir_ud->chd_file)) == CHDERR_NONE)
    {
        char tmp[512];
        int track = 0, frames = 0, pregap = 0, postgap = 0;
        char type[64], subtype[32], pgtype[32], pgsub[32];
        int numtrks = 0;

        for (int i = 0; i < 99; i++)
        {
            if (chd_get_metadata(fenrir_ud->chd_file, CDROM_TRACK_METADATA2_TAG, i, tmp, sizeof(tmp), NULL, NULL, NULL) == CHDERR_NONE)
            {
                sscanf(tmp, CDROM_TRACK_METADATA2_FORMAT, &track, type, subtype, &frames, &pregap, pgtype, pgsub, &postgap);

                // Fenrir toc
                fenrir_toc[3 + i].ctrladr = type == CD_TRACK_AUDIO ? 0x01 : 0x41;
                fenrir_toc[3 + i].frame = 0;
                fenrir_toc[3 + i].min = 0;
                fenrir_toc[3 + i].sec = 2;

                fenrir_toc[3 + i].pframe = 0;
                fenrir_toc[3 + i].pmin = 0;
                fenrir_toc[3 + i].psec = 0;

                fenrir_toc[3 + i].point = i + 1;
                fenrir_toc[3 + i].tno = 0;
                fenrir_toc[3 + i].zero = 0;

               // uint32_t fad = cdrom_get_track_start(&outtoc, i) + 150;
               // fad_to_msf(fad, &fenrir_toc[3 + i].pmin, &fenrir_toc[3 + i].psec, &fenrir_toc[3 + i].pframe);
            }
            else
            {
                break;
            }

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

            //uint32_t fad = cdrom_get_track_start(&outtoc, 0xAA) + 150;
            //fad_to_msf(fad, &fenrir_toc[2].pmin, &fenrir_toc[2].psec, &fenrir_toc[2].pframe);
        }

        return 0;
    }
    return 1;
}

uint32_t parse_toc(const char *tocfname, fenrir_user_data_t *fenrir_ud, raw_toc_dto_t *fenrir_toc)
{
    if (0)
    {
        return chd_parse_toc(tocfname, fenrir_ud, fenrir_toc);
    }
    else
    {
        return mame_parse_toc(tocfname, &fenrir_ud->toc, fenrir_toc);
    }
}

static uint8_t get_track_number(cdrom_toc_t *toc, uint32_t frame)
{
    uint8_t track = 0;

    for (track = 0; track < toc->numtrks; track++)
    {
        if (frame < toc->tracks[track].logframeofs)
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

    uint8_t track = get_track_number(&fenrir_user_data->toc, fad);
    cdrom_track_info_t *track_info = &fenrir_user_data->toc.tracks[track];

    uint64_t offset = track_info->offset + (fad - track_info->logframeofs) * track_info->datasize;
    log_debug("read at: %08x", offset);

    if (fseek(track_info->fp, offset, SEEK_SET) == 0)
    {
        if (fread(data, 1, size, track_info->fp) == size)
        {
            return 0;
        }
    }

    return 1;
}