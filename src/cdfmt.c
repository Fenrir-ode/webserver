#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "cdfmt.h"
#include "fenrir.h"
#include "libchdr/chd.h"
#include "libchdr/cdrom.h"

#ifdef _MSC_VER
#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

/* mame stuff */
extern void cdrom_get_info_from_type_string(const char *typestring, uint32_t *trktype, uint32_t *datasize);
extern uint32_t mame_parse_toc(const char *tocfname, cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t *fenrir_toc);

/* ECC utilities */
extern int ecc_verify(const uint8_t *sector);
extern void ecc_generate(uint8_t *sector);
extern void ecc_clear(uint8_t *sector);

// =============================================================
// utilities
// =============================================================
static uint32_t dec_2_bcd(uint32_t a)
{
    uint32_t result = 0;
    int shift = 0;

    while (a != 0)
    {
        result |= (a % 10) << shift;
        a /= 10;
        shift += 4;
    }
    return result;
}

static void fad_to_msf(uint32_t val, uint8_t *m, uint8_t *s, uint8_t *f)
{
    uint32_t temp;
    m[0] = val / 4500;
    temp = val % 4500;
    s[0] = temp / 75;
    f[0] = temp % 75;
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
    // No track found, return the last track
    return 0;
}

void fenrir_set_track(raw_toc_dto_t *fenrir_toc, uint8_t track, uint8_t ctrladr, uint32_t fad)
{
    fenrir_toc[2 + track].ctrladr = ctrladr;
    fenrir_toc[2 + track].frame = 0;
    fenrir_toc[2 + track].min = 0;
    fenrir_toc[2 + track].sec = 2;
    fenrir_toc[2 + track].pframe = 0;
    fenrir_toc[2 + track].pmin = 0;
    fenrir_toc[2 + track].psec = 0;
    fenrir_toc[2 + track].point = track;
    fenrir_toc[2 + track].tno = 0;
    fenrir_toc[2 + track].zero = 0;

    fad_to_msf(fad, &fenrir_toc[2 + track].pmin, &fenrir_toc[2 + track].psec, &fenrir_toc[2 + track].pframe);
}

static void chd_fenrir_set_leadin_leadout(raw_toc_dto_t *fenrir_toc, uint8_t numtrks, uint32_t leadout)
{
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

        fad_to_msf(leadout, &fenrir_toc[2].pmin, &fenrir_toc[2].psec, &fenrir_toc[2].pframe);
    }
}

void fenrir_set_leadin_leadout(cdrom_toc_t *out_cdrom_toc, raw_toc_dto_t *fenrir_toc, uint8_t numtrks, uint32_t leadout)
{

    if (numtrks == 1)
    {
        log_warn("Only 1 tracks found, add a fake audio track");
        // change leadout
        leadout = out_cdrom_toc->tracks[0].logframes + (2 * 150);

        // Add a fake audio track at end
        fenrir_set_track(fenrir_toc, numtrks + 1, 0x01, leadout);
        numtrks++;
    }

    chd_fenrir_set_leadin_leadout(fenrir_toc, numtrks, leadout);
}
// =============================================================
// debug
// =============================================================
void print_raw_toc(raw_toc_dto_t *toc, uint8_t numtrks)
{
#define DUMP_M(x) log_debug("\t" #x ": %02x", toc[i].x);
    log_debug("dump toc");
    for (uint8_t i = 0; i < numtrks + 3; i++)
    {
        log_debug("track: %d", i);
        DUMP_M(ctrladr);
        DUMP_M(tno);
        DUMP_M(point);
        DUMP_M(min);
        DUMP_M(sec);
        DUMP_M(frame);
        DUMP_M(zero);
        DUMP_M(pmin);
        DUMP_M(psec);
        DUMP_M(pframe);
    }
#undef DUMP_M
}

// =============================================================
// parse toc
// =============================================================
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
            fenrir_set_track(fenrir_toc, track, (strcmp(type, "AUDIO") == 0) ? 0x01 : 0x41, toc->tracks[track - 1].logframeofs + 150);
            fad += frames;

            numtrks++;
        }

        chd_fenrir_set_leadin_leadout(fenrir_toc, numtrks, fad + 150);
        // print_raw_toc(fenrir_toc, numtrks);
        fenrir_ud->toc.numtrks = numtrks;

        return 0;
    }
    return 1;
}

static int compare_track(raw_toc_dto_t *a, raw_toc_dto_t *b)
{
    // order: A0 A1 A2 00 01 ....
    int a_point = a->point;
    int b_point = b->point;

    if (a_point < 100)
    {
        a_point += 0x100;
    }
    if (b_point < 100)
    {
        b_point += 0x100;
    }

    return a_point - b_point;
}

static void convert_toc_to_sat(raw_toc_dto_t *fenrir_toc, uint32_t nb_tracks)
{
    // step 1 - sort by track nb
    qsort(fenrir_toc, nb_tracks, sizeof(raw_toc_dto_t), compare_track);

    // step 2 - dec to bcd
    for (uint32_t i = 0; i < nb_tracks; i++)
    {
        if (fenrir_toc[i].point < 100)
            fenrir_toc[i].point = dec_2_bcd(fenrir_toc[i].point);

        fenrir_toc[i].pmin = dec_2_bcd(fenrir_toc[i].pmin);
        fenrir_toc[i].psec = dec_2_bcd(fenrir_toc[i].psec);
        fenrir_toc[i].pframe = dec_2_bcd(fenrir_toc[i].pframe);
    }
}

uint32_t cdfmt_parse_toc(const char *tocfname, fenrir_user_data_t *fenrir_ud, raw_toc_dto_t *fenrir_toc)
{
    int len = strlen(tocfname);
    char *ext = (char *)tocfname + len;
    int i = 0;
    while (i < len && (*--ext != '.'))
        i++;

    if (strcasecmp(ext, ".chd") == 0)
    {
        fenrir_ud->type = IMAGE_TYPE_CHD;
        return chd_parse_toc(tocfname, fenrir_ud, fenrir_toc);
    }
    else
    {
        fenrir_ud->type = IMAGE_TYPE_MAME_LDR;
        uint32_t mame_toc = mame_parse_toc(tocfname, &fenrir_ud->toc, fenrir_toc);

        if (mame_toc == 0)
        {
            if (fenrir_ud->toc.numtrks == 1)
            {
                // Add a fake track
                fenrir_ud->toc.numtrks = 2;
                fenrir_ud->toc.tracks[1].logframeofs = 0;
                fenrir_ud->toc.tracks[1].fp = fenrir_ud->toc.tracks[0].fp;
            }
            convert_toc_to_sat(fenrir_toc, fenrir_ud->toc.numtrks + 3);
        }
        print_raw_toc(fenrir_toc, fenrir_ud->toc.numtrks);
        return mame_toc;
    }
}

// =============================================================
// data read
// =============================================================
uint32_t cdfmt_read_data(fenrir_user_data_t *fenrir_user_data, uint8_t *data, uint32_t fad, uint32_t size)
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
        // log_trace("read at: %08x", offset);

        if (fseek(track_info->fp, offset, SEEK_SET) == 0)
        {
            if (fread(data, 1, track_info->datasize, track_info->fp) == track_info->datasize)
            {
                // convert to 2352
                if (track_info->datasize == 2048)
                {
                    uint32_t msf = lba_to_msf_alt(fad + 150);
                    memmove(data + 16, data, track_info->datasize);

                    uint8_t sync_header[] = {
                        0x00, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0x00,
                        dec_2_bcd((msf >> 16) & 0xFF), dec_2_bcd((msf >> 8) & 0xFF), dec_2_bcd((msf >> 0) & 0xFF), 0x01};

                    memcpy(data, sync_header, 16);
#if 0
                    char *hex = mg_hexdump(sync_header, sizeof(sync_header));
                    log_debug("sync_header %s", hex);
                    free(hex);
#endif
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
        uint32_t hunknumber = (fad) / fenrir_user_data->sectors_per_hunk;
        uint32_t hunkoffset = (fad) % fenrir_user_data->sectors_per_hunk;

        // log_debug("chdread at: %08x/%08x (%d)", hunknumber, hunkoffset, fad);
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

// =============================================================
// Close all alocated ressources
// =============================================================
uint32_t cdfmt_close(fenrir_user_data_t *fenrir_user_data)
{
    if (fenrir_user_data->type == IMAGE_TYPE_MAME_LDR)
    {
        for (uint32_t i = 0; i < fenrir_user_data->toc.numtrks; i++)
        {
            fclose(fenrir_user_data->toc.tracks[i].fp);
        }
    }
    else if (fenrir_user_data->type == IMAGE_TYPE_CHD)
    {
        chd_close(fenrir_user_data->chd_file);
        if (fenrir_user_data->chd_hunk_buffer)
        {
            free(fenrir_user_data->chd_hunk_buffer);
        }
    }
    memset(fenrir_user_data, 0, sizeof(fenrir_user_data_t));
    return 0;
}
