/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Various utilities related to object subsurface light transport.
 */

#include "eevee_subsurface_shared.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_utildefines_lib.glsl"

float subsurface_transmittance_profile([[resource_table]] const UtilityTexture &util_tx, float u)
{
  return util_tx.sample_extend(float2(u, 0.0f), UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER).r;
}

/**
 * Returns the amount of light that can travels through a uniform medium and exit at the backface.
 */
float3 subsurface_transmission([[resource_table]] const UtilityTexture &util,
                               float3 sss_radii,
                               float thickness)
{
  sss_radii *= SSS_TRANSMIT_LUT_RADIUS;
  float3 channels_co = saturate(thickness / sss_radii) * SSS_TRANSMIT_LUT_SCALE +
                       SSS_TRANSMIT_LUT_BIAS;
  float3 translucency;
  translucency.x = (sss_radii.x > 0.0f) ? subsurface_transmittance_profile(util, channels_co.x) :
                                          0.0f;
  translucency.y = (sss_radii.y > 0.0f) ? subsurface_transmittance_profile(util, channels_co.y) :
                                          0.0f;
  translucency.z = (sss_radii.z > 0.0f) ? subsurface_transmittance_profile(util, channels_co.z) :
                                          0.0f;
  return translucency;
}
