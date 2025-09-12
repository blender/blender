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

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
