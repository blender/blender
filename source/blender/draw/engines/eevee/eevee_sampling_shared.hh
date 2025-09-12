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

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
