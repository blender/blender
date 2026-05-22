/* SPDX-FileCopyrightText: 2017-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Wrapper objects for data from Look Up Tables for BxDFs. Refer to
 * `eevee_bxdf_lut.bsl.hh` for table computation.
 */

#pragma once

#include "eevee_defines.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_compat.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::lut {

/**
 * Output from the 2D GGX BRDF LUT.
 *
 * The result is interpreted as:
 * - `integral = F0 * scale + F90 * bias - F82_tint * metal_bias`,
 *   where `F82_tint = mix(F0, float3(1), pow5f(6/7) * 7 / pow6f(6/7)) * (1 - F82)`.
 */
struct GGXBrdfData {
  float scale;
  float bias;
  float metal_bias;

  float4 pack() const
  {
    return float4(scale, bias, metal_bias, 0.0f);
  }

  static GGXBrdfData unpack(float4 data)
  {
    return {.scale = data.x, .bias = data.y, .metal_bias = data.z};
  }

  static float2 lut_coords_get(float cos_theta, float roughness)
  {
    return float2(roughness, sqrt(saturate(1.0f - cos_theta)));
  }

  static GGXBrdfData sample_utility_tx(const sampler2DArray &util_tx,
                                       float cos_theta,
                                       float roughness)
  {
    const float2 coords = lut_coords_get(cos_theta, roughness);
    const float4 data = utility_tx_sample_lut(util_tx, coords, UTIL_BRDF_LAYER);
    return unpack(data);
  }
};

/**
 * Output from the 3D GGX BSDF LUT.
 *
 * The result is interpreted as:
 * - `reflectance = F0 * scale + F90 * bias`,
 * - `transmittance = (1 - F0) * transmission_factor`.
 */
struct GGXBsdfData {
  float scale;
  float bias;
  float transmission_factor;

  float4 pack() const
  {
    return float4(scale, bias, transmission_factor, 0.0f);
  }

  static GGXBsdfData unpack(float4 data)
  {
    return {.scale = data.x, .bias = data.y, .transmission_factor = data.z};
  }

  static float3 lut_coords_get(float cos_theta, float roughness, float ior)
  {
    /* IOR is the sine of the critical angle. */
    float critical_cos = sqrt(1.0f - ior * ior);

    float3 coords;
    coords.x = square(ior);
    coords.y = cos_theta;
    coords.y -= critical_cos;
    coords.y /= (coords.y > 0.0f) ? (1.0f - critical_cos) : critical_cos;
    coords.y = coords.y * 0.5f + 0.5f;
    coords.z = roughness;

    return saturate(coords);
  }

  static GGXBsdfData sample_utility_tx(const sampler2DArray &util_tx,
                                       float cos_theta,
                                       float roughness,
                                       float ior)
  {
    const float3 coords = lut_coords_get(cos_theta, roughness, ior);
    const float4 data = utility_tx_sample_bsdf_lut(util_tx, coords.xy, coords.z);
    return unpack(data);
  }
};

/**
 * Output from the 3D GGX BTDF LUT, for IOR > 1.
 *
 * The result is interpreted as:
 * - `transmittance = (1 - F0) * transmission_factor`.
 */
struct GGXBtdfGt1Data {
  float transmission_factor;

  float4 pack() const
  {
    return float4(transmission_factor, 0.0f, 0.0f, 0.0f);
  }

  static GGXBtdfGt1Data unpack(float4 data)
  {
    return {.transmission_factor = data.w};
  }

  static float3 lut_coords_get(float cos_theta, float roughness, float f0)
  {
    return float3(sqrt(f0), sqrt(1.0f - cos_theta), roughness);
  }

  static GGXBtdfGt1Data sample_utility_tx(const sampler2DArray &util_tx,
                                          float cos_theta,
                                          float roughness,
                                          float f0)
  {
    const float3 coords = lut_coords_get(cos_theta, roughness, f0);
    const float4 data = utility_tx_sample_bsdf_lut(util_tx, coords.xy, coords.z);
    return unpack(data);
  }
};

}  // namespace eevee::lut
