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

#include "draw_view.bsl.hh" /* IWYU pragma: export. For nodetree functions. */
#include "eevee_cryptomatte.bsl.hh"
#include "eevee_gbuffer_write.bsl.hh"
#include "eevee_nodetree_frag_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_surf_common.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"

float4 closure_to_rgba(KernelGlobals &kg, ShadingData &sd, Closure /*cl*/)
{
  float4 out_color;
  out_color.rgb = sd.emission;
  out_color.a = saturate(1.0f - average(sd.transmittance));

  if (!kg.pipe.is_occupancy_pipe) [[static_branch]] {
    /* Reset for the next closure tree. */
    float noise = kg.util_tx.fetch(sd.frag_co.xy, UTIL_BLUE_NOISE_LAYER).r;
    float closure_rand = fract(noise + kg.sampling.rng_1D_get(SAMPLING_CLOSURE));
    closure_weights_reset(kg, sd, closure_rand);
  }
  else {
    closure_weights_reset(kg, sd, 0.0f);
  }
  return out_color;
}

namespace eevee {

struct SurfaceDeferred {
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

struct DeferredFragOut {
  /* Direct output. (Emissive, Holdout) */
  [[frag_color(0)]] float4 radiance;

  [[frag_color(1), raster_order_group(DEFERRED_GBUFFER_ROG_ID)]] uint gbuf_header;
  [[frag_color(2)]] float2 gbuf_normal;
  [[frag_color(3)]] float4 gbuf_closure1;
  [[frag_color(4)]] float4 gbuf_closure2;
};

/* NOTE: This removes the possibility of using gl_FragDepth. */
[[fragment]] [[early_fragment_tests]]
void surf_deferred([[resource_table]] KernelGlobals &kg,
                   [[resource_table]] PipelineConstants &pipe,
                   [[resource_table]] SurfaceDeferred &srt,
                   [[resource_table]] gbuffer::PackParameters &gbuf_params,
                   [[resource_table]] RenderPassOutput &render_passes,
                   [[resource_table]] CryptomatteOutput &cryptomatte,
                   [[resource_table]] const draw::Infos &infos,
                   [[resource_table]] const draw::View &views,
                   [[resource_table]] const Uniform &uni,
                   [[resource_table]] const Sampling &sampling,
                   [[resource_table]] const UtilityTexture &util_tx,
                   [[in]] const VertOutCommon &interp,
                   [[in]] [[condition(is_curves)]] const VertOutCurves &curves_interp,
                   [[in]] [[condition(is_pointcloud)]] const VertOutPointcloud &ptcloud_interp,
                   [[frag_coord]] const float4 frag_co,
                   [[out]] DeferredFragOut &frag_out,
                   [[front_facing]] const bool front_face)
{
  draw::ID id{interp.resource_id_raw};
  const uint resource_id = id.resource_id<1>();

  const ViewMatrices view = views.get(0);

  ShadingData sd = init_globals(uni, interp, view, front_face, frag_co);
  if (pipe.is_mesh) [[static_branch]] {
    init_globals_mesh(interp, sd);
  }
  else if (pipe.is_curves) [[static_branch]] {
    init_globals_curves(interp, curves_interp, sd, view);
  }
  else if (pipe.is_pointcloud) [[static_branch]] {
    init_globals_pointcloud(ptcloud_interp, sd);
  }

  float noise = util_tx.fetch(frag_co.xy, UTIL_BLUE_NOISE_LAYER).r;
  float closure_rand = fract(noise + sampling.rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement(kg, sd);

  nodetree_surface(kg, sd, closure_rand);

  sd.holdout = saturate(sd.holdout);

  Thickness thickness = Thickness::from(nodetree_thickness(kg, sd),
                                        ThicknessMode(kg.nt.node_tree.thickness_mode));

  /** Transparency weight is already applied through dithering, remove it from other closures. */
  float alpha = 1.0f - average(sd.transmittance);
  float alpha_rcp = safe_rcp(alpha);

  /* Object holdout. */
  eObjectInfoFlag ob_flag = kg.object_infos_get(sd).flag;
  if (flag_test(ob_flag, OBJECT_HOLDOUT)) {
    /* alpha is set from rejected pixels / dithering. */
    sd.holdout = 1.0f;

    /* Set alpha to 0.0 so that lighting is not computed. */
    alpha_rcp = 0.0f;
  }

  sd.emission *= alpha_rcp;

  int2 out_texel = int2(frag_co.xy);

  ObjectInfos object_infos = infos.get(resource_id);
  bool use_light_linking = receiver_light_set_get(object_infos) != 0;
  bool use_terminator_offset = object_infos.shadow_terminator_normal_offset > 0.0;

  /* ----- Render Passes output ----- */

  /* Some render pass can be written during the gbuffer pass. Light passes are written later. */
  cryptomatte.store(out_texel, kg.nt.node_tree.crypto_hash, resource_id);
  render_passes.store_color(
      out_texel, uni.uniform_buf.render_pass.emission_id, float4(sd.emission, 1.0f));

  /* ----- GBuffer output ----- */

  gbuffer::InputClosures gbuf_data;
  /* Make sure we do not read uninitialized data (see #159161). */
  if (pipe.closure_bin_count == 0) [[static_branch]] {
    gbuf_data.closure[0] = ClosureUndetermined{};
  }
  for (int i = 0; i < 3; i++) [[unroll]] {
    if (pipe.closure_bin_count > i) [[static_branch]] {
      gbuf_data.closure[i] = sd.closure_get_resolved(i, alpha_rcp);
    }
  }
  const bool use_object_id = pipe.use_sss || use_light_linking || use_terminator_offset;

  float3 gbuffer_dither = sampling.rng_3D_get(SAMPLING_GBUFFER_U);
  gbuffer::Packed gbuf = gbuffer::pack(
      gbuf_params, gbuf_data, sd.Ng, sd.N, thickness, use_object_id);

  /* Output header and first closure using frame-buffer attachment. */
  frag_out.gbuf_header = gbuf.header;
  frag_out.gbuf_closure1 = gbuffer::closure_data_layer_dither_round_to_nearest(
      gbuf.closure[0], frag_co.xy, 0u, gbuffer_dither);
  frag_out.gbuf_closure2 = gbuffer::closure_data_layer_dither_round_to_nearest(
      gbuf.closure[1], frag_co.xy, 1u, gbuffer_dither);
  frag_out.gbuf_normal = gbuf.normal[0];

  /* Output remaining closures using image store. */
  if (gbuf_params.gbuffer_layer_max >= 2) [[static_branch]] {
    if (!gbuf_params.gbuffer_simple_layout) [[static_branch]] {
      if (flag_test(gbuf.used_layers, CLOSURE_DATA_2)) {
        srt.write_closure_data(out_texel,
                               2,
                               gbuffer::closure_data_layer_dither_flush_to_zero(
                                   gbuf.closure[2], frag_co.xy, 2u, gbuffer_dither));
      }
      if (flag_test(gbuf.used_layers, CLOSURE_DATA_3)) {
        srt.write_closure_data(out_texel,
                               3,
                               gbuffer::closure_data_layer_dither_flush_to_zero(
                                   gbuf.closure[3], frag_co.xy, 3u, gbuffer_dither));
      }
    }
    if (flag_test(gbuf.used_layers, NORMAL_DATA_1)) {
      srt.write_normal_data(out_texel, 1, gbuf.normal[1]);
    }
  }
  if (gbuf_params.gbuffer_layer_max >= 3) [[static_branch]] {
    if (flag_test(gbuf.used_layers, CLOSURE_DATA_4)) {
      srt.write_closure_data(out_texel,
                             4,
                             gbuffer::closure_data_layer_dither_flush_to_zero(
                                 gbuf.closure[4], frag_co.xy, 4u, gbuffer_dither));
    }
    if (flag_test(gbuf.used_layers, CLOSURE_DATA_5)) {
      srt.write_closure_data(out_texel,
                             5,
                             gbuffer::closure_data_layer_dither_flush_to_zero(
                                 gbuf.closure[5], frag_co.xy, 5u, gbuffer_dither));
    }
    if (flag_test(gbuf.used_layers, NORMAL_DATA_2)) {
      srt.write_normal_data(out_texel, 2, gbuf.normal[2]);
    }
  }

  if (pipe.use_additional_data) [[static_branch]] {
    if (flag_test(gbuf.used_layers, ADDITIONAL_DATA)) {
      srt.write_normal_data(
          out_texel, uni.pipeline_buf.gbuffer_additional_data_layer_id, gbuf.additional_info);
    }
  }

  if (flag_test(gbuf.used_layers, OBJECT_ID)) {
    srt.write_header_data(out_texel, 1, resource_id);
  }

  /* ----- Radiance output ----- */

  /* Only output emission during the gbuffer pass. */
  frag_out.radiance = float4(sd.emission, 0.0f);
  frag_out.radiance.rgb *= 1.0f - sd.holdout;
  frag_out.radiance.a = sd.holdout;
}

}  // namespace eevee
