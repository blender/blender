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

#ifndef SQUARE
#  define SQUARE(x) ((x) * (x))
#endif

/* Look Up Tables. */
#define LUT_WORKGROUP_SIZE 16

/* Hierarchical Z down-sampling. */
#define HIZ_MIP_COUNT 7
/* NOTE: The shader is written to update 5 mipmaps using LDS. */
#define HIZ_GROUP_SIZE 32

/* Avoid too much overhead caused by resizing the light buffers too many time. */
#define LIGHT_CHUNK 256

#define CULLING_SELECT_GROUP_SIZE 256
#define CULLING_SORT_GROUP_SIZE 256
#define CULLING_ZBIN_GROUP_SIZE 1024
#define CULLING_TILE_GROUP_SIZE 256

/* Reflection Probes. */
/* When changed update parallel sum loop in `eevee_lightprobe_sphere_remap_comp.glsl`. */
#define SPHERE_PROBE_REMAP_GROUP_SIZE 32
#define SPHERE_PROBE_GROUP_SIZE 16
#define SPHERE_PROBE_SELECT_GROUP_SIZE 64
#define SPHERE_PROBE_MIPMAP_LEVELS 5
#define SPHERE_PROBE_SH_GROUP_SIZE 256
#define SPHERE_PROBE_SH_SAMPLES_PER_GROUP 64
/* Must be power of two for correct partitioning. */
#define SPHERE_PROBE_ATLAS_MAX_SUBDIV 12
#define SPHERE_PROBE_ATLAS_RES (1 << SPHERE_PROBE_ATLAS_MAX_SUBDIV)
/* Maximum number of thread-groups dispatched for remapping a probe to octahedral mapping. */
#define SPHERE_PROBE_MAX_HARMONIC SQUARE(SPHERE_PROBE_ATLAS_RES / SPHERE_PROBE_REMAP_GROUP_SIZE)
/* Start and end value for mixing sphere probe and volume probes. */
#define SPHERE_PROBE_MIX_START_ROUGHNESS 0.7
#define SPHERE_PROBE_MIX_END_ROUGHNESS 0.9
/* Roughness of the last mip map for sphere probes. */
#define SPHERE_PROBE_MIP_MAX_ROUGHNESS 0.7
/**
 * Limited by the UBO size limit `(16384 bytes / sizeof(SphereProbeData))`.
 */
#define SPHERE_PROBE_MAX 128

/** NOTE: Runtime format only. */
#define VOLUME_PROBE_FORMAT SFLOAT_16_16_16_16

/**
 * Limited by the performance impact it can cause.
 * Limited by the max layer count supported by a hardware (256).
 * Limited by the UBO size limit `(16384 bytes / sizeof(PlanarProbeData))`.
 */
#define PLANAR_PROBE_MAX 16

/**
 * IMPORTANT: Some data packing are tweaked for these values.
 * Be sure to update them accordingly.
 * SHADOW_TILEMAP_RES max is 32 because of the shared bitmaps used for LOD tagging.
 * It is also limited by the maximum thread group size (1024).
 */
#if 0
/* Useful for debugging the tile-copy version of the shadow rendering without making debugging
 * tools unresponsive. */
#  define SHADOW_TILEMAP_RES 4
#  define SHADOW_TILEMAP_LOD 2 /* LOG2(SHADOW_TILEMAP_RES) */
#else
#  define SHADOW_TILEMAP_RES 32
#  define SHADOW_TILEMAP_LOD 5 /* LOG2(SHADOW_TILEMAP_RES) */
#endif
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
/* Maximum number of relative LOD distance we can store. */
#define SHADOW_TILEMAP_MAX_CLIPMAP_LOD 8
#if 0
/* Useful for debugging the tile-copy version of the shadow rendering without making debugging
 * tools unresponsive. */
#  define SHADOW_PAGE_CLEAR_GROUP_SIZE 8
#  define SHADOW_PAGE_RES 8
#  define SHADOW_PAGE_LOD 3 /* LOG2(SHADOW_PAGE_RES) */
#else
#  define SHADOW_PAGE_CLEAR_GROUP_SIZE 32
#  define SHADOW_PAGE_RES 256
#  define SHADOW_PAGE_LOD 8 /* LOG2(SHADOW_PAGE_RES) */
#endif
/* For testing only. */
// #define SHADOW_FORCE_LOD0
#define SHADOW_MAP_MAX_RES (SHADOW_PAGE_RES * SHADOW_TILEMAP_RES)
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
#define SHADOW_MAX_STEP 16
#define SHADOW_MAX_RAY 4
#define SHADOW_ROG_ID 0

/* Gbuffer. */
/** IMPORTANT: Make sure all Gbuffer frame-buffer setup matches this. */
#define GBUF_HEADER_FB_LAYER_COUNT 1
#define GBUF_CLOSURE_FB_LAYER_COUNT 2
#define GBUF_NORMAL_FB_LAYER_COUNT 1

/* Deferred Lighting. */
#define DEFERRED_RADIANCE_FORMAT UINT_32
#define DEFERRED_GBUFFER_ROG_ID 0

/* Ray-tracing. */
#define RAYTRACE_GROUP_SIZE 8
/* Keep this as a define to avoid shader variations. */
#define RAYTRACE_RADIANCE_FORMAT UFLOAT_11_11_10
#define RAYTRACE_RAYTIME_FORMAT SFLOAT_32
#define RAYTRACE_VARIANCE_FORMAT SFLOAT_16
#define RAYTRACE_TILEMASK_FORMAT UINT_8

/* Sub-Surface Scattering. */
#define SUBSURFACE_GROUP_SIZE RAYTRACE_GROUP_SIZE
#define SUBSURFACE_RADIANCE_FORMAT UFLOAT_11_11_10
#define SUBSURFACE_OBJECT_ID_FORMAT UINT_16

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
#define VOLUME_HIT_DEPTH_MAX 16

/* Velocity. */
#define VERTEX_COPY_GROUP_SIZE 64

/* Utility Texture. */
#define UTIL_TEX_SIZE 64
#define UTIL_BTDF_LAYER_COUNT 16
/* Scale and bias to avoid interpolation of the border pixel.
 * Remap UVs to the border pixels centers. */
#define UTIL_TEX_UV_SCALE ((UTIL_TEX_SIZE - 1.0f) / UTIL_TEX_SIZE)
#define UTIL_TEX_UV_BIAS (0.5f / UTIL_TEX_SIZE)

#define UTIL_BLUE_NOISE_LAYER 0
#define UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER 1
#define UTIL_LTC_MAT_LAYER 2
#define UTIL_BSDF_LAYER 3
#define UTIL_BTDF_LAYER 4
#define UTIL_DISK_INTEGRAL_LAYER UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER
#define UTIL_DISK_INTEGRAL_COMP 3

/* Could be somewhere else. */
#ifdef GPU_SHADER
#  if defined(GPU_FRAGMENT_SHADER)
#    define UTIL_TEXEL float2(gl_FragCoord.xy)
#  elif defined(GPU_COMPUTE_SHADER)
#    define UTIL_TEXEL float2(gl_GlobalInvocationID.xy)
#  elif defined(GPU_VERTEX_SHADER)
#    define UTIL_TEXEL float2(gl_VertexID, 0)
#  elif defined(GPU_LIBRARY_SHADER)
#    define UTIL_TEXEL float2(0)
#  endif
#endif

/* Resource bindings. */

