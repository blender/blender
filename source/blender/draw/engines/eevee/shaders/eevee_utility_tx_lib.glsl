/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

#include "gpu_shader_compat.hh"

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
  float2 coords = float2(roughness, sqrt(clamp(1.0 - cos_theta, 0.0, 1.0)));
  return utility_tx_sample_lut(util_tx, coords, layer);
}
