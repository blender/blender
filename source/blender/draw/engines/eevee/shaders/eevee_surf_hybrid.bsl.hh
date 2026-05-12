/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Deferred lighting evaluation: Lighting is evaluated in a separate pass.
 *
 * Outputs shading parameter per pixel using a randomized set of BSDFs.
 * Some render-pass are written during this pass.
 */

#pragma once

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_nodetree_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_nodetree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)
FRAGMENT_SHADER_CREATE_INFO(eevee_render_pass_out)
FRAGMENT_SHADER_CREATE_INFO(eevee_cryptomatte_out)

#include "draw_curves_lib.glsl" /* IWYU pragma: export. For nodetree functions. */
#include "draw_view_lib.glsl"   /* IWYU pragma: export. For nodetree functions. */
#include "eevee_forward_lib.glsl"
#include "eevee_gbuffer_write_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"

/* Global thickness because it is needed for closure_to_rgba. */
Thickness g_thickness;

float4 closure_to_rgba_hybrid(Closure /*cl*/)
{
  float3 radiance, transmittance;
  forward_lighting_eval(g_thickness, radiance, transmittance);

  /* Reset for the next closure tree. */
  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));
  closure_weights_reset(closure_rand);

#if defined(MAT_TRANSPARENT) && defined(MAT_SHADER_TO_RGBA)
  float3 V = -drw_world_incident_vector(g_data.P);
  LightProbeSample samp = lightprobe_load(gl_FragCoord.xy, g_data.P, g_data.Ng, V);
  float3 radiance_behind = lightprobe_spherical_sample_normalized_with_parallax(
      samp, g_data.P, V, 0.0);

#  ifndef MAT_FIRST_LAYER
  int2 texel = int2(gl_FragCoord.xy);
  if (texelFetchExtend(hiz_prev_tx, texel, 0).x != 1.0f) {
    radiance_behind = texelFetch(previous_layer_radiance_tx, texel, 0).xyz;
  }
#  endif

  radiance += radiance_behind * saturate(transmittance);
#endif

  return float4(radiance, saturate(1.0f - average(transmittance)));
}

namespace eevee {

struct SurfaceHybrid {
  /* Added at runtime because of test shaders not having `node_tree`. */
  // [[legacy_info]] ShaderCreateInfo eevee_render_pass_out;
  // [[legacy_info]] ShaderCreateInfo eevee_cryptomatte_out;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo draw_view_culling;

  /* For closure_to_rgba. */
  [[legacy_info]] ShaderCreateInfo eevee_light_data;
  [[legacy_info]] ShaderCreateInfo eevee_lightprobe_data;
  [[legacy_info]] ShaderCreateInfo eevee_shadow_data;

  /* Everything is stored inside a two layered target, one for each format. This is to fit the
   * limitation of the number of images we can bind on a single shader. */
  [[image(GBUF_CLOSURE_SLOT, write, UNORM_10_10_10_2)]] image2DArray gbuf_closure_img;
  [[image(GBUF_NORMAL_SLOT, write, UNORM_16_16)]] image2DArray gbuf_normal_img;
  /* Storage for additional infos that are shared across closures. */
  [[image(GBUF_HEADER_SLOT, write, UINT_32)]] uimage2DArray gbuf_header_img;

  void write_closure_data(int2 texel, int layer, float4 data)
  {
    /* NOTE: The image view start at layer GBUF_CLOSURE_FB_LAYER_COUNT so all destination layer is
     * `layer - GBUF_CLOSURE_FB_LAYER_COUNT`. */
    imageStoreFast(gbuf_closure_img, int3(texel, layer - GBUF_CLOSURE_FB_LAYER_COUNT), data);
  }

  void write_normal_data(int2 texel, int layer, float2 data)
  {
    /* NOTE: The image view start at layer GBUF_NORMAL_FB_LAYER_COUNT so all destination layer is
     * `layer - GBUF_NORMAL_FB_LAYER_COUNT`. */
    imageStoreFast(gbuf_normal_img, int3(texel, layer - GBUF_NORMAL_FB_LAYER_COUNT), data.xyyy);
  }

  void write_header_data(int2 texel, int layer, uint data)
  {
    /* NOTE: The image view start at layer GBUF_HEADER_FB_LAYER_COUNT so all destination layer is
     * `layer - GBUF_HEADER_FB_LAYER_COUNT`. */
    imageStoreFast(gbuf_header_img, int3(texel, layer - GBUF_HEADER_FB_LAYER_COUNT), uint4(data));
  }
};

struct HybridFragOut {
  /* Direct output. (Emissive, Holdout) */
  [[frag_color(0)]] float4 radiance;

