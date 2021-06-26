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

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#include "BLI_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct Object;

typedef struct LightProbe {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  /** For realtime probe objects. */
  char type;
  /** General purpose flags for probes. */
  char flag;
  /** Attenuation type. */
  char attenuation_type;
  /** Parallax type. */
  char parallax_type;

  /** Influence Radius. */
  float distinf;
  /** Parallax Radius. */
  float distpar;
  /** Influence falloff. */
  float falloff;

  float clipsta, clipend;

  /** VSM visibility biases. */
  float vis_bias, vis_bleedbias;
  float vis_blur;

  /** Intensity multiplier. */
  float intensity;

  /** Irradiance grid resolution. */
  int grid_resolution_x;
  int grid_resolution_y;
  int grid_resolution_z;
  char _pad1[4];

  /** Object to use as a parallax origin. */
  struct Object *parallax_ob;
  /** Image to use on as lighting data. */
  struct Image *image;
  /** Object visibility group, inclusive or exclusive. */
  struct Collection *visibility_grp;

  /* Runtime display data */
  float distfalloff, distgridinf;
  char _pad[8];
} LightProbe;

/* Probe->type */
enum {
  LIGHTPROBE_TYPE_CUBE = 0,
  LIGHTPROBE_TYPE_PLANAR = 1,
  LIGHTPROBE_TYPE_GRID = 2,
};

/* Probe->flag */
enum {
  LIGHTPROBE_FLAG_CUSTOM_PARALLAX = (1 << 0),
  LIGHTPROBE_FLAG_SHOW_INFLUENCE = (1 << 1),
  LIGHTPROBE_FLAG_SHOW_PARALLAX = (1 << 2),
  LIGHTPROBE_FLAG_SHOW_CLIP_DIST = (1 << 3),
  LIGHTPROBE_FLAG_SHOW_DATA = (1 << 4),
  LIGHTPROBE_FLAG_INVERT_GROUP = (1 << 5),
};

/* Probe->display */
enum {
  LIGHTPROBE_DISP_WIRE = 0,
  LIGHTPROBE_DISP_SHADED = 1,
  LIGHTPROBE_DISP_DIFFUSE = 2,
  LIGHTPROBE_DISP_REFLECTIVE = 3,
};

/* Probe->parallax && Probe->attenuation_type. */
enum {
  LIGHTPROBE_SHAPE_ELIPSOID = 0,
  LIGHTPROBE_SHAPE_BOX = 1,
};

/* ------- Eevee LightProbes ------- */
/* Needs to be there because written to file with the light-cache. */

/* IMPORTANT Padding in these structs is essential. It must match
 * GLSL struct definition in lightprobe_lib.glsl. */

/* Must match CubeData. */
typedef struct LightProbeCache {
  float position[3], parallax_type;
  float attenuation_fac;
  float attenuation_type;
  float _pad3[2];
  float attenuationmat[4][4];
  float parallaxmat[4][4];
} LightProbeCache;

/* Must match GridData. */
typedef struct LightGridCache {
  float mat[4][4];
  /** Offset to the first irradiance sample in the pool. */
  int resolution[3], offset;
  float corner[3], attenuation_scale;
  /** World space vector between 2 opposite cells. */
  float increment_x[3], attenuation_bias;
  float increment_y[3], level_bias;
  float increment_z[3], _pad4;
  float visibility_bias, visibility_bleed, visibility_range, _pad5;
} LightGridCache;

/* These are used as UBO data. They need to be aligned to size of vec4. */
BLI_STATIC_ASSERT_ALIGN(LightProbeCache, 16)
BLI_STATIC_ASSERT_ALIGN(LightGridCache, 16)

/* ------ Eevee Lightcache ------- */

typedef struct LightCacheTexture {
  struct GPUTexture *tex;
  /** Copy of GPU datas to create GPUTextures on file read. */
  char *data;
  int tex_size[3];
  char data_type;
  char components;
  char _pad[2];
} LightCacheTexture;

typedef struct LightCache {
  int flag;
  /** Version number to know if the cache data is compatible with this version of blender. */
  int version;
  /** Type of data this cache contains. */
  int type;
  /* only a single cache for now */
  /** Number of probes to use for rendering. */
  int cube_len, grid_len;
  /** Number of mipmap level to use. */
  int mips_len;
  /** Size of a visibility/reflection sample. */
  int vis_res, ref_res;
  char _pad[4][2];
  /* In the future, we could create a bigger texture containing
   * multiple caches (for animation) and interpolate between the
   * caches overtime to another texture. */
  LightCacheTexture grid_tx;
  /** Contains data for mipmap level 0. */
  LightCacheTexture cube_tx;
  /** Does not contains valid GPUTexture, only data. */
  LightCacheTexture *cube_mips;
  /* All lightprobes data contained in the cache. */
  LightProbeCache *cube_data;
  LightGridCache *grid_data;
} LightCache;

/* Bump the version number for lightcache data structure changes. */
#define LIGHTCACHE_STATIC_VERSION 2

/* LightCache->type */
enum {
  LIGHTCACHE_TYPE_STATIC = 0,
};

/* LightCache->flag */
enum {
  LIGHTCACHE_BAKED = (1 << 0),
  LIGHTCACHE_BAKING = (1 << 1),
  LIGHTCACHE_CUBE_READY = (1 << 2),
  LIGHTCACHE_GRID_READY = (1 << 3),
  /* Update tagging */
  LIGHTCACHE_UPDATE_CUBE = (1 << 4),
  LIGHTCACHE_UPDATE_GRID = (1 << 5),
  LIGHTCACHE_UPDATE_WORLD = (1 << 6),
  LIGHTCACHE_UPDATE_AUTO = (1 << 7),
  /** Invalid means we tried to alloc it but failed. */
  LIGHTCACHE_INVALID = (1 << 8),
  /** The data present in the cache is valid but unusable on this GPU. */
  LIGHTCACHE_NOT_USABLE = (1 << 9),
};

/* EEVEE_LightCacheTexture->data_type */
enum {
  LIGHTCACHETEX_BYTE = (1 << 0),
  LIGHTCACHETEX_FLOAT = (1 << 1),
  LIGHTCACHETEX_UINT = (1 << 2),
};

#ifdef __cplusplus
}
#endif
