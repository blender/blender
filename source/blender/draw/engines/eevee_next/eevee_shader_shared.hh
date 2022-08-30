/* SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared structures, enums & defines between C++ and GLSL.
 * Can also include some math functions but they need to be simple enough to be valid in both
 * language.
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  pragma once

#  include "BLI_memory_utils.hh"
#  include "DRW_gpu_wrapper.hh"

#  include "eevee_defines.hh"

#  include "GPU_shader_shared.h"

namespace blender::eevee {

using draw::Framebuffer;
using draw::SwapChain;
using draw::Texture;
using draw::TextureFromPool;

constexpr eGPUSamplerState no_filter = GPU_SAMPLER_DEFAULT;
constexpr eGPUSamplerState with_filter = GPU_SAMPLER_FILTER;

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
   * Show incorrectly downsample tiles in red.
   */
  DEBUG_HIZ_VALIDATION = 2u,
  /**
   * Tile-maps to screen. Is also present in other modes.
   * - Black pixels, no pages allocated.
   * - Green pixels, pages cached.
   * - Red pixels, pages allocated.
   */
  DEBUG_SHADOW_TILEMAPS = 10u,
  /**
   * Random color per pages. Validates page density allocation and sampling.
   */
  DEBUG_SHADOW_PAGES = 11u,
  /**
   * Outputs random color per tile-map (or tile-map level). Validates tile-maps coverage.
   * Black means not covered by any tile-maps LOD of the shadow.
   */
  DEBUG_SHADOW_LOD = 12u,
  /**
   * Outputs white pixels for pages allocated and black pixels for unused pages.
   * This needs DEBUG_SHADOW_PAGE_ALLOCATION_ENABLED defined in order to work.
   */
  DEBUG_SHADOW_PAGE_ALLOCATION = 13u,
  /**
   * Outputs the tile-map atlas. Default tile-map is too big for the usual screen resolution.
   * Try lowering SHADOW_TILEMAP_PER_ROW and SHADOW_MAX_TILEMAP before using this option.
   */
  DEBUG_SHADOW_TILE_ALLOCATION = 14u,
  /**
   * Visualize linear depth stored in the atlas regions of the active light.
   * This way, one can check if the rendering, the copying and the shadow sampling functions works.
   */
  DEBUG_SHADOW_SHADOW_DEPTH = 15u
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
  SAMPLING_RAYTRACE_X = 18u
};

/**
 * IMPORTANT: Make sure the array can contain all sampling dimensions.
 * Also note that it needs to be multiple of 4.
 */
#define SAMPLING_DIMENSION_COUNT 20

/* NOTE(@fclem): Needs to be used in #StorageBuffer because of arrays of scalar. */
struct SamplingData {
  /** Array containing random values from Low Discrepancy Sequence in [0..1) range. */
  float dimensions[SAMPLING_DIMENSION_COUNT];
};
BLI_STATIC_ASSERT_ALIGN(SamplingData, 16)

