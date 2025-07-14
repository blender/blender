/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_endian_switch.h"
#include "BLI_sys_types.h"

struct FileReader;

struct BHead {
  /** Identifier for this #BHead. Can be any of BLO_CODE_* or an ID code like ID_OB. */
  int code;
  /** Identifier of the struct type that is stored in this block. */
  int SDNAnr;
  /**
   * Identifier the block had when it was written. This is used to remap memory blocks on load.
   * Typically, this is the pointer that the memory had when it was written.
   * This should be unique across the whole blend-file, except for `BLEND_DATA` blocks, which
   * should be unique within a same ID.
   */
  const void *old;
  /** Number of bytes in the block. */
  int64_t len;
  /** Number of structs in the array (1 for simple structs). */
  int64_t nr;
};

struct BHead4 {
  int code, len;
  uint old;
  int SDNAnr, nr;
};

struct SmallBHead8 {
  int code, len;
  uint64_t old;
  int SDNAnr, nr;
};

struct LargeBHead8 {
  int code;
  int SDNAnr;
  uint64_t old;
  int64_t len;
  int64_t nr;
};

enum class BHeadType {
  BHead4,
  SmallBHead8,
  LargeBHead8,
};

/** Make #BHead.code from 4 chars. */
#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define BLEND_MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* Little Endian */
#  define BLEND_MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif

/**
 * Codes used for #BHead.code.
 *
 * These coexist with ID codes such as #ID_OB, #ID_SCE ... etc.
 */
enum {
  /**
   * Arbitrary allocated memory
   * (typically owned by #ID's, will be freed when there are no users).
   */
  BLO_CODE_DATA = BLEND_MAKE_ID('D', 'A', 'T', 'A'),
  /**
   * Used for #Global struct.
   */
  BLO_CODE_GLOB = BLEND_MAKE_ID('G', 'L', 'O', 'B'),
  /**
   * Used for storing the encoded SDNA string
   * (decoded into an #SDNA on load).
   */
  BLO_CODE_DNA1 = BLEND_MAKE_ID('D', 'N', 'A', '1'),
  /**
   * Used to store thumbnail previews, written between #REND and #GLOB blocks,
   * (ignored for regular file reading).
   */
  BLO_CODE_TEST = BLEND_MAKE_ID('T', 'E', 'S', 'T'),
  /**
   * Used for #RenderInfo, basic Scene and frame range info,
   * can be easily read by other applications without writing a full blend file parser.
   */
  BLO_CODE_REND = BLEND_MAKE_ID('R', 'E', 'N', 'D'),
  /**
   * Used for #UserDef, (user-preferences data).
   * (written to #BLENDER_STARTUP_FILE & #BLENDER_USERPREF_FILE).
   */
  BLO_CODE_USER = BLEND_MAKE_ID('U', 'S', 'E', 'R'),
  /**
   * Terminate reading (no data).
   */
  BLO_CODE_ENDB = BLEND_MAKE_ID('E', 'N', 'D', 'B'),
};

/**
 * Parse the next #BHead in the file, increasing the file reader to after the #BHead.
 * This automatically converts the stored BHead (one of #BHeadType) to the runtime #BHead type.
 *
 * \return The next #BHEad or #std::nullopt if the file is exhausted.
 */
std::optional<BHead> BLO_readfile_read_bhead(FileReader *file, BHeadType type);

/**
 * Converts a BHead.old pointer from 64 to 32 bit. This can't work in the general case, but only
 * when the lower 32 bits of all relevant 64 bit pointers are different. Otherwise two different
 * pointers will map to the same, which will break things later on. There is no way to check for
 * that here unfortunately.
 */
inline uint32_t uint32_from_uint64_ptr(uint64_t ptr)
{
  /* NOTE: this is endianness-sensitive. */
  /* Switching endianness would be required to reduce the risk of two different 64bits pointers
   * generating the same 32bits value. */
  /* Behavior has to match #cast_pointer_64_to_32. */
  ptr >>= 3;
  return uint32_t(ptr);
}
