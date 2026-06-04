/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "gpu_shader_compat.hh"

struct UtilityTexture {
  [[sampler(RBUFS_UTILITY_TEX_SLOT)]] sampler2DArray utility_tx;

  /* Fetch texel. Repeat extend mode if above range. */
  float4 fetch(float2 texel, float layer) const
  {
    return texelFetch(utility_tx, int3(int2(texel) % UTIL_TEX_SIZE, int(layer)), 0);
  }

  /* Sample at uv position. Filtered & extend mode enabled. */
  float4 sample_extend(float2 uv, float layer) const
  {
    return textureLod(utility_tx, float3(uv, layer), 0.0f);
  }

  /* Sample at uv position but with scale and bias so that uv space bounds lie on texel centers. */
  float4 sample_lut(float2 uv, float layer) const
  {
    /* Scale and bias coordinates, for correct filtered lookup. */
    uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;
    return textureLod(utility_tx, float3(uv, layer), 0.0f);
  }

  /* Sample GGX BSDF LUT. */
  float4 sample_bsdf_lut(float2 uv, float layer) const
  {
    /* Scale and bias coordinates, for correct filtered lookup. */
    uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;
    layer = layer * UTIL_BSDF_LAYER_COUNT + UTIL_BSDF_LAYER;

    float layer_floored;
    float interp = modf(layer, layer_floored);

    float4 tex_low = textureLod(utility_tx, float3(uv, layer_floored), 0.0f);
    float4 tex_high = textureLod(utility_tx, float3(uv, layer_floored + 1.0f), 0.0f);

    /* Manual trilinear interpolation. */
    return mix(tex_low, tex_high, interp);
  }
};
