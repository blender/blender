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

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
