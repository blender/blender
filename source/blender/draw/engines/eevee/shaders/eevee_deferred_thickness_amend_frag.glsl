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

#include "infos/eevee_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_deferred_thickness_amend)

#include "draw_view_lib.glsl"
#include "eevee_gbuffer_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_shadow_tracing_lib.glsl"

void thickness_from_shadow_single(uint l_idx,
                                  const bool is_directional,
                                  float3 P,
                                  float3 Ng,
                                  float gbuffer_thickness,
                                  inout float thickness_accum,
                                  inout float weight_accum)
{
  LightData light = light_buf[l_idx];

  if (light.tilemap_index == LIGHT_NO_SHADOW) {
    return;
  }

  LightVector lv = light_vector_get(light, is_directional, P);
  float attenuation = light_attenuation_surface(light, is_directional, lv);
  attenuation *= light_attenuation_facing(light, lv.L, lv.dist, -Ng, true);

  if (attenuation < LIGHT_ATTENUATION_THRESHOLD) {
    return;
  }

  float texel_radius = shadow_texel_radius_at_position(light, is_directional, P);

  float2 pcf_random = pcg4d(float4(gl_FragCoord.xyz, sampling_rng_1D_get(SAMPLING_SHADOW_X))).xy;

  float3 P_offset = P;
  /* Invert all biases to get value inside the surface.
   * The additional offset is to make the pcf kernel fully inside the object. */
  float normal_offset = shadow_normal_offset(Ng, lv.L, texel_radius);
  P_offset -= Ng * normal_offset;
  /* Inverting this bias means we will over estimate the distance. Which removes some artifacts. */
  P_offset -= texel_radius * shadow_pcf_offset(lv.L, Ng, pcf_random);

  float occluder_delta = shadow_sample(
      is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P_offset);
  if (occluder_delta > 0.0f) {
    float hit_distance = abs(occluder_delta);
    /* Add back the amount of offset we added to the original position.
     * This avoids self shadowing issue. */
    hit_distance += (normal_offset + 1.0f) * texel_radius;

    if ((hit_distance > abs(gbuffer_thickness) * 0.001f) &&
        (hit_distance < abs(gbuffer_thickness) * 1.0f))
    {
      float weight = 1.0f;
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
float thickness_from_shadow(float3 P, float3 Ng, float vPz, float gbuffer_thickness)
{
  float thickness_accum = 0.0f;
  float weight_accum = 0.0f;

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    thickness_from_shadow_single(
        l_idx, true, P, Ng, gbuffer_thickness, thickness_accum, weight_accum);
  }
  LIGHT_FOREACH_END

  float2 pixel = gl_FragCoord.xy;
  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vPz, l_idx) {
    thickness_from_shadow_single(
        l_idx, false, P, Ng, gbuffer_thickness, thickness_accum, weight_accum);
  }
  LIGHT_FOREACH_END

  if (weight_accum == 0.0f) {
    return -1.0f;
  }

  float thickness = thickness_accum / weight_accum;
  /* Add a bias because it is usually too small to prevent self shadowing. */
  return thickness;
}

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  constexpr float bias = 2.4e-7f;
  const float depth = texelFetch(hiz_tx, texel, 0).r - bias;

  const float3 P = drw_point_screen_to_world(float3(screen_uv, depth));
  const float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  const float3 Ng = gbuffer::normal_unpack(imageLoad(gbuf_normal_img, int3(texel, 0)).rg);

  /* Use manual fetch because gbuffer::read_thickness expect a read only texture input. */
  gbuffer::Header header = gbuffer::Header::from_data(
      texelFetch(gbuf_header_tx, int3(texel, 0), 0).r);

  uchar data_layer = header.closure_len();
  float2 data_packed = imageLoad(gbuf_normal_img, int3(texel, data_layer)).rg;
  float gbuffer_thickness = gbuffer::thickness_unpack(data_packed.x);
  if (gbuffer_thickness == 0.0f) {
    return;
  }

  float shadow_thickness = thickness_from_shadow(P, Ng, vPz, gbuffer_thickness);
  if (shadow_thickness <= 0.0f) {
    return;
  }

  if ((shadow_thickness < abs(gbuffer_thickness))) {
    data_packed.x = gbuffer::thickness_pack(sign(gbuffer_thickness) * shadow_thickness);
    imageStore(gbuf_normal_img, int3(texel, data_layer), float4(data_packed, 0.0f, 0.0f));
  }
}
