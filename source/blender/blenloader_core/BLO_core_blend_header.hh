/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "BLO_core_bhead.hh"

/**
 * A low level blend file version number. Also see #decode_blender_header for how the first few
 * bytes of a .blend file are structured.
 *
 * 0: Uses #BHead4 or #SmallBHead8 for block headers depending on a .blend file header byte.
 * 1: Uses #LargeBHead8 for block headers.
 */

/**
 * Low level version 0: the header is 12 bytes long.
 * 0-6:  'BLENDER'
 * 7:    '-' for 8-byte pointers (#SmallBHead8) or '_' for 4-byte pointers (#BHead4)
 * 8:    'v' for little endian or 'V' for big endian
 * 9-11: 3 ASCII digits encoding #BLENDER_FILE_VERSION (e.g. '305' for Blender 3.5)
 */
#define BLEND_FILE_FORMAT_VERSION_0 0
/**
 * Lower level version 1: the header is 17 bytes long.
 * 0-6:   'BLENDER'
 * 7-8:   size of the header in bytes encoded as ASCII digits (always '17' currently)
 * 9:     always '-'
 * 10-11: File version format as ASCII digits (always '01' currently)
 * 12:    always 'v'
 * 13-16: 4 ASCII digits encoding #BLENDER_FILE_VERSION (e.g. '0405' for Blender 4.5)
 *
 * With this header, #LargeBHead8 is always used.
 */
#define BLEND_FILE_FORMAT_VERSION_1 1

/**
 * Only "modern" systems support writing files with #LargeBHead8 headers. Other systems are
 * deprecated. This reduces the amount of variation we have to deal with when reading .blend files.
 */
#define SYSTEM_SUPPORTS_WRITING_FILE_VERSION_1 (ENDIAN_ORDER == L_ENDIAN && sizeof(void *) == 8)

#define MIN_SIZEOFBLENDERHEADER 12
#define MAX_SIZEOFBLENDERHEADER 17

/** See #BLEND_FILE_FORMAT_VERSION_0 for the structure. */
#define SIZEOFBLENDERHEADER_VERSION_0 12
/** See #BLEND_FILE_FORMAT_VERSION_1 for the structure. */
#define SIZEOFBLENDERHEADER_VERSION_1 17

/** A header that has been parsed successfully. */
struct BlenderHeader {
  /** 4 or 8. */
  int pointer_size;
  /** L_ENDIAN or B_ENDIAN. */
  int endian;
  /** #BLENDER_FILE_VERSION. */
  int file_version;
  /** #BLEND_FILE_FORMAT_VERSION. */
  int file_format_version;

  BHeadType bhead_type() const;
};

/** The file is detected to be a Blender file, but it could not be decoded successfully. */
struct BlenderHeaderUnknown {};

/** The file is not a Blender file. */
struct BlenderHeaderInvalid {};

using BlenderHeaderVariant =
    std::variant<BlenderHeaderInvalid, BlenderHeaderUnknown, BlenderHeader>;

/**
 * Reads the header at the beginning of a .blend file and decodes it.
 */
BlenderHeaderVariant BLO_readfile_blender_header_decode(FileReader *file);
