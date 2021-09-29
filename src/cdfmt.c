#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "cdfmt.h"

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

    fseek(track_info->fp, offset, SEEK_SET);
    fread(data, 1, size, track_info->fp);

    return 0;
}