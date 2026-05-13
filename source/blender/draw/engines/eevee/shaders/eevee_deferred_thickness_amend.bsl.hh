/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Amend thickness after object rasterization using shadow-maps.
 */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_light_infos.hh"
#include "infos/eevee_shadow_infos.hh"

SHADER_LIBRARY_CREATE_INFO(draw_view)
SHADER_LIBRARY_CREATE_INFO(eevee_hiz_data)
SHADER_LIBRARY_CREATE_INFO(eevee_shadow_data)
SHADER_LIBRARY_CREATE_INFO(eevee_light_data)

#include "draw_view_lib.glsl"
#include "eevee_gbuffer_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_shadow_tracing_lib.glsl"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee::thickness {

struct FromShadowEvalCtx {
  float3 P;
  float3 Ng;
  Thickness gbuffer_thickness;
  float thickness_accum;
  float weight_accum;
  float2 pcf_random;

  void eval(LightData light, const bool is_directional)
  {
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

    float3 P_offset = P;
    /* Invert all biases to get value inside the surface.
     * The additional offset is to make the pcf kernel fully inside the object. */
    float normal_offset = shadow_normal_offset(Ng, lv.L, texel_radius);
    P_offset -= Ng * normal_offset;
    /* Inverting this bias means we will over estimate the distance. Which removes some artifacts.
     */
    P_offset -= texel_radius * shadow_pcf_offset(lv.L, Ng, pcf_random);

    float occluder_delta = shadow_sample(
        is_directional, shadow_atlas_tx, shadow_tilemaps_tx, light, P_offset);
    if (occluder_delta > 0.0f) {
      float hit_distance = abs(occluder_delta);
      /* Add back the amount of offset we added to the original position.
       * This avoids self shadowing issue. */
      hit_distance += (normal_offset + 1.0f) * texel_radius;

      if ((hit_distance > gbuffer_thickness.value() * 0.001f) &&
          (hit_distance < gbuffer_thickness.value() * 1.0f))
      {
        float weight = 1.0f;
        saturate(dot(lv.L, -Ng));
        thickness_accum += hit_distance * weight;
        weight_accum += weight;
      }
    }
  }

  void eval_directional(uint /*l_idx*/, LightData light)
  {
    eval(light, true);
  }
  void eval_local(uint /*l_idx*/, LightData light)
  {
    eval(light, false);
  }
};

}  // namespace eevee::thickness

namespace eevee {

struct ThicknessAmend {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo eevee_light_data;

  [[sampler(0)]] usampler2DArray gbuf_header_tx;
  [[image(0, read_write, UNORM_16_16)]] image2DArray gbuf_normal_img;
};

struct VertOut {
  [[smooth]] float2 uv;
};

[[vertex]]
void amend_vert([[vertex_id]] const int vert_id,
                [[out]] VertOut &v_out,
                [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position, v_out.uv);
}

/* Early fragment test is needed to discard fragment that do not need this processing. */
[[fragment]] [[early_fragment_tests]]
void amend_frag([[resource_table]] ThicknessAmend &srt,
                [[in]] const VertOut &v_out,
                [[frag_coord]] const float4 frag_co)
{
  int2 texel = int2(frag_co.xy);

  /* Bias the shading point position because of depth buffer precision.
   * Constant is taken from https://www.terathon.com/gdc07_lengyel.pdf. */
  constexpr float bias = 2.4e-7f;
  const float depth = texelFetch(hiz_tx, texel, 0).r - bias;

  const float3 P = drw_point_screen_to_world(float3(v_out.uv, depth));
  const float vPz = dot(drw_view_forward(), P) - dot(drw_view_forward(), drw_view_position());

  const float3 Ng = gbuffer::normal_unpack(imageLoad(srt.gbuf_normal_img, int3(texel, 0)).rg);

  uchar data_layer = pipeline_buf.gbuffer_additional_data_layer_id;
  float2 data_packed = imageLoad(srt.gbuf_normal_img, int3(texel, int(data_layer))).rg;
  Thickness gbuffer_thickness = gbuffer::thickness_unpack(data_packed.x);
  if (gbuffer_thickness.value() == 0.0f) {
    return;
  }

  thickness::FromShadowEvalCtx ctx = {
      .P = P,
      .Ng = Ng,
      .gbuffer_thickness = gbuffer_thickness,
      .thickness_accum = 0.0f,
      .weight_accum = 0.0f,
      .pcf_random = pcg4d(float4(frag_co.xyz, sampling_rng_1D_get(SAMPLING_SHADOW_X))).xy,
  };

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    LightData light = light_buf[l_idx];
    ctx.eval_directional(l_idx, light);
  }
  LIGHT_FOREACH_END

  float2 pixel = frag_co.xy;
  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, pixel, vPz, l_idx) {
    LightData light = light_buf[l_idx];
    ctx.eval_local(l_idx, light);
  }
  LIGHT_FOREACH_END

  /* The apparent thickness of an object behind its surface considering all shadow maps
   * available. If no shadow-map has a record of the other side of the surface, do not amend
   * the gbuffer thickness. */
  if (ctx.weight_accum == 0.0f) {
    return;
  }

  float shadow_thickness = ctx.thickness_accum / ctx.weight_accum;
  if (shadow_thickness < gbuffer_thickness.value()) {
    data_packed.x = gbuffer::thickness_pack(
        Thickness::from(shadow_thickness, gbuffer_thickness.mode()));
    imageStore(srt.gbuf_normal_img, int3(texel, int(data_layer)), float4(data_packed, 0.0f, 0.0f));
  }
}

PipelineGraphic deferred_thickness_amend(amend_vert, amend_frag);

}  // namespace eevee