/* Textures. */
/** WARNING: Don't forget to update the reserved slots info. */
/* Used anywhere. (Starts at index 2, since 0 and 1 are used by draw_gpencil) */
#define RBUFS_UTILITY_TEX_SLOT 2
#define HIZ_TEX_SLOT 3
/* Only during surface shading (forward and deferred eval). */
#define SHADOW_TILEMAPS_TEX_SLOT 4
#define SHADOW_ATLAS_TEX_SLOT 5
#define VOLUME_PROBE_TEX_SLOT 6
#define SPHERE_PROBE_TEX_SLOT 7
#define VOLUME_SCATTERING_TEX_SLOT 8
#define VOLUME_TRANSMITTANCE_TEX_SLOT 9
/* Currently only used by ray-tracing, but might become used by forward too. */
#define PLANAR_PROBE_DEPTH_TEX_SLOT 10
#define PLANAR_PROBE_RADIANCE_TEX_SLOT 11
/* Reserved slots info */
#define MATERIAL_TEXTURE_RESERVED_SLOT_FIRST RBUFS_UTILITY_TEX_SLOT
#define MATERIAL_TEXTURE_RESERVED_SLOT_LAST_NO_EVAL HIZ_TEX_SLOT
#define MATERIAL_TEXTURE_RESERVED_SLOT_LAST_HYBRID SPHERE_PROBE_TEX_SLOT
#define MATERIAL_TEXTURE_RESERVED_SLOT_LAST_FORWARD VOLUME_TRANSMITTANCE_TEX_SLOT
#define MATERIAL_TEXTURE_RESERVED_SLOT_LAST_WORLD SPHERE_PROBE_TEX_SLOT

/* Images. */
#define RBUFS_COLOR_SLOT 0
#define RBUFS_VALUE_SLOT 1
#define RBUFS_CRYPTOMATTE_SLOT 2
#define GBUF_CLOSURE_SLOT 3
#define GBUF_NORMAL_SLOT 4
#define GBUF_HEADER_SLOT 5
/* Volume properties pass do not write to `rbufs`. Reuse the same bind points. */
#define VOLUME_PROP_SCATTERING_IMG_SLOT 0
#define VOLUME_PROP_EXTINCTION_IMG_SLOT 1
#define VOLUME_PROP_EMISSION_IMG_SLOT 2
#define VOLUME_PROP_PHASE_IMG_SLOT 3
#define VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT 4
#define VOLUME_OCCUPANCY_SLOT 5
/* Only during volume pre-pass. */
#define VOLUME_HIT_DEPTH_SLOT 0
#define VOLUME_HIT_COUNT_SLOT 1
/* Only during shadow rendering. */
#define SHADOW_ATLAS_IMG_SLOT 4

/* Uniform Buffers. */
/* Slot 0 is GPU_NODE_TREE_UBO_SLOT. */
#define UNIFORM_BUF_SLOT 1
/* Only during surface shading (forward and deferred eval). */
#define IRRADIANCE_GRID_BUF_SLOT 2
#define SPHERE_PROBE_BUF_SLOT 3
#define PLANAR_PROBE_BUF_SLOT 4
/* Only during pre-pass. */
#define VELOCITY_CAMERA_PREV_BUF 2
#define VELOCITY_CAMERA_CURR_BUF 3
#define VELOCITY_CAMERA_NEXT_BUF 4
#define CLIP_PLANE_BUF 5

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
#define SHADOW_RENDER_VIEW_BUF_SLOT 5

/* Only during pre-pass. */
#define VELOCITY_OBJ_PREV_BUF_SLOT 0
#define VELOCITY_OBJ_NEXT_BUF_SLOT 1
#define VELOCITY_GEO_PREV_BUF_SLOT 2
#define VELOCITY_GEO_NEXT_BUF_SLOT 3
#define VELOCITY_INDIRECTION_BUF_SLOT 4

#define CLOSURE_WEIGHT_CUTOFF 1e-5f
/* Treat closure as singular if the roughness is below this threshold. */
#define BSDF_ROUGHNESS_THRESHOLD 2e-2

/* Cannot use math libraries in shared headers yet. */
#define EEVEE_PI 3.14159265358979323846 /* pi */
