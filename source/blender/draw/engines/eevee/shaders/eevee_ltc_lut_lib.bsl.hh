/* SPDX-FileCopyrightText: 2017-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_compat.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::lut::ltc {
float2 lut_coords_get(float cos_theta, float roughness)
{
  return float2(roughness, sqrt(saturate(1.0f - cos_theta)));
}

/**
 * Sample a packed ltc matrix from the LUT
 */
packed_float4 sample_utility_tx(sampler2DArray util_tx, float cos_theta, float roughness)
{
  const float2 coords = lut_coords_get(cos_theta, roughness);
  return utility_tx_sample_lut(util_tx, coords, UTIL_LTC_MAT_LAYER);
}

/**
 * Return a packed identity matrix, resulting in a plain cosine distribution.
 */
packed_float4 identity()
{
  return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

/**
 * Load inverse LTC matrix M^{-1} from packed ltc value.
 */
float3x3 unpack(packed_float4 v)
{
  return float3x3(float3(v.x, 0, v.y), float3(0, 1, 0), float3(v.z, 0, v.w));
}
}  // namespace eevee::lut::ltc
