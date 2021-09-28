// license:BSD-3-Clause
// copyright-holders:R. Belmont
/***************************************************************************

    CDRDAO TOC parser for CHD compression frontend

***************************************************************************/
#ifndef MAME_LIB_UTIL_CHDCD_H
#define MAME_LIB_UTIL_CHDCD_H

#pragma once

#include "cdrom.h"

#include <system_error>

// error types
enum chd_error
{
	CHDERR_NONE,
	CHDERR_NO_INTERFACE,
	CHDERR_OUT_OF_MEMORY,
	CHDERR_NOT_OPEN,
	CHDERR_ALREADY_OPEN,
	CHDERR_INVALID_FILE,
	CHDERR_INVALID_PARAMETER,
	CHDERR_INVALID_DATA,
	CHDERR_FILE_NOT_FOUND,
	CHDERR_REQUIRES_PARENT,
	CHDERR_FILE_NOT_WRITEABLE,
	CHDERR_READ_ERROR,
	CHDERR_WRITE_ERROR,
	CHDERR_CODEC_ERROR,
	CHDERR_INVALID_PARENT,
	CHDERR_HUNK_OUT_OF_RANGE,
	CHDERR_DECOMPRESSION_ERROR,
	CHDERR_COMPRESSION_ERROR,
	CHDERR_CANT_CREATE_FILE,
	CHDERR_CANT_VERIFY,
	CHDERR_NOT_SUPPORTED,
	CHDERR_METADATA_NOT_FOUND,
	CHDERR_INVALID_METADATA_SIZE,
	CHDERR_UNSUPPORTED_VERSION,
	CHDERR_VERIFY_INCOMPLETE,
	CHDERR_INVALID_METADATA,
	CHDERR_INVALID_STATE,
	CHDERR_OPERATION_PENDING,
	CHDERR_UNSUPPORTED_FORMAT,
	CHDERR_UNKNOWN_COMPRESSION,
	CHDERR_WALKING_PARENT,
	CHDERR_COMPRESSING
};

struct chdcd_track_input_entry
{
	chdcd_track_input_entry() { reset(); }
	void reset() { fname.clear(); offset = idx0offs = idx1offs = 0; swap = false; }

	std::string fname;      // filename for each track
	uint32_t offset;      // offset in the data file for each track
	bool swap;          // data needs to be byte swapped
	uint32_t idx0offs;
	uint32_t idx1offs;
};

struct chdcd_track_input_info
{
	void reset() { for (auto & elem : track) elem.reset(); }

	chdcd_track_input_entry track[CD_MAX_TRACKS];
};


// std::error_condition chdcd_parse_toc(const char *tocfname, cdrom_toc &outtoc, chdcd_track_input_info &outinfo);

#endif // MAME_LIB_UTIL_CHDCD_H