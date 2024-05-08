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

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tracing_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)

void thickness_from_shadow_single(uint l_idx,
                                  const bool is_directional,
                                  vec3 P,
                                  vec3 Ng,
                                  float gbuffer_thickness,
                                  inout float thickness_accum,
                                  inout float weight_accum)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  LightVector lv = light_vector_get(light, is_directional, P);
  float attenuation = light_attenuation_surface(light, is_directional, true, false, Ng, lv);
  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return;
  }

  float texel_radius = shadow_texel_radius_at_position(light, is_directional, P);

  vec2 pcf_random = pcg4d(vec4(gl_FragCoord.xyz, sampling_rng_1D_get(SAMPLING_SHADOW_X))).xy;

  vec3 P_offset = P;
  /* Invert all biases to get value inside the surface.
   * The additional offset is to make the pcf kernel fully inside the object. */
  float normal_offset = shadow_normal_offset(Ng, lv.L);
  P_offset -= Ng * (texel_radius * normal_offset);
  /* Inverting this bias means we will over estimate the distance. Which removes some artifacts. */
  P_offset -= texel_radius * shadow_pcf_offset(lv.L, Ng, pcf_random);

  float occluder_delta = shadow_sample(
      is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P_offset);
  if (occluder_delta > 0.0) {
    float hit_distance = abs(occluder_delta);
    /* Add back the amount of offset we added to the original position.
     * This avoids self shadowing issue. */
    hit_distance += (normal_offset + 1.0) * texel_radius;

    if ((hit_distance > abs(gbuffer_thickness) * 0.001) &&
        (hit_distance < abs(gbuffer_thickness) * 1.0))
    {
      float weight = 1.0;
      saturate(dot(lv.L, -Ng));
      thickness_accum += hit_distance * weight;
      weight_accum += weight;
    }
  }
}

/**
 * Return the apparent thickness of an object behind surface considering all shadow maps
 * available. If no shadow-map has a record of the other side of the surface, this function
 * returns -1.
 */
float thickness_from_shadow(vec3 P, vec3 Ng, float vPz, float gbuffer_thickness)
{
  float thickness_accum = 0.0;
  float weight_accum = 0.0;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    thickness_from_shadow_single(
        l_idx, true, P, Ng, gbuffer_thickness, thickness_accum, weight_accum);
  }
  LIGHT_FOREACH_END

  vec2 pixel = gl_FragCoord.xy;
  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vPz, l_idx) {
    thickness_from_shadow_single(
        l_idx, false, P, Ng, gbuffer_thickness, thickness_accum, weight_accum);
  }
  LIGHT_FOREACH_END

  if (weight_accum == 0.0) {
    return -1.0;
  }

  float thickness = thickness_accum / weight_accum;
  /* Add a bias because it is usually too small to prevent self shadowing. */
  return thickness;
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  const float bias = 2.4e-7;
  depth -= bias;

  vec3 P = drw_point_screen_to_world(vec3(uvcoordsvar.xy, depth));
  float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  vec3 Ng = gbuffer_normal_unpack(imageLoad(gbuf_normal_img, ivec3(texel, 0)).rg);

  /* Use manual fetch because gbuffer_read_thickness expect a read only texture input. */
  uint header = texelFetch(gbuf_header_tx, texel, 0).r;
  int data_layer = gbuffer_normal_count(header);
  vec2 data_packed = imageLoad(gbuf_normal_img, ivec3(texel, data_layer)).rg;
  float gbuffer_thickness = gbuffer_thickness_unpack(data_packed.x);
  if (gbuffer_thickness == 0.0) {
    return;
  }

  float shadow_thickness = thickness_from_shadow(P, Ng, vPz, gbuffer_thickness);
  if (shadow_thickness <= 0.0) {
    return;
  }

  if ((shadow_thickness < abs(gbuffer_thickness))) {
    data_packed.x = gbuffer_thickness_pack(sign(gbuffer_thickness) * shadow_thickness);
    imageStore(gbuf_normal_img, ivec3(texel, data_layer), vec4(data_packed, 0.0, 0.0));
  }
}
