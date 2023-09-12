/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * List of defines that are shared with the GPUShaderCreateInfos. We do this to avoid
 * dragging larger headers into the createInfo pipeline which would cause problems.
 */

#ifndef GPU_SHADER
#  pragma once
#endif

/* Look Up Tables. */
#define LUT_WORKGROUP_SIZE 16

/* Hierarchical Z down-sampling. */
#define HIZ_MIP_COUNT 8
/* NOTE: The shader is written to update 5 mipmaps using LDS. */
#define HIZ_GROUP_SIZE 32

/* Avoid too much overhead caused by resizing the light buffers too many time. */
#define LIGHT_CHUNK 256

#define CULLING_SELECT_GROUP_SIZE 256
#define CULLING_SORT_GROUP_SIZE 256
#define CULLING_ZBIN_GROUP_SIZE 1024
#define CULLING_TILE_GROUP_SIZE 256

/* Reflection Probes. */
#define REFLECTION_PROBES_MAX 256
#define REFLECTION_PROBE_GROUP_SIZE 16
/* Number of additional pixels on the border of an octahedral map to reserve for fixing seams.
 * Border size requires depends on the max number of mipmap levels. */
#define REFLECTION_PROBE_MIPMAP_LEVELS 5
#define REFLECTION_PROBE_BORDER_SIZE float(1 << (REFLECTION_PROBE_MIPMAP_LEVELS - 1))
#define REFLECTION_PROBE_SH_GROUP_SIZE 512
#define REFLECTION_PROBE_SH_SAMPLES_PER_GROUP 64

/**
 * IMPORTANT: Some data packing are tweaked for these values.
 * Be sure to update them accordingly.
 * SHADOW_TILEMAP_RES max is 32 because of the shared bitmaps used for LOD tagging.
 * It is also limited by the maximum thread group size (1024).
 */
#define SHADOW_TILEMAP_RES 32
#define SHADOW_TILEMAP_LOD 5 /* LOG2(SHADOW_TILEMAP_RES) */
#define SHADOW_TILEMAP_LOD0_LEN ((SHADOW_TILEMAP_RES / 1) * (SHADOW_TILEMAP_RES / 1))
#define SHADOW_TILEMAP_LOD1_LEN ((SHADOW_TILEMAP_RES / 2) * (SHADOW_TILEMAP_RES / 2))
#define SHADOW_TILEMAP_LOD2_LEN ((SHADOW_TILEMAP_RES / 4) * (SHADOW_TILEMAP_RES / 4))
#define SHADOW_TILEMAP_LOD3_LEN ((SHADOW_TILEMAP_RES / 8) * (SHADOW_TILEMAP_RES / 8))
#define SHADOW_TILEMAP_LOD4_LEN ((SHADOW_TILEMAP_RES / 16) * (SHADOW_TILEMAP_RES / 16))
#define SHADOW_TILEMAP_LOD5_LEN ((SHADOW_TILEMAP_RES / 32) * (SHADOW_TILEMAP_RES / 32))
#define SHADOW_TILEMAP_PER_ROW 64
#define SHADOW_TILEDATA_PER_TILEMAP \
  (SHADOW_TILEMAP_LOD0_LEN + SHADOW_TILEMAP_LOD1_LEN + SHADOW_TILEMAP_LOD2_LEN + \
   SHADOW_TILEMAP_LOD3_LEN + SHADOW_TILEMAP_LOD4_LEN + SHADOW_TILEMAP_LOD5_LEN)
#define SHADOW_PAGE_CLEAR_GROUP_SIZE 32
#define SHADOW_PAGE_RES 256
#define SHADOW_PAGE_LOD 8 /* LOG2(SHADOW_PAGE_RES) */
#define SHADOW_DEPTH_SCAN_GROUP_SIZE 8
#define SHADOW_AABB_TAG_GROUP_SIZE 64
#define SHADOW_MAX_TILEMAP 4096
#define SHADOW_MAX_TILE (SHADOW_MAX_TILEMAP * SHADOW_TILEDATA_PER_TILEMAP)
#define SHADOW_MAX_PAGE 4096
#define SHADOW_BOUNDS_GROUP_SIZE 64
#define SHADOW_CLIPMAP_GROUP_SIZE 64
#define SHADOW_VIEW_MAX 64 /* Must match DRW_VIEW_MAX. */
#define SHADOW_RENDER_MAP_SIZE (SHADOW_VIEW_MAX * SHADOW_TILEMAP_LOD0_LEN)
#define SHADOW_ATOMIC 1
#define SHADOW_PAGE_PER_ROW 4
#define SHADOW_PAGE_PER_COL 4
#define SHADOW_PAGE_PER_LAYER (SHADOW_PAGE_PER_ROW * SHADOW_PAGE_PER_COL)

/* Ray-tracing. */
#define RAYTRACE_GROUP_SIZE 8
/* Keep this as a define to avoid shader variations. */
#define RAYTRACE_RADIANCE_FORMAT GPU_R11F_G11F_B10F
#define RAYTRACE_VARIANCE_FORMAT GPU_R16F
#define RAYTRACE_TILEMASK_FORMAT GPU_R8UI

/* Minimum visibility size. */
#define LIGHTPROBE_FILTER_VIS_GROUP_SIZE 16

