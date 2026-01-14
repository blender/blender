/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

#include "BLI_assert.h"

namespace blender {

struct AnimData;
struct Object;
namespace gpu {
class Texture;
}  // namespace gpu

/** Bump the version number for light-cache data structure changes. */
#define LIGHTCACHE_STATIC_VERSION 2

/* Probe->type */
enum {
  LIGHTPROBE_TYPE_SPHERE = 0,
  LIGHTPROBE_TYPE_PLANE = 1,
  LIGHTPROBE_TYPE_VOLUME = 2,
};

/* Probe->flag */
enum {
  LIGHTPROBE_FLAG_CUSTOM_PARALLAX = (1 << 0),
  LIGHTPROBE_FLAG_SHOW_INFLUENCE = (1 << 1),
  LIGHTPROBE_FLAG_SHOW_PARALLAX = (1 << 2),
  LIGHTPROBE_FLAG_SHOW_CLIP_DIST = (1 << 3),
  LIGHTPROBE_FLAG_SHOW_DATA = (1 << 4),
  LIGHTPROBE_FLAG_INVERT_GROUP = (1 << 5),
  LIGHTPROBE_DS_EXPAND = (1 << 6),
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

/** #LightProbeGridCacheFrame.data_layout (int) */
enum {
  /** Simple uniform grid. Raw output from GPU. Used during the baking process. */
  LIGHTPROBE_CACHE_UNIFORM_GRID = 0,
  /** Fills the space with different level of resolution. More efficient storage. */
  LIGHTPROBE_CACHE_ADAPTIVE_RESOLUTION = 1,
};

/** #LightProbeObjectCache.type (int) */
enum {
  /** Light cache was just created and is not yet baked. Keep as 0 for default value. */
  LIGHTPROBE_CACHE_TYPE_NONE = 0,
  /** Light cache is baked for one specific frame and capture all indirect lighting. */
  LIGHTPROBE_CACHE_TYPE_STATIC = 1,
};

struct LightProbe {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_LP;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  /** For realtime probe objects. */
  char type = 0;
  /** General purpose flags for probes. */
  char flag = LIGHTPROBE_FLAG_SHOW_INFLUENCE;
  /** Attenuation type. */
  char attenuation_type = 0;
  /** Parallax type. */
  char parallax_type = 0;
  /** Grid specific flags. */
  char grid_flag = LIGHTPROBE_GRID_CAPTURE_INDIRECT | LIGHTPROBE_GRID_CAPTURE_EMISSION;
  char _pad0[3] = {};

  /** Influence Radius. */
  float distinf = 2.5f;
  /** Parallax Radius. */
  float distpar = 2.5f;
  /** Influence falloff. */
  float falloff = 0.2f;

  float clipsta = 0.8f, clipend = 20.0f;

  /** VSM visibility biases. */
  float vis_bias = 1.0f, vis_bleedbias = 0;
  float vis_blur = 0.2f;

  /** Intensity multiplier. */
  float intensity = 1.0f;

  /** Irradiance grid resolution. */
  int grid_resolution_x = 4;
  int grid_resolution_y = 4;
  int grid_resolution_z = 4;
  /** Irradiance grid: number of directions to evaluate light transfer in. */
  int grid_bake_samples = 2048;
  /** Irradiance grid: Virtual offset parameters. */
  float grid_surface_bias = 0.05;
  float grid_escape_bias = 0.1;
  /** Irradiance grid: Sampling biases. */
  float grid_normal_bias = 0.3f;
  float grid_view_bias = 0.0f;
  float grid_facing_bias = 0.5f;
  float grid_validity_threshold = 0.40f;
  /** Irradiance grid: Dilation. */
  float grid_dilation_threshold = 0.5f;
  float grid_dilation_radius = 1.0f;

  /** Light intensity clamp. */
  float grid_clamp_direct = 0.0f;
  float grid_clamp_indirect = 10.0f;

  /** Surface element density for scene surface cache. In surfel per unit distance. */
  int grid_surfel_density = 20;

  /** Object visibility group, inclusive or exclusive. */
  struct Collection *visibility_grp = nullptr;

