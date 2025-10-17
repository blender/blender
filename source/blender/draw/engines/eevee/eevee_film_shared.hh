/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

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
ENUM_OPERATORS(PassCategory)

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

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
