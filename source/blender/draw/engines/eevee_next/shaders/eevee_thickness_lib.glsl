/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Various utilities related to object subsurface thickness approximation.
 *
 * Required resources:
 * - atlas_tx
 * - tilemaps_tx
 */

#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)

void thickness_from_shadow_single(
    uint l_idx, const bool is_directional, vec3 P, vec3 Ng, inout vec2 thickness)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  LightVector lv = light_vector_get(light, is_directional, P);
  /* Note that we reverse the surface normal to reject surfaces facing the light. */
  float attenuation = light_attenuation_surface(light, is_directional, Ng, true, lv);
  if ((attenuation < LIGHT_ATTENUATION_THRESHOLD)) {
    return;
  }
  /* Weight result by facing ratio to avoid harsh transitions. */
  float weight = saturate(dot(lv.L, -Ng));
  ShadowEvalResult result = shadow_sample(
      is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P);
  /* Flatten the accumulation to avoid weighting the outliers too much. */
  thickness += vec2(safe_sqrt(result.occluder_distance), 1.0) * weight;
}

#define THICKNESS_NO_VALUE 1.0e6
/**
 * Return the apparent thickness of an object behind surface considering all shadow maps
 * available. If no shadow-map has a record of the other side of the surface, this function
 * returns THICKNESS_NO_VALUE.
 */
float thickness_from_shadow(vec3 P, vec3 Ng, float vPz)
{
  /* Bias surface inward to avoid shadow map aliasing. */
  float normal_offset = uniform_buf.shadow.normal_bias;
  P += -Ng * normal_offset;

  vec2 thickness = vec2(0.0);

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    thickness_from_shadow_single(l_idx, true, P, Ng, thickness);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    thickness_from_shadow_single(l_idx, false, P, Ng, thickness);
  }
  LIGHT_FOREACH_END

  return (thickness.y > 0.0) ? square(thickness.x / thickness.y) : THICKNESS_NO_VALUE;
}
