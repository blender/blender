/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Various utilities related to object subsurface light transport.
 *
 * Required resources:
 * - utility_tx
 */

#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)

#ifdef EEVEE_UTILITY_TX

float subsurface_transmittance_profile(float u)
{
  return utility_tx_sample(utility_tx, vec2(u, 0.0), UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER).r;
}

/**
 * Returns the amount of light that can travels through a uniform medium and exit at the backface.
 */
vec3 subsurface_transmission(vec3 sss_radii, float thickness)
{
  sss_radii *= SSS_TRANSMIT_LUT_RADIUS;
  vec3 channels_co = saturate(thickness / sss_radii) * SSS_TRANSMIT_LUT_SCALE +
                     SSS_TRANSMIT_LUT_BIAS;
  vec3 translucency;
  translucency.x = (sss_radii.x > 0.0) ? subsurface_transmittance_profile(channels_co.x) : 0.0;
  translucency.y = (sss_radii.y > 0.0) ? subsurface_transmittance_profile(channels_co.y) : 0.0;
  translucency.z = (sss_radii.z > 0.0) ? subsurface_transmittance_profile(channels_co.z) : 0.0;
  return translucency;
}

#endif
