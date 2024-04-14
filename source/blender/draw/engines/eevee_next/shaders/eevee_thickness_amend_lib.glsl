/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Amend thickness after object rasterization using shadow-maps.
 *
 * Required resources:
 * - atlas_tx
 * - tilemaps_tx
 */

#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

/* If using compute, the shader should define its own pixel. */
#if !defined(PIXEL) && defined(GPU_FRAGMENT_SHADER)
#  define PIXEL gl_FragCoord.xy
#endif

void thickness_from_shadow_single(uint l_idx,
                                  const bool is_directional,
                                  vec3 P,
                                  vec3 Ng,
                                  inout float thickness_accum,
                                  inout float weight_accum)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  LightVector lv = light_vector_get(light, is_directional, P);
  float attenuation = light_attenuation_surface(light, is_directional, true, false, Ng, lv);
  if ((attenuation < LIGHT_ATTENUATION_THRESHOLD)) {
    return;
  }
  /* Weight result by facing ratio to avoid harsh transitions. */
  float weight = saturate(dot(lv.L, -Ng));
  ShadowEvalResult result = shadow_sample(
      is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P);

  if (result.light_visibilty == 0.0) {
    /* Flatten the accumulation to avoid weighting the outliers too much. */
    thickness_accum += safe_sqrt(result.occluder_distance) * weight;
    weight_accum += weight;
  }
}

#define THICKNESS_NO_VALUE -1.0
/**
 * Return the apparent thickness of an object behind surface considering all shadow maps
 * available. If no shadow-map has a record of the other side of the surface, this function
 * returns THICKNESS_NO_VALUE.
 */
float thickness_from_shadow(vec3 P, vec3 Ng, float vPz)
{
  float thickness_accum = 0.0;
  float weight_accum = 0.0;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    thickness_from_shadow_single(l_idx, true, P, Ng, thickness_accum, weight_accum);
  }
  LIGHT_FOREACH_END

  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, PIXEL, vPz, l_idx) {
    thickness_from_shadow_single(l_idx, false, P, Ng, thickness_accum, weight_accum);
  }
  LIGHT_FOREACH_END

  if (weight_accum == 0.0) {
    return THICKNESS_NO_VALUE;
  }

  float thickness = thickness_accum / weight_accum;
  /* Flatten the accumulation to avoid weighting the outliers too much. */
  thickness = square(thickness);

  /* Add a bias because it is usually too small to prevent self shadowing. */
  return thickness;
}
