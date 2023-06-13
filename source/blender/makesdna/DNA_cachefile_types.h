/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/* CacheFileLayer::flag */
enum { CACHEFILE_LAYER_HIDDEN = (1 << 0) };

typedef struct CacheFileLayer {
  struct CacheFileLayer *next, *prev;

  /** 1024 = FILE_MAX. */
  char filepath[1024];
  int flag;
  int _pad;
} CacheFileLayer;

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

  ListBase layers;

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

  char _pad[4];

  /** Animation flag. */
  short flag;

  /* eCacheFileType enum. */
  char type;

  /**
   * Do not load data from the cache file and display objects in the scene as boxes, Cycles will
   * load objects directly from the CacheFile. Other render engines which can load Alembic data
   * directly can take care of rendering it themselves.
   */
  char use_render_procedural;

  char _pad1[3];

  /** Enable data prefetching when using the Cycles Procedural. */
  char use_prefetch;

  /** Size in megabytes for the prefetch cache used by the Cycles Procedural. */
  int prefetch_cache_size;

  /** Index of the currently selected layer in the UI, starts at 1. */
  int active_layer;

  char _pad2[3];

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
