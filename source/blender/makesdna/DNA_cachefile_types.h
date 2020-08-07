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

/* Representation of an object's path inside the Alembic file.
 * Note that this is not a file path. */
typedef struct AlembicObjectPath {
  struct AlembicObjectPath *next, *prev;

  char path[4096];
} AlembicObjectPath;

/* CacheFile::velocity_unit
 * Determines what temporal unit is used to interpret velocity vectors for motion blur effects. */
enum {
  CACHEFILE_VELOCITY_UNIT_FRAME,
  CACHEFILE_VELOCITY_UNIT_SECOND,
};

typedef struct CacheFile {
  ID id;
  struct AnimData *adt;

  /** Paths of the objects inside of the Alembic archive referenced by this CacheFile. */
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

  char _pad[3];

  char velocity_unit;
  /* Name of the velocity property in the Alembic file. */
  char velocity_name[64];

  /* Runtime */
  struct AbcArchiveHandle *handle;
  char handle_filepath[1024];
  struct GSet *handle_readers;
} CacheFile;

#ifdef __cplusplus
}
#endif
