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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GSet;

/* CacheFile::type */
typedef enum {
  CACHEFILE_TYPE_ALEMBIC = 1,
  CACHEFILE_TYPE_USD = 2,
  CACHE_FILE_TYPE_INVALID = 0,
} eCacheFileType;

/* CacheFile::flag */
enum {
  CACHEFILE_DS_EXPAND = (1 << 0),
  CACHEFILE_UNUSED_0 = (1 << 1),
};

#if 0 /* UNUSED */
/* CacheFile::draw_flag */
enum {
  CACHEFILE_KEYFRAME_DRAWN = (1 << 0),
};
#endif

/* Representation of an object's path inside the archive.
 * Note that this is not a file path. */
typedef struct CacheObjectPath {
  struct CacheObjectPath *next, *prev;

  char path[4096];
} CacheObjectPath;

/* CacheFile::velocity_unit
 * Determines what temporal unit is used to interpret velocity vectors for motion blur effects. */
enum {
  CACHEFILE_VELOCITY_UNIT_FRAME,
  CACHEFILE_VELOCITY_UNIT_SECOND,
};

typedef struct CacheFile {
  ID id;
  struct AnimData *adt;

  /** Paths of the objects inside of the archive referenced by this CacheFile. */
  ListBase object_paths;

  /** 1024 = FILE_MAX. */
  char filepath[1024];

  char is_sequence;
  char forward_axis;
  char up_axis;
  char override_frame;

  float scale;
  /** The frame/time to lookup in the cache file. */
  float frame;
  /** The frame offset to subtract. */
  float frame_offset;

  /** Animation flag. */
  short flag;
  short draw_flag; /* UNUSED */

  /* eCacheFileType enum. */
  char type;

  char _pad[2];

  char velocity_unit;
  /* Name of the velocity property in the archive. */
  char velocity_name[64];

  /* Runtime */
  struct CacheArchiveHandle *handle;
  char handle_filepath[1024];
  struct GSet *handle_readers;
} CacheFile;

#ifdef __cplusplus
}
#endif
