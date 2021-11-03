/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008-2021 Blender Foundation.
 * All rights reserved.
 */

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
eThumbStatus blendthumb_create_thumb_from_file(struct FileReader *rawfile, Thumbnail *thumb);

/* INTEGER CODES */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif
