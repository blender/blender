/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared structures, enums & defines between C++ and GLSL.
 * Can also include some math functions but they need to be simple enough to be valid in both
 * language.
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  pragma once

#  include "BLI_memory_utils.hh"
#  include "DRW_gpu_wrapper.hh"

#  include "draw_manager.hh"
#  include "draw_pass.hh"

#  include "eevee_defines.hh"

#  include "GPU_shader_shared.h"

namespace blender::eevee {

class ShadowDirectional;
class ShadowPunctual;

using namespace draw;

constexpr GPUSamplerState no_filter = GPUSamplerState::default_sampler();
constexpr GPUSamplerState with_filter = {GPU_SAMPLER_FILTERING_LINEAR};

#endif

#define UBO_MIN_MAX_SUPPORTED_SIZE 1 << 14

/* -------------------------------------------------------------------- */
/** \name Debug Mode
 * \{ */

/** These are just to make more sense of G.debug_value's values. Reserved range is 1-30. */
enum eDebugMode : uint32_t {
  DEBUG_NONE = 0u,
  /**
   * Gradient showing light evaluation hot-spots.
   */
  DEBUG_LIGHT_CULLING = 1u,
  /**
   * Show incorrectly down-sample tiles in red.
   */
  DEBUG_HIZ_VALIDATION = 2u,
  /**
   * Display IrradianceCache surfels.
   */
  DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL = 3u,
  DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE = 4u,
  DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY = 5u,
  DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER = 6u,
  /**
   * Display IrradianceCache virtual offset.
   */
  DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET = 7u,
  DEBUG_IRRADIANCE_CACHE_VALIDITY = 8u,
  /**
   * Show tiles depending on their status.
   */
  DEBUG_SHADOW_TILEMAPS = 10u,
  /**
   * Show content of shadow map. Used to verify projection code.
   */
  DEBUG_SHADOW_VALUES = 11u,
  /**
   * Show random color for each tile. Verify allocation and LOD assignment.
   */
  DEBUG_SHADOW_TILE_RANDOM_COLOR = 12u,
  /**
   * Show random color for each tile. Verify distribution and LOD transitions.
   */
  DEBUG_SHADOW_TILEMAP_RANDOM_COLOR = 13u,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Look-Up Table Generation
 * \{ */

enum PrecomputeType : uint32_t {
  LUT_GGX_BRDF_SPLIT_SUM = 0u,
  LUT_GGX_BTDF_SPLIT_SUM = 1u,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling
 * \{ */

enum eSamplingDimension : uint32_t {
  SAMPLING_FILTER_U = 0u,
  SAMPLING_FILTER_V = 1u,
  SAMPLING_LENS_U = 2u,
  SAMPLING_LENS_V = 3u,
  SAMPLING_TIME = 4u,
  SAMPLING_SHADOW_U = 5u,
  SAMPLING_SHADOW_V = 6u,
  SAMPLING_SHADOW_W = 7u,
  SAMPLING_SHADOW_X = 8u,
  SAMPLING_SHADOW_Y = 9u,
  SAMPLING_CLOSURE = 10u,
  SAMPLING_LIGHTPROBE = 11u,
  SAMPLING_TRANSPARENCY = 12u,
  SAMPLING_SSS_U = 13u,
  SAMPLING_SSS_V = 14u,
  SAMPLING_RAYTRACE_U = 15u,
  SAMPLING_RAYTRACE_V = 16u,
  SAMPLING_RAYTRACE_W = 17u,
  SAMPLING_RAYTRACE_X = 18u,
  SAMPLING_AO_U = 19u,
  SAMPLING_AO_V = 20u,
  SAMPLING_CURVES_U = 21u,
  SAMPLING_VOLUME_U = 22u,
  SAMPLING_VOLUME_V = 23u,
  SAMPLING_VOLUME_W = 24u
};

/**
 * IMPORTANT: Make sure the array can contain all sampling dimensions.
 * Also note that it needs to be multiple of 4.
 */
#define SAMPLING_DIMENSION_COUNT 28

/* NOTE(@fclem): Needs to be used in #StorageBuffer because of arrays of scalar. */
struct SamplingData {
  /** Array containing random values from Low Discrepancy Sequence in [0..1) range. */
  float dimensions[SAMPLING_DIMENSION_COUNT];
};
BLI_STATIC_ASSERT_ALIGN(SamplingData, 16)

/* Returns total sample count in a web pattern of the given size. */
static inline int sampling_web_sample_count_get(int web_density, int in_ring_count)
{
  return ((in_ring_count * in_ring_count + in_ring_count) / 2) * web_density + 1;
}

/* Returns lowest possible ring count that contains at least sample_count samples. */
static inline int sampling_web_ring_count_get(int web_density, int sample_count)
{
  /* Inversion of web_sample_count_get(). */
  float x = 2.0f * (float(sample_count) - 1.0f) / float(web_density);
  /* Solving polynomial. We only search positive solution. */
  float discriminant = 1.0f + 4.0f * x;
  return int(ceilf(0.5f * (sqrtf(discriminant) - 1.0f)));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

enum eCameraType : uint32_t {
  CAMERA_PERSP = 0u,
  CAMERA_ORTHO = 1u,
  CAMERA_PANO_EQUIRECT = 2u,
  CAMERA_PANO_EQUISOLID = 3u,
  CAMERA_PANO_EQUIDISTANT = 4u,
  CAMERA_PANO_MIRROR = 5u
};

static inline bool is_panoramic(eCameraType type)
{
  return type > CAMERA_ORTHO;
}

struct CameraData {
  /* View Matrices of the camera, not from any view! */
  float4x4 persmat;
  float4x4 persinv;
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;
  /** Camera UV scale and bias. */
  float2 uv_scale;
  float2 uv_bias;
  /** Panorama parameters. */
  float2 equirect_scale;
  float2 equirect_scale_inv;
  float2 equirect_bias;
  float fisheye_fov;
  float fisheye_lens;
  /** Clipping distances. */
  float clip_near;
  float clip_far;
  eCameraType type;
  /** World space distance between view corners at unit distance from camera. */
  float screen_diagonal_length;
  float _pad0;
  float _pad1;
  float _pad2;

  bool1 initialized;

#ifdef __cplusplus
  /* Small constructor to allow detecting new buffers. */
  CameraData() : initialized(false){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(CameraData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Film
 * \{ */

#define FILM_PRECOMP_SAMPLE_MAX 16

enum eFilmWeightLayerIndex : uint32_t {
  FILM_WEIGHT_LAYER_ACCUMULATION = 0u,
  FILM_WEIGHT_LAYER_DISTANCE = 1u,
};

enum ePassStorageType : uint32_t {
  PASS_STORAGE_COLOR = 0u,
  PASS_STORAGE_VALUE = 1u,
  PASS_STORAGE_CRYPTOMATTE = 2u,
};

struct FilmSample {
  int2 texel;
  float weight;
  /** Used for accumulation. */
  float weight_sum_inv;
};
BLI_STATIC_ASSERT_ALIGN(FilmSample, 16)

struct FilmData {
  /** Size of the film in pixels. */
  int2 extent;
  /** Offset to convert from Display space to Film space, in pixels. */
  int2 offset;
  /** Size of the render buffers when rendering the main views, in pixels. */
  int2 render_extent;
  /** Offset to convert from Film space to Render space, in pixels. */
  int2 render_offset;
  /**
   * Sub-pixel offset applied to the window matrix.
   * NOTE: In final film pixel unit.
   * NOTE: Positive values makes the view translate in the negative axes direction.
   * NOTE: The origin is the center of the lower left film pixel of the area covered by a render
   * pixel if using scaled resolution rendering.
   */
  float2 subpixel_offset;
  /** Scaling factor to convert texel to uvs. */
  float2 extent_inv;
  /** Is true if history is valid and can be sampled. Bypass history to resets accumulation. */
  bool1 use_history;
  /** Is true if combined buffer is valid and can be re-projected to reduce variance. */
  bool1 use_reprojection;
  /** Is true if accumulation of non-filtered passes is needed. */
  bool1 has_data;
  /** Is true if accumulation of filtered passes is needed. */
  bool1 any_render_pass_1;
  bool1 any_render_pass_2;
  /** Controlled by user in lookdev mode or by render settings. */
  float background_opacity;
  float _pad0, _pad1, _pad2;
  /** Output counts per type. */
  int color_len, value_len;
  /** Index in color_accum_img or value_accum_img of each pass. -1 if pass is not enabled. */
  int mist_id;
  int normal_id;
  int vector_id;
  int diffuse_light_id;
  int diffuse_color_id;
  int specular_light_id;
  int specular_color_id;
  int volume_light_id;
  int emission_id;
  int environment_id;
  int shadow_id;
  int ambient_occlusion_id;
  /** Not indexed but still not -1 if enabled. */
  int depth_id;
  int combined_id;
  /** Id of the render-pass to be displayed. -1 for combined. */
  int display_id;
  /** Storage type of the render-pass to be displayed. */
  ePassStorageType display_storage_type;
  /** True if we bypass the accumulation and directly output the accumulation buffer. */
  bool1 display_only;
  /** Start of AOVs and number of aov. */
  int aov_color_id, aov_color_len;
  int aov_value_id, aov_value_len;
  /** Start of cryptomatte per layer (-1 if pass is not enabled). */
  int cryptomatte_object_id;
  int cryptomatte_asset_id;
  int cryptomatte_material_id;
  /** Max number of samples stored per layer (is even number). */
  int cryptomatte_samples_len;
  /** Settings to render mist pass */
  float mist_scale, mist_bias, mist_exponent;
  /** Scene exposure used for better noise reduction. */
  float exposure_scale;
  /** Scaling factor for scaled resolution rendering. */
  int scaling_factor;
  /** Film pixel filter radius. */
  float filter_radius;
  /** Precomputed samples. First in the table is the closest one. The rest is unordered. */
  int samples_len;
  /** Sum of the weights of all samples in the sample table. */
  float samples_weight_total;
  FilmSample samples[FILM_PRECOMP_SAMPLE_MAX];
};
BLI_STATIC_ASSERT_ALIGN(FilmData, 16)

static inline float film_filter_weight(float filter_radius, float sample_distance_sqr)
{
#if 1 /* Faster */
  /* Gaussian fitted to Blackman-Harris. */
  float r = sample_distance_sqr / (filter_radius * filter_radius);
  const float sigma = 0.284;
  const float fac = -0.5 / (sigma * sigma);
  float weight = expf(fac * r);
#else
  /* Blackman-Harris filter. */
  float r = M_2PI * saturate(0.5 + sqrtf(sample_distance_sqr) / (2.0 * filter_radius));
  float weight = 0.35875 - 0.48829 * cosf(r) + 0.14128 * cosf(2.0 * r) - 0.01168 * cosf(3.0 * r);
#endif
  return weight;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RenderBuffers
 * \{ */

/* Theoretical max is 128 as we are using texture array and VRAM usage.
 * However, the output_aov() function perform a linear search inside all the hashes.
 * If we find a way to avoid this we could bump this number up. */
#define AOV_MAX 16

struct AOVsInfoData {
  /* Use uint4 to workaround std140 packing rules.
   * Only the x value is used. */
  uint4 hash_value[AOV_MAX];
  uint4 hash_color[AOV_MAX];
  /* Length of used data. */
  int color_len;
  int value_len;
  /** Id of the AOV to be displayed (from the start of the AOV array). -1 for combined. */
  int display_id;
  /** True if the AOV to be displayed is from the value accumulation buffer. */
  bool1 display_is_value;
};
BLI_STATIC_ASSERT_ALIGN(AOVsInfoData, 16)

struct RenderBuffersInfoData {
  AOVsInfoData aovs;
  /* Color. */
  int color_len;
  int normal_id;
  int diffuse_light_id;
  int diffuse_color_id;
  int specular_light_id;
  int specular_color_id;
  int volume_light_id;
  int emission_id;
  int environment_id;
  /* Value */
  int value_len;
  int shadow_id;
  int ambient_occlusion_id;
};
BLI_STATIC_ASSERT_ALIGN(RenderBuffersInfoData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name VelocityModule
 * \{ */

#define VELOCITY_INVALID 512.0

enum eVelocityStep : uint32_t {
  STEP_PREVIOUS = 0,
  STEP_NEXT = 1,
  STEP_CURRENT = 2,
};

struct VelocityObjectIndex {
  /** Offset inside #VelocityObjectBuf for each time-step. Indexed using eVelocityStep. */
  packed_int3 ofs;
  /** Temporary index to copy this to the #VelocityIndexBuf. */
  uint resource_id;

#ifdef __cplusplus
  VelocityObjectIndex() : ofs(-1, -1, -1), resource_id(-1){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(VelocityObjectIndex, 16)

struct VelocityGeometryIndex {
  /** Offset inside #VelocityGeometryBuf for each time-step. Indexed using eVelocityStep. */
  packed_int3 ofs;
  /** If true, compute deformation motion blur. */
  bool1 do_deform;
  /**
   * Length of data inside #VelocityGeometryBuf for each time-step.
   * Indexed using eVelocityStep.
   */
  packed_int3 len;

  int _pad0;

#ifdef __cplusplus
  VelocityGeometryIndex() : ofs(-1, -1, -1), do_deform(false), len(-1, -1, -1), _pad0(1){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(VelocityGeometryIndex, 16)

struct VelocityIndex {
  VelocityObjectIndex obj;
  VelocityGeometryIndex geo;
};
BLI_STATIC_ASSERT_ALIGN(VelocityGeometryIndex, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Blur
 * \{ */

#define MOTION_BLUR_TILE_SIZE 32
#define MOTION_BLUR_MAX_TILE 512 /* 16384 / MOTION_BLUR_TILE_SIZE */
struct MotionBlurData {
  /** As the name suggests. Used to avoid a division in the sampling. */
  float2 target_size_inv;
  /** Viewport motion scaling factor. Make blur relative to frame time not render time. */
  float2 motion_scale;
  /** Depth scaling factor. Avoid blurring background behind moving objects. */
  float depth_scale;

  float _pad0, _pad1, _pad2;
};
BLI_STATIC_ASSERT_ALIGN(MotionBlurData, 16)

/* For some reasons some GLSL compilers do not like this struct.
 * So we declare it as a uint array instead and do indexing ourselves. */
#ifdef __cplusplus
struct MotionBlurTileIndirection {
  /**
   * Stores indirection to the tile with the highest velocity covering each tile.
   * This is stored using velocity in the MSB to be able to use atomicMax operations.
   */
  uint prev[MOTION_BLUR_MAX_TILE][MOTION_BLUR_MAX_TILE];
  uint next[MOTION_BLUR_MAX_TILE][MOTION_BLUR_MAX_TILE];
};
BLI_STATIC_ASSERT_ALIGN(MotionBlurTileIndirection, 16)
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volumes
 * \{ */

struct VolumesInfoData {
  float2 coord_scale;
  float2 viewport_size_inv;
  packed_int3 tex_size;
  float light_clamp;
  packed_float3 inv_tex_size;
  int tile_size;
  int tile_size_lod;
  float shadow_steps;
  bool1 use_lights;
  bool1 use_soft_shadows;
  float depth_near;
  float depth_far;
  float depth_distribution;
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(VolumesInfoData, 16)

/* Volume slice to view space depth. */
static inline float volume_z_to_view_z(
    float near, float far, float distribution, bool is_persp, float z)
{
  if (is_persp) {
    /* Exponential distribution. */
    return (exp2(z / distribution) - near) / far;
  }
  else {
    /* Linear distribution. */
    return near + (far - near) * z;
  }
}

static inline float view_z_to_volume_z(
    float near, float far, float distribution, bool is_persp, float depth)
{
  if (is_persp) {
    /* Exponential distribution. */
    return distribution * log2(depth * far + near);
  }
  else {
    /* Linear distribution. */
    return (depth - near) * distribution;
  }
}

static inline float3 ndc_to_volume(float4x4 projection_matrix,
                                   float near,
                                   float far,
                                   float distribution,
                                   float2 coord_scale,
                                   float3 coord)
{
  bool is_persp = projection_matrix[3][3] == 0.0;

  /* get_view_z_from_depth */
  float d = 2.0 * coord.z - 1.0;
  if (is_persp) {
    coord.z = -projection_matrix[3][2] / (d + projection_matrix[2][2]);
  }
  else {
    coord.z = (d - projection_matrix[3][2]) / projection_matrix[2][2];
  }

  coord.z = view_z_to_volume_z(near, far, distribution, is_persp, coord.z);
  coord.x *= coord_scale.x;
  coord.y *= coord_scale.y;
  return coord;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth of field
 * \{ */

/* 5% error threshold. */
#define DOF_FAST_GATHER_COC_ERROR 0.05
#define DOF_GATHER_RING_COUNT 5
#define DOF_DILATE_RING_COUNT 3

struct DepthOfFieldData {
  /** Size of the render targets for gather & scatter passes. */
  int2 extent;
  /** Size of a pixel in uv space (1.0 / extent). */
  float2 texel_size;
  /** Scale factor for anisotropic bokeh. */
  float2 bokeh_anisotropic_scale;
  float2 bokeh_anisotropic_scale_inv;
  /* Correction factor to align main target pixels with the filtered mipmap chain texture. */
  float2 gather_uv_fac;
  /** Scatter parameters. */
  float scatter_coc_threshold;
  float scatter_color_threshold;
  float scatter_neighbor_max_color;
  int scatter_sprite_per_row;
  /** Number of side the bokeh shape has. */
  float bokeh_blades;
  /** Rotation of the bokeh shape. */
  float bokeh_rotation;
  /** Multiplier and bias to apply to linear depth to Circle of confusion (CoC). */
  float coc_mul, coc_bias;
  /** Maximum absolute allowed Circle of confusion (CoC). Min of computed max and user max. */
  float coc_abs_max;
  /** Copy of camera type. */
  eCameraType camera_type;
  /** Weights of spatial filtering in stabilize pass. Not array to avoid alignment restriction. */
  float4 filter_samples_weight;
  float filter_center_weight;
  /** Max number of sprite in the scatter pass for each ground. */
  int scatter_max_rect;

  int _pad0, _pad1;
};
BLI_STATIC_ASSERT_ALIGN(DepthOfFieldData, 16)

struct ScatterRect {
  /** Color and CoC of the 4 pixels the scatter sprite represents. */
  float4 color_and_coc[4];
  /** Rect center position in half pixel space. */
  float2 offset;
  /** Rect half extent in half pixel space. */
  float2 half_extent;
};
BLI_STATIC_ASSERT_ALIGN(ScatterRect, 16)

/** WORKAROUND(@fclem): This is because this file is included before common_math_lib.glsl. */
#ifndef M_PI
#  define EEVEE_PI
#  define M_PI 3.14159265358979323846 /* pi */
#endif

static inline float coc_radius_from_camera_depth(DepthOfFieldData dof, float depth)
{
  depth = (dof.camera_type != CAMERA_ORTHO) ? 1.0f / depth : depth;
  return dof.coc_mul * depth + dof.coc_bias;
}

static inline float regular_polygon_side_length(float sides_count)
{
  return 2.0f * sinf(M_PI / sides_count);
}

/* Returns intersection ratio between the radius edge at theta and the regular polygon edge.
 * Start first corners at theta == 0. */
static inline float circle_to_polygon_radius(float sides_count, float theta)
{
  /* From Graphics Gems from CryENGINE 3 (Siggraph 2013) by Tiago Sousa (slide 36). */
  float side_angle = (2.0f * M_PI) / sides_count;
  return cosf(side_angle * 0.5f) /
         cosf(theta - side_angle * floorf((sides_count * theta + M_PI) / (2.0f * M_PI)));
}

/* Remap input angle to have homogenous spacing of points along a polygon edge.
 * Expects theta to be in [0..2pi] range. */
static inline float circle_to_polygon_angle(float sides_count, float theta)
{
  float side_angle = (2.0f * M_PI) / sides_count;
  float halfside_angle = side_angle * 0.5f;
  float side = floorf(theta / side_angle);
  /* Length of segment from center to the middle of polygon side. */
  float adjacent = circle_to_polygon_radius(sides_count, 0.0f);

  /* This is the relative position of the sample on the polygon half side. */
  float local_theta = theta - side * side_angle;
  float ratio = (local_theta - halfside_angle) / halfside_angle;

  float halfside_len = regular_polygon_side_length(sides_count) * 0.5f;
  float opposite = ratio * halfside_len;

  /* NOTE: atan(y_over_x) has output range [-M_PI_2..M_PI_2]. */
  float final_local_theta = atanf(opposite / adjacent);

  return side * side_angle + final_local_theta;
}

#ifdef EEVEE_PI
#  undef M_PI
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Culling
 * \{ */

/* Number of items we can cull. Limited by how we store CullingZBin. */
#define CULLING_MAX_ITEM 65536
/* Fine grained subdivision in the Z direction. Limited by the LDS in z-binning compute shader. */
#define CULLING_ZBIN_COUNT 4096
/* Max tile map resolution per axes. */
#define CULLING_TILE_RES 16

struct LightCullingData {
  /** Scale applied to tile pixel coordinates to get target UV coordinate. */
  float2 tile_to_uv_fac;
  /** Scale and bias applied to linear Z to get zbin. */
  float zbin_scale;
  float zbin_bias;
  /** Valid item count in the source data array. */
  uint items_count;
  /** Items that are processed by the 2.5D culling. */
  uint local_lights_len;
  /** Items that are **NOT** processed by the 2.5D culling (i.e: Sun Lights). */
  uint sun_lights_len;
  /** Number of items that passes the first culling test. (local lights only) */
  uint visible_count;
  /** Extent of one square tile in pixels. */
  float tile_size;
  /** Number of tiles on the X/Y axis. */
  uint tile_x_len;
  uint tile_y_len;
  /** Number of word per tile. Depends on the maximum number of lights. */
  uint tile_word_len;
};
BLI_STATIC_ASSERT_ALIGN(LightCullingData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lights
 * \{ */

#define LIGHT_NO_SHADOW -1

enum eLightType : uint32_t {
  LIGHT_SUN = 0u,
  LIGHT_SUN_ORTHO = 1u,
  LIGHT_POINT = 10u,
  LIGHT_SPOT = 11u,
  LIGHT_RECT = 20u,
  LIGHT_ELLIPSE = 21u
};

static inline bool is_area_light(eLightType type)
{
  return type >= LIGHT_RECT;
}

static inline bool is_sun_light(eLightType type)
{
  return type < LIGHT_POINT;
}

struct LightData {
  /** Normalized object matrix. Last column contains data accessible using the following macros. */
  float4x4 object_mat;
  /** Packed data in the last column of the object_mat. */
#define _area_size_x object_mat[0][3]
#define _area_size_y object_mat[1][3]
#define _radius _area_size_x
#define _spot_mul object_mat[2][3]
#define _spot_bias object_mat[3][3]
  /** Scale to convert from world units to tile space of the clipmap_lod_max. */
#define _clipmap_origin_x object_mat[2][3]
#define _clipmap_origin_y object_mat[3][3]
  /** Aliases for axes. */
#ifndef USE_GPU_SHADER_CREATE_INFO
#  define _right object_mat[0].xyz()
#  define _up object_mat[1].xyz()
#  define _back object_mat[2].xyz()
#  define _position object_mat[3].xyz()
#else
#  define _right object_mat[0].xyz
#  define _up object_mat[1].xyz
#  define _back object_mat[2].xyz
#  define _position object_mat[3].xyz
#endif
  /** Punctual : Influence radius (inverted and squared) adjusted for Surface / Volume power. */
  float influence_radius_invsqr_surface;
  float influence_radius_invsqr_volume;
  /** Punctual : Maximum influence radius. Used for culling. Equal to clip far distance. */
  float influence_radius_max;
  /** Special radius factor for point lighting. */
  float radius_squared;
  /** NOTE: It is ok to use float3 here. A float is declared right after it.
   * float3 is also aligned to 16 bytes. */
  packed_float3 color;
  /** Light Type. */
  eLightType type;
  /** Spot size. Aligned to size of float2. */
  float2 spot_size_inv;
  /** Spot angle tangent. */
  float spot_tan;
  /** Reuse for directional LOD bias. */
#define _clipmap_lod_bias spot_tan
  /** Power depending on shader type. */
  float diffuse_power;
  float specular_power;
  float volume_power;
  float transmit_power;

  /** --- Shadow Data --- */
  /** Directional : Near clip distance. Float stored as int for atomic operations. */
  int clip_near;
  int clip_far;
  /** Directional : Clip-map LOD range to avoid sampling outside of valid range. */
  int clipmap_lod_min;
  int clipmap_lod_max;
  /** Index of the first tile-map. */
  int tilemap_index;
  /** Directional : Offset of the LOD min in LOD min tile units. */
  int2 clipmap_base_offset;
  /** Punctual & Directional : Normal matrix packed for automatic bias. */
  float2 normal_mat_packed;
};
BLI_STATIC_ASSERT_ALIGN(LightData, 16)

static inline int light_tilemap_max_get(LightData light)
{
  /* This is not something we need in performance critical code. */
  return light.tilemap_index + (light.clipmap_lod_max - light.clipmap_lod_min);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadows
 *
 * Shadow data for either a directional shadow or a punctual shadow.
 *
 * A punctual shadow is composed of 1, 5 or 6 shadow regions.
 * Regions are sorted in this order -Z, +X, -X, +Y, -Y, +Z.
 * Face index is computed from light's object space coordinates.
 *
 * A directional light shadow is composed of multiple clip-maps with each level
 * covering twice as much area as the previous one.
 * \{ */

enum eShadowProjectionType : uint32_t {
  SHADOW_PROJECTION_CUBEFACE = 0u,
  SHADOW_PROJECTION_CLIPMAP = 1u,
  SHADOW_PROJECTION_CASCADE = 2u,
};

static inline int2 shadow_cascade_grid_offset(int2 base_offset, int level_relative)
{
  return (base_offset * level_relative) / (1 << 16);
}

/**
 * Small descriptor used for the tile update phase. Updated by CPU & uploaded to GPU each redraw.
 */
struct ShadowTileMapData {
  /** Cached, used for rendering. */
  float4x4 viewmat;
  /** Precomputed matrix, not used for rendering but for tagging. */
  float4x4 winmat;
  /** Punctual : Corners of the frustum. (vec3 padded to vec4) */
  float4 corners[4];
  /** Integer offset of the center of the 16x16 tiles from the origin of the tile space. */
  int2 grid_offset;
  /** Shift between previous and current grid_offset. Allows update tagging. */
  int2 grid_shift;
  /** True for punctual lights. */
  eShadowProjectionType projection_type;
  /** Multiple of SHADOW_TILEDATA_PER_TILEMAP. Offset inside the tile buffer. */
  int tiles_index;
  /** Index of persistent data in the persistent data buffer. */
  int clip_data_index;
  /** Bias LOD to tag for usage to lower the amount of tile used. */
  float lod_bias;
  int _pad0;
  int _pad1;
  int _pad2;
  /** Near and far clip distances for punctual. */
  float clip_near;
  float clip_far;
  /** Half of the tilemap size in world units. Used to compute directional window matrix. */
  float half_size;
  /** Offset in local space to the tilemap center in world units. Used for directional winmat. */
  float2 center_offset;
};
BLI_STATIC_ASSERT_ALIGN(ShadowTileMapData, 16)

/**
 * Per tilemap data persistent on GPU.
 */
struct ShadowTileMapClip {
  /** Clip distances that were used to render the pages. */
  float clip_near_stored;
  float clip_far_stored;
  /** Near and far clip distances for directional. Float stored as int for atomic operations. */
  /** NOTE: These are positive just like camera parameters. */
  int clip_near;
  int clip_far;
};
BLI_STATIC_ASSERT_ALIGN(ShadowTileMapClip, 16)

struct ShadowPagesInfoData {
  /** Number of free pages in the free page buffer. */
  int page_free_count;
  /** Number of page allocations needed for this cycle. */
  int page_alloc_count;
  /** Index of the next cache page in the cached page buffer. */
  uint page_cached_next;
  /** Index of the first page in the buffer since the last defragment. */
  uint page_cached_start;
  /** Index of the last page in the buffer since the last defragment. */
  uint page_cached_end;

  int _pad0;
  int _pad1;
  int _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ShadowPagesInfoData, 16)

struct ShadowStatistics {
  /** Statistics that are read back to CPU after a few frame (to avoid stall). */
  int page_used_count;
  int page_update_count;
  int page_allocated_count;
  int page_rendered_count;
  int view_needed_count;
  int _pad0;
  int _pad1;
  int _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ShadowStatistics, 16)

/** Decoded tile data structure. */
struct ShadowTileData {
  /** Page inside the virtual shadow map atlas. */
  uint3 page;
  /** Page index inside pages_cached_buf. Only valid if `is_cached` is true. */
  uint cache_index;
  /** LOD pointed to LOD 0 tile page. (cube-map only). */
  uint lod;
  /** If the tile is needed for rendering. */
  bool is_used;
  /** True if an update is needed. This persists even if the tile gets unused. */
  bool do_update;
  /** True if the tile owns the page (mutually exclusive with `is_cached`). */
  bool is_allocated;
  /** True if the tile has been staged for rendering. This will remove the `do_update` flag. */
  bool is_rendered;
  /** True if the tile is inside the pages_cached_buf (mutually exclusive with `is_allocated`). */
  bool is_cached;
};
/** \note Stored packed as a uint. */
#define ShadowTileDataPacked uint

enum eShadowFlag : uint32_t {
  SHADOW_NO_DATA = 0u,
  SHADOW_IS_CACHED = (1u << 27u),
  SHADOW_IS_ALLOCATED = (1u << 28u),
  SHADOW_DO_UPDATE = (1u << 29u),
  SHADOW_IS_RENDERED = (1u << 30u),
  SHADOW_IS_USED = (1u << 31u)
};

static inline uint shadow_page_pack(uint3 page)
{
  /* NOTE: Trust the input to be in valid range.
   * But sometime this is used to encode invalid pages uint3(-1) and it needs to output uint(-1).
   */
  return (page.x << 0u) | (page.y << 2u) | (page.z << 4u);
}

static inline uint3 shadow_page_unpack(uint data)
{
  uint3 page;
  /* Tweaked for SHADOW_PAGE_PER_ROW = 4. */
  page.x = data & uint(SHADOW_PAGE_PER_ROW - 1);
  page.y = (data >> 2u) & uint(SHADOW_PAGE_PER_COL - 1);
  page.z = (data >> 4u);
  return page;
}

static inline ShadowTileData shadow_tile_unpack(ShadowTileDataPacked data)
{
  ShadowTileData tile;
  /* Tweaked for SHADOW_MAX_PAGE = 4096. */
  tile.page = shadow_page_unpack(data & uint(SHADOW_MAX_PAGE - 1));
  /* -- 12 bits -- */
  /* Tweaked for SHADOW_TILEMAP_LOD < 8. */
  tile.lod = (data >> 12u) & 7u;
  /* -- 15 bits -- */
  /* Tweaked for SHADOW_MAX_TILEMAP = 4096. */
  tile.cache_index = (data >> 15u) & 4095u;
  /* -- 27 bits -- */
  tile.is_used = (data & SHADOW_IS_USED) != 0;
  tile.is_cached = (data & SHADOW_IS_CACHED) != 0;
  tile.is_allocated = (data & SHADOW_IS_ALLOCATED) != 0;
  tile.is_rendered = (data & SHADOW_IS_RENDERED) != 0;
  tile.do_update = (data & SHADOW_DO_UPDATE) != 0;
  return tile;
}

static inline ShadowTileDataPacked shadow_tile_pack(ShadowTileData tile)
{
  uint data = shadow_page_pack(tile.page) & uint(SHADOW_MAX_PAGE - 1);
  data |= (tile.lod & 7u) << 12u;
  data |= (tile.cache_index & 4095u) << 15u;
  data |= (tile.is_used ? uint(SHADOW_IS_USED) : 0);
  data |= (tile.is_allocated ? uint(SHADOW_IS_ALLOCATED) : 0);
  data |= (tile.is_cached ? uint(SHADOW_IS_CACHED) : 0);
  data |= (tile.is_rendered ? uint(SHADOW_IS_RENDERED) : 0);
  data |= (tile.do_update ? uint(SHADOW_DO_UPDATE) : 0);
  return data;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Irradiance Cache
 * \{ */

struct SurfelRadiance {
  /* Actually stores radiance and world (sky) visibility. Stored normalized. */
  float4 front;
  float4 back;
  /* Accumulated weights per face. */
  float front_weight;
  float back_weight;
  float _pad0;
  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(SurfelRadiance, 16)

struct Surfel {
  /** World position of the surfel. */
  packed_float3 position;
  /** Previous surfel index in the ray link-list. Only valid after sorting. */
  int prev;
  /** World orientation of the surface. */
  packed_float3 normal;
  /** Next surfel index in the ray link-list. */
  int next;
  /** Surface albedo to apply to incoming radiance. */
  packed_float3 albedo_front;
  /** Distance along the ray direction for sorting. */
  float ray_distance;
  /** Surface albedo to apply to incoming radiance. */
  packed_float3 albedo_back;
  /** Cluster this surfel is assigned to. */
  int cluster_id;
  /** Surface radiance: Emission + Direct Lighting. */
  SurfelRadiance radiance_direct;
  /** Surface radiance: Indirect Lighting. Double buffered to avoid race conditions. */
  SurfelRadiance radiance_indirect[2];
};
BLI_STATIC_ASSERT_ALIGN(Surfel, 16)

struct CaptureInfoData {
  /** Number of surfels inside the surfel buffer or the needed len. */
  packed_int3 irradiance_grid_size;
  /** True if the surface shader needs to write the surfel data. */
  bool1 do_surfel_output;
  /** True if the surface shader needs to increment the surfel_len. */
  bool1 do_surfel_count;
  /** Number of surfels inside the surfel buffer or the needed len. */
  uint surfel_len;
  /** Total number of a ray for light transportation. */
  float sample_count;
  /** 0 based sample index. */
  float sample_index;
  /** Transform of the light-probe object. */
  float4x4 irradiance_grid_local_to_world;
  /** Transform of the light-probe object. */
  float4x4 irradiance_grid_world_to_local;
  /** Transform vectors from world space to local space. Does not have location component. */
  /** TODO(fclem): This could be a float3x4 or a float3x3 if padded correctly. */
  float4x4 irradiance_grid_world_to_local_rotation;
  /** Scene bounds. Stored as min & max and as int for atomic operations. */
  int scene_bound_x_min;
  int scene_bound_y_min;
  int scene_bound_z_min;
  int scene_bound_x_max;
  int scene_bound_y_max;
  int scene_bound_z_max;
  /* Max intensity a ray can have. */
  float clamp_direct;
  float clamp_indirect;
  float _pad1;
  float _pad2;
  /** Minimum distance between a grid sample and a surface. Used to compute virtual offset. */
  float min_distance_to_surface;
  /** Maximum world scale offset an irradiance grid sample can be baked with. */
  float max_virtual_offset;
  /** Radius of surfels. */
  float surfel_radius;
  /** Capture options. */
  bool1 capture_world_direct;
  bool1 capture_world_indirect;
  bool1 capture_visibility_direct;
  bool1 capture_visibility_indirect;
  bool1 capture_indirect;
  bool1 capture_emission;
  int _pad0;
};
BLI_STATIC_ASSERT_ALIGN(CaptureInfoData, 16)

struct SurfelListInfoData {
  /** Size of the grid used to project the surfels into linked lists. */
  int2 ray_grid_size;
  /** Maximum number of list. Is equal to `ray_grid_size.x * ray_grid_size.y`. */
  int list_max;

  int _pad0;
};
BLI_STATIC_ASSERT_ALIGN(SurfelListInfoData, 16)

struct IrradianceGridData {
  /** World to non-normalized local grid space [0..size-1]. Stored transposed for compactness. */
  float3x4 world_to_grid_transposed;
  /** Number of bricks for this grid. */
  packed_int3 grid_size;
  /** Index in brick descriptor list of the first brick of this grid. */
  int brick_offset;
  /** Biases to apply to the shading point in order to sample a valid probe. */
  float normal_bias;
  float view_bias;
  float facing_bias;
  int _pad1;
};
BLI_STATIC_ASSERT_ALIGN(IrradianceGridData, 16)

struct IrradianceBrick {
  /* Offset in pixel to the start of the data inside the atlas texture. */
  uint2 atlas_coord;
};
/** \note Stored packed as a uint. */
#define IrradianceBrickPacked uint

static inline IrradianceBrickPacked irradiance_brick_pack(IrradianceBrick brick)
{
  uint2 data = (uint2(brick.atlas_coord) & 0xFFFFu) << uint2(0u, 16u);
  IrradianceBrickPacked brick_packed = data.x | data.y;
  return brick_packed;
}

static inline IrradianceBrick irradiance_brick_unpack(IrradianceBrickPacked brick_packed)
{
  IrradianceBrick brick;
  brick.atlas_coord = (uint2(brick_packed) >> uint2(0u, 16u)) & uint2(0xFFFFu);
  return brick;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hierarchical-Z Buffer
 * \{ */

struct HiZData {
  /** Scale factor to remove HiZBuffer padding. */
  float2 uv_scale;

  float2 _pad0;
};
BLI_STATIC_ASSERT_ALIGN(HiZData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray-Tracing
 * \{ */

enum eClosureBits : uint32_t {
  CLOSURE_NONE = 0u,
  /** NOTE: These are used as stencil bits. So we are limited to 8bits. */
  CLOSURE_DIFFUSE = (1u << 0u),
  CLOSURE_SSS = (1u << 1u),
  CLOSURE_REFLECTION = (1u << 2u),
  CLOSURE_REFRACTION = (1u << 3u),
  /* Non-stencil bits. */
  CLOSURE_TRANSPARENCY = (1u << 8u),
  CLOSURE_EMISSION = (1u << 9u),
  CLOSURE_HOLDOUT = (1u << 10u),
  CLOSURE_VOLUME = (1u << 11u),
  CLOSURE_AMBIENT_OCCLUSION = (1u << 12u),
};

struct RayTraceData {
  /** ViewProjection matrix used to render the previous frame. */
  float4x4 history_persmat;
  /** Input resolution. */
  int2 full_resolution;
  /** Inverse of input resolution to get screen UVs. */
  float2 full_resolution_inv;
  /** Scale and bias to go from ray-trace resolution to input resolution. */
  int2 resolution_bias;
  int resolution_scale;
  /** View space thickness the objects. */
  float thickness;
  /** Determine how fast the sample steps are getting bigger. */
  float quality;
  /** Maximum brightness during lighting evaluation. */
  float brightness_clamp;
  /** Maximum roughness for which we will trace a ray. */
  float max_trace_roughness;
  /** If set to true will bypass spatial denoising. */
  bool1 skip_denoise;
  /** Closure being ray-traced. */
  eClosureBits closure_active;
  int _pad0;
  int _pad1;
  int _pad2;
};
BLI_STATIC_ASSERT_ALIGN(RayTraceData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ambient Occlusion
 * \{ */

struct AOData {
  float distance;
  float quality;
  float2 pixel_size;
};
BLI_STATIC_ASSERT_ALIGN(AOData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subsurface
 * \{ */

#define SSS_SAMPLE_MAX 64
#define SSS_BURLEY_TRUNCATE 16.0
#define SSS_BURLEY_TRUNCATE_CDF 0.9963790093708328
#define SSS_TRANSMIT_LUT_SIZE 64.0
#define SSS_TRANSMIT_LUT_RADIUS 1.218
#define SSS_TRANSMIT_LUT_SCALE ((SSS_TRANSMIT_LUT_SIZE - 1.0) / float(SSS_TRANSMIT_LUT_SIZE))
#define SSS_TRANSMIT_LUT_BIAS (0.5 / float(SSS_TRANSMIT_LUT_SIZE))
#define SSS_TRANSMIT_LUT_STEP_RES 64.0

struct SubsurfaceData {
  /** xy: 2D sample position [-1..1], zw: sample_bounds. */
  /* NOTE(fclem) Using float4 for alignment. */
  float4 samples[SSS_SAMPLE_MAX];
  /** Sample index after which samples are not randomly rotated anymore. */
  int jitter_threshold;
  /** Number of samples precomputed in the set. */
  int sample_len;
  int _pad0;
  int _pad1;
};
BLI_STATIC_ASSERT_ALIGN(SubsurfaceData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reflection Probes
 * \{ */

/** Mapping data to locate a reflection probe in texture. */
struct ReflectionProbeData {
  /**
   * Position of the light probe in world space.
   * World probe uses origin.
   *
   * 4th component is not used.
   */
  float4 pos;

  /** On which layer of the texture array is this reflection probe stored. */
  int layer;

  /**
   * Subdivision of the layer. 0 = no subdivision and resolution would be
   * ReflectionProbeModule::MAX_RESOLUTION.
   */
  int layer_subdivision;

  /**
   * Which area of the subdivided layer is the reflection probe located.
   *
   * A layer has (2^layer_subdivision)^2 areas.
   */
  int area_index;

  /**
   * LOD factor for mipmap selection.
   */
  float lod_factor;
};
BLI_STATIC_ASSERT_ALIGN(ReflectionProbeData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Data
 * \{ */

/* Combines data from several modules to avoid wasting binding slots. */
struct UniformData {
  AOData ao;
  CameraData camera;
  FilmData film;
  HiZData hiz;
  RayTraceData raytrace;
  RenderBuffersInfoData render_pass;
  SubsurfaceData subsurface;
  VolumesInfoData volumes;
};
BLI_STATIC_ASSERT_ALIGN(UniformData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Texture
 * \{ */

#define UTIL_TEX_SIZE 64
#define UTIL_BTDF_LAYER_COUNT 16
/* Scale and bias to avoid interpolation of the border pixel.
 * Remap UVs to the border pixels centers. */
#define UTIL_TEX_UV_SCALE ((UTIL_TEX_SIZE - 1.0f) / UTIL_TEX_SIZE)
#define UTIL_TEX_UV_BIAS (0.5f / UTIL_TEX_SIZE)

#define UTIL_BLUE_NOISE_LAYER 0
#define UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER 1
#define UTIL_LTC_MAT_LAYER 2
#define UTIL_LTC_MAG_LAYER 3
#define UTIL_BSDF_LAYER 3
#define UTIL_BTDF_LAYER 4
#define UTIL_DISK_INTEGRAL_LAYER 4
#define UTIL_DISK_INTEGRAL_COMP 3

/* __cplusplus is true when compiling with MSL, so include if inside a shader. */
#if !defined(__cplusplus) || defined(GPU_SHADER)
/* Fetch texel. Wrapping if above range. */
float4 utility_tx_fetch(sampler2DArray util_tx, float2 texel, float layer)
{
  return texelFetch(util_tx, int3(int2(texel) % UTIL_TEX_SIZE, layer), 0);
}

/* Sample at uv position. Filtered & Wrapping enabled. */
float4 utility_tx_sample(sampler2DArray util_tx, float2 uv, float layer)
{
  return textureLod(util_tx, float3(uv, layer), 0.0);
}

/* Sample at uv position but with scale and bias so that uv space bounds lie on texel centers. */
float4 utility_tx_sample_lut(sampler2DArray util_tx, float2 uv, float layer)
{
  /* Scale and bias coordinates, for correct filtered lookup. */
  uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;
  return textureLod(util_tx, float3(uv, layer), 0.0);
}

/* Sample LTC or BSDF LUTs with `cos_theta` and `roughness` as inputs. */
float4 utility_tx_sample_lut(sampler2DArray util_tx, float cos_theta, float roughness, float layer)
{
  /* LUTs are parameterized by `sqrt(1.0 - cos_theta)` for more precision near grazing incidence.
   */
  vec2 coords = vec2(roughness, sqrt(clamp(1.0 - cos_theta, 0.0, 1.0)));
  return utility_tx_sample_lut(util_tx, coords, layer);
}

#endif

/** \} */

/* __cplusplus is true when compiling with MSL, so ensure we are not inside a shader. */
#if defined(__cplusplus) && !defined(GPU_SHADER)

using AOVsInfoDataBuf = draw::StorageBuffer<AOVsInfoData>;
using CameraDataBuf = draw::UniformBuffer<CameraData>;
using DepthOfFieldDataBuf = draw::UniformBuffer<DepthOfFieldData>;
using DepthOfFieldScatterListBuf = draw::StorageArrayBuffer<ScatterRect, 16, true>;
using DrawIndirectBuf = draw::StorageBuffer<DrawCommand, true>;
using DispatchIndirectBuf = draw::StorageBuffer<DispatchCommand>;
using UniformDataBuf = draw::UniformBuffer<UniformData>;
using IrradianceGridDataBuf = draw::UniformArrayBuffer<IrradianceGridData, IRRADIANCE_GRID_MAX>;
using IrradianceBrickBuf = draw::StorageVectorBuffer<IrradianceBrickPacked, 16>;
using LightCullingDataBuf = draw::StorageBuffer<LightCullingData>;
using LightCullingKeyBuf = draw::StorageArrayBuffer<uint, LIGHT_CHUNK, true>;
using LightCullingTileBuf = draw::StorageArrayBuffer<uint, LIGHT_CHUNK, true>;
using LightCullingZbinBuf = draw::StorageArrayBuffer<uint, CULLING_ZBIN_COUNT, true>;
using LightCullingZdistBuf = draw::StorageArrayBuffer<float, LIGHT_CHUNK, true>;
using LightDataBuf = draw::StorageArrayBuffer<LightData, LIGHT_CHUNK>;
using MotionBlurDataBuf = draw::UniformBuffer<MotionBlurData>;
using MotionBlurTileIndirectionBuf = draw::StorageBuffer<MotionBlurTileIndirection, true>;
using RayTraceTileBuf = draw::StorageArrayBuffer<uint, 1024, true>;
using ReflectionProbeDataBuf =
    draw::UniformArrayBuffer<ReflectionProbeData, REFLECTION_PROBES_MAX>;
using SamplingDataBuf = draw::StorageBuffer<SamplingData>;
using ShadowStatisticsBuf = draw::StorageBuffer<ShadowStatistics>;
using ShadowPagesInfoDataBuf = draw::StorageBuffer<ShadowPagesInfoData>;
using ShadowPageHeapBuf = draw::StorageVectorBuffer<uint, SHADOW_MAX_PAGE>;
using ShadowPageCacheBuf = draw::StorageArrayBuffer<uint2, SHADOW_MAX_PAGE, true>;
using ShadowTileMapDataBuf = draw::StorageVectorBuffer<ShadowTileMapData, SHADOW_MAX_TILEMAP>;
using ShadowTileMapClipBuf = draw::StorageArrayBuffer<ShadowTileMapClip, SHADOW_MAX_TILEMAP, true>;
using ShadowTileDataBuf = draw::StorageArrayBuffer<ShadowTileDataPacked, SHADOW_MAX_TILE, true>;
using SurfelBuf = draw::StorageArrayBuffer<Surfel, 64>;
using SurfelRadianceBuf = draw::StorageArrayBuffer<SurfelRadiance, 64>;
using CaptureInfoBuf = draw::StorageBuffer<CaptureInfoData>;
using SurfelListInfoBuf = draw::StorageBuffer<SurfelListInfoData>;
using VelocityGeometryBuf = draw::StorageArrayBuffer<float4, 16, true>;
using VelocityIndexBuf = draw::StorageArrayBuffer<VelocityIndex, 16>;
using VelocityObjectBuf = draw::StorageArrayBuffer<float4x4, 16>;
using CryptomatteObjectBuf = draw::StorageArrayBuffer<float2, 16>;

}  // namespace blender::eevee
#endif
