/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Various utilities related to object subsurface light transport.
 *
 * Required resources:
 * - utility_tx
 */

#include "infos/eevee_common_infos.hh"

#include "eevee_utility_tx_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float subsurface_transmittance_profile(float u)
{
  auto &utility_tx = sampler_get(eevee_utility_texture, utility_tx);
  return utility_tx_sample(utility_tx, float2(u, 0.0f), UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER).r;
}

/**
 * Returns the amount of light that can travels through a uniform medium and exit at the backface.
 */
float3 subsurface_transmission(float3 sss_radii, float thickness)
{
  sss_radii *= SSS_TRANSMIT_LUT_RADIUS;
  float3 channels_co = saturate(thickness / sss_radii) * SSS_TRANSMIT_LUT_SCALE +
                       SSS_TRANSMIT_LUT_BIAS;
  float3 translucency;
  translucency.x = (sss_radii.x > 0.0f) ? subsurface_transmittance_profile(channels_co.x) : 0.0f;
  translucency.y = (sss_radii.y > 0.0f) ? subsurface_transmittance_profile(channels_co.y) : 0.0f;
  translucency.z = (sss_radii.z > 0.0f) ? subsurface_transmittance_profile(channels_co.z) : 0.0f;
  return translucency;
}