  [[frag_color(1), raster_order_group(DEFERRED_GBUFFER_ROG_ID)]] uint gbuf_header;
  [[frag_color(2)]] float2 gbuf_normal;
  [[frag_color(3)]] float4 gbuf_closure1;
  [[frag_color(4)]] float4 gbuf_closure2;
};

/* NOTE: This removes the possibility of using gl_FragDepth. */
[[fragment]] [[early_fragment_tests]]
void surf_hybrid([[resource_table]] SurfaceHybrid &srt,
                 [[frag_coord]] const float4 frag_co,
                 [[out]] HybridFragOut &frag_out)
{
  init_globals();

  float noise = utility_tx_fetch(utility_tx, frag_co.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  g_thickness = Thickness::from(nodetree_thickness(), thickness_mode);

  fragment_displacement();

  nodetree_surface(closure_rand);

  g_holdout = saturate(g_holdout);

  /** Transparency weight is already applied through dithering, remove it from other closures. */
  float alpha = 1.0f - average(g_transmittance);
  float alpha_rcp = safe_rcp(alpha);

  /* Object holdout. */
  eObjectInfoFlag ob_flag = drw_object_infos().flag;
  if (flag_test(ob_flag, OBJECT_HOLDOUT)) {
    /* alpha is set from rejected pixels / dithering. */
    g_holdout = 1.0f;

    /* Set alpha to 0.0 so that lighting is not computed. */
    alpha_rcp = 0.0f;
  }

  g_emission *= alpha_rcp;

  int2 out_texel = int2(frag_co.xy);

#ifdef MAT_SUBSURFACE
  constexpr bool use_sss = true;
#else
  constexpr bool use_sss = false;
#endif

  ObjectInfos object_infos = drw_infos[drw_resource_id()];
  bool use_light_linking = receiver_light_set_get(object_infos) != 0;
  bool use_terminator_offset = object_infos.shadow_terminator_normal_offset > 0.0;

  /* ----- Render Passes output ----- */

#ifdef MAT_RENDER_PASS_SUPPORT /* Needed because node_tree isn't present in test shaders. */
  /* Some render pass can be written during the gbuffer pass. Light passes are written later. */
  if (imageSize(rp_cryptomatte_img).x > 1) {
    float4 cryptomatte_output = float4(
        cryptomatte_object_buf[drw_resource_id()], node_tree.crypto_hash, 0.0f);
    imageStoreFast(rp_cryptomatte_img, out_texel, cryptomatte_output);
  }
  output_renderpass_color(uniform_buf.render_pass.emission_id, float4(g_emission, 1.0f));
#endif

  /* ----- GBuffer output ----- */

  gbuffer::InputClosures gbuf_data;
  gbuf_data.closure[0] = g_closure_get_resolved(0, alpha_rcp);
#if CLOSURE_BIN_COUNT > 1
  gbuf_data.closure[1] = g_closure_get_resolved(1, alpha_rcp);
#endif
#if CLOSURE_BIN_COUNT > 2
  gbuf_data.closure[2] = g_closure_get_resolved(2, alpha_rcp);
#endif
  const bool use_object_id = use_sss || use_light_linking || use_terminator_offset;

  gbuffer::Packed gbuf = gbuffer::pack(gbuf_data, g_data.Ng, g_data.N, g_thickness, use_object_id);

  /* Output header and first closure using frame-buffer attachment. */
  frag_out.gbuf_header = gbuf.header;
  frag_out.gbuf_closure1 = gbuf.closure[0];
  frag_out.gbuf_closure2 = gbuf.closure[1];
  frag_out.gbuf_normal = gbuf.normal[0];

  /* Output remaining closures using image store. */
#if GBUFFER_LAYER_MAX >= 2 && !defined(GBUFFER_SIMPLE_CLOSURE_LAYOUT)
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_2)) {
    srt.write_closure_data(out_texel, 2, gbuf.closure[2]);
  }
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_3)) {
    srt.write_closure_data(out_texel, 3, gbuf.closure[3]);
  }
#endif
#if GBUFFER_LAYER_MAX >= 3
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_4)) {
    srt.write_closure_data(out_texel, 4, gbuf.closure[4]);
  }
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_5)) {
    srt.write_closure_data(out_texel, 5, gbuf.closure[5]);
  }
#endif

#if GBUFFER_LAYER_MAX >= 2
  if (flag_test(gbuf.used_layers, NORMAL_DATA_1)) {
    srt.write_normal_data(out_texel, 1, gbuf.normal[1]);
  }
#endif
#if GBUFFER_LAYER_MAX >= 3
  if (flag_test(gbuf.used_layers, NORMAL_DATA_2)) {
    srt.write_normal_data(out_texel, 2, gbuf.normal[2]);
  }
#endif

#if defined(GBUFFER_HAS_REFRACTION) || defined(GBUFFER_HAS_SUBSURFACE) || \
    defined(GBUFFER_HAS_TRANSLUCENT)
  if (flag_test(gbuf.used_layers, ADDITIONAL_DATA)) {
    srt.write_normal_data(
        out_texel, pipeline_buf.gbuffer_additional_data_layer_id, gbuf.additional_info);
  }
#endif

  if (flag_test(gbuf.used_layers, OBJECT_ID)) {
    srt.write_header_data(out_texel, 1, drw_resource_id());
  }

  /* ----- Radiance output ----- */

  /* Only output emission during the gbuffer pass. */
  frag_out.radiance = float4(g_emission, 0.0f);
  frag_out.radiance.rgb *= 1.0f - g_holdout;
  frag_out.radiance.a = g_holdout;
}
}  // namespace eevee
