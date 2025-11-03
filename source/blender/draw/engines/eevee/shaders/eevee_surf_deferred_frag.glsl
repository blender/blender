/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Deferred lighting evaluation: Lighting is evaluated in a separate pass.
 *
 * Outputs shading parameter per pixel using a randomized set of BSDFs.
 * Some render-pass are written during this pass.
 */

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_surf_deferred_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_node_tree)
FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)
FRAGMENT_SHADER_CREATE_INFO(eevee_surf_deferred)
FRAGMENT_SHADER_CREATE_INFO(eevee_render_pass_out)
FRAGMENT_SHADER_CREATE_INFO(eevee_cryptomatte_out)

#include "draw_curves_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_gbuffer_write_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_surf_lib.glsl"

float4 closure_to_rgba(Closure cl)
{
  float4 out_color;
  out_color.rgb = g_emission;
  out_color.a = saturate(1.0f - average(g_transmittance));

  /* Reset for the next closure tree. */
  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));
  closure_weights_reset(closure_rand);

  return out_color;
}

void write_closure_data(int2 texel, int layer, float4 data)
{
  /* NOTE: The image view start at layer GBUF_CLOSURE_FB_LAYER_COUNT so all destination layer is
   * `layer - GBUF_CLOSURE_FB_LAYER_COUNT`. */
  imageStoreFast(out_gbuf_closure_img, int3(texel, layer - GBUF_CLOSURE_FB_LAYER_COUNT), data);
}

void write_normal_data(int2 texel, int layer, float2 data)
{
  /* NOTE: The image view start at layer GBUF_NORMAL_FB_LAYER_COUNT so all destination layer is
   * `layer - GBUF_NORMAL_FB_LAYER_COUNT`. */
  imageStoreFast(out_gbuf_normal_img, int3(texel, layer - GBUF_NORMAL_FB_LAYER_COUNT), data.xyyy);
}

void write_header_data(int2 texel, int layer, uint data)
{
  /* NOTE: The image view start at layer GBUF_HEADER_FB_LAYER_COUNT so all destination layer is
   * `layer - GBUF_HEADER_FB_LAYER_COUNT`. */
  imageStoreFast(
      out_gbuf_header_img, int3(texel, layer - GBUF_HEADER_FB_LAYER_COUNT), uint4(data));
}

void main()
{
  init_globals();

  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement();

  nodetree_surface(closure_rand);

  g_holdout = saturate(g_holdout);

  float thickness = nodetree_thickness() * thickness_mode;

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

  int2 out_texel = int2(gl_FragCoord.xy);

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

  gbuffer::Packed gbuf = gbuffer::pack(gbuf_data, g_data.Ng, g_data.N, thickness, use_object_id);

  /* Output header and first closure using frame-buffer attachment. */
  out_gbuf_header = gbuf.header;
  out_gbuf_closure1 = gbuf.closure[0];
  out_gbuf_closure2 = gbuf.closure[1];
  out_gbuf_normal = gbuf.normal[0];

  /* Output remaining closures using image store. */
#if GBUFFER_LAYER_MAX >= 2 && !defined(GBUFFER_SIMPLE_CLOSURE_LAYOUT)
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_2)) {
    write_closure_data(out_texel, 2, gbuf.closure[2]);
  }
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_3)) {
    write_closure_data(out_texel, 3, gbuf.closure[3]);
  }
#endif
#if GBUFFER_LAYER_MAX >= 3
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_4)) {
    write_closure_data(out_texel, 4, gbuf.closure[4]);
  }
  if (flag_test(gbuf.used_layers, CLOSURE_DATA_5)) {
    write_closure_data(out_texel, 5, gbuf.closure[5]);
  }
#endif

#if GBUFFER_LAYER_MAX >= 2
  if (flag_test(gbuf.used_layers, NORMAL_DATA_1)) {
    write_normal_data(out_texel, 1, gbuf.normal[1]);
  }
#endif
#if GBUFFER_LAYER_MAX >= 3
  if (flag_test(gbuf.used_layers, NORMAL_DATA_2)) {
    write_normal_data(out_texel, 2, gbuf.normal[2]);
  }
#endif

#if defined(GBUFFER_HAS_REFRACTION) || defined(GBUFFER_HAS_SUBSURFACE) || \
    defined(GBUFFER_HAS_TRANSLUCENT)
  if (flag_test(gbuf.used_layers, ADDITIONAL_DATA)) {
    write_normal_data(
        out_texel, uniform_buf.pipeline.gbuffer_additional_data_layer_id, gbuf.additional_info);
  }
#endif

  if (flag_test(gbuf.used_layers, OBJECT_ID)) {
    write_header_data(out_texel, 1, drw_resource_id());
  }

  /* ----- Radiance output ----- */

  /* Only output emission during the gbuffer pass. */
  out_radiance = float4(g_emission, 0.0f);
  out_radiance.rgb *= 1.0f - g_holdout;
  out_radiance.a = g_holdout;
}
