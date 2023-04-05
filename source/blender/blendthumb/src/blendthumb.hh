/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008-2021 Blender Foundation */

/** \file
 * \ingroup blendthumb
 *
 * Shared thumbnail extraction logic.
 *
 * Used for both MS-Windows DLL and Unix command line.
 */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_vector.hh"

struct FileReader;

struct Thumbnail {
  blender::Array<uint8_t> data;
  int width;
  int height;
};

enum eThumbStatus {
  BT_OK = 0,
  BT_FILE_ERR = 1,
  BT_COMPRES_ERR = 2,
  BT_DECOMPRESS_ERR = 3,
  BT_INVALID_FILE = 4,
  BT_EARLY_VERSION = 5,
  BT_INVALID_THUMB = 6,
  BT_ERROR = 9
};

std::optional<blender::Vector<uint8_t>> blendthumb_create_png_data_from_thumb(
    const Thumbnail *thumb);
/**
 * This function extracts the thumbnail from the .blend file into thumb.
 * Returns #BT_OK for success and the relevant error code otherwise.
 */
eThumbStatus blendthumb_create_thumb_from_file(struct FileReader *rawfile, Thumbnail *thumb);

/* INTEGER CODES */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif
