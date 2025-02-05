/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared structures, enums & defines between C++ and GLSL.
 * Can also include some math functions but they need to be simple enough to be valid in both
 * language.
 */

/* __cplusplus is true when compiling with MSL, so ensure we are not inside a shader. */
#if defined(GPU_SHADER) || defined(GLSL_CPP_STUBS)
#  define IS_CPP 0
#else
#  define IS_CPP 1
#endif

#if IS_CPP || defined(GLSL_CPP_STUBS)
#  pragma once

#  include "eevee_defines.hh"
#endif

#if IS_CPP
#  include "BLI_math_bits.h"
#  include "BLI_memory_utils.hh"

#  include "DRW_gpu_wrapper.hh"

#  include "draw_manager.hh"
#  include "draw_pass.hh"

#  include "GPU_shader_shared.hh"

namespace blender::eevee {

class ShadowDirectional;
class ShadowPunctual;

using namespace draw;

constexpr GPUSamplerState no_filter = GPUSamplerState::default_sampler();
constexpr GPUSamplerState with_filter = {GPU_SAMPLER_FILTERING_LINEAR};
#endif

/** WORKAROUND(@fclem): This is because this file is included before common_math_lib.glsl. */
#ifndef M_PI
#  define EEVEE_PI
#  define M_PI 3.14159265358979323846 /* pi */
#endif

enum eCubeFace : uint32_t {
  /* Ordering by culling order. If cone aperture is shallow, we cull the later view. */
  Z_NEG = 0u,
  X_POS = 1u,
  X_NEG = 2u,
  Y_POS = 3u,
  Y_NEG = 4u,
  Z_POS = 5u,
};

/* -------------------------------------------------------------------- */
/** \name Transform
 * \{ */

struct Transform {
  /* The transform is stored transposed for compactness. */
  float4 x, y, z;
#if IS_CPP
  Transform() = default;
  Transform(const float4x4 &tx)
      : x(tx[0][0], tx[1][0], tx[2][0], tx[3][0]),
        y(tx[0][1], tx[1][1], tx[2][1], tx[3][1]),
        z(tx[0][2], tx[1][2], tx[2][2], tx[3][2])
  {
  }

  operator float4x4() const
  {
    return float4x4(float4(x.x, y.x, z.x, 0.0f),
                    float4(x.y, y.y, z.y, 0.0f),
                    float4(x.z, y.z, z.z, 0.0f),
                    float4(x.w, y.w, z.w, 1.0f));
  }
#endif
};

static inline float4x4 transform_to_matrix(Transform t)
{
  return float4x4(float4(t.x.x, t.y.x, t.z.x, 0.0f),
                  float4(t.x.y, t.y.y, t.z.y, 0.0f),
                  float4(t.x.z, t.y.z, t.z.z, 0.0f),
                  float4(t.x.w, t.y.w, t.z.w, 1.0f));
}

static inline Transform transform_from_matrix(float4x4 m)
{
  Transform t;
  t.x = float4(m[0][0], m[1][0], m[2][0], m[3][0]);
  t.y = float4(m[0][1], m[1][1], m[2][1], m[3][1]);
  t.z = float4(m[0][2], m[1][2], m[2][2], m[3][2]);
  return t;
}

static inline float3 transform_x_axis(Transform t)
{
  return float3(t.x.x, t.y.x, t.z.x);
}
static inline float3 transform_y_axis(Transform t)
{
  return float3(t.x.y, t.y.y, t.z.y);
}
static inline float3 transform_z_axis(Transform t)
{
  return float3(t.x.z, t.y.z, t.z.z);
}
static inline float3 transform_location(Transform t)
{
  return float3(t.x.w, t.y.w, t.z.w);
}

#if !IS_CPP
static inline bool transform_equal(Transform a, Transform b)
{
  return all(equal(a.x, b.x)) && all(equal(a.y, b.y)) && all(equal(a.z, b.z));
}
#endif

static inline float3 transform_point(Transform t, float3 point)
{
  return float4(point, 1.0f) * float3x4(t.x, t.y, t.z);
}

static inline float3 transform_direction(Transform t, float3 direction)
{
  return direction * float3x3(float3(t.x.x, t.x.y, t.x.z),
                              float3(t.y.x, t.y.y, t.y.z),
                              float3(t.z.x, t.z.y, t.z.z));
}

static inline float3 transform_direction_transposed(Transform t, float3 direction)
{
  return float3x3(float3(t.x.x, t.x.y, t.x.z),
                  float3(t.y.x, t.y.y, t.y.z),
                  float3(t.z.x, t.z.y, t.z.z)) *
         direction;
}

/* Assumes the transform has unit scale. */
static inline float3 transform_point_inversed(Transform t, float3 point)
{
  return float3x3(float3(t.x.x, t.x.y, t.x.z),
                  float3(t.y.x, t.y.y, t.y.z),
                  float3(t.z.x, t.z.y, t.z.z)) *
         (point - transform_location(t));
}

/** \} */

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
  /**
   * Show storage cost of each pixel in the gbuffer.
   */
  DEBUG_GBUFFER_STORAGE = 14u,
  /**
   * Show evaluation cost of each pixel.
   */
  DEBUG_GBUFFER_EVALUATION = 15u,
  /**
   * Color different buffers of the depth of field.
   */
  DEBUG_DOF_PLANES = 16u,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Look-Up Table Generation
 * \{ */

enum PrecomputeType : uint32_t {
  LUT_GGX_BRDF_SPLIT_SUM = 0u,
  LUT_GGX_BTDF_IOR_GT_ONE = 1u,
  LUT_GGX_BSDF_SPLIT_SUM = 2u,
  LUT_BURLEY_SSS_PROFILE = 3u,
  LUT_RANDOM_WALK_SSS_PROFILE = 4u,
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
  SAMPLING_AO_W = 21u,
  SAMPLING_CURVES_U = 22u,
  SAMPLING_VOLUME_U = 23u,
  SAMPLING_VOLUME_V = 24u,
  SAMPLING_VOLUME_W = 25u,
  SAMPLING_SHADOW_I = 26u,
  SAMPLING_SHADOW_J = 27u,
  SAMPLING_SHADOW_K = 28u,
  SAMPLING_UNUSED_0 = 29u,
  SAMPLING_UNUSED_1 = 30u,
  SAMPLING_UNUSED_2 = 31u,
};

/**
 * IMPORTANT: Make sure the array can contain all sampling dimensions.
 * Also note that it needs to be multiple of 4.
 */
#define SAMPLING_DIMENSION_COUNT 32

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
  float orhodox_distance;
  float orhodox_factor;
  eCameraType type;
  /** World space distance between view corners at unit distance from camera. */
  float screen_diagonal_length;
  float _pad0;
  float _pad1;
  float _pad2;

  bool32_t initialized;

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

enum PassCategory : uint32_t {
  PASS_CATEGORY_DATA = 1u << 0,
  PASS_CATEGORY_COLOR_1 = 1u << 1,
  PASS_CATEGORY_COLOR_2 = 1u << 2,
  PASS_CATEGORY_COLOR_3 = 1u << 3,
  PASS_CATEGORY_AOV = 1u << 4,
  PASS_CATEGORY_CRYPTOMATTE = 1u << 5,
};
ENUM_OPERATORS(PassCategory, PASS_CATEGORY_CRYPTOMATTE)

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
  /** Size of the render buffers including overscan when rendering the main views, in pixels. */
  int2 render_extent;
  /**
   * Sub-pixel offset applied to the window matrix.
   * NOTE: In render target pixel unit.
   * NOTE: Positive values makes the view translate in the negative axes direction.
   * NOTE: The origin is the center of the lower left film pixel of the area covered by a render
   * pixel if using scaled resolution rendering.
   */
  float2 subpixel_offset;
  /** Scaling factor to convert texel to uvs. */
  float2 extent_inv;
  /**
   * Number of border pixels on all sides inside the render_extent that do not contribute to the
   * final image.
   */
  int overscan;
  /** Is true if history is valid and can be sampled. Bypass history to resets accumulation. */
  bool32_t use_history;
  /** Controlled by user in lookdev mode or by render settings. */
  float background_opacity;
  /** Output counts per type. */
  int color_len, value_len;
  /** Index in color_accum_img or value_accum_img of each pass. -1 if pass is not enabled. */
  int mist_id;
  int normal_id;
  int position_id;
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
  int transparent_id;
  /** Not indexed but still not -1 if enabled. */
  int depth_id;
  int combined_id;
  /** Id of the render-pass to be displayed. -1 for combined. */
  int display_id;
  /** Storage type of the render-pass to be displayed. */
  ePassStorageType display_storage_type;
  /** True if we bypass the accumulation and directly output the accumulation buffer. */
  bool32_t display_only;
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
  /** Software LOD bias to apply to when sampling texture inside the node-tree evaluation. */
  float texture_lod_bias;
  /** Film pixel filter radius. */
  float filter_radius;
  /** Precomputed samples. First in the table is the closest one. The rest is unordered. */
  int samples_len;
  /** Sum of the weights of all samples in the sample table. */
  float samples_weight_total;
  int _pad2;
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
  float r = M_TAU * saturate(0.5 + sqrtf(sample_distance_sqr) / (2.0 * filter_radius));
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
  bool32_t display_is_value;
};
BLI_STATIC_ASSERT_ALIGN(AOVsInfoData, 16)

struct RenderBuffersInfoData {
  AOVsInfoData aovs;
  /* Color. */
  int color_len;
  int normal_id;
  int position_id;
  int diffuse_light_id;
  int diffuse_color_id;
  int specular_light_id;
  int specular_color_id;
  int volume_light_id;
  int emission_id;
  int environment_id;
  int transparent_id;
  /* Value */
  int value_len;
  int shadow_id;
  int ambient_occlusion_id;
  int _pad0, _pad1;
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
  bool32_t do_deform;
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
  /* During object voxelization, we need to use an infinite projection matrix to avoid clipping
   * faces. But they cannot be used for recovering the view position from froxel position as they
   * are not invertible. We store the finite projection matrix and use it for this purpose. */
  float4x4 winmat_finite;
  float4x4 wininv_finite;
  /* Copies of the matrices above but without jittering. Used for re-projection. */
  float4x4 wininv_stable;
  float4x4 winmat_stable;
  /* Previous render sample copy of winmat_stable. */
  float4x4 history_winmat_stable;
  /* Transform from current view space to previous render sample view space. */
  float4x4 curr_view_to_past_view;
  /* Size of the froxel grid texture. */
  packed_int3 tex_size;
  /* Maximum light intensity during volume lighting evaluation. */
  float light_clamp;
  /* Inverse of size of the froxel grid. */
  packed_float3 inv_tex_size;
  /* Maximum light intensity during volume lighting evaluation. */
  float shadow_steps;
  /* 2D scaling factor to make froxel squared. */
  float2 coord_scale;
  /* Extent and inverse extent of the main shading view (render extent, not film extent). */
  float2 main_view_extent;
  float2 main_view_extent_inv;
  /* Size in main view pixels of one froxel in XY. */
  int tile_size;
  /* Hi-Z LOD to use during volume shadow tagging. */
  int tile_size_lod;
  /* Depth to froxel mapping. */
  float depth_near;
  float depth_far;
  float depth_distribution;
  /* Previous render sample copy of the depth mapping parameters. */
  float history_depth_near;
  float history_depth_far;
  float history_depth_distribution;
  /* Amount of history to blend during the scatter phase. */
  float history_opacity;

  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(VolumesInfoData, 16)

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
  uint scatter_max_rect;

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
  /* From Graphics Gems from CryENGINE 3 (SIGGRAPH 2013) by Tiago Sousa (slide 36). */
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
  /** Is the view being processed by light culling flipped (true for light probe planes). */
  bool32_t view_is_flipped;
  uint _pad0;
  uint _pad1;
  uint _pad2;
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
  /* Point light. */
  LIGHT_OMNI_SPHERE = 10u,
  LIGHT_OMNI_DISK = 11u,
  /* Spot light. */
  LIGHT_SPOT_SPHERE = 12u,
  LIGHT_SPOT_DISK = 13u,
  /* Area light. */
  LIGHT_RECT = 20u,
  LIGHT_ELLIPSE = 21u
};

enum LightingType : uint32_t {
  LIGHT_DIFFUSE = 0u,
  LIGHT_SPECULAR = 1u,
  LIGHT_TRANSMISSION = 2u,
  LIGHT_VOLUME = 3u,
  /* WORKAROUND: Special value used to tag translucent BSDF with thickness.
   * Fallback to LIGHT_DIFFUSE. */
  LIGHT_TRANSLUCENT_WITH_THICKNESS = 4u,
};

static inline bool is_area_light(eLightType type)
{
  return type >= LIGHT_RECT;
}

static inline bool is_point_light(eLightType type)
{
  return type >= LIGHT_OMNI_SPHERE && type <= LIGHT_SPOT_DISK;
}

static inline bool is_spot_light(eLightType type)
{
  return type == LIGHT_SPOT_SPHERE || type == LIGHT_SPOT_DISK;
}

static inline bool is_sphere_light(eLightType type)
{
  return type == LIGHT_SPOT_SPHERE || type == LIGHT_OMNI_SPHERE;
}

static inline bool is_oriented_disk_light(eLightType type)
{
  return type == LIGHT_SPOT_DISK || type == LIGHT_OMNI_DISK;
}

static inline bool is_sun_light(eLightType type)
{
  return type < LIGHT_OMNI_SPHERE;
}

static inline bool is_local_light(eLightType type)
{
  return type >= LIGHT_OMNI_SPHERE;
}

/* Using define because GLSL doesn't have inheritance, and encapsulation forces us to add some
 * unneeded padding. */
#define LOCAL_LIGHT_COMMON \
  /** --- Shadow Data --- */ \
  /** Shift to apply to the light origin to get the shadow projection origin. In light space. */ \
  packed_float3 shadow_position; \
  float _pad0; \
  /** Radius of the light for shadow ray casting. Simple scaling factor for rectangle lights. */ \
  float shadow_radius; \
  /** Radius of the light for shading. Bounding radius for rectangle lights. */ \
  float shape_radius; \
  /** Maximum influence radius. Used for culling. Equal to clip far distance. */ \
  float influence_radius_max; \
  /** Influence radius (inverted and squared) adjusted for Surface / Volume power. */ \
  float influence_radius_invsqr_surface; \
  float influence_radius_invsqr_volume; \
  /** Number of allocated tilemap for this local light. */ \
  int tilemaps_count;

/* Untyped local light data. Gets reinterpreted to LightSpotData and LightAreaData.
 * Allow access to local light common data without casting. */
struct LightLocalData {
  LOCAL_LIGHT_COMMON

  float _pad1;
  float _pad2;

  float2 _pad3;
  float _pad4;
  float _pad5;
};
BLI_STATIC_ASSERT_ALIGN(LightLocalData, 16)

/* Despite the name, is also used for omni light. */
struct LightSpotData {
  LOCAL_LIGHT_COMMON

  float _pad1;
  /** Scale and bias to spot equation parameter. Used for adjusting the falloff. */
  float spot_mul;

  /** Inverse spot size (in X and Y axes). */
  float2 spot_size_inv;
  /** Spot angle tangent. */
  float spot_tan;
  float spot_bias;
};
BLI_STATIC_ASSERT(sizeof(LightSpotData) == sizeof(LightLocalData), "Data size must match")

struct LightAreaData {
  LOCAL_LIGHT_COMMON

  float _pad2;
  float _pad3;

  /** Shape size. */
  float2 size;
  /** Scale to apply on top of `size` to get shadow tracing shape size. */
  float shadow_scale;
  float _pad6;
};
BLI_STATIC_ASSERT(sizeof(LightAreaData) == sizeof(LightLocalData), "Data size must match")

struct LightSunData {
  /* Sun direction for shading. Use object_to_world for getting into shadow space. */
  packed_float3 direction;
  /* Radius of the sun disk, one unit away from a shading point. */
  float shape_radius;

  /** --- Shadow Data --- */
  /** Offset of the LOD min in LOD min tile units. Split positive and negative for bit-shift. */
  int2 clipmap_base_offset_neg;
  int2 clipmap_base_offset_pos;

  /** Angle covered by the light shape for shadow ray casting. */
  float shadow_angle;
  float _pad5;
  float _pad3;
  float _pad4;

  /** Offset to convert from world units to tile space of the clipmap_lod_max. */
  float2 clipmap_origin;
  /** Clip-map LOD range to avoid sampling outside of valid range. */
  int clipmap_lod_min;
  int clipmap_lod_max;
};
BLI_STATIC_ASSERT(sizeof(LightSunData) == sizeof(LightLocalData), "Data size must match")

/* Enable when debugging. This is quite costly. */
#define SAFE_UNION_ACCESS 0

#if IS_CPP
/* C++ always uses union. */
#  define USE_LIGHT_UNION 1
#elif defined(GPU_BACKEND_METAL) && !SAFE_UNION_ACCESS
/* Metal supports union, but force usage of the getters if SAFE_UNION_ACCESS is enabled. */
#  define USE_LIGHT_UNION 1
#else
/* Use getter functions on GPU if not supported or if SAFE_UNION_ACCESS is enabled. */
#  define USE_LIGHT_UNION 0
#endif

struct LightData {
  /**
   * Normalized object to world matrix. Stored transposed for compactness.
   * Used for shading and shadowing local lights, or shadowing sun lights.
   * IMPORTANT: Not used for shading sun lights as this matrix is jittered.
   */
  Transform object_to_world;

  /** Power depending on shader type. Referenced by LightingType. */
  float4 power;
  /** Light Color. */
  packed_float3 color;
  /** Light Type. */
  eLightType type;

  /** --- Shadow Data --- */
  /** Near clip distances. Float stored as orderedIntBitsToFloat for atomic operations. */
  int clip_near;
  int clip_far;
  /** Index of the first tile-map. Set to LIGHT_NO_SHADOW if light is not casting shadow. */
  int tilemap_index;
  /* Radius in pixels for shadow filtering. */
  float filter_radius;

  /* Shadow Map resolution bias. */
  float lod_bias;
  /* Shadow Map resolution maximum resolution. */
  float lod_min;
  /* True if the light uses jittered soft shadows. */
  bool32_t shadow_jitter;
  float _pad2;
  uint2 light_set_membership;
  /** Used by shadow sync. */
  /* TODO(fclem): this should be part of #eevee::Light struct. But for some reason it gets cleared
   * to zero after each sync cycle. */
  uint2 shadow_set_membership;

#if USE_LIGHT_UNION
  union {
    LightLocalData local;
    LightSpotData spot;
    LightAreaData area;
    LightSunData sun;
  };
#else
  /* Use `light_*_data_get(light)` to access typed data. */
  LightLocalData do_not_access_directly;
#endif
};
BLI_STATIC_ASSERT_ALIGN(LightData, 16)

static inline float3 light_x_axis(LightData light)
{
  return transform_x_axis(light.object_to_world);
}
static inline float3 light_y_axis(LightData light)
{
  return transform_y_axis(light.object_to_world);
}
static inline float3 light_z_axis(LightData light)
{
  return transform_z_axis(light.object_to_world);
}
static inline float3 light_position_get(LightData light)
{
  return transform_location(light.object_to_world);
}

#ifdef GPU_SHADER
#  define CHECK_TYPE_PAIR(a, b)
#  define CHECK_TYPE(a, b)
#  define FLOAT_AS_INT floatBitsToInt
#  define INT_AS_FLOAT intBitsToFloat
#  define TYPECAST_NOOP

#else /* C++ */
#  define FLOAT_AS_INT float_as_int
#  define INT_AS_FLOAT int_as_float
#  define TYPECAST_NOOP
#endif

/* In addition to the static asserts that verify correct member assignment, also verify access on
 * the GPU so that only lights of a certain type can read for the appropriate union member.
 * Return cross platform garbage data as some platform can return cleared memory if we early exit.
 */
#if SAFE_UNION_ACCESS
#  ifdef GPU_SHADER
/* Should result in a beautiful zebra pattern on invalid load. */
#    if defined(GPU_FRAGMENT_SHADER)
#      define GARBAGE_VALUE sin(gl_FragCoord.x + gl_FragCoord.y)
#    elif defined(GPU_COMPUTE_SHADER)
#      define GARBAGE_VALUE \
        sin(float(gl_GlobalInvocationID.x + gl_GlobalInvocationID.y + gl_GlobalInvocationID.z))
#    else
#      define GARBAGE_VALUE sin(float(gl_VertexID))
#    endif

/* Can be set to zero if zebra creates out-of-bound accesses and crashes. At least avoid UB. */
// #    define GARBAGE_VALUE 0.0

#  else /* C++ */
#    define GARBAGE_VALUE 0.0f
#  endif

#  define SAFE_BEGIN(dst_type, src_type, src_, check) \
    src_type _src = src_; \
    dst_type _dst; \
    bool _validity_check = check; \
    float _garbage = GARBAGE_VALUE;

/* Assign garbage value if the light type check fails. */
#  define SAFE_ASSIGN_LIGHT_TYPE_CHECK(_type, _value) \
    (_validity_check ? (_value) : _type(_garbage))
#else
#  define SAFE_BEGIN(dst_type, src_type, src_, check) \
    UNUSED_VARS(check); \
    src_type _src = src_; \
    dst_type _dst;

#  define SAFE_ASSIGN_LIGHT_TYPE_CHECK(_type, _value) _value
#endif

#if USE_LIGHT_UNION
#  define DATA_MEMBER local
#else
#  define DATA_MEMBER do_not_access_directly
#endif

#define SAFE_READ_BEGIN(dst_type, light, check) \
  SAFE_BEGIN(dst_type, LightLocalData, light.DATA_MEMBER, check)
#define SAFE_READ_END() _dst

#define SAFE_WRITE_BEGIN(src_type, src, check) SAFE_BEGIN(LightLocalData, src_type, src, check)
#define SAFE_WRITE_END(light) light.DATA_MEMBER = _dst;

#define ERROR_OFS(a, b) "Offset of " STRINGIFY(a) " mismatch offset of " STRINGIFY(b)

/* This is a dangerous process, make sure to static assert every assignment. */
#define SAFE_ASSIGN(a, reinterpret_fn, in_type, b) \
  CHECK_TYPE_PAIR(_src.b, in_type(_dst.a)); \
  CHECK_TYPE_PAIR(_dst.a, reinterpret_fn(_src.b)); \
  _dst.a = reinterpret_fn(SAFE_ASSIGN_LIGHT_TYPE_CHECK(in_type, _src.b)); \
  BLI_STATIC_ASSERT(offsetof(decltype(_dst), a) == offsetof(decltype(_src), b), ERROR_OFS(a, b))

#define SAFE_ASSIGN_FLOAT(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, float, b);
#define SAFE_ASSIGN_FLOAT2(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, float2, b);
#define SAFE_ASSIGN_FLOAT3(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, float3, b);
#define SAFE_ASSIGN_INT(a, b) SAFE_ASSIGN(a, TYPECAST_NOOP, int, b);
#define SAFE_ASSIGN_FLOAT_AS_INT(a, b) SAFE_ASSIGN(a, FLOAT_AS_INT, float, b);
#define SAFE_ASSIGN_INT_AS_FLOAT(a, b) SAFE_ASSIGN(a, INT_AS_FLOAT, int, b);

#if !USE_LIGHT_UNION || IS_CPP

/* These functions are not meant to be used in C++ code. They are only defined on the C++ side for
 * static assertions. Hide them. */
#  if IS_CPP
namespace do_not_use {
#  endif

static inline LightSpotData light_local_data_get_ex(LightData light, bool check)
{
  SAFE_READ_BEGIN(LightSpotData, light, check)
  SAFE_ASSIGN_FLOAT3(shadow_position, shadow_position)
  SAFE_ASSIGN_FLOAT(_pad0, _pad0)
  SAFE_ASSIGN_FLOAT(shadow_radius, shadow_radius)
  SAFE_ASSIGN_FLOAT(shape_radius, shape_radius)
  SAFE_ASSIGN_FLOAT(influence_radius_max, influence_radius_max)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_surface, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, influence_radius_invsqr_volume)
  SAFE_ASSIGN_INT(tilemaps_count, tilemaps_count)
  SAFE_ASSIGN_FLOAT(spot_mul, _pad2)
  SAFE_ASSIGN_FLOAT2(spot_size_inv, _pad3)
  SAFE_ASSIGN_FLOAT(spot_tan, _pad4)
  SAFE_ASSIGN_FLOAT(spot_bias, _pad5)
  return SAFE_READ_END();
}

static inline LightData light_local_data_set(LightData light, LightSpotData spot_data)
{
  SAFE_WRITE_BEGIN(LightSpotData, spot_data, is_local_light(light.type))
  SAFE_ASSIGN_FLOAT3(shadow_position, shadow_position)
  SAFE_ASSIGN_FLOAT(_pad0, _pad0)
  SAFE_ASSIGN_FLOAT(shadow_radius, shadow_radius)
  SAFE_ASSIGN_FLOAT(shape_radius, shape_radius)
  SAFE_ASSIGN_FLOAT(influence_radius_max, influence_radius_max)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_surface, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, influence_radius_invsqr_volume)
  SAFE_ASSIGN_INT(tilemaps_count, tilemaps_count)
  SAFE_ASSIGN_FLOAT(_pad2, spot_mul)
  SAFE_ASSIGN_FLOAT2(_pad3, spot_size_inv)
  SAFE_ASSIGN_FLOAT(_pad4, spot_tan)
  SAFE_ASSIGN_FLOAT(_pad5, spot_bias)
  SAFE_WRITE_END(light)
  return light;
}

static inline LightSpotData light_local_data_get(LightData light)
{
  return light_local_data_get_ex(light, is_local_light(light.type));
}

static inline LightSpotData light_spot_data_get(LightData light)
{
  return light_local_data_get_ex(light, is_spot_light(light.type) || is_point_light(light.type));
}

static inline LightAreaData light_area_data_get(LightData light)
{
  SAFE_READ_BEGIN(LightAreaData, light, is_area_light(light.type))
  SAFE_ASSIGN_FLOAT(shape_radius, shape_radius)
  SAFE_ASSIGN_FLOAT(influence_radius_max, influence_radius_max)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_surface, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, influence_radius_invsqr_volume)
  SAFE_ASSIGN_FLOAT3(shadow_position, shadow_position)
  SAFE_ASSIGN_FLOAT(shadow_radius, shadow_radius)
  SAFE_ASSIGN_INT(tilemaps_count, tilemaps_count)
  SAFE_ASSIGN_FLOAT2(size, _pad3)
  SAFE_ASSIGN_FLOAT(shadow_scale, _pad4)
  return SAFE_READ_END();
}

static inline LightSunData light_sun_data_get(LightData light)
{
  SAFE_READ_BEGIN(LightSunData, light, is_sun_light(light.type))
  SAFE_ASSIGN_FLOAT3(direction, shadow_position)
  SAFE_ASSIGN_FLOAT(shape_radius, _pad0)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_neg.x, shadow_radius)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_neg.y, shape_radius)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_pos.x, influence_radius_max)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_base_offset_pos.y, influence_radius_invsqr_surface)
  SAFE_ASSIGN_FLOAT(shadow_angle, influence_radius_invsqr_volume)
  SAFE_ASSIGN_FLOAT2(clipmap_origin, _pad3)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_lod_min, _pad4)
  SAFE_ASSIGN_FLOAT_AS_INT(clipmap_lod_max, _pad5)
  return SAFE_READ_END();
}

static inline LightData light_sun_data_set(LightData light, LightSunData sun_data)
{
  SAFE_WRITE_BEGIN(LightSunData, sun_data, is_sun_light(light.type))
  SAFE_ASSIGN_FLOAT3(shadow_position, direction)
  SAFE_ASSIGN_FLOAT(_pad0, shape_radius)
  SAFE_ASSIGN_INT_AS_FLOAT(shadow_radius, clipmap_base_offset_neg.x)
  SAFE_ASSIGN_INT_AS_FLOAT(shape_radius, clipmap_base_offset_neg.y)
  SAFE_ASSIGN_INT_AS_FLOAT(influence_radius_max, clipmap_base_offset_pos.x)
  SAFE_ASSIGN_INT_AS_FLOAT(influence_radius_invsqr_surface, clipmap_base_offset_pos.y)
  SAFE_ASSIGN_FLOAT(influence_radius_invsqr_volume, shadow_angle)
  SAFE_ASSIGN_FLOAT2(_pad3, clipmap_origin)
  SAFE_ASSIGN_INT_AS_FLOAT(_pad4, clipmap_lod_min)
  SAFE_ASSIGN_INT_AS_FLOAT(_pad5, clipmap_lod_max)
  SAFE_WRITE_END(light)
  return light;
}

#  if IS_CPP
}  // namespace do_not_use
#  endif

#endif

#if USE_LIGHT_UNION
#  define light_local_data_get(light) light.local
#  define light_spot_data_get(light) light.spot
#  define light_area_data_get(light) light.area
#  define light_sun_data_get(light) light.sun
#endif

#undef DATA_MEMBER
#undef GARBAGE_VALUE
#undef FLOAT_AS_INT
#undef TYPECAST_NOOP
#undef SAFE_BEGIN
#undef SAFE_ASSIGN_LIGHT_TYPE_CHECK
#undef ERROR_OFS
#undef SAFE_ASSIGN
#undef SAFE_ASSIGN_FLOAT
#undef SAFE_ASSIGN_FLOAT2
#undef SAFE_ASSIGN_INT
#undef SAFE_ASSIGN_FLOAT_AS_INT
#undef SAFE_ASSIGN_INT_AS_FLOAT

static inline int light_tilemap_max_get(LightData light)
{
  /* This is not something we need in performance critical code. */
  if (is_sun_light(light.type)) {
    return light.tilemap_index +
           (light_sun_data_get(light).clipmap_lod_max - light_sun_data_get(light).clipmap_lod_min);
  }
  return light.tilemap_index + light_local_data_get(light).tilemaps_count - 1;
}

/* Return the number of tilemap needed for a local light. */
static inline int light_local_tilemap_count(LightData light)
{
  if (is_spot_light(light.type)) {
    return (light_spot_data_get(light).spot_tan > tanf(M_PI / 4.0)) ? 5 : 1;
  }
  if (is_area_light(light.type)) {
    return 5;
  }
  return 6;
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
  /** Light type this tilemap is from. */
  eLightType light_type;
  /** Entire tilemap (all tiles) needs to be tagged as dirty. */
  bool32_t is_dirty;
  /** Effective minimum resolution after update throttle. */
  int effective_lod_min;
  float _pad2;
  /** Near and far clip distances for punctual. */
  float clip_near;
  float clip_far;
  /** Half of the tilemap size in world units. Used to compute window matrix. */
  float half_size;
  /** Offset in local space to the tilemap center in world units. Used for directional winmat. */
  float2 center_offset;
  /** Shadow set bitmask of the light using this tilemap. */
  uint2 shadow_set_membership;
  uint2 _pad3;
};
BLI_STATIC_ASSERT_ALIGN(ShadowTileMapData, 16)

/**
 * Lightweight version of ShadowTileMapData that only contains data used for rendering the shadow.
 */
struct ShadowRenderView {
  /**
   * Is either:
   * - positive radial distance for point lights.
   * - zero if disabled.
   */
  float clip_distance_inv;
  /** Viewport to submit the geometry of this tile-map view to. */
  uint viewport_index;
  /** True if coming from a sun light shadow. */
  bool32_t is_directional;
  /** If directional, distance along the negative Z axis of the near clip in view space. */
  float clip_near;
  /** Copy of `ShadowTileMapData.tiles_index`. */
  int tilemap_tiles_index;
  /** The level of detail of the tilemap this view is rendering. */
  int tilemap_lod;
  /** Updated region of the tilemap. */
  int2 rect_min;
  /** Shadow set bitmask of the light generating this view. */
  uint2 shadow_set_membership;
  uint2 _pad0;
};
BLI_STATIC_ASSERT_ALIGN(ShadowRenderView, 16)

/**
 * Per tilemap data persistent on GPU.
 * Kept separately for easier clearing on GPU.
 */