/* Returns total sample count in a web pattern of the given size. */
static inline int sampling_web_sample_count_get(int web_density, int ring_count)
{
  return ((ring_count * ring_count + ring_count) / 2) * web_density + 1;
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
  /** Camera UV scale and bias. Also known as `viewcamtexcofac`. */
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
  /** Offset of the film in the full-res frame, in pixels. */
  int2 offset;
  /** Extent used by the render buffers when rendering the main views. */
  int2 render_extent;
  /** Sub-pixel offset applied to the window matrix.
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
  float _pad0;
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
  /** True if the render-pass to be displayed is from the value accum buffer. */
  bool1 display_is_value;
  /** True if we bypass the accumulation and directly output the accumulation buffer. */
  bool1 display_only;
  /** Start of AOVs and number of aov. */
  int aov_color_id, aov_color_len;
  int aov_value_id, aov_value_len;
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
/** \name Render passes
 * \{ */

enum eRenderPassLayerIndex : uint32_t {
  RENDER_PASS_LAYER_DIFFUSE_LIGHT = 0u,
  RENDER_PASS_LAYER_SPECULAR_LIGHT = 1u,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arbitrary Output Variables
 * \{ */

/* Theoretical max is 128 as we are using texture array and VRAM usage.
 * However, the output_aov() function perform a linear search inside all the hashes.
 * If we find a way to avoid this we could bump this number up. */
#define AOV_MAX 16

/* NOTE(@fclem): Needs to be used in #StorageBuffer because of arrays of scalar. */
struct AOVsInfoData {
  uint hash_value[AOV_MAX];
  uint hash_color[AOV_MAX];
  /* Length of used data. */
  uint color_len;
  uint value_len;
  /** Id of the AOV to be displayed (from the start of the AOV array). -1 for combined. */
  int display_id;
  /** True if the AOV to be displayed is from the value accum buffer. */
  bool1 display_is_value;
};
BLI_STATIC_ASSERT_ALIGN(AOVsInfoData, 16)

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
  /** Offset inside #VelocityObjectBuf for each timestep. Indexed using eVelocityStep. */
  int3 ofs;
  /** Temporary index to copy this to the #VelocityIndexBuf. */
  uint resource_id;

#ifdef __cplusplus
  VelocityObjectIndex() : ofs(-1, -1, -1), resource_id(-1){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(VelocityObjectIndex, 16)

struct VelocityGeometryIndex {
  /** Offset inside #VelocityGeometryBuf for each timestep. Indexed using eVelocityStep. */
  int3 ofs;
  /** If true, compute deformation motion blur. */
  bool1 do_deform;
  /** Length of data inside #VelocityGeometryBuf for each timestep. Indexed using eVelocityStep. */
  int3 len;

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
  /* From Graphics Gems from CryENGINE 3 (Siggraph 2013) by Tiago Sousa (slide
   * 36). */
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
  /** Number of items that passes the first culling test. */
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
  LIGHT_POINT = 1u,
  LIGHT_SPOT = 2u,
  LIGHT_RECT = 3u,
  LIGHT_ELLIPSE = 4u
};

static inline bool is_area_light(eLightType type)
{
  return type >= LIGHT_RECT;
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
  /** Aliases for axes. */
#ifndef USE_GPU_SHADER_CREATE_INFO
#  define _right object_mat[0]
#  define _up object_mat[1]
#  define _back object_mat[2]
#  define _position object_mat[3]
#else
#  define _right object_mat[0].xyz
#  define _up object_mat[1].xyz
#  define _back object_mat[2].xyz
#  define _position object_mat[3].xyz
#endif
  /** Influence radius (inverted and squared) adjusted for Surface / Volume power. */
  float influence_radius_invsqr_surface;
  float influence_radius_invsqr_volume;
  /** Maximum influence radius. Used for culling. */
  float influence_radius_max;
  /** Index of the shadow struct on CPU. -1 means no shadow. */
  int shadow_id;
  /** NOTE: It is ok to use float3 here. A float is declared right after it.
   * float3 is also aligned to 16 bytes. */
  float3 color;
  /** Power depending on shader type. */
  float diffuse_power;
  float specular_power;
  float volume_power;
  float transmit_power;
  /** Special radius factor for point lighting. */
  float radius_squared;
  /** Light Type. */
  eLightType type;
  /** Spot angle tangent. */
  float spot_tan;
  /** Spot size. Aligned to size of float2. */
  float2 spot_size_inv;
  /** Associated shadow data. Only valid if shadow_id is not LIGHT_NO_SHADOW. */
  // ShadowData shadow_data;
};
BLI_STATIC_ASSERT_ALIGN(LightData, 16)

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
/** \name Utility Texture
 * \{ */

#define UTIL_TEX_SIZE 64
#define UTIL_BTDF_LAYER_COUNT 16
/* Scale and bias to avoid interpolation of the border pixel.
 * Remap UVs to the border pixels centers. */
#define UTIL_TEX_UV_SCALE ((UTIL_TEX_SIZE - 1.0f) / UTIL_TEX_SIZE)
#define UTIL_TEX_UV_BIAS (0.5f / UTIL_TEX_SIZE)

#define UTIL_BLUE_NOISE_LAYER 0
#define UTIL_LTC_MAT_LAYER 1
#define UTIL_LTC_MAG_LAYER 2
#define UTIL_BSDF_LAYER 2
#define UTIL_BTDF_LAYER 3
#define UTIL_DISK_INTEGRAL_LAYER 3
#define UTIL_DISK_INTEGRAL_COMP 2

#ifndef __cplusplus
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
#endif

/** \} */

#ifdef __cplusplus

using AOVsInfoDataBuf = draw::StorageBuffer<AOVsInfoData>;
using CameraDataBuf = draw::UniformBuffer<CameraData>;
using DepthOfFieldDataBuf = draw::UniformBuffer<DepthOfFieldData>;
using DepthOfFieldScatterListBuf = draw::StorageArrayBuffer<ScatterRect, 16, true>;
using DrawIndirectBuf = draw::StorageBuffer<DrawCommand, true>;
using FilmDataBuf = draw::UniformBuffer<FilmData>;
using HiZDataBuf = draw::UniformBuffer<HiZData>;
using LightCullingDataBuf = draw::StorageBuffer<LightCullingData>;
using LightCullingKeyBuf = draw::StorageArrayBuffer<uint, LIGHT_CHUNK, true>;
using LightCullingTileBuf = draw::StorageArrayBuffer<uint, LIGHT_CHUNK, true>;
using LightCullingZbinBuf = draw::StorageArrayBuffer<uint, CULLING_ZBIN_COUNT, true>;
using LightCullingZdistBuf = draw::StorageArrayBuffer<float, LIGHT_CHUNK, true>;
using LightDataBuf = draw::StorageArrayBuffer<LightData, LIGHT_CHUNK>;
using MotionBlurDataBuf = draw::UniformBuffer<MotionBlurData>;
using MotionBlurTileIndirectionBuf = draw::StorageBuffer<MotionBlurTileIndirection, true>;
using SamplingDataBuf = draw::StorageBuffer<SamplingData>;
using VelocityGeometryBuf = draw::StorageArrayBuffer<float4, 16, true>;
using VelocityIndexBuf = draw::StorageArrayBuffer<VelocityIndex, 16>;
using VelocityObjectBuf = draw::StorageArrayBuffer<float4x4, 16>;

}  // namespace blender::eevee
#endif
