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

#endif

#define UBO_MIN_MAX_SUPPORTED_SIZE 1 << 14

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

  int _pad0;
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
  /** Sub-pixel offset applied to the window matrix.
   * NOTE: In final film pixel unit.
   * NOTE: Positive values makes the view translate in the negative axes direction.
   * NOTE: The origin is the center of the lower left film pixel of the area covered by a render
   * pixel if using scaled resolution rendering.
   */
  float2 subpixel_offset;
  /** Is true if history is valid and can be sampled. Bypass history to resets accumulation. */
  bool1 use_history;
  /** Is true if combined buffer is valid and can be re-projected to reduce variance. */
  bool1 use_reprojection;
  /** Is true if accumulation of non-filtered passes is needed. */
  bool1 has_data;
  /** Is true if accumulation of filtered passes is needed. */
  bool1 any_render_pass_1;
  bool1 any_render_pass_2;
  int _pad0, _pad1;
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
  float exposure;
  /** Scaling factor for scaled resolution rendering. */
  int scaling_factor;
  /** Film pixel filter radius. */
  float filter_size;
  /** Precomputed samples. First in the table is the closest one. The rest is unordered. */
  int samples_len;
  /** Sum of the weights of all samples in the sample table. */
  float samples_weight_total;
  FilmSample samples[FILM_PRECOMP_SAMPLE_MAX];
};
BLI_STATIC_ASSERT_ALIGN(FilmData, 16)

static inline float film_filter_weight(float filter_size, float sample_distance_sqr)
{
#if 1 /* Faster */
  /* Gaussian fitted to Blackman-Harris. */
  float r = sample_distance_sqr / (filter_size * filter_size);
  const float sigma = 0.284;
  const float fac = -0.5 / (sigma * sigma);
  float weight = expf(fac * r);
#else
  /* Blackman-Harris filter. */
  float r = M_2PI * saturate(0.5 + sqrtf(sample_distance_sqr) / (2.0 * filter_size));
  float weight = 0.35875 - 0.48829 * cosf(r) + 0.14128 * cosf(2.0 * r) - 0.01168 * cosf(3.0 * r);
#endif
  return weight;
}

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
using FilmDataBuf = draw::UniformBuffer<FilmData>;
using SamplingDataBuf = draw::StorageBuffer<SamplingData>;
using VelocityGeometryBuf = draw::StorageArrayBuffer<float4, 16, true>;
using VelocityIndexBuf = draw::StorageArrayBuffer<VelocityIndex, 16>;
using VelocityObjectBuf = draw::StorageArrayBuffer<float4x4, 16>;

}  // namespace blender::eevee
#endif
