/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_codegen_lib.glsl"

[[node]]
void differentiate_texco(float3 v, float3 &df)
{
  /* Implementation defined. */
  df = v + dF_impl(v);
}

/* Overload for UVs which are loaded as generic attributes. */
[[node]]
void differentiate_texco(float4 v, float3 &df)
{
  /* Implementation defined. */
  df = v.xyz + dF_impl(v.xyz);
}

[[node]]
void node_bump([[maybe_unused]] float strength,
               [[maybe_unused]] float dist,
               [[maybe_unused]] float filter_width,
               [[maybe_unused]] float height,
               float3 N,
               [[maybe_unused]] float2 height_xy,
               [[maybe_unused]] float invert,
               float3 &result)
{
  N = normalize(N);
#ifdef GPU_FRAGMENT_SHADER
  dist *= FrontFacing ? invert : -invert;

  float3 dPdx = gpu_dfdx(g_data.P) * derivative_scale_get();
  float3 dPdy = gpu_dfdy(g_data.P) * derivative_scale_get();

  /* Get surface tangents from normal. */
  float3 Rx = cross(dPdy, N);
  float3 Ry = cross(N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  float2 dHd = height_xy - float2(height);
  float3 surfgrad = dHd.x * Rx + dHd.y * Ry;

  strength = max(strength, 0.0f);

  result = normalize(filter_width * abs(det) * N - dist * sign(det) * surfgrad);
  result = normalize(mix(N, result, strength));
#else
  result = N;
#endif
}