  /** LIGHTPROBE_FLAG_SHOW_DATA display size. */
  float data_display_size = 0.1f;
  char _pad1[4] = {};
};

/* ------- Eevee LightProbes ------- */
/* Needs to be there because written to file with the light-cache. */

/* IMPORTANT Padding in these structs is essential. It must match
 * GLSL struct definition in lightprobe_lib.glsl. */

/* Must match CubeData. */
struct LightProbeCache {
  float position[3] = {}, parallax_type = 0;
  float attenuation_fac = 0;
  float attenuation_type = 0;
  float _pad3[2] = {};
  float attenuationmat[4][4] = {};
  float parallaxmat[4][4] = {};
};

/* Must match GridData. */
struct LightGridCache {
  float mat[4][4] = {};
  /** Offset to the first irradiance sample in the pool. */
  int resolution[3] = {}, offset = 0;
  float corner[3] = {}, attenuation_scale = 0;
  /** World space vector between 2 opposite cells. */
  float increment_x[3] = {}, attenuation_bias = 0;
  float increment_y[3] = {}, level_bias = 0;
  float increment_z[3] = {}, _pad4 = 0;
  float visibility_bias = 0, visibility_bleed = 0, visibility_range = 0, _pad5 = 0;
};

/* These are used as UBO data. They need to be aligned to size of vec4. */
BLI_STATIC_ASSERT_ALIGN(LightProbeCache, 16)
BLI_STATIC_ASSERT_ALIGN(LightGridCache, 16)

/* ------ Eevee Lightcache ------- */

struct LightCacheTexture {
  gpu::Texture *tex = nullptr;
  /** Copy of GPU data to create gpu::Textures on file read. */
  char *data = nullptr;
  int tex_size[3] = {};
  char data_type = 0;
  char components = 0;
  char _pad[2] = {};
};

struct LightCache {
  int flag = 0;
  /** Version number to know if the cache data is compatible with this version of blender. */
  int version = 0;
  /** Type of data this cache contains. */
  int type = 0;
  /* only a single cache for now */
  /** Number of probes to use for rendering. */
  int cube_len = 0, grid_len = 0;
  /** Number of mipmap level to use. */
  int mips_len = 0;
  /** Size of a visibility/reflection sample. */
  int vis_res = 0, ref_res = 0;
  char _pad[4][2] = {};
  /* In the future, we could create a bigger texture containing
   * multiple caches (for animation) and interpolate between the
   * caches overtime to another texture. */
  LightCacheTexture grid_tx;
  /** Contains data for mipmap level 0. */
  LightCacheTexture cube_tx;
  /** Does not contains valid gpu::Texture, only data. */
  LightCacheTexture *cube_mips = nullptr;
  /* All light-probes data contained in the cache. */
  LightProbeCache *cube_data = nullptr;
  LightGridCache *grid_data = nullptr;
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
struct LightProbeBakingData {
  float (*L0)[4] = {};
  float (*L1_a)[4] = {};
  float (*L1_b)[4] = {};
  float (*L1_c)[4] = {};
  float *validity = nullptr;
  /* Capture offset. Only for debugging. */
  float (*virtual_offset)[4] = {};
};

/**
 * Irradiance stored as RGB triple using scene linear color space.
 */
struct LightProbeIrradianceData {
  float (*L0)[3] = {};
  float (*L1_a)[3] = {};
  float (*L1_b)[3] = {};
  float (*L1_c)[3] = {};
};

/**
 * Normalized visibility of distant light. Used for compositing grids together.
 */
struct LightProbeVisibilityData {
  float *L0 = nullptr;
  float *L1_a = nullptr;
  float *L1_b = nullptr;
  float *L1_c = nullptr;
};

/**
 * Used to avoid light leaks. Validate visibility between each grid sample.
 */
struct LightProbeConnectivityData {
  /** Stores validity of the lighting for each grid sample. */
  uint8_t *validity = nullptr;
};

/**
 * Defines one block of data inside the grid cache data arrays.
 * The block size if the same for all the blocks.
 */
struct LightProbeBlockData {
  /* Offset inside the level-of-detail this block starts. */
  int offset[3] = {};
  /* Level-of-detail this block is from. */
  int level = 0;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightProbeGridCacheFrame
 *
 * \{ */

/**
 * A frame worth of baked lighting data.
 */
struct LightProbeGridCacheFrame {
  /** Number of samples in the highest level of detail. */
  int size[3] = {};
  /** Spatial layout type of the data stored inside the data arrays. */
  int data_layout = 0;

  /** Sparse or adaptive layout only: number of blocks inside data arrays. */
  int block_len = 0;
  /** Sparse or adaptive layout only: size of a block in samples. All 3 dimensions are equal. */
  int block_size = 0;
  /** Sparse or adaptive layout only: specify the blocks positions. */
  LightProbeBlockData *block_infos = nullptr;

  /** In-progress baked data. Not stored in file. */
  LightProbeBakingData baking;
  /** Baked data. */
  LightProbeIrradianceData irradiance;
  LightProbeVisibilityData visibility;
  LightProbeConnectivityData connectivity;

  char _pad[4] = {};

  /** Number of debug surfels. */
  int surfels_len = 0;
  /** Debug surfels used to visualize the baking process. Not stored in file. */
  void *surfels = nullptr;
};

/**
 * Per object container of baked data.
 * Should be called #LightProbeCache but name is already taken.
 */
struct LightProbeObjectCache {
  /** Allow correct versioning / different types of data for the same layout. */
  int cache_type = 0;
  /** True if this cache references the original object's cache. */
  char shared = 0;
  /** True if the cache has been tagged for automatic baking. */
  char dirty = 0;

  char _pad0[2] = {};

  struct LightProbeGridCacheFrame *grid_static_cache = nullptr;
};

/** \} */

}  // namespace blender
