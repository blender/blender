/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  /** Grid specific flags. */
  char grid_flag;
  char _pad0[3];

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
  /** Irradiance grid: number of directions to evaluate light transfer in. */
  int grid_bake_samples;
  /** Irradiance grid: Virtual offset parameters. */
  float grid_surface_bias;
  float grid_escape_bias;
  /** Irradiance grid: Sampling biases. */
  float grid_normal_bias;
  float grid_view_bias;
  float grid_facing_bias;
  float grid_validity_threshold;
  /** Irradiance grid: Dilation. */
  float grid_dilation_threshold;
  float grid_dilation_radius;
  char _pad1[4];

  /** Surface element density for scene surface cache. In surfel per unit distance. */
  float surfel_density;

  /**
   * Resolution of the light probe when baked to a texture. Contains `eLightProbeResolution`.
   */
  int resolution;

  /** Object visibility group, inclusive or exclusive. */
  struct Collection *visibility_grp;
} LightProbe;

/* LightProbe->resolution, World->probe_resolution. */
typedef enum eLightProbeResolution {
  LIGHT_PROBE_RESOLUTION_64 = 6,
  LIGHT_PROBE_RESOLUTION_128 = 7,
  LIGHT_PROBE_RESOLUTION_256 = 8,
  LIGHT_PROBE_RESOLUTION_512 = 9,
  LIGHT_PROBE_RESOLUTION_1024 = 10,
  LIGHT_PROBE_RESOLUTION_2048 = 11,
} eLightProbeResolution;

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

/* Probe->grid_flag */
enum {
  LIGHTPROBE_GRID_CAPTURE_WORLD = (1 << 0),
  LIGHTPROBE_GRID_CAPTURE_INDIRECT = (1 << 1),
  LIGHTPROBE_GRID_CAPTURE_EMISSION = (1 << 2),
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
  /** Copy of GPU data to create GPUTextures on file read. */
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

/* -------------------------------------------------------------------- */
/** \name Irradiance grid data storage
 *
 * Each spherical harmonic band is stored separately. This allow loading only a specific band.
 * The layout of each array is set by the #LightProbeGridType.
 * Any unavailable data is be set to nullptr.
 * \{ */

/**
 * Irradiance data (RGB) stored along visibility (A).
 * This is the format used during baking and is used for visualizing the baking process.
 */
typedef struct LightProbeBakingData {
  float (*L0)[4];
  float (*L1_a)[4];
  float (*L1_b)[4];
  float (*L1_c)[4];
  float *validity;
  /* Capture offset. Only for debugging. */
  float (*virtual_offset)[4];
} LightProbeBakingData;

/**
 * Irradiance stored as RGB triple using scene linear color space.
 */
typedef struct LightProbeIrradianceData {
  float (*L0)[3];
  float (*L1_a)[3];
  float (*L1_b)[3];
  float (*L1_c)[3];
} LightProbeIrradianceData;

/**
 * Normalized visibility of distant light. Used for compositing grids together.
 */
typedef struct LightProbeVisibilityData {
  float *L0;
  float *L1_a;
  float *L1_b;
  float *L1_c;
} LightProbeVisibilityData;

/**
 * Used to avoid light leaks. Validate visibility between each grid sample.
 */
typedef struct LightProbeConnectivityData {
  /** Stores validity of the lighting for each grid sample. */
  uint8_t *validity;
} LightProbeConnectivityData;

/**
 * Defines one block of data inside the grid cache data arrays.
 * The block size if the same for all the blocks.
 */
typedef struct LightProbeBlockData {
  /* Offset inside the level-of-detail this block starts. */
  int offset[3];
  /* Level-of-detail this block is from. */
  int level;
} LightProbeBlockData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightProbeGridCacheFrame
 *
 * \{ */

/**
 * A frame worth of baked lighting data.
 */
typedef struct LightProbeGridCacheFrame {
  /** Number of samples in the highest level of detail. */
  int size[3];
  /** Spatial layout type of the data stored inside the data arrays. */
  int data_layout;

  /** Sparse or adaptive layout only: number of blocks inside data arrays. */
  int block_len;
  /** Sparse or adaptive layout only: size of a block in samples. All 3 dimensions are equal. */
  int block_size;
  /** Sparse or adaptive layout only: specify the blocks positions. */
  LightProbeBlockData *block_infos;

  /** In-progress baked data. Not stored in file. */
  LightProbeBakingData baking;
  /** Baked data. */
  LightProbeIrradianceData irradiance;
  LightProbeVisibilityData visibility;
  LightProbeConnectivityData connectivity;

  char _pad[4];

  /** Number of debug surfels. */
  int surfels_len;
  /** Debug surfels used to visualize the baking process. Not stored in file. */
  void *surfels;
} LightProbeGridCacheFrame;

/** #LightProbeGridCacheFrame.data_layout (int) */
enum {
  /** Simple uniform grid. Raw output from GPU. Used during the baking process. */
  LIGHTPROBE_CACHE_UNIFORM_GRID = 0,
  /** Fills the space with different level of resolution. More efficient storage. */
  LIGHTPROBE_CACHE_ADAPTIVE_RESOLUTION = 1,
};

/**
 * Per object container of baked data.
 * Should be called #LightProbeCache but name is already taken.
 */
typedef struct LightProbeObjectCache {
  /** Allow correct versioning / different types of data for the same layout. */
  int cache_type;
  /** True if this cache references the original object's cache. */
  char shared;
  /** True if the cache has been tagged for automatic baking. */
  char dirty;

  char _pad0[2];

  struct LightProbeGridCacheFrame *grid_static_cache;
} LightProbeObjectCache;

/** #LightProbeObjectCache.type (int) */
enum {
  /** Light cache was just created and is not yet baked. Keep as 0 for default value. */
  LIGHTPROBE_CACHE_TYPE_NONE = 0,
  /** Light cache is baked for one specific frame and capture all indirect lighting. */
  LIGHTPROBE_CACHE_TYPE_STATIC = 1,
};

/** \} */

#ifdef __cplusplus
}
#endif