/* Film. */
#define FILM_GROUP_SIZE 16

/* Motion Blur. */
#define MOTION_BLUR_GROUP_SIZE 32
#define MOTION_BLUR_DILATE_GROUP_SIZE 512

/* Irradiance Cache. */
/** Maximum number of entities inside the cache. */
#define IRRADIANCE_GRID_MAX 64

/* Depth Of Field. */
#define DOF_TILES_SIZE 8
#define DOF_TILES_FLATTEN_GROUP_SIZE DOF_TILES_SIZE
#define DOF_TILES_DILATE_GROUP_SIZE 8
#define DOF_BOKEH_LUT_SIZE 32
#define DOF_MAX_SLIGHT_FOCUS_RADIUS 5
#define DOF_SLIGHT_FOCUS_SAMPLE_MAX 16
#define DOF_MIP_COUNT 4
#define DOF_REDUCE_GROUP_SIZE (1 << (DOF_MIP_COUNT - 1))
#define DOF_DEFAULT_GROUP_SIZE 32
#define DOF_STABILIZE_GROUP_SIZE 16
#define DOF_FILTER_GROUP_SIZE 8
#define DOF_GATHER_GROUP_SIZE DOF_TILES_SIZE
#define DOF_RESOLVE_GROUP_SIZE (DOF_TILES_SIZE * 2)

/* Ambient Occlusion. */
#define AMBIENT_OCCLUSION_PASS_TILE_SIZE 16

/* IrradianceBake. */
#define SURFEL_GROUP_SIZE 256
#define SURFEL_LIST_GROUP_SIZE 256
#define IRRADIANCE_GRID_GROUP_SIZE 4 /* In each dimension, so 4x4x4 workgroup size. */
#define IRRADIANCE_GRID_BRICK_SIZE 4 /* In each dimension, so 4x4x4 brick size. */
#define IRRADIANCE_BOUNDS_GROUP_SIZE 64

/* Volumes. */
#define VOLUME_GROUP_SIZE 4
#define VOLUME_INTEGRATION_GROUP_SIZE 8

/* Resource bindings. */

/* Textures. */
/* Used anywhere. (Starts at index 2, since 0 and 1 are used by draw_gpencil) */
#define RBUFS_UTILITY_TEX_SLOT 2
#define HIZ_TEX_SLOT 3
/* Only during surface shading (forward and deferred eval). */
#define SHADOW_TILEMAPS_TEX_SLOT 4
#define SHADOW_ATLAS_TEX_SLOT 5
#define IRRADIANCE_ATLAS_TEX_SLOT 6
#define REFLECTION_PROBE_TEX_SLOT 7
#define VOLUME_SCATTERING_TEX_SLOT 8
#define VOLUME_TRANSMITTANCE_TEX_SLOT 9

/* Images. */
#define RBUFS_COLOR_SLOT 0
#define RBUFS_VALUE_SLOT 1
#define RBUFS_CRYPTOMATTE_SLOT 2
#define GBUF_CLOSURE_SLOT 3
#define GBUF_COLOR_SLOT 4
/* Volume properties pass do not write to `rbufs`. Reuse the same bind points. */
#define VOLUME_PROP_SCATTERING_IMG_SLOT 0
#define VOLUME_PROP_EXTINCTION_IMG_SLOT 1
#define VOLUME_PROP_EMISSION_IMG_SLOT 2
#define VOLUME_PROP_PHASE_IMG_SLOT 3
/* Only during shadow rendering. */
#define SHADOW_ATLAS_IMG_SLOT 4

/* Uniform Buffers. */
/* Slot 0 is GPU_NODE_TREE_UBO_SLOT. */
#define UNIFORM_BUF_SLOT 1
/* Only during surface shading (forward and deferred eval). */
#define IRRADIANCE_GRID_BUF_SLOT 2
#define REFLECTION_PROBE_BUF_SLOT 3
/* Only during pre-pass. */
#define VELOCITY_CAMERA_PREV_BUF 2
#define VELOCITY_CAMERA_CURR_BUF 3
#define VELOCITY_CAMERA_NEXT_BUF 4

/* Storage Buffers. */
#define LIGHT_CULL_BUF_SLOT 0
#define LIGHT_BUF_SLOT 1
#define LIGHT_ZBIN_BUF_SLOT 2
#define LIGHT_TILE_BUF_SLOT 3
#define IRRADIANCE_BRICK_BUF_SLOT 4
#define SAMPLING_BUF_SLOT 6
#define CRYPTOMATTE_BUF_SLOT 7
/* Only during surface capture. */
#define SURFEL_BUF_SLOT 4
/* Only during surface capture. */
#define CAPTURE_BUF_SLOT 5
/* Only during shadow rendering. */
#define SHADOW_RENDER_MAP_BUF_SLOT 3
#define SHADOW_PAGE_INFO_SLOT 4
#define SHADOW_VIEWPORT_INDEX_BUF_SLOT 5

/* Only during pre-pass. */
#define VELOCITY_OBJ_PREV_BUF_SLOT 0
#define VELOCITY_OBJ_NEXT_BUF_SLOT 1
#define VELOCITY_GEO_PREV_BUF_SLOT 2
#define VELOCITY_GEO_NEXT_BUF_SLOT 3
#define VELOCITY_INDIRECTION_BUF_SLOT 4