struct ShadowTileMapClip {
  /** Clip distances that were used to render the pages. */
  float clip_near_stored;
  float clip_far_stored;
  /** Near and far clip distances for directional. Float stored as int for atomic operations. */
  /** NOTE: These are positive just like camera parameters. */
  int clip_near;
  int clip_far;
  /* Transform the shadow is rendered with. Used to detect updates on GPU. */
  Transform object_to_world;
  /* Integer offset of the center of the 16x16 tiles from the origin of the tile space. */
  int2 grid_offset;
  int _pad0;
  int _pad1;
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
  /**
   * WARNING: Excepting `view_needed_count` it is uncertain if these are accurate.
   * This is because `eevee_shadow_page_allocate_comp` runs on all pages even for
   * directional. There might be some lingering states somewhere as relying on
   * `page_update_count` was causing non-deterministic infinite loop. Needs further investigation.
   */
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

/* NOTE: Trust the input to be in valid range (max is [3,3,255]).
 * If it is in valid range, it should pack to 12bits so that `shadow_tile_pack()` can use it.
 * But sometime this is used to encode invalid pages uint3(-1) and it needs to output uint(-1). */
static inline uint shadow_page_pack(uint3 page)
{
  return (page.x << 0u) | (page.y << 2u) | (page.z << 4u);
}
static inline uint3 shadow_page_unpack(uint data)
{
  uint3 page;
  BLI_STATIC_ASSERT(SHADOW_PAGE_PER_ROW <= 4 && SHADOW_PAGE_PER_COL <= 4, "Update page packing")
  page.x = (data >> 0u) & 3u;
  page.y = (data >> 2u) & 3u;
  BLI_STATIC_ASSERT(SHADOW_MAX_PAGE <= 4096, "Update page packing")
  page.z = (data >> 4u) & 255u;
  return page;
}

static inline ShadowTileData shadow_tile_unpack(ShadowTileDataPacked data)
{
  ShadowTileData tile;
  tile.page = shadow_page_unpack(data);
  /* -- 12 bits -- */
  /* Unused bits. */
  /* -- 15 bits -- */
  BLI_STATIC_ASSERT(SHADOW_MAX_PAGE <= 4096, "Update page packing")
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
  uint data;
  /* NOTE: Page might be set to invalid values for tracking invalid usages.
   * So we have to mask the result. */
  data = shadow_page_pack(tile.page) & uint(SHADOW_MAX_PAGE - 1);
  data |= (tile.cache_index & 4095u) << 15u;
  data |= (tile.is_used ? uint(SHADOW_IS_USED) : 0);
  data |= (tile.is_allocated ? uint(SHADOW_IS_ALLOCATED) : 0);
  data |= (tile.is_cached ? uint(SHADOW_IS_CACHED) : 0);
  data |= (tile.is_rendered ? uint(SHADOW_IS_RENDERED) : 0);
  data |= (tile.do_update ? uint(SHADOW_DO_UPDATE) : 0);
  return data;
}

/**
 * Decoded tile data structure.
 * Similar to ShadowTileData, this one is only used for rendering and packed into `tilemap_tx`.
 * This allow to reuse some bits for other purpose.
 */
struct ShadowSamplingTile {
  /** Page inside the virtual shadow map atlas. */
  uint3 page;
  /** LOD pointed by LOD 0 tile page. */
  uint lod;
  /** Offset to the texel position to align with the LOD page start. (directional only). */
  uint2 lod_offset;
  /** If the tile is needed for rendering. */
  bool is_valid;
};
/** \note Stored packed as a uint. */
#define ShadowSamplingTilePacked uint

/* NOTE: Trust the input to be in valid range [0, (1 << SHADOW_TILEMAP_MAX_CLIPMAP_LOD) - 1].
 * Maximum LOD level index we can store is SHADOW_TILEMAP_MAX_CLIPMAP_LOD,
 * so we need SHADOW_TILEMAP_MAX_CLIPMAP_LOD bits to store the offset in each dimension.
 * Result fits into SHADOW_TILEMAP_MAX_CLIPMAP_LOD * 2 bits. */
static inline uint shadow_lod_offset_pack(uint2 ofs)
{
  BLI_STATIC_ASSERT(SHADOW_TILEMAP_MAX_CLIPMAP_LOD <= 8, "Update page packing")
  return ofs.x | (ofs.y << SHADOW_TILEMAP_MAX_CLIPMAP_LOD);
}
static inline uint2 shadow_lod_offset_unpack(uint data)
{
  return (uint2(data) >> uint2(0, SHADOW_TILEMAP_MAX_CLIPMAP_LOD)) &
         uint2((1 << SHADOW_TILEMAP_MAX_CLIPMAP_LOD) - 1);
}

static inline ShadowSamplingTile shadow_sampling_tile_unpack(ShadowSamplingTilePacked data)
{
  ShadowSamplingTile tile;
  tile.page = shadow_page_unpack(data);
  /* -- 12 bits -- */
  /* Max value is actually SHADOW_TILEMAP_MAX_CLIPMAP_LOD but we mask the bits. */
  tile.lod = (data >> 12u) & 15u;
  /* -- 16 bits -- */
  tile.lod_offset = shadow_lod_offset_unpack(data >> 16u);
  /* -- 32 bits -- */
  tile.is_valid = data != 0u;
#ifndef GPU_SHADER
  /* Make tests pass on CPU but it is not required for proper rendering. */
  if (tile.lod == 0) {
    tile.lod_offset.x = 0;
  }
#endif
  return tile;
}

static inline ShadowSamplingTilePacked shadow_sampling_tile_pack(ShadowSamplingTile tile)
{
  if (!tile.is_valid) {
    return 0u;
  }
  /* Tag a valid tile of LOD0 valid by setting their offset to 1.
   * This doesn't change the sampling and allows to use of all bits for data.
   * This makes sure no valid packed tile is 0u. */
  if (tile.lod == 0) {
    tile.lod_offset.x = 1;
  }
  uint data = shadow_page_pack(tile.page);
  /* Max value is actually SHADOW_TILEMAP_MAX_CLIPMAP_LOD but we mask the bits. */
  data |= (tile.lod & 15u) << 12u;
  data |= shadow_lod_offset_pack(tile.lod_offset) << 16u;
  return data;
}

static inline ShadowSamplingTile shadow_sampling_tile_create(ShadowTileData tile_data, uint lod)
{
  ShadowSamplingTile tile;
  tile.page = tile_data.page;
  tile.lod = lod;
  tile.lod_offset = uint2(0, 0); /* Computed during tilemap amend phase. */
  /* At this point, it should be the case that all given tiles that have been tagged as used are
   * ready for sampling. Otherwise tile_data should be SHADOW_NO_DATA. */
  tile.is_valid = tile_data.is_used;
  return tile;
}

struct ShadowSceneData {
  /* Number of shadow rays to shoot for each light. */
  int ray_count;
  /* Number of shadow samples to take for each shadow ray. */
  int step_count;
  /* Bounding radius for a film pixel at 1 unit from the camera. */
  float film_pixel_radius;
  /* Global switch for jittered shadows. */
  bool32_t use_jitter;
};
BLI_STATIC_ASSERT_ALIGN(ShadowSceneData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light-probe Sphere
 * \{ */

struct ReflectionProbeLowFreqLight {
  packed_float3 direction;
  float ambient;
};
BLI_STATIC_ASSERT_ALIGN(ReflectionProbeLowFreqLight, 16)

enum LightProbeShape : uint32_t {
  SHAPE_ELIPSOID = 0u,
  SHAPE_CUBOID = 1u,
};

/* Sampling coordinates using UV space. */
struct SphereProbeUvArea {
  /* Offset in UV space to the start of the sampling space of the octahedron map. */
  float2 offset;
  /* Scaling of the squared UV space of the octahedron map. */
  float scale;
  /* Layer of the atlas where the octahedron map is stored. */
  float layer;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeUvArea, 16)

/* Pixel read/write coordinates using pixel space. */
struct SphereProbePixelArea {
  /* Offset in pixel space to the start of the writing space of the octahedron map.
   * Note that the writing space is not the same as the sampling space as we have borders. */
  int2 offset;
  /* Size of the area in pixel that is covered by this probe mip-map. */
  int extent;
  /* Layer of the atlas where the octahedron map is stored. */
  int layer;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbePixelArea, 16)

/** Mapping data to locate a reflection probe in texture. */
struct SphereProbeData {
  /** Transform to probe local position with non-uniform scaling. */
  float3x4 world_to_probe_transposed;

  packed_float3 location;
  /** Shape of the parallax projection. */
  float parallax_distance;
  LightProbeShape parallax_shape;
  LightProbeShape influence_shape;
  /** Influence factor based on the distance to the parallax shape. */
  float influence_scale;
  float influence_bias;

  SphereProbeUvArea atlas_coord;

  /**
   * Irradiance at the probe location encoded as spherical harmonics.
   * Only contain the average luminance. Used for cube-map normalization.
   */
  ReflectionProbeLowFreqLight low_freq_light;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeData, 16)

/** Viewport Display Pass. */
struct SphereProbeDisplayData {
  int probe_index;
  float display_size;
  float _pad0;
  float _pad1;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeDisplayData, 16)

/* Used for sphere probe spherical harmonics extraction. Output one for each thread-group
 * and do a sum afterward. Reduces bandwidth usage. */
struct SphereProbeHarmonic {
  float4 L0_M0;
  float4 L1_Mn1;
  float4 L1_M0;
  float4 L1_Mp1;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeHarmonic, 16)

struct SphereProbeSunLight {
  float4 direction;
  packed_float3 radiance;
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(SphereProbeSunLight, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Probe Cache
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
  /** True if the light can bounce or be emitted by the surfel back face. */
  bool32_t double_sided;
  /** Surface receiver light set for light linking. */
  uint receiver_light_set;
  int _pad0;
  int _pad1;
  /** Surface radiance: Emission + Direct Lighting. */
  SurfelRadiance radiance_direct;
  /** Surface radiance: Indirect Lighting. Double buffered to avoid race conditions. */
  SurfelRadiance radiance_indirect[2];
};
BLI_STATIC_ASSERT_ALIGN(Surfel, 16)

struct CaptureInfoData {
  /** Grid size without padding. */
  packed_int3 irradiance_grid_size;
  /** True if the surface shader needs to write the surfel data. */
  bool32_t do_surfel_output;
  /** True if the surface shader needs to increment the surfel_len. */
  bool32_t do_surfel_count;
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
  bool32_t capture_world_direct;
  bool32_t capture_world_indirect;
  bool32_t capture_visibility_direct;
  bool32_t capture_visibility_indirect;
  bool32_t capture_indirect;
  bool32_t capture_emission;
  int _pad0;
  /* World light probe atlas coordinate. */
  SphereProbeUvArea world_atlas_coord;
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

struct VolumeProbeData {
  /** World to non-normalized local grid space [0..size-1]. Stored transposed for compactness. */
  float3x4 world_to_grid_transposed;
  /** Number of bricks for this grid. */
  packed_int3 grid_size_padded;
  /** Index in brick descriptor list of the first brick of this grid. */
  int brick_offset;
  /** Biases to apply to the shading point in order to sample a valid probe. */
  float normal_bias;
  float view_bias;
  float facing_bias;
  int _pad1;
};
BLI_STATIC_ASSERT_ALIGN(VolumeProbeData, 16)

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
/** \name Light Clamping
 * \{ */

struct ClampData {
  float sun_threshold;
  float surface_direct;
  float surface_indirect;
  float volume_direct;
  float volume_indirect;
  float _pad0;
  float _pad1;
  float _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ClampData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray-Tracing
 * \{ */

enum eClosureBits : uint32_t {
  CLOSURE_NONE = 0u,
  CLOSURE_DIFFUSE = (1u << 0u),
  CLOSURE_SSS = (1u << 1u),
  CLOSURE_REFLECTION = (1u << 2u),
  CLOSURE_REFRACTION = (1u << 3u),
  CLOSURE_TRANSLUCENT = (1u << 4u),
  CLOSURE_TRANSPARENCY = (1u << 8u),
  CLOSURE_EMISSION = (1u << 9u),
  CLOSURE_HOLDOUT = (1u << 10u),
  CLOSURE_VOLUME = (1u << 11u),
  CLOSURE_AMBIENT_OCCLUSION = (1u << 12u),
  CLOSURE_SHADER_TO_RGBA = (1u << 13u),
  CLOSURE_CLEARCOAT = (1u << 14u),

  CLOSURE_TRANSMISSION = CLOSURE_SSS | CLOSURE_REFRACTION | CLOSURE_TRANSLUCENT,
};

enum GBufferMode : uint32_t {
  /** None mode for pixels not rendered. */
  GBUF_NONE = 0u,

  /* Reflection.  */
  GBUF_DIFFUSE = 1u,
  GBUF_REFLECTION = 2u,
  GBUF_REFLECTION_COLORLESS = 3u,
  /** Used for surfaces that have no lit closure and just encode a normal layer. */
  GBUF_UNLIT = 4u,

  /**
   * Special bit that marks all closures with refraction.
   * Allows to detect the presence of transmission more easily.
   * Note that this left only 2^3 values (minus 0) for encoding the BSDF.
   * Could be removed if that's too cumbersome to add more BSDF.
   */
  GBUF_TRANSMISSION_BIT = 1u << 3u,

  /* Transmission. */
  GBUF_REFRACTION = 0u | GBUF_TRANSMISSION_BIT,
  GBUF_REFRACTION_COLORLESS = 1u | GBUF_TRANSMISSION_BIT,
  GBUF_TRANSLUCENT = 2u | GBUF_TRANSMISSION_BIT,
  GBUF_SUBSURFACE = 3u | GBUF_TRANSMISSION_BIT,

  /** IMPORTANT: Needs to be less than 16 for correct packing in g-buffer header. */
};

struct RayTraceData {
  /** ViewProjection matrix used to render the previous frame. */
  float4x4 history_persmat;
  /** ViewProjection matrix used to render the radiance texture. */
  float4x4 radiance_persmat;
  /** Input resolution. */
  int2 full_resolution;
  /** Inverse of input resolution to get screen UVs. */
  float2 full_resolution_inv;
  /** Scale and bias to go from ray-trace resolution to input resolution. */
  int2 resolution_bias;
  int resolution_scale;
  /** View space thickness the objects. */
  float thickness;
  /** Scale and bias to go from horizon-trace resolution to input resolution. */
  int2 horizon_resolution_bias;
  int horizon_resolution_scale;
  /** Determine how fast the sample steps are getting bigger. */
  float quality;
  /** Maximum roughness for which we will trace a ray. */
  float roughness_mask_scale;
  float roughness_mask_bias;
  /** If set to true will bypass spatial denoising. */
  bool32_t skip_denoise;
  /** If set to false will bypass tracing for refractive closures. */
  bool32_t trace_refraction;
  /** Closure being ray-traced. */
  int closure_index;
  int _pad0;
  int _pad1;
};
BLI_STATIC_ASSERT_ALIGN(RayTraceData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ambient Occlusion
 * \{ */

struct AOData {
  float2 pixel_size;
  float distance;
  float lod_factor;

  float thickness_near;
  float thickness_far;
  float angle_bias;
  float gi_distance;

  float lod_factor_ao;
  float _pad0;
  float _pad1;
  float _pad2;
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
#define SSS_TRANSMIT_LUT_RADIUS 2.0
#define SSS_TRANSMIT_LUT_SCALE ((SSS_TRANSMIT_LUT_SIZE - 1.0) / float(SSS_TRANSMIT_LUT_SIZE))
#define SSS_TRANSMIT_LUT_BIAS (0.5 / float(SSS_TRANSMIT_LUT_SIZE))
#define SSS_TRANSMIT_LUT_STEP_RES 64.0

struct SubsurfaceData {
  /** xy: 2D sample position [-1..1], zw: sample_bounds. */
  /* NOTE(fclem) Using float4 for alignment. */
  float4 samples[SSS_SAMPLE_MAX];
  /** Number of samples precomputed in the set. */
  int sample_len;
  /** WORKAROUND: To avoid invalid integral for components that have very small radius, we clamp
   * the minimal radius. This add bias to the SSS effect but this is the simplest workaround I
   * could find to ship this without visible artifact. */
  float min_radius;
  int _pad1;
  int _pad2;
};
BLI_STATIC_ASSERT_ALIGN(SubsurfaceData, 16)

static inline float3 burley_setup(float3 radius, float3 albedo)
{
  /* TODO(fclem): Avoid constant duplication. */
  const float m_1_pi = 0.318309886183790671538;

  float3 A = albedo;
  /* Diffuse surface transmission, equation (6). */
  float3 s = 1.9 - A + 3.5 * ((A - 0.8) * (A - 0.8));
  /* Mean free path length adapted to fit ancient Cubic and Gaussian models. */
  float3 l = 0.25 * m_1_pi * radius;

  return l / s;
}

static inline float3 burley_eval(float3 d, float r)
{
  /* Slide 33. */
  float3 exp_r_3_d;
  /* TODO(fclem): Vectorize. */
  exp_r_3_d.x = expf(-r / (3.0 * d.x));
  exp_r_3_d.y = expf(-r / (3.0 * d.y));
  exp_r_3_d.z = expf(-r / (3.0 * d.z));
  float3 exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
  /* NOTE:
   * - Surface albedo is applied at the end.
   * - This is normalized diffuse model, so the equation is multiplied
   *   by 2*pi, which also matches `cdf()`.
   */
  return (exp_r_d + exp_r_3_d) / (4.0 * d);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light-probe Planar Data
 * \{ */

struct PlanarProbeData {
  /** Matrices used to render the planar capture. */
  float4x4 viewmat;
  float4x4 winmat;
  /** Transform world to local position with influence distance as Z scale. */
  float3x4 world_to_object_transposed;
  /** World space plane normal. */
  packed_float3 normal;
  /** Layer in the planar capture textures used by this probe. */
  int layer_id;
};
BLI_STATIC_ASSERT_ALIGN(PlanarProbeData, 16)

struct ClipPlaneData {
  /** World space clip plane equation. Used to render planar light-probes. */
  float4 plane;
};
BLI_STATIC_ASSERT_ALIGN(ClipPlaneData, 16)

/** Viewport Display Pass. */
struct PlanarProbeDisplayData {
  float4x4 plane_to_world;
  int probe_index;
  float _pad0;
  float _pad1;
  float _pad2;
};
BLI_STATIC_ASSERT_ALIGN(PlanarProbeDisplayData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline Data
 * \{ */

struct PipelineInfoData {
  float alpha_hash_scale;
  bool32_t is_sphere_probe;
  float _pad1;
  float _pad2;
};
BLI_STATIC_ASSERT_ALIGN(PipelineInfoData, 16)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Data
 * \{ */

/* Combines data from several modules to avoid wasting binding slots. */
struct UniformData {
  AOData ao;
  CameraData camera;
  ClampData clamp;
  FilmData film;
  HiZData hiz;
  RayTraceData raytrace;
  RenderBuffersInfoData render_pass;
  ShadowSceneData shadow;
  SubsurfaceData subsurface;
  VolumesInfoData volumes;
  PipelineInfoData pipeline;
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
#define UTIL_BSDF_LAYER 3
#define UTIL_BTDF_LAYER 4
#define UTIL_DISK_INTEGRAL_LAYER UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER
#define UTIL_DISK_INTEGRAL_COMP 3

#ifdef GPU_SHADER

#  if defined(GPU_FRAGMENT_SHADER)
#    define UTIL_TEXEL vec2(gl_FragCoord.xy)
#  elif defined(GPU_COMPUTE_SHADER)
#    define UTIL_TEXEL vec2(gl_GlobalInvocationID.xy)
#  elif defined(GPU_VERTEX_SHADER)
#    define UTIL_TEXEL vec2(gl_VertexID, 0)
#  elif defined(GPU_LIBRARY_SHADER)
#    define UTIL_TEXEL vec2(0)
#  endif

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

/* Sample GGX BSDF LUT. */
float4 utility_tx_sample_bsdf_lut(sampler2DArray util_tx, float2 uv, float layer)
{
  /* Scale and bias coordinates, for correct filtered lookup. */
  uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;
  layer = layer * UTIL_BTDF_LAYER_COUNT + UTIL_BTDF_LAYER;

  float layer_floored;
  float interp = modf(layer, layer_floored);

  float4 tex_low = textureLod(util_tx, float3(uv, layer_floored), 0.0);
  float4 tex_high = textureLod(util_tx, float3(uv, layer_floored + 1.0), 0.0);

  /* Manual trilinear interpolation. */
  return mix(tex_low, tex_high, interp);
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

#ifdef EEVEE_PI
#  undef M_PI
#endif

/** \} */

#if IS_CPP

using AOVsInfoDataBuf = draw::StorageBuffer<AOVsInfoData>;
using CameraDataBuf = draw::UniformBuffer<CameraData>;
using ClosureTileBuf = draw::StorageArrayBuffer<uint, 1024, true>;
using DepthOfFieldDataBuf = draw::UniformBuffer<DepthOfFieldData>;
using DepthOfFieldScatterListBuf = draw::StorageArrayBuffer<ScatterRect, 16, true>;
using DrawIndirectBuf = draw::StorageBuffer<DrawCommand, true>;
using DispatchIndirectBuf = draw::StorageBuffer<DispatchCommand>;
using UniformDataBuf = draw::UniformBuffer<UniformData>;
using VolumeProbeDataBuf = draw::UniformArrayBuffer<VolumeProbeData, IRRADIANCE_GRID_MAX>;
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
using SubsurfaceTileBuf = RayTraceTileBuf;
using SphereProbeDataBuf = draw::UniformArrayBuffer<SphereProbeData, SPHERE_PROBE_MAX>;
using SphereProbeDisplayDataBuf = draw::StorageArrayBuffer<SphereProbeDisplayData>;
using PlanarProbeDataBuf = draw::UniformArrayBuffer<PlanarProbeData, PLANAR_PROBE_MAX>;
using PlanarProbeDisplayDataBuf = draw::StorageArrayBuffer<PlanarProbeDisplayData>;
using SamplingDataBuf = draw::StorageBuffer<SamplingData>;
using ShadowStatisticsBuf = draw::StorageBuffer<ShadowStatistics>;
using ShadowPagesInfoDataBuf = draw::StorageBuffer<ShadowPagesInfoData>;
using ShadowPageHeapBuf = draw::StorageVectorBuffer<uint, SHADOW_MAX_PAGE>;
using ShadowPageCacheBuf = draw::StorageArrayBuffer<uint2, SHADOW_MAX_PAGE, true>;
using ShadowTileMapDataBuf = draw::StorageVectorBuffer<ShadowTileMapData, SHADOW_MAX_TILEMAP>;
using ShadowTileMapClipBuf = draw::StorageArrayBuffer<ShadowTileMapClip, SHADOW_MAX_TILEMAP, true>;
using ShadowTileDataBuf = draw::StorageArrayBuffer<ShadowTileDataPacked, SHADOW_MAX_TILE, true>;
using ShadowRenderViewBuf = draw::StorageArrayBuffer<ShadowRenderView, SHADOW_VIEW_MAX, true>;
using SurfelBuf = draw::StorageArrayBuffer<Surfel, 64>;
using SurfelRadianceBuf = draw::StorageArrayBuffer<SurfelRadiance, 64>;
using CaptureInfoBuf = draw::StorageBuffer<CaptureInfoData>;
using SurfelListInfoBuf = draw::StorageBuffer<SurfelListInfoData>;
using VelocityGeometryBuf = draw::StorageArrayBuffer<float4, 16, true>;
using VelocityIndexBuf = draw::StorageArrayBuffer<VelocityIndex, 16>;
using VelocityObjectBuf = draw::StorageArrayBuffer<float4x4, 16>;
using CryptomatteObjectBuf = draw::StorageArrayBuffer<float2, 16>;
using ClipPlaneBuf = draw::UniformBuffer<ClipPlaneData>;
}  // namespace blender::eevee
#endif
