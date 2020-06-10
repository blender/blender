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
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_POINTCACHE_TYPES_H__
#define __DNA_POINTCACHE_TYPES_H__

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Point cache file data types:
 * - Used as `(1 << flag)` so poke jahka if you reach the limit of 15.
 * - To add new data types update:
 *   - #BKE_ptcache_data_size()
 *   - #ptcache_file_pointers_init()
 */
#define BPHYS_DATA_INDEX 0
#define BPHYS_DATA_LOCATION 1
#define BPHYS_DATA_SMOKE_LOW 1
#define BPHYS_DATA_VELOCITY 2
#define BPHYS_DATA_SMOKE_HIGH 2
#define BPHYS_DATA_ROTATION 3
#define BPHYS_DATA_DYNAMICPAINT 3
#define BPHYS_DATA_AVELOCITY 4 /* used for particles */
#define BPHYS_DATA_XCONST 4    /* used for cloth */
#define BPHYS_DATA_SIZE 5
#define BPHYS_DATA_TIMES 6
#define BPHYS_DATA_BOIDS 7

#define BPHYS_TOT_DATA 8

#define BPHYS_EXTRA_FLUID_SPRINGS 1
#define BPHYS_EXTRA_CLOTH_ACCELERATION 2

typedef struct PTCacheExtra {
  struct PTCacheExtra *next, *prev;
  unsigned int type, totdata;
  void *data;
} PTCacheExtra;

typedef struct PTCacheMem {
  struct PTCacheMem *next, *prev;
  unsigned int frame, totpoint;
  unsigned int data_types, flag;

  /** BPHYS_TOT_DATA. */
  void *data[8];
  /** BPHYS_TOT_DATA. */
  void *cur[8];

  struct ListBase extradata;
} PTCacheMem;

typedef struct PointCache {
  struct PointCache *next, *prev;
  /** Generic flag. */
  int flag;

  /**
   * The number of frames between cached frames.
   * This should probably be an upper bound for a per point adaptive step in the future,
   * buf for now it's the same for all points. Without adaptivity this can effect the perceived
   * simulation quite a bit though. If for example particles are colliding with a horizontal
   * plane (with high damping) they quickly come to a stop on the plane, however there are still
   * forces acting on the particle (gravity and collisions), so the particle velocity isn't
   * necessarily zero for the whole duration of the frame even if the particle seems stationary.
   * If all simulation frames aren't cached (step > 1) these velocities are interpolated into
   * movement for the non-cached frames.
   * The result will look like the point is oscillating around the collision location.
   * So for now cache step should be set to 1 for accurate reproduction of collisions.
   */
  int step;

  /** Current frame of simulation (only if SIMULATION_VALID). */
  int simframe;
  /** Simulation start frame. */
  int startframe;
  /** Simulation end frame. */
  int endframe;
  /** Frame being edited (runtime only). */
  int editframe;
  /** Last exact frame that's cached. */
  int last_exact;
  /** Used for editing cache - what is the last baked frame. */
  int last_valid;
  char _pad[4];

  /* for external cache files */
  /** Number of cached points. */
  int totpoint;
  /** Modifier stack index. */
  int index;
  short compression, rt;

  char name[64];
  char prev_name[64];
  char info[128];
  /** File path, 1024 = FILE_MAX. */
  char path[1024];

  /**
   * Array of length `endframe - startframe + 1` with flags to indicate cached frames.
   * Can be later used for other per frame flags too if needed.
   */
  char *cached_frames;
  int cached_frames_len;
  char _pad1[4];

  struct ListBase mem_cache;

  struct PTCacheEdit *edit;
  /** Free callback. */
  void (*free_edit)(struct PTCacheEdit *edit);
} PointCache;

/* pointcache->flag */
#define PTCACHE_BAKED (1 << 0)
#define PTCACHE_OUTDATED (1 << 1)
#define PTCACHE_SIMULATION_VALID (1 << 2)
#define PTCACHE_BAKING (1 << 3)
//#define PTCACHE_BAKE_EDIT         (1 << 4)
//#define PTCACHE_BAKE_EDIT_ACTIVE  (1 << 5)
#define PTCACHE_DISK_CACHE (1 << 6)
///* removed since 2.64 - [#30974], could be added back in a more useful way */
//#define PTCACHE_QUICK_CACHE       (1 << 7)
#define PTCACHE_FRAMES_SKIPPED (1 << 8)
#define PTCACHE_EXTERNAL (1 << 9)
#define PTCACHE_READ_INFO (1 << 10)
/** don't use the filename of the blendfile the data is linked from (write a local cache) */
#define PTCACHE_IGNORE_LIBPATH (1 << 11)
/**
 * High resolution cache is saved for smoke for backwards compatibility,
 * so set this flag to know it's a "fake" cache.
 */
#define PTCACHE_FAKE_SMOKE (1 << 12)
#define PTCACHE_IGNORE_CLEAR (1 << 13)

#define PTCACHE_FLAG_INFO_DIRTY (1 << 14)

/* PTCACHE_OUTDATED + PTCACHE_FRAMES_SKIPPED */
#define PTCACHE_REDO_NEEDED 258

#define PTCACHE_COMPRESS_NO 0
#define PTCACHE_COMPRESS_LZO 1
#define PTCACHE_COMPRESS_LZMA 2

#ifdef __cplusplus
}
#endif

#endif /* __DNA_POINTCACHE_TYPES_H__ */
