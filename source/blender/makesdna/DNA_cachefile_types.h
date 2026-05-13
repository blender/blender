/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

#include "BLI_enum_flags.hh"
#include "BLI_set.hh"

namespace blender {

/* CacheFile::type */
enum eCacheFileType : char {
  CACHEFILE_TYPE_ALEMBIC = 1,
  CACHEFILE_TYPE_USD = 2,
  CACHE_FILE_TYPE_INVALID = 0,
};

/* CacheFile::flag */
enum eCacheFile_Flag : short {
  CACHEFILE_DS_EXPAND = (1 << 0),
  CACHEFILE_UNUSED_0 = (1 << 1),
};
ENUM_OPERATORS(eCacheFile_Flag)

#if 0 /* UNUSED */
/* CacheFile::draw_flag */
enum eCacheFile_DrawFlag : short {
  CACHEFILE_KEYFRAME_DRAWN = (1 << 0),
};
#endif

/* CacheFileLayer::flag */
enum eCacheFileLayer_Flag : int {
  CACHEFILE_LAYER_HIDDEN = (1 << 0),
};
ENUM_OPERATORS(eCacheFileLayer_Flag)

/* CacheFile::velocity_unit
 * Determines what temporal unit is used to interpret velocity vectors for motion blur effects. */
enum eCacheFile_VelocityUnit : char {
  CACHEFILE_VELOCITY_UNIT_FRAME = 0,
  CACHEFILE_VELOCITY_UNIT_SECOND = 1,
};

/* Representation of an object's path inside the archive.
 * Note that this is not a file path. */
struct CacheObjectPath {
  struct CacheObjectPath *next = nullptr, *prev = nullptr;

  char path[/*PATH_MAX*/ 4096] = "";
};

struct CacheFileLayer {
  struct CacheFileLayer *next = nullptr, *prev = nullptr;

  char filepath[/*FILE_MAX*/ 1024] = "";
  eCacheFileLayer_Flag flag = {};
  int _pad = {};
};

struct CacheReader;
using CacheFileHandleReaderSet = Set<CacheReader **>;

struct CacheFile {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_CF;
#endif

  ID id;
  struct AnimData *adt = nullptr;

  /** Paths of the objects inside of the archive referenced by this CacheFile. */
  ListBaseT<CacheObjectPath> object_paths = {nullptr, nullptr};

  ListBaseT<CacheFileLayer> layers = {nullptr, nullptr};

  char filepath[/*FILE_MAX*/ 1024] = "";

  char is_sequence = false;
  char forward_axis = 0;
  char up_axis = 0;
  char override_frame = false;

  float scale = 1.0f;
  /** The frame/time to lookup in the cache file. */
  float frame = 0.0f;
  /** The frame offset to subtract. */
  float frame_offset = 0;

  /** Animation flag. */
  eCacheFile_Flag flag = {};

  eCacheFileType type = CACHE_FILE_TYPE_INVALID;

  char _pad1[1] = {};

  /** Index of the currently selected layer in the UI, starts at 1. */
  int active_layer = 0;

  char _pad2[3] = {};

  eCacheFile_VelocityUnit velocity_unit = {};
  /* Name of the velocity property in the archive. */
  char velocity_name[64] = "";

  char _pad3[4] = {};

  /* Runtime */
  struct CacheArchiveHandle *handle = nullptr;
  char handle_filepath[/*FILE_MAX*/ 1024] = "";
  CacheFileHandleReaderSet *handle_readers = nullptr;
};

}  // namespace blender
